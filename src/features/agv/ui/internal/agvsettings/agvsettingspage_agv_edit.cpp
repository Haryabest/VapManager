#include "agvsettingspage.h"

#include "app_session.h"
#include "databus.h"
#include "db_users.h"
#include "notifications_logs.h"

#include <QComboBox>
#include <QDebug>
#include <QDateTime>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>

void AgvSettingsPage::enterAgvEditMode()
{
    agvEditMode = true;

    auto styleEdit = [this](QLineEdit *e){
        e->setReadOnly(false);
        e->setStyleSheet(QString(
            "QLineEdit{font-family:Inter;font-size:%1px;font-weight:700;color:#111;"
            "border:1px solid #C8C8C8;border-radius:%2px;background:white;padding:4px 8px;}"
            "QLineEdit:focus{border:1px solid #0F00DB;}"
        ).arg(s(15)).arg(s(6)));
    };
    styleEdit(idEdit);
    styleEdit(serialEdit);
    statusCombo->setEnabled(true);
    statusCombo->setStyleSheet(QString(
        "QComboBox{font-family:Inter;font-size:%1px;font-weight:700;color:#111;"
        "border:1px solid #C8C8C8;border-radius:%2px;background:white;padding:4px 8px;}"
        "QComboBox:hover{border:1px solid #0F00DB;}"
    ).arg(s(15)).arg(s(6)));
    styleEdit(kmEdit);
    styleEdit(currentTaskEdit);

    editAgvBtn->hide();
    saveAgvBtn->show();
    cancelAgvBtn->show();
    QString er = getUserRole(AppSession::currentUsername());
    if (er == "admin" || er == "tech") {
        assignedUserLabel->hide();
        assignedUserCombo->show();
    }
    refreshAgvEditButtons();
}

void AgvSettingsPage::leaveAgvEditMode()
{
    agvEditMode = false;

    idEdit->setText(originalAgvId);
    serialEdit->setText(originalSerial);
    int idx = statusCombo->findText(originalStatus, Qt::MatchFixedString);
    if (idx < 0) {
        statusCombo->addItem(originalStatus);
        idx = statusCombo->findText(originalStatus, Qt::MatchFixedString);
    }
    statusCombo->setCurrentIndex(idx);
    kmEdit->setText(QString::number(originalKm));
    currentTaskEdit->setText(originalCurrentTask);

    auto styleReadonly = [this](QLineEdit *e){
        e->setReadOnly(true);
        e->setStyleSheet(QString(
            "QLineEdit{font-family:Inter;font-size:%1px;font-weight:800;color:#111;"
            "border:none;background:transparent;padding:0;}"
            "QLineEdit:focus{border:none;}"
        ).arg(s(16)));
    };
    styleReadonly(idEdit);
    styleReadonly(serialEdit);
    statusCombo->setEnabled(false);
    statusCombo->setStyleSheet(QString(
        "QComboBox{font-family:Inter;font-size:%1px;font-weight:800;color:#111;"
        "border:none;background:transparent;padding:0;}"
        "QComboBox::drop-down{border:none;width:0px;}"
        "QComboBox::down-arrow{image:none;}"
    ).arg(s(16)));
    styleReadonly(kmEdit);
    styleReadonly(currentTaskEdit);

    editAgvBtn->show();
    saveAgvBtn->hide();
    cancelAgvBtn->hide();
    saveAgvBtn->setEnabled(false);
    assignedUserLabel->show();
    assignedUserCombo->hide();
    int ai = assignedUserCombo->findData(originalAssignedUser);
    if (ai >= 0) assignedUserCombo->setCurrentIndex(ai);
}

void AgvSettingsPage::refreshAgvEditButtons()
{
    if (!agvEditMode) {
        saveAgvBtn->setEnabled(false);
        return;
    }

    QString newAssigned = assignedUserCombo ? assignedUserCombo->currentData().toString() : QString();
    const bool changed =
        idEdit->text().trimmed() != originalAgvId ||
        serialEdit->text().trimmed() != originalSerial ||
        statusCombo->currentText().trimmed() != originalStatus ||
        kmEdit->text().toInt() != originalKm ||
        currentTaskEdit->text().trimmed() != originalCurrentTask ||
        newAssigned != originalAssignedUser;

    saveAgvBtn->setEnabled(changed);
}

