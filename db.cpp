#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

bool connectToDB()
{
    // ВАЖНО: добавляем ИМЯ соединения
    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL", "main_connection");
    db.setHostName("localhost");
    db.setDatabaseName("agv_manager_db");
    db.setUserName("root");
    db.setPassword(""); // если пароль есть — впиши

    if (!db.open()) {
        qDebug() << "Ошибка подключения к базе данных:" << db.lastError().text();
        return false;
    }

    qDebug() << "Успешно подключились к базе данных!";
    return true;
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
