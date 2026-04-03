#include "db_models.h"
#include "db_users.h"
#include "app_session.h"
#include <QCoreApplication>
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
    QString sql = QStringLiteral(
        "SELECT name, version_po, version_eplan, category, capacityKg, maxSpeed, dimensions, "
        "coupling_count, direction FROM agv_models ORDER BY created_at DESC");
    if (QCoreApplication::instance()
        && QCoreApplication::instance()->property("autotest_running").toBool()) {
        sql += QStringLiteral(" LIMIT 100");
    }
    q.prepare(sql);

    if (!q.exec()) {
        qDebug() << "loadModelList error:" << q.lastError().text();
        return list;
    }

    while (q.next()) {
        ModelInfo m;
        m.name = q.value(0).toString();
        m.versionPo = q.value(1).toString();
        m.versionEplan = q.value(2).toString();
        m.category = q.value(3).toString();
        m.capacityKg = q.value(4).toInt();
        m.maxSpeed = q.value(5).toInt();
        m.dimensions = q.value(6).toString();
        m.couplingCount = q.value(7).toInt();
        m.direction = q.value(8).toString();
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
        INSERT INTO agv_models (name, version_po, version_eplan, category, capacityKg, maxSpeed, dimensions, coupling_count, direction, created_at)
        VALUES (:name, :vpo, :veplan, :category, :capacityKg, :maxSpeed, :dim, :coup, :dir, NOW())
    )");
    q.bindValue(":name", m.name);
    q.bindValue(":vpo", m.versionPo);
    q.bindValue(":veplan", m.versionEplan);
    q.bindValue(":category", m.category);
    q.bindValue(":capacityKg", m.capacityKg);
    q.bindValue(":maxSpeed", m.maxSpeed);
    q.bindValue(":dim", m.dimensions);
    q.bindValue(":coup", m.couplingCount);
    q.bindValue(":dir", m.direction);

    if (!q.exec()) {
        qDebug() << "insertModelToDb error:" << q.lastError().text();
        return false;
    }
    db.commit();
    qDebug() << "Модель добавлена в БД:" << db.hostName() << db.databaseName() << "name=" << m.name;

    logAction(AppSession::currentUsername(),
              "model_created",
              QString("Создана модель: %1").arg(m.name));

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

    logAction(AppSession::currentUsername(),
              "model_deleted",
              QString("Удалена модель: %1").arg(name));

    return true;
}