bool AgvSettingsPage::saveAgvInfo()
{
    const QString newId = idEdit->text().trimmed();
    const QString newSerial = serialEdit->text().trimmed();
    const QString newStatus = statusCombo->currentText().trimmed();
    const QString newCurrentTask = currentTaskEdit->text().trimmed();
    bool kmOk = false;
    const int newKm = kmEdit->text().trimmed().toInt(&kmOk);

    if (newId.isEmpty() || newSerial.isEmpty() || newStatus.isEmpty() || !kmOk || newKm < 0 || newCurrentTask.size() > 40) {
        QMessageBox::warning(this, "AGV", "Проверьте поля: ID, S/N, Статус, Пробег и Текущая задача.");
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        QMessageBox::warning(this, "AGV", "База данных не открыта.");
        return false;
    }

    if (newId != originalAgvId) {
        QSqlQuery chk(db);
        chk.prepare("SELECT COUNT(*) FROM agv_list WHERE agv_id = :id");
        chk.bindValue(":id", newId);
        if (chk.exec() && chk.next() && chk.value(0).toInt() > 0) {
            QMessageBox::warning(this, "AGV", "AGV с таким ID уже существует.");
            return false;
        }
    }

    const bool txSupported = db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions);
    bool txStarted = false;
    if (txSupported) {
        txStarted = db.transaction();
        if (!txStarted)
            qDebug() << "saveAgvInfo: transaction start failed, fallback to non-transaction:" << db.lastError().text();
    }

    QString newAssignedUser = assignedUserCombo ? assignedUserCombo->currentData().toString() : originalAssignedUser;
    const QString whoAssigned = AppSession::currentUsername();
    const QString newAssignedBy = newAssignedUser.isEmpty() ? QString() : whoAssigned;

    QSqlQuery updAgv(db);
    updAgv.prepare("UPDATE agv_list SET agv_id = :new_id, serial = :serial, status = :status, kilometers = :km, alias = :alias, assigned_user = :assigned, assigned_by = :assigned_by "
                   "WHERE agv_id = :old_id");
    updAgv.bindValue(":new_id", newId);
    updAgv.bindValue(":serial", newSerial);
    updAgv.bindValue(":status", newStatus);
    updAgv.bindValue(":km", newKm);
    updAgv.bindValue(":alias", newCurrentTask);
    updAgv.bindValue(":assigned", newAssignedUser);
    updAgv.bindValue(":assigned_by", newAssignedBy);
    updAgv.bindValue(":old_id", originalAgvId);

    if (!updAgv.exec()) {
        if (txStarted) db.rollback();
        QMessageBox::warning(this, "AGV", "Не удалось обновить AGV: " + updAgv.lastError().text());
        return false;
    }

    if (newId != originalAgvId) {
        QSqlQuery updTasks(db);
        updTasks.prepare("UPDATE agv_tasks SET agv_id = :new_id WHERE agv_id = :old_id");
        updTasks.bindValue(":new_id", newId);
        updTasks.bindValue(":old_id", originalAgvId);
        if (!updTasks.exec()) {
            if (txStarted) db.rollback();
            QMessageBox::warning(this, "AGV", "Не удалось обновить задачи AGV: " + updTasks.lastError().text());
            return false;
        }
    }

    if (newAssignedUser != originalAssignedUser) {
        QSqlQuery delegateTasks(db);
        if (newAssignedUser.isEmpty()) {
            delegateTasks.prepare("UPDATE agv_tasks SET assigned_to = '', delegated_by = '' WHERE agv_id = :id");
        } else {
            delegateTasks.prepare("UPDATE agv_tasks SET assigned_to = :u, delegated_by = :by WHERE agv_id = :id");
            delegateTasks.bindValue(":u", newAssignedUser);
            delegateTasks.bindValue(":by", whoAssigned);
        }
        delegateTasks.bindValue(":id", newId);
        delegateTasks.exec();

        if (!newAssignedUser.isEmpty()) {
            const QString whoDisplay = userDisplayName(whoAssigned);
            addNotificationForUser(
                newAssignedUser,
                "AGV закреплена за вами",
                QString("За вами закреплена AGV %1. Закрепил: %2, %3 [peer:%4]")
                    .arg(newId, whoDisplay, QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm"), whoAssigned)
            );
            emit DataBus::instance().notificationsChanged();
        }
    }

    if (txStarted) {
        if (!db.commit()) {
            db.rollback();
            QMessageBox::warning(this, "AGV", "Ошибка сохранения.");
            return false;
        }
    } else {
        if (db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions))
            db.commit();
    }

    qDebug() << "AGV сохранён в БД:" << db.hostName() << db.databaseName() << "agv_id=" << newId;

    currentAgvId = newId;
    originalAgvId = newId;
    originalSerial = newSerial;
    originalStatus = newStatus;
    originalKm = newKm;
    originalCurrentTask = newCurrentTask;
    originalAssignedUser = newAssignedUser;
    originalAssignedBy = newAssignedBy;

    emit DataBus::instance().agvListChanged();
    emit tasksChanged();
    loadAgv(currentAgvId);
    return true;
}
