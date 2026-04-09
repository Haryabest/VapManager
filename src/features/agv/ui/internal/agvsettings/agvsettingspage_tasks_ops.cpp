#include "agvsettingspage.h"

#include "app_session.h"
#include "databus.h"
#include "db_users.h"
#include "notifications_logs.h"

#include <QCheckBox>
#include <QDate>
#include <QDebug>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QVBoxLayout>

bool AgvSettingsPage::ensureTaskHistoryTable() const
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    if (!q.exec(
            "CREATE TABLE IF NOT EXISTS agv_task_history ("
            "  id INT AUTO_INCREMENT PRIMARY KEY,"
            "  agv_id VARCHAR(64) NOT NULL,"
            "  task_id INT NULL,"
            "  task_name VARCHAR(255) NOT NULL,"
            "  interval_days INT NOT NULL DEFAULT 0,"
            "  completed_at DATE NOT NULL,"
            "  completed_ts DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "  next_date_after DATE NULL,"
            "  performed_by VARCHAR(128) NULL,"
            "  INDEX idx_hist_agv_date (agv_id, completed_at),"
            "  INDEX idx_hist_completed_ts (completed_ts)"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4")) {
        qDebug() << "AgvSettingsPage::ensureTaskHistoryTable failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool AgvSettingsPage::completeTaskNow(const QString &taskId, const AgvTask &task)
{
    if (taskId.trimmed().isEmpty())
        return false;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        QMessageBox::warning(this, "Задача", "База данных не открыта.");
        return false;
    }
    if (!ensureTaskHistoryTable()) {
        QMessageBox::warning(this, "Задача", "Не удалось подготовить таблицу истории задач.");
        return false;
    }

    QString currentUser = AppSession::currentUsername();
    QString effectiveAssignee = !task.assignedTo.isEmpty() ? task.assignedTo : originalAssignedUser;
    if (!effectiveAssignee.isEmpty() && currentUser != effectiveAssignee) {
        QMessageBox::warning(this, "Задача",
                             "Выполнить эту задачу может только пользователь, которому она назначена.");
        return false;
    }

    const QDate performedDate = QDate::currentDate();
    const QDate nextDate = computeNextDate(performedDate, task.intervalDays);

    const bool useTransaction = db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions);
    if (useTransaction && !db.transaction()) {
        qDebug() << "completeTaskNow: transaction start failed, fallback to non-transaction mode:"
                 << db.lastError().text();
    }

    QSqlQuery upd(db);
    if (currentUser == originalAssignedUser) {
        upd.prepare("UPDATE agv_tasks SET next_date = :next WHERE id = :id");
    } else {
        upd.prepare("UPDATE agv_tasks SET next_date = :next, assigned_to = :assign, delegated_by = :delegated WHERE id = :id");
        upd.bindValue(":assign", originalAssignedUser);
        upd.bindValue(":delegated", originalAssignedBy);
    }
    upd.bindValue(":next", nextDate.toString("yyyy-MM-dd"));
    upd.bindValue(":id", taskId.toInt());
    if (!upd.exec()) {
        if (useTransaction) db.rollback();
        QMessageBox::warning(this, "Задача", "Не удалось обновить дату задачи: " + upd.lastError().text());
        return false;
    }

    QSqlQuery ins(db);
    ins.prepare("INSERT INTO agv_task_history "
                "(agv_id, task_id, task_name, interval_days, completed_at, next_date_after, performed_by) "
                "VALUES (:agv, :tid, :name, :intv, :done, :next, :by)");
    ins.bindValue(":agv", currentAgvId);
    ins.bindValue(":tid", taskId.toInt());
    ins.bindValue(":name", task.taskName);
    ins.bindValue(":intv", task.intervalDays);
    ins.bindValue(":done", performedDate.toString("yyyy-MM-dd"));
    ins.bindValue(":next", nextDate.toString("yyyy-MM-dd"));
    ins.bindValue(":by", currentUser);
    if (!ins.exec()) {
        if (useTransaction) db.rollback();
        QMessageBox::warning(this, "Задача", "Не удалось записать историю задачи: " + ins.lastError().text());
        return false;
    }

    if (useTransaction) {
        if (!db.commit()) {
            db.rollback();
            QMessageBox::warning(this, "Задача", "Ошибка сохранения проведения задачи.");
            return false;
        }
    }

    logAction(currentUser,
              "agv_task_completed",
              QString("AGV=%1, task=%2, next=%3")
              .arg(currentAgvId, task.taskName, nextDate.toString("dd.MM.yyyy")));

    if (!task.delegatedBy.isEmpty() && task.delegatedBy != currentUser) {
        const QString whoDid = userDisplayName(currentUser);
        addNotificationForUser(
            task.delegatedBy,
            "Задача выполнена",
            QString("Задача \"%1\" для AGV %2 выполнена: %3 (%4) [peer:%5]")
                .arg(task.taskName, currentAgvId, whoDid, performedDate.toString("dd.MM.yyyy"), currentUser));
        emit DataBus::instance().notificationsChanged();
    }
    else if (!originalAssignedBy.isEmpty() && originalAssignedBy != currentUser) {
        const QString whoDid = userDisplayName(currentUser);
        addNotificationForUser(
            originalAssignedBy,
            "Задача выполнена",
            QString("Задача \"%1\" для AGV %2 выполнена: %3 (%4) [peer:%5]")
                .arg(task.taskName, currentAgvId, whoDid, performedDate.toString("dd.MM.yyyy"), currentUser));
        emit DataBus::instance().notificationsChanged();
    }

    QMessageBox::information(this, "Задача",
                             QString("Задача проведена.\nСледующая дата: %1")
                             .arg(nextDate.toString("dd.MM.yyyy")));
    return true;
}

