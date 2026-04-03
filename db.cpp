#include "db.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QDir>

static const char *DB_HOST_KEY = "db_host";

// Портабельный конфиг: config.ini рядом с exe
// Для 1000+ устройств: на каждом клиенте в config.ini задайте один и тот же db_host (IP сервера MySQL).
static QSettings* portableSettings()
{
    static QSettings* s = nullptr;
    if (!s) {
        QString path = QCoreApplication::applicationDirPath() + "/config.ini";
        s = new QSettings(path, QSettings::IniFormat);
    }
    return s;
}

QString getDbHost()
{
    QString h = portableSettings()->value(DB_HOST_KEY, "localhost").toString();
    return h.trimmed().isEmpty() ? "localhost" : h;
}

bool connectToDB(QString *outError)
{
    QString host = getDbHost();
    int port = 3306;
    // Поддержка "host:port" в config без изменений UI.
    const int colonPos = host.lastIndexOf(':');
    if (colonPos > 0 && host.indexOf(':') == colonPos) {
        bool okPort = false;
        const int parsedPort = host.mid(colonPos + 1).toInt(&okPort);
        if (okPort && parsedPort > 0 && parsedPort <= 65535) {
            port = parsedPort;
            host = host.left(colonPos).trimmed();
        }
    }
    if (!QSqlDatabase::isDriverAvailable("QMYSQL")) {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QString pluginPath = appDir + "/sqldrivers/qsqlmysql.dll";
        const QString mysqlClient = appDir + "/libmysql.dll";
        QString err = QString(
            "QMYSQL driver not loaded.\n"
            "Не найден/не загружен драйвер MySQL.\n\n"
            "Проверьте файлы рядом с .exe:\n"
            "1) %1\n"
            "2) %2\n\n"
            "Если файлов нет — скопируйте их через windeployqt и добавьте libmysql.dll той же архитектуры (x64/x86).\n\n"
            "Где обычно лежат файлы по умолчанию:\n"
            "- qsqlmysql.dll:\n"
            "  C:/Qt/5.15.2/<kit>/plugins/sqldrivers/qsqlmysql.dll\n"
            "  C:/Qt/6.x.x/<kit>/plugins/sqldrivers/qsqlmysql.dll\n"
            "- libmysql.dll:\n"
            "  C:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.dll\n"
            "  C:/Program Files/MySQL/MySQL Server 8.4/lib/libmysql.dll"
        ).arg(pluginPath, mysqlClient);
        qDebug() << err;
        if (outError) *outError = err;
        return false;
    }
    QSqlDatabase db;
    if (QSqlDatabase::contains("main_connection")) {
        db = QSqlDatabase::database("main_connection");
        if (db.isOpen())
            db.close();
    } else {
        db = QSqlDatabase::addDatabase("QMYSQL", "main_connection");
    }
    db.setHostName(host);
    db.setPort(port);
    db.setDatabaseName("agv_manager_db");
    db.setUserName("root");
    db.setPassword("");
    db.setConnectOptions("MYSQL_OPT_RECONNECT=1;MYSQL_OPT_CONNECT_TIMEOUT=5;MYSQL_OPT_READ_TIMEOUT=8;MYSQL_OPT_WRITE_TIMEOUT=8");

    if (!db.open()) {
        QString err = QString("host=%1 port=%2 | %3")
                          .arg(host)
                          .arg(port)
                          .arg(db.lastError().text());
        qDebug() << "Ошибка подключения к базе данных:" << err;
        if (outError) *outError = err;
        return false;
    }
    QSqlQuery setCharset(db);
    setCharset.exec("SET NAMES utf8mb4");

    // Лёгкая авто-оптимизация чтения по календарю/уведомлениям/чатам.
    static bool perfIndexesEnsured = false;
    if (!perfIndexesEnsured) {
        perfIndexesEnsured = true;
        QSqlQuery idx(db);
        auto tryExec = [&](const QString &sql) {
            if (!idx.exec(sql)) {
                const QString e = idx.lastError().text();
                // Дубликат индекса/отсутствие таблицы на старой БД не считаем фатальной ошибкой.
                if (!e.contains("1061") && !e.contains("Duplicate key name", Qt::CaseInsensitive)
                    && !e.contains("already exists", Qt::CaseInsensitive)
                    && !e.contains("1146") && !e.contains("doesn't exist", Qt::CaseInsensitive)) {
                    qDebug() << "DB index optimize warning:" << e << "SQL:" << sql;
                }
            }
        };
        tryExec("ALTER TABLE agv_tasks ADD INDEX idx_agv_tasks_next_date (next_date)");
        tryExec("ALTER TABLE agv_tasks ADD INDEX idx_agv_tasks_agv_next (agv_id, next_date)");
        tryExec("ALTER TABLE agv_tasks ADD INDEX idx_agv_tasks_assigned_to (assigned_to)");
        tryExec("ALTER TABLE agv_list ADD INDEX idx_agv_list_created_at (created_at)");
        tryExec("ALTER TABLE agv_task_history ADD INDEX idx_hist_done_task (completed_at, agv_id, task_name)");
        tryExec("ALTER TABLE notifications ADD INDEX idx_notifications_target_read (target_user, is_read, created_at)");
        tryExec("ALTER TABLE users ADD INDEX idx_users_full_name (full_name)");
        tryExec("ALTER TABLE task_chat_messages ADD INDEX idx_chat_messages_thread_created (thread_id, created_at)");
        tryExec("ALTER TABLE task_chat_messages ADD INDEX idx_chat_messages_thread_id_id (thread_id, id)");
        tryExec("ALTER TABLE task_chat_threads ADD INDEX idx_chat_threads_created_at (created_by, created_at)");
        tryExec("ALTER TABLE task_chat_threads ADD INDEX idx_chat_threads_recipient_at (recipient_user, created_at)");
        tryExec("ALTER TABLE task_chat_hidden ADD INDEX idx_chat_hidden_user_thread (username, thread_id)");
    }

    qDebug() << "Успешно подключились к базе данных!";
    return true;
}

bool reconnectWithHost(const QString &host, QString *outError)
{
    portableSettings()->setValue(DB_HOST_KEY, host.trimmed().isEmpty() ? "localhost" : host.trimmed());
    portableSettings()->sync();

    {
        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase("main_connection");

    return connectToDB(outError);
}

void testConnection()
{
    QSqlQuery query(QSqlDatabase::database("main_connection"));
    query.prepare("SELECT COUNT(*) FROM agv_models");
    if (!query.exec()) {
        qDebug() << "Ошибка выполнения запроса:" << query.lastError().text();
    } else {
        if (query.next()) {
            qDebug() << "Количество записей в таблице models_agv:" << query.value(0).toInt();
        }
    }
}
