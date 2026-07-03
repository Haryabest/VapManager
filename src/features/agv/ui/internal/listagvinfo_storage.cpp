#include "listagvinfo.h"

#include "app_session.h"
#include "databus.h"
#include "db_users.h"
#include "ui_action_logger.h"

#include <QDebug>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QMap>
#include <algorithm>

QVector<AgvInfo> ListAgvInfo::loadAgvList()
{
    QVector<AgvInfo> list;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        qDebug() << "loadAgvList: main_connection НЕ ОТКРЫТА!";
        return list;
    }

    QSqlQuery q(db);
    if (!q.exec(R"(
        SELECT a.agv_id, a.model, a.serial, a.status, a.alias, a.kilometers,
               a."blueprintPath", a."lastActive",
               COALESCE(a.assigned_user, ''),
               COALESCE(tf.has_overdue, 0),
               COALESCE(tf.has_soon, 0)
        FROM agv_list a
        LEFT JOIN (
            SELECT agv_id,
                   MAX(CASE WHEN next_date <= CURRENT_DATE + 3 THEN 1 ELSE 0 END) AS has_overdue,
                   MAX(CASE WHEN next_date > CURRENT_DATE + 3
                             AND next_date <= CURRENT_DATE + 6 THEN 1 ELSE 0 END) AS has_soon
            FROM agv_tasks
            WHERE next_date IS NOT NULL
            GROUP BY agv_id
        ) tf ON tf.agv_id = a.agv_id
        ORDER BY a.created_at DESC
    )")) {
        qDebug() << "loadAgvList: запрос не выполнился:" << q.lastError().text();
        return list;
    }

    list.reserve(4096);

    while (q.next()) {
        AgvInfo info;
        info.id           = q.value(0).toString();
        info.model        = q.value(1).toString();
        info.serial       = q.value(2).toString();
        info.status       = q.value(3).toString();
        info.task         = q.value(4).toString().trimmed();
        if (info.task == "—")
            info.task.clear();
        info.kilometers   = q.value(5).toInt();
        info.blueprintPath= q.value(6).toString();
        info.lastActive   = q.value(7).toDate();
        info.assignedUser = q.value(8).toString().trimmed();

        const bool hasOverdue = q.value(9).toInt() > 0;
        const bool hasSoon = q.value(10).toInt() > 0;
        info.hasOverdueMaintenance = hasOverdue;
        info.hasSoonMaintenance = hasSoon;
        if (hasOverdue)
            info.maintenanceState = "red";
        else if (hasSoon)
            info.maintenanceState = "orange";
        else
            info.maintenanceState = "green";

        if (info.blueprintPath.isEmpty())
            info.blueprintPath = ":/new/mainWindowIcons/noback/blueprint.png";

        list.push_back(info);
    }

    const QString me = AppSession::currentUsername();
    if (getUserRole(me) == QStringLiteral("viewer")) {
        list.erase(std::remove_if(list.begin(), list.end(),
            [&me](const AgvInfo &a) {
                const QString u = a.assignedUser.trimmed();
                return !u.isEmpty() && u != me;
            }), list.end());
    }

    qDebug() << "loadAgvList: загружено записей:" << list.size();
    return list;
}

void ListAgvInfo::addAgv(const AgvInfo &info)
{
    QVector<AgvInfo> agvs = loadAgvList();
    agvs.prepend(info);
    rebuildList(agvs);
}

bool insertAgvToDb(const AgvInfo &info)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO agv_list
        (agv_id, model, serial, status, alias, kilometers, "blueprintPath", "lastActive")
        VALUES (:agv_id, :model, :serial, :status, :alias, :kilometers, :blueprintPath, :lastActive)
    )");
    q.bindValue(":agv_id", info.id);
    q.bindValue(":model", info.model);
    q.bindValue(":serial", info.serial);
    q.bindValue(":status", info.status);
    q.bindValue(":alias", info.task);
    q.bindValue(":kilometers", info.kilometers);
    q.bindValue(":blueprintPath", info.blueprintPath);
    q.bindValue(":lastActive", info.lastActive.isValid() ? info.lastActive : QVariant());

    if (!q.exec()) {
        qDebug() << "insertAgvToDb failed:" << q.lastError().text();
        return false;
    }

    logAction(AppSession::currentUsername(),
              QStringLiteral("agv_created"),
              QStringLiteral("Создан AGV: %1; модель=%2; serial=%3")
                  .arg(info.id, info.model, info.serial));

    DataBus::instance().triggerAgvListChanged();
    return true;
}

static bool deleteAgvFromDb(const QString &id)
{
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("main_connection")));
    q.prepare(QStringLiteral("DELETE FROM agv_list WHERE agv_id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        qDebug() << "Ошибка удаления AGV из БД:" << q.lastError().text();
        return false;
    }

    logAction(AppSession::currentUsername(),
              QStringLiteral("agv_deleted"),
              QStringLiteral("Удален AGV: %1").arg(id));
    return true;
}

void ListAgvInfo::removeAgvById(const QString &id)
{
    if (!deleteAgvFromDb(id)) {
        qDebug() << "removeAgvById: не удалось удалить AGV из базы";
        return;
    }

    QVector<AgvInfo> agvs = loadAgvList();
    rebuildList(agvs);
    emit agvListChanged();
    DataBus::instance().triggerAgvListChanged();
}