void AgvSettingsPage::deleteTask(const QString &taskId)
{
    AgvTask t = loadTaskById(taskId);
    if (!t.id.isEmpty())
        recentlyDeleted.append(t);

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    QSqlQuery q(db);
    q.prepare("DELETE FROM agv_tasks WHERE id = :id");
    q.bindValue(":id", taskId.toInt());
    if (!q.exec()) {
        qDebug() << "AgvSettingsPage::deleteTask: delete failed:" << q.lastError().text();
    }

    startUndoTimer();
    loadAgv(currentAgvId);
    emit tasksChanged();
}

void AgvSettingsPage::deleteSelectedTasks()
{
    recentlyDeleted.clear();

    QSqlDatabase db = QSqlDatabase::database("main_connection");

    for (int i = 0; i < tasksLayout->count(); ++i) {
        QWidget *w = tasksLayout->itemAt(i)->widget();
        if (!w) continue;

        QCheckBox *c = w->findChild<QCheckBox *>();
        if (!c || !c->isChecked())
            continue;

        QString taskId = c->property("task_id").toString();
        if (taskId.isEmpty())
            continue;

        AgvTask t = loadTaskById(taskId);
        if (!t.id.isEmpty())
            recentlyDeleted.append(t);

        QSqlQuery q(db);
        q.prepare("DELETE FROM agv_tasks WHERE id = :id");
        q.bindValue(":id", taskId.toInt());
        if (!q.exec()) {
            qDebug() << "AgvSettingsPage::deleteSelectedTasks: delete failed:" << q.lastError().text();
        }
    }

    if (!recentlyDeleted.isEmpty())
        startUndoTimer();

    loadAgv(currentAgvId);
    emit tasksChanged();
}

void AgvSettingsPage::toggleEditMode()
{
    editMode = !editMode;

    if (editMode) {
        editModeBtn->setText("Готово");
    } else {
        editModeBtn->setText("Редактировать");
        deleteSelectedBtn->hide();
    }

    updateCheckboxVisibility();
}

void AgvSettingsPage::updateCheckboxVisibility()
{
    for (int i = 0; i < tasksLayout->count(); ++i) {
        QWidget *w = tasksLayout->itemAt(i)->widget();
        if (!w) continue;

        QCheckBox *c = w->findChild<QCheckBox *>();
        if (!c) continue;

        c->setVisible(editMode);
        if (!editMode)
            c->setChecked(false);
    }
}

AgvTask AgvSettingsPage::loadTaskById(const QString &taskId) const
{
    AgvTask t;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return t;

    QSqlQuery q(db);
    q.prepare("SELECT id, agv_id, task_name, task_description, interval_days, duration_minutes, is_default, next_date, assigned_to, delegated_by "
              "FROM agv_tasks WHERE id = :id");
    q.bindValue(":id", taskId.toInt());
    if (!q.exec() || !q.next())
        return t;

    t.id = q.value(0).toString();
    t.agvId = q.value(1).toString();
    t.taskName = q.value(2).toString();
    t.taskDescription = q.value(3).toString();
    t.intervalDays = q.value(4).toInt();
    t.durationMinutes = q.value(5).toInt();
    t.isDefault = q.value(6).toInt() != 0;
    t.nextDate = q.value(7).toDate();
    t.assignedTo = q.value(8).toString();
    t.delegatedBy = q.value(9).toString();
    if (t.intervalDays > 0 && t.nextDate.isValid())
        t.lastService = t.nextDate.addDays(-t.intervalDays);
    else
        t.lastService = QDate();

    return t;
}

void AgvSettingsPage::startUndoTimer()
{
    if (recentlyDeleted.isEmpty())
        return;

    undoDeleteBtn->show();
    undoTimer->start(10000);
}

void AgvSettingsPage::cancelUndo()
{
    recentlyDeleted.clear();
    undoDeleteBtn->hide();
}

void AgvSettingsPage::restoreDeletedTasks()
{
    if (recentlyDeleted.isEmpty())
        return;

    QSqlDatabase db = QSqlDatabase::database("main_connection");

    for (const AgvTask &t : recentlyDeleted) {
        QSqlQuery q(db);
        q.prepare("INSERT INTO agv_tasks (agv_id, task_name, task_description, interval_days, duration_minutes, is_default, next_date) "
                  "VALUES (:id, :n, :dsc, :d, :m, :isdef, :next)");
        q.bindValue(":id", t.agvId);
        q.bindValue(":n", t.taskName);
        q.bindValue(":dsc", t.taskDescription);
        q.bindValue(":d", t.intervalDays);
        q.bindValue(":m", t.durationMinutes);
        q.bindValue(":isdef", t.isDefault ? 1 : 0);
        q.bindValue(":next", t.nextDate.toString("yyyy-MM-dd"));
        if (!q.exec()) {
            qDebug() << "AgvSettingsPage::restoreDeletedTasks: insert failed:" << q.lastError().text();
        }
    }

    cancelUndo();
    loadAgv(currentAgvId);
    emit tasksChanged();
}
