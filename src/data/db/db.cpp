#include "db.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>
#include <QCoreApplication>
#include <QPluginLoader>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

static const char *KEY_HOST = "db_host";
static const char *KEY_PORT = "db_port";
static const char *KEY_NAME = "db_name";
static const char *KEY_USER = "db_user";
static const char *KEY_PASSWORD = "db_password";
static const char *KEY_DRIVER = "db_driver";

static void removeMainConnection()
{
    if (QSqlDatabase::contains("main_connection")) {
        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (db.isOpen())
            db.close();
        QSqlDatabase::removeDatabase("main_connection");
    }
}

static QString loadSqlPlugin(const QString &fileName)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QCoreApplication::addLibraryPath(appDir);
    QCoreApplication::addLibraryPath(appDir + QStringLiteral("/sqldrivers"));
    QCoreApplication::addLibraryPath(appDir + QStringLiteral("/plugins"));

    const QStringList candidates = {
        appDir + QStringLiteral("/sqldrivers/") + fileName,
        appDir + QStringLiteral("/plugins/sqldrivers/") + fileName,
    };
    for (const QString &path : candidates) {
        if (!QFileInfo::exists(path))
            continue;
        QPluginLoader loader(path);
        if (loader.load())
            return QString();
        return loader.errorString();
    }
    return QStringLiteral("file not found: ") + fileName;
}

static bool openOdbcConnection(const QString &host, int port, const QString &dbName,
                               const QString &user, const QString &password, QString &errOut)
{
    const QString pluginErr = loadSqlPlugin(QStringLiteral("qsqlodbc.dll"));
    if (!pluginErr.isEmpty() && !QSqlDatabase::isDriverAvailable(QStringLiteral("QODBC"))) {
        errOut = QStringLiteral("qsqlodbc.dll: %1 | drivers: %2")
                     .arg(pluginErr, QSqlDatabase::drivers().join(QLatin1Char(',')));
        return false;
    }

    const QStringList driverNames = {
        QStringLiteral("PostgreSQL ODBC Driver(UNICODE)"),
        QStringLiteral("PostgreSQL ODBC Driver(ANSI)"),
    };

    removeMainConnection();
    for (const QString &driverName : driverNames) {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QODBC"), QStringLiteral("main_connection"));
        const QString conn = QStringLiteral("DRIVER={%1};SERVER=%2;PORT=%3;DATABASE=%4;UID=%5;PWD=%6;")
                                 .arg(driverName, host)
                                 .arg(port)
                                 .arg(dbName, user, password);
        db.setDatabaseName(conn);
        if (db.open())
            return true;
        errOut = db.lastError().text();
        if (!errOut.contains(QStringLiteral("Driver not loaded"), Qt::CaseInsensitive))
            errOut += QStringLiteral(" [") + driverName + QLatin1Char(']');
        removeMainConnection();
    }
    return false;
}

static QSettings *portableSettings()
{
    static QSettings *s = nullptr;
    if (!s) {
        const QString path = QCoreApplication::applicationDirPath() + "/config.ini";
        s = new QSettings(path, QSettings::IniFormat);
    }
    return s;
}

QString getDbHost()
{
    QString h = portableSettings()->value(KEY_HOST, "localhost").toString().trimmed();
    const int colonPos = h.lastIndexOf(':');
    if (colonPos > 0 && h.indexOf(':') == colonPos) {
        bool ok = false;
        const int p = h.mid(colonPos + 1).toInt(&ok);
        if (ok && p > 0 && p <= 65535)
            h = h.left(colonPos).trimmed();
    }
    return h.isEmpty() ? QStringLiteral("localhost") : h;
}

int getDbPort()
{
    QSettings *s = portableSettings();
    if (s->contains(KEY_PORT))
        return s->value(KEY_PORT, 5432).toInt();

    QString h = s->value(KEY_HOST, "localhost").toString();
    const int colonPos = h.lastIndexOf(':');
    if (colonPos > 0 && h.indexOf(':') == colonPos) {
        bool ok = false;
        const int p = h.mid(colonPos + 1).toInt(&ok);
        if (ok && p > 0 && p <= 65535)
            return p;
    }
    return 5432;
}

QString getDbName()
{
    const QString n = portableSettings()->value(KEY_NAME, "agv_manager_db").toString().trimmed();
    return n.isEmpty() ? QStringLiteral("agv_manager_db") : n;
}

QString getDbUser()
{
    const QString u = portableSettings()->value(KEY_USER, "vapmanager").toString().trimmed();
    return u.isEmpty() ? QStringLiteral("vapmanager") : u;
}

QString getDbPassword()
{
    const QString fromCfg = portableSettings()->value(KEY_PASSWORD).toString().trimmed();
    if (!fromCfg.isEmpty())
        return fromCfg;
    return QStringLiteral("51525354");
}

