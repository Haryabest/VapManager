#include "db_agv_tasks.h"
#include "db_users.h"
#include "app_session.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace {

QSqlDatabase openMainDb()
{
    return QSqlDatabase::database("main_connection");
}

bool ensureDbOpen(const QSqlDatabase &db, const char *context)
{
    if (db.isOpen())
        return true;
    qDebug() << context << "main_connection НЕ ОТКРЫТА!";
    return false;
}

AgvTask rowToAgvTask(const QSqlQuery &q)
{
    AgvTask t;
    t.id              = q.value(0).toString();
    t.agvId           = q.value(1).toString();
    t.taskName        = q.value(2).toString();
    t.taskDescription = q.value(3).toString();
    t.intervalDays    = q.value(4).toInt();
    t.durationMinutes = q.value(5).toInt();
    t.isDefault       = q.value(6).toBool();
    t.nextDate        = q.value(7).toDate();
    t.assignedTo      = q.value(8).toString();
    t.delegatedBy     = q.value(9).toString();
    return t;
}

void applyAgvAutoDelegation(QSqlDatabase &db, const QString &agvId)
{
    QSqlQuery chk(db);
    chk.prepare("SELECT assigned_user, assigned_by FROM agv_list WHERE agv_id = :id");
    chk.bindValue(":id", agvId);
    if (!(chk.exec() && chk.next()))
        return;

    const QString assignedUser = chk.value(0).toString().trimmed();
    const QString assignedBy = chk.value(1).toString().trimmed();
    if (assignedUser.isEmpty())
        return;

    QSqlQuery upd(db);
    upd.prepare("UPDATE agv_tasks SET assigned_to = :u, delegated_by = :by WHERE agv_id = :id AND (assigned_to IS NULL OR assigned_to = '')");
    upd.bindValue(":u", assignedUser);
    upd.bindValue(":by", assignedBy);
    upd.bindValue(":id", agvId);
    upd.exec();
}

}

bool copyModelTasksToAgv(const QString &agvId, const QString &modelName)
{
    QSqlDatabase db = openMainDb();
    if (!ensureDbOpen(db, "copyModelTasksToAgv:")) {
        return false;
    }

    // Копируем шаблонные задачи модели → задачи AGV
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO agv_tasks
            (agv_id, task_name, task_description, interval_days, duration_minutes, is_default, next_date)
        SELECT
            :agv_id,
            task_name,
            task_description,
            interval_days,
            duration_minutes,
            is_default,
            DATE_ADD(CURDATE(), INTERVAL interval_days DAY)
        FROM model_maintenance_template
        WHERE model_name = :model_name
    )");

    q.bindValue(":agv_id", agvId);
    q.bindValue(":model_name", modelName.toUpper());

    if (!q.exec()) {
        qDebug() << "copyModelTasksToAgv error:" << q.lastError().text();
        return false;
    }

    applyAgvAutoDelegation(db, agvId);

    logAction(AppSession::currentUsername(),
              "agv_tasks_copied",
              QString("Скопированы задачи модели %1 в AGV %2").arg(modelName, agvId));

    return true;
}

QVector<AgvTask> loadAgvTasks(const QString &agvId)
{
    QVector<AgvTask> list;

    QSqlDatabase db = openMainDb();
    if (!ensureDbOpen(db, "loadAgvTasks:")) {
        return list;
    }

    QSqlQuery q(db);
    q.prepare(R"(
        SELECT id, agv_id, task_name, task_description,
               interval_days, duration_minutes, is_default, next_date,
               assigned_to, delegated_by
        FROM agv_tasks
        WHERE agv_id = :agv_id
        ORDER BY next_date ASC, id ASC
    )");

    q.bindValue(":agv_id", agvId);

    if (!q.exec()) {
        qDebug() << "loadAgvTasks error:" << q.lastError().text();
        return list;
    }

    while (q.next())
        list.push_back(rowToAgvTask(q));

    return list;
}

bool ensureAssignedToColumn()
{
    QSqlDatabase db = openMainDb();
    if (!db.isOpen()) return false;

    QSqlQuery q(db);
    q.exec("ALTER TABLE agv_tasks ADD COLUMN assigned_to VARCHAR(64) DEFAULT ''");
    q.exec("ALTER TABLE agv_tasks ADD COLUMN delegated_by VARCHAR(64) DEFAULT ''");
    return true;
}

bool ensureAgvListAssignedUserColumn()
{
    QSqlDatabase db = openMainDb();
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    q.exec("ALTER TABLE agv_list ADD COLUMN assigned_user VARCHAR(64) DEFAULT ''");
    return true;
}

QStringList getAgvIdsAssignedToUser(const QString &username)
{
    QStringList list;
    QSqlDatabase db = openMainDb();
    if (!db.isOpen()) return list;
    QSqlQuery q(db);
    q.prepare("SELECT agv_id FROM agv_list WHERE assigned_user = :u ORDER BY agv_id");
    q.bindValue(":u", username);
    if (!q.exec()) return list;
    while (q.next())
        list << q.value(0).toString();
    return list;
}
