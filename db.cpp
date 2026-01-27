#include "db.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QDebug>

bool connectToDB()
{
    // Если соединение уже существует — используем его
    if (QSqlDatabase::contains("main_connection")) {
        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (db.isOpen())
            return true;
    }

    // Создаём новое соединение
    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL", "main_connection");

    db.setHostName("localhost");
    db.setDatabaseName("agv_db");
    db.setUserName("root");
    db.setPassword("");

    if (!db.open()) {
        qDebug() << "[DB] Ошибка подключения:" << db.lastError().text();
        return false;
    }

    qDebug() << "[DB] Подключение к MySQL успешно!";
    return true;
}