bool connectToDB(QString *outError)
{
    const QString host = getDbHost();
    const int port = getDbPort();
    const QString dbName = getDbName();
    const QString user = getDbUser();
    const QString password = getDbPassword();
    const QString driverPref = portableSettings()->value(KEY_DRIVER, "odbc").toString().trimmed().toLower();

    removeMainConnection();

    QSqlDatabase db;
    QString connectLabel;

    const auto fail = [&](const QString &details) {
        const QString err = QString("host=%1 port=%2 db=%3 user=%4 | %5")
                                .arg(host)
                                .arg(port)
                                .arg(dbName)
                                .arg(user)
                                .arg(details);
        qDebug() << "Ошибка подключения к PostgreSQL:" << err;
        if (outError)
            *outError = err;
        removeMainConnection();
        return false;
    };

    if (driverPref == QLatin1String("psql") || driverPref == QLatin1String("qpsql")) {
        const QString pluginErr = loadSqlPlugin(QStringLiteral("qsqlpsql.dll"));
        if (!pluginErr.isEmpty() && !QSqlDatabase::isDriverAvailable(QStringLiteral("QPSQL")))
            return fail(QStringLiteral("qsqlpsql.dll: %1").arg(pluginErr));
        db = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), QStringLiteral("main_connection"));
        db.setHostName(host);
        db.setPort(port);
        db.setDatabaseName(dbName);
        db.setUserName(user);
        db.setPassword(password);
        db.setConnectOptions(QStringLiteral("connect_timeout=5"));
        connectLabel = QStringLiteral("QPSQL");
        if (!db.open())
            return fail(db.lastError().text());
    } else {
        QString odbcErr;
        if (!openOdbcConnection(host, port, dbName, user, password, odbcErr))
            return fail(odbcErr);
        connectLabel = QStringLiteral("QODBC");
        db = QSqlDatabase::database(QStringLiteral("main_connection"));
    }

    QSqlQuery enc(db);
    enc.exec(QStringLiteral("SET client_encoding TO 'UTF8'"));

    static bool perfIndexesEnsured = false;
    if (!perfIndexesEnsured) {
        perfIndexesEnsured = true;
        QSqlQuery idx(db);
        const QStringList indexSql = {
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_agv_tasks_next_date ON agv_tasks (next_date)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_agv_tasks_agv_next ON agv_tasks (agv_id, next_date)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_agv_tasks_assigned_to ON agv_tasks (assigned_to)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_agv_list_created_at ON agv_list (created_at)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_hist_done_task ON agv_task_history (completed_at, agv_id, task_name)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_notifications_target_read ON notifications (target_user, is_read, created_at)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_users_full_name ON users (full_name)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_chat_messages_thread_created ON task_chat_messages (thread_id, created_at)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_chat_messages_thread_id_id ON task_chat_messages (thread_id, id)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_chat_threads_created_at ON task_chat_threads (created_by, created_at)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_chat_threads_recipient_at ON task_chat_threads (recipient_user, created_at)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_chat_hidden_user_thread ON task_chat_hidden (username, thread_id)"),
        };
        for (const QString &sql : indexSql) {
            if (!idx.exec(sql)) {
                const QString e = idx.lastError().text();
                if (!e.contains(QStringLiteral("already exists"), Qt::CaseInsensitive)
                    && !e.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive)) {
                    qDebug() << "DB index warning:" << e << "SQL:" << sql;
                }
            }
        }
    }

    qDebug() << "Успешно подключились к PostgreSQL через" << connectLabel;
    return true;
}

bool reconnectWithHost(const QString &host, QString *outError)
{
    return reconnectWithSettings(host, getDbPassword(), outError);
}

bool reconnectWithSettings(const QString &host, const QString &password, QString *outError)
{
    QString h = host.trimmed();
    int port = getDbPort();
    const int colonPos = h.lastIndexOf(':');
    if (colonPos > 0 && h.indexOf(':') == colonPos) {
        bool ok = false;
        const int p = h.mid(colonPos + 1).toInt(&ok);
        if (ok && p > 0 && p <= 65535) {
            port = p;
            h = h.left(colonPos).trimmed();
        }
    }
    QSettings *s = portableSettings();
    s->setValue(KEY_HOST, h.isEmpty() ? QStringLiteral("localhost") : h);
    s->setValue(KEY_PORT, port);
    s->setValue(KEY_PASSWORD, password);
    s->sync();

    removeMainConnection();
    return connectToDB(outError);
}

void testConnection()
{
    QSqlQuery query(QSqlDatabase::database("main_connection"));
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM agv_models"));
    if (!query.exec()) {
        qDebug() << "Ошибка выполнения запроса:" << query.lastError().text();
    } else if (query.next()) {
        qDebug() << "Количество записей в agv_models:" << query.value(0).toInt();
    }
}
