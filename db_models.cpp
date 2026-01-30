#include "db_models.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// ===============================
// Загрузка только названий моделей
// ===============================
QStringList loadModelNames()
{
    QStringList list;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        qDebug() << "loadModelNames: БД не открыта!";
        return list;
    }

    QSqlQuery q(db);
    q.prepare("SELECT name FROM agv_models ORDER BY created_at DESC");

    if (!q.exec()) {
        qDebug() << "loadModelNames error:" << q.lastError().text();
        return list;
    }

    while (q.next())
        list << q.value(0).toString();

    return list;
}

// ===============================
// Загрузка всех полей модели
// ===============================
QVector<ModelInfo> loadModelList()
{
    QVector<ModelInfo> list;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        qDebug() << "loadModelList: БД не открыта!";
        return list;
    }

    QSqlQuery q(db);
    q.prepare("SELECT name, category, capacityKg, maxSpeed FROM agv_models ORDER BY created_at DESC");

    if (!q.exec()) {
        qDebug() << "loadModelList error:" << q.lastError().text();
        return list;
    }

    while (q.next()) {
        ModelInfo m;
        m.name       = q.value(0).toString();
        m.category   = q.value(1).toString();
        m.capacityKg = q.value(2).toInt();
        m.maxSpeed   = q.value(3).toInt();
        list.push_back(m);
    }

    return list;
}

// ===============================
// Добавление модели
// ===============================
bool insertModelToDb(const ModelInfo &m)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        qDebug() << "insertModelToDb: БД не открыта!";
        return false;
    }

    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO agv_models (name, category, capacityKg, maxSpeed, created_at)
        VALUES (:name, :category, :capacityKg, :maxSpeed, NOW())
    )");

    q.bindValue(":name", m.name);
    q.bindValue(":category", m.category);
    q.bindValue(":capacityKg", m.capacityKg);
    q.bindValue(":maxSpeed", m.maxSpeed);

    if (!q.exec()) {
        qDebug() << "insertModelToDb error:" << q.lastError().text();
        return false;
    }

    return true;
}

// ===============================
// Удаление модели по имени
// ===============================
bool deleteModelByName(const QString &name)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        qDebug() << "deleteModelByName: БД не открыта!";
        return false;
    }

    QSqlQuery q(db);
    q.prepare("DELETE FROM agv_models WHERE name = :name");
    q.bindValue(":name", name);

    if (!q.exec()) {
        qDebug() << "deleteModelByName error:" << q.lastError().text();
        return false;
    }

    return true;
}
