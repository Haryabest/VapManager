#include "listagvinfo.h"

#include "app_session.h"
#include "databus.h"
#include "db_users.h"

#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>

void ListAgvInfo::showUndoToast()
{
    if (!undoToast_)
        return;
    undoToast_->show();
    undoToast_->raise();
    undoTimer_->start(15000);
}

void ListAgvInfo::clearUndoSnapshot()
{
    lastDeletedAgvs_.clear();
    lastDeletedTasks_.clear();
    lastDeletedHistory_.clear();
}

void ListAgvInfo::restoreDeletedAgvs()
{
    if (lastDeletedAgvs_.isEmpty())
        return;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        QMessageBox::warning(this, "Восстановление AGV", "База данных не открыта.");
        return;
    }

    const bool txSupported = db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions);
    bool txStarted = false;
    if (txSupported)
        txStarted = db.transaction();

    for (const AgvInfo &info : lastDeletedAgvs_) {
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
            if (txStarted) db.rollback();
            QMessageBox::warning(this, "Восстановление AGV", "Ошибка восстановления AGV: " + q.lastError().text());
            return;
        }
    }

    for (const AgvTask &t : lastDeletedTasks_) {
        QSqlQuery q(db);
        q.prepare(R"(
            INSERT INTO agv_tasks
            (agv_id, task_name, task_description, interval_days, duration_minutes, is_default, next_date)
            VALUES (:agv_id, :name, :dsc, :days, :mins, :def, :next)
        )");
        q.bindValue(":agv_id", t.agvId);
        q.bindValue(":name", t.taskName);
        q.bindValue(":dsc", t.taskDescription);
        q.bindValue(":days", t.intervalDays);
        q.bindValue(":mins", t.durationMinutes);
        q.bindValue(":def", t.isDefault ? 1 : 0);
        q.bindValue(":next", t.nextDate);
        if (!q.exec()) {
            if (txStarted) db.rollback();
            QMessageBox::warning(this, "Восстановление AGV", "Ошибка восстановления задач: " + q.lastError().text());
            return;
        }
    }

    for (const DeletedHistoryRow &h : lastDeletedHistory_) {
        QSqlQuery q(db);
        q.prepare(R"(
            INSERT INTO agv_task_history
            (agv_id, task_id, task_name, interval_days, completed_at, next_date_after, performed_by)
            VALUES (:agv, :tid, :name, :intv, :done, :next, :by)
        )");
        q.bindValue(":agv", h.agvId);
        q.bindValue(":tid", h.taskId);
        q.bindValue(":name", h.taskName);
        q.bindValue(":intv", h.intervalDays);
        q.bindValue(":done", h.completedAt);
        q.bindValue(":next", h.nextDateAfter);
        q.bindValue(":by", h.performedBy);
        q.exec();
    }

    if (txStarted && !db.commit()) {
        db.rollback();
        QMessageBox::warning(this, "Восстановление AGV", "Ошибка сохранения восстановления.");
        return;
    }

    logAction(AppSession::currentUsername(), "agv_restore_batch",
              QString("Восстановлено AGV: %1 шт.").arg(lastDeletedAgvs_.size()));

    clearUndoSnapshot();
    if (undoToast_) undoToast_->hide();

    QVector<AgvInfo> agvs = loadAgvList();
    rebuildList(agvs);
    emit agvListChanged();
    emit DataBus::instance().agvListChanged();
    emit DataBus::instance().calendarChanged();
}
