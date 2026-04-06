#include "listagvinfo.h"

#include "app_session.h"
#include "databus.h"
#include "db_users.h"

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
    q.prepare("SELECT agv_id, model, serial, status, alias, kilometers, blueprintPath, lastActive, "
              "COALESCE(assigned_user, '') FROM agv_list ORDER BY created_at DESC");

    if (!q.exec()) {
        qDebug() << "loadAgvList: запрос не выполнился:" << q.lastError().text();
        return list;
    }

    while (q.next()) {
        AgvInfo info;
        info.id           = q.value(0).toString();
        info.model        = q.value(1).toString();
        info.serial       = q.value(2).toString();
        info.status       = q.value(3).toString();
        info.maintenanceState = "green";
        info.hasOverdueMaintenance = false;
        info.hasSoonMaintenance = false;
        info.task         = q.value(4).toString().trimmed();
        if (info.task == "—")
            info.task.clear();
        info.kilometers   = q.value(5).toInt();
        info.blueprintPath= q.value(6).toString();
        info.lastActive   = q.value(7).toDate();
        info.assignedUser = q.value(8).toString().trimmed();

        if (info.blueprintPath.isEmpty())
            info.blueprintPath = ":/new/mainWindowIcons/noback/blueprint.png";

        list.push_back(info);
    }

    QMap<QString, QString> maintenanceByAgv;
    QMap<QString, bool> hasOverdueByAgv;
    QMap<QString, bool> hasSoonByAgv;
    for (int i = 0; i < list.size(); ++i)
    {
        maintenanceByAgv[list[i].id] = "green";
        hasOverdueByAgv[list[i].id] = false;
        hasSoonByAgv[list[i].id] = false;
    }

    QSqlQuery tasksQ(db);
    tasksQ.prepare(R"(
        SELECT
            agv_id,
            MAX(CASE WHEN next_date <= DATE_ADD(CURDATE(), INTERVAL 3 DAY) THEN 1 ELSE 0 END) AS has_overdue,
            MAX(CASE WHEN next_date > DATE_ADD(CURDATE(), INTERVAL 3 DAY)
                      AND next_date <= DATE_ADD(CURDATE(), INTERVAL 6 DAY)
                     THEN 1 ELSE 0 END) AS has_soon
        FROM agv_tasks
        GROUP BY agv_id
    )");
    if (tasksQ.exec()) {
        while (tasksQ.next()) {
            const QString agvId = tasksQ.value(0).toString();
            if (!maintenanceByAgv.contains(agvId))
                continue;

            const bool hasOverdue = tasksQ.value(1).toInt() > 0;
            const bool hasSoon = tasksQ.value(2).toInt() > 0;
            if (hasOverdue) {
                hasOverdueByAgv[agvId] = true;
                maintenanceByAgv[agvId] = "red";
            }
            if (hasSoon) {
                hasSoonByAgv[agvId] = true;
                if (!hasOverdue)
                    maintenanceByAgv[agvId] = "orange";
            }
        }
    } else {
        qDebug() << "loadAgvList: agv_tasks query failed:" << tasksQ.lastError().text();
    }

    for (int i = 0; i < list.size(); ++i) {
        list[i].maintenanceState = maintenanceByAgv.value(list[i].id, "green");
        list[i].hasOverdueMaintenance = hasOverdueByAgv.value(list[i].id, false);
        list[i].hasSoonMaintenance = hasSoonByAgv.value(list[i].id, false);
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
    AgvItem *item = new AgvItem(info, s, content);

    connect(item, &AgvItem::openDetailsRequested, this, [this](const QString &id){
        emit openAgvDetails(id);
    });

    int count = layout->count();
    if (count > 0) {
        QLayoutItem *last = layout->itemAt(count - 1);
        if (last->spacerItem()) {
            layout->insertWidget(count - 1, item);
        } else {
            layout->addWidget(item);
        }
    } else {
        layout->addWidget(item);
        layout->addStretch();
    }
}

static bool deleteAgvFromDb(const QString &id)
{
    QSqlQuery q(QSqlDatabase::database("main_connection"));
    q.prepare("DELETE FROM agv_list WHERE agv_id = :id");
    q.bindValue(":id", id);

    if (!q.exec()) {
        qDebug() << "Ошибка удаления AGV из БД:" << q.lastError().text();
        return false;
    }

    logAction(AppSession::currentUsername(),
              "agv_deleted",
              QString("Удален AGV: %1").arg(id));

    return true;
}

void ListAgvInfo::removeAgvById(const QString &id)
{
    if (!deleteAgvFromDb(id)) {
        qDebug() << "removeAgvById: не удалось удалить AGV из базы";
        return;
    }

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem *it = layout->itemAt(i);
        QWidget *w = it ? it->widget() : nullptr;
        AgvItem *agvItem = qobject_cast<AgvItem*>(w);

        if (agvItem && agvItem->agvId() == id) {
            layout->removeItem(it);
            agvItem->deleteLater();
            delete it;
            break;
        }
    }

    QVector<AgvInfo> agvs = loadAgvList();
    rebuildList(agvs);
    qDebug() << "AGV" << id << "успешно удалён из БД и UI";
}

bool insertAgvToDb(const AgvInfo &info)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        qDebug() << "insertAgvToDb: main_connection НЕ ОТКРЫТА!";
        return false;
    }

    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO agv_list
        (agv_id, model, serial, status, alias, kilometers, blueprintPath, lastActive)
        VALUES (:agv_id, :model, :serial, :status, :alias, :kilometers, :blueprintPath, :lastActive)
    )");

    q.bindValue(":agv_id", info.id);
    q.bindValue(":model", info.model);
    q.bindValue(":serial", info.serial);
    q.bindValue(":status", info.status);
    q.bindValue(":alias", info.task);
    q.bindValue(":kilometers", info.kilometers);
    q.bindValue(":blueprintPath", info.blueprintPath);
    q.bindValue(":lastActive", info.lastActive);

    if (!q.exec()) {
        qDebug() << "Ошибка вставки AGV:" << q.lastError().text();
        return false;
    }
    db.commit();
    qDebug() << "AGV добавлен в БД:" << db.hostName() << db.databaseName() << "agv_id=" << info.id;

    logAction(AppSession::currentUsername(),
              "agv_created",
              QString("Создан AGV: %1; модель=%2; serial=%3")
                  .arg(info.id, info.model, info.serial));

    emit DataBus::instance().agvListChanged();
    return true;
}
