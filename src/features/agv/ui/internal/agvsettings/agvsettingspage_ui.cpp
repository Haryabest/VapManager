#include "agvsettingspage.h"

#include "app_session.h"
#include "databus.h"
#include "db_users.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDebug>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVBoxLayout>

void AgvSettingsPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

bool AgvSettingsPage::eventFilter(QObject *obj, QEvent *event)
{
    if (!editMode && event->type() == QEvent::MouseButtonRelease) {
        QString role = getUserRole(AppSession::currentUsername());
        if (role == "viewer")
            return QWidget::eventFilter(obj, event);

        QWidget *row = qobject_cast<QWidget*>(obj);
        if (!row)
            return false;

        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        QWidget *child = row->childAt(me->pos());
        if (qobject_cast<QCheckBox*>(child) || qobject_cast<QPushButton*>(child))
            return false;

        QString taskId = row->property("task_id").toString();
        if (taskId.isEmpty())
            return false;

        AgvTask t = loadTaskById(taskId);
        if (!t.id.isEmpty())
            openEditTaskForm(taskId, t);

        return true;
    }

    return QWidget::eventFilter(obj, event);
}

void AgvSettingsPage::buildTasksHeader()
{
    if (headerRow)
        headerRow->deleteLater();

    headerRow = new QWidget(this);
    QHBoxLayout *h = new QHBoxLayout(headerRow);
    h->setContentsMargins(s(10), s(5), s(10), s(5));
    h->setSpacing(s(10));

    auto makeHeader = [&](const QString &text, int stretch, int minWidth = 0){
        QLabel *l = new QLabel(text, headerRow);
        l->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
        ).arg(s(16)));
        if (minWidth > 0)
            l->setMinimumWidth(minWidth);
        h->addWidget(l, stretch);
    };

    QWidget *cbSpacer = new QWidget(headerRow);
    cbSpacer->setFixedWidth(s(32));
    h->addWidget(cbSpacer, 0);

    makeHeader("Название задачи", 3);
    makeHeader("Интервал", 0, s(90));
    makeHeader("Минуты", 0, s(90));
    makeHeader("Следующее", 0, s(110));
    makeHeader("Делегировано", 0, s(110));
    QWidget *btnSpacer = new QWidget(headerRow);
    btnSpacer->setFixedWidth(s(130));
    h->addWidget(btnSpacer, 0);
}

void AgvSettingsPage::clearTasks()
{
    QLayoutItem *child;
    while ((child = tasksLayout->takeAt(0)) != nullptr) {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }
    deleteSelectedBtn->hide();
}

QDate AgvSettingsPage::computeNextDate(const QDate &lastService, int intervalDays) const
{
    if (!lastService.isValid())
        return QDate::currentDate().addDays(intervalDays);
    return lastService.addDays(intervalDays);
}

void AgvSettingsPage::loadAgv(const QString &agvId)
{
    currentAgvId = agvId;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        qDebug() << "AgvSettingsPage::loadAgv: main_connection not open";
        return;
    }

    {
        QSqlQuery q(db);
        q.prepare("SELECT agv_id, model, serial, status, kilometers, blueprintPath, lastActive, assigned_user, assigned_by, alias "
                  "FROM agv_list WHERE agv_id = :id");
        q.bindValue(":id", agvId);
        if (!q.exec() || !q.next()) {
            qDebug() << "AgvSettingsPage::loadAgv: agv_list query failed:" << q.lastError().text();
            return;
        }

        originalAgvId = q.value(0).toString();
        originalSerial = q.value(2).toString();
        originalStatus = q.value(3).toString();
        originalKm = q.value(4).toInt();
        originalAssignedUser = q.value(7).toString();
        originalAssignedBy = q.value(8).toString();
        originalCurrentTask = q.value(9).toString().trimmed();
        if (originalCurrentTask == "—")
            originalCurrentTask.clear();

        idEdit->setText(originalAgvId);
        modelEdit->setText(q.value(1).toString());
        serialEdit->setText(originalSerial);
        int idx = statusCombo->findText(originalStatus, Qt::MatchFixedString);
        if (idx < 0) {
            statusCombo->addItem(originalStatus);
            idx = statusCombo->findText(originalStatus, Qt::MatchFixedString);
        }
        statusCombo->setCurrentIndex(idx);
        kmEdit->setText(QString::number(originalKm));
        currentTaskEdit->setText(originalCurrentTask);

        QString blueprint = q.value(5).toString();
        if (!blueprint.isEmpty()) {
            QPixmap bpPix(blueprint);
            if (!bpPix.isNull())
                blueprintLabel->setPixmap(bpPix.scaled(s(300), s(200), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            else
                blueprintLabel->setPixmap(QPixmap());
        } else {
            blueprintLabel->setPixmap(QPixmap());
        }
    }

    const bool isOffline = (originalStatus.trimmed().toLower() == "offline");

    QString curRole = getUserRole(AppSession::currentUsername());
    assignedUserLabel->setText(originalAssignedUser.isEmpty() ? "—" : originalAssignedUser);
    if (pinAgvBtn) {
        pinAgvBtn->setText(originalAssignedUser.isEmpty() ? "Закрепить за" : QString("Закреплён за %1").arg(originalAssignedUser));
    }
    assignedUserLabel->setVisible(!agvEditMode);
    if (curRole == "admin" || curRole == "tech") {
        assignedUserCombo->clear();
        assignedUserCombo->addItem("— Нет —", "");
        QVector<UserInfo> allUsers = getAllUsers(false);
        for (const UserInfo &u : allUsers) {
            if (u.role != "viewer") continue;
            QString display = u.fullName.isEmpty() ? u.username : QString("%1 (%2)").arg(u.fullName, u.username);
            assignedUserCombo->addItem(display, u.username);
        }
        int idx = assignedUserCombo->findData(originalAssignedUser);
        if (idx >= 0) assignedUserCombo->setCurrentIndex(idx);
        assignedUserCombo->setVisible(agvEditMode);
    } else {
        assignedUserCombo->hide();
    }

    if (!agvEditMode) {
        refreshAgvEditButtons();
    }

    const bool canEditAgv = (curRole == "admin" || curRole == "tech");
    bool canEditTasks = canEditAgv && !isOffline;
    if (canEditAgv) {
        editAgvBtn->show();
    } else {
        editAgvBtn->hide();
    }

    if (canEditTasks) {
        addTaskBtn->show();
        editModeBtn->show();
        if (pinAgvBtn) pinAgvBtn->show();
    } else {
        addTaskBtn->hide();
        editModeBtn->hide();
        if (pinAgvBtn) pinAgvBtn->hide();
    }

    clearTasks();

    QSqlQuery q(db);
    q.prepare("SELECT id, agv_id, task_name, task_description, interval_days, duration_minutes, is_default, next_date, assigned_to, delegated_by "
              "FROM agv_tasks WHERE agv_id = :id ORDER BY id ASC");
    q.bindValue(":id", agvId);
    if (!q.exec()) {
        qDebug() << "AgvSettingsPage::loadAgv: agv_tasks query failed:" << q.lastError().text();
        return;
    }

    while (q.next())
    {
        AgvTask t;
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

        addTaskRow(t, t.id);
    }

    tasksLayout->addStretch();
    updateCheckboxVisibility();
}

void AgvSettingsPage::addTaskRow(const AgvTask &task, const QString &taskId)
{
    QFrame *row = new QFrame(this);
    row->setStyleSheet("background:white;");
    row->setMinimumHeight(s(48));
    row->setProperty("task_id", taskId);
    row->installEventFilter(this);
    row->setAttribute(Qt::WA_Hover);
    row->setCursor(Qt::PointingHandCursor);

    QHBoxLayout *h = new QHBoxLayout(row);
    h->setContentsMargins(s(10), s(4), s(10), s(4));
    h->setSpacing(s(10));

    QCheckBox *cb = new QCheckBox(row);
    cb->setFixedSize(s(20), s(20));
    cb->setVisible(editMode);
    cb->setProperty("task_id", taskId);

    connect(cb, &QCheckBox::stateChanged, this, [this](){
        bool any = false;
        for (int i = 0; i < tasksLayout->count(); ++i) {
            QWidget *w = tasksLayout->itemAt(i)->widget();
            if (!w) continue;
            QCheckBox *c = w->findChild<QCheckBox *>();
            if (c && c->isVisible() && c->isChecked()) {
                any = true;
                break;
            }
        }
        deleteSelectedBtn->setVisible(editMode && any);
    });

    h->addWidget(cb, 0);

    QLabel *name = new QLabel(task.taskName, row);
    name->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:700;color:#000;"
    ).arg(s(16)));
    name->setMinimumWidth(s(120));
    name->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    h->addWidget(name, 3);

    QLabel *interval = new QLabel(QString::number(task.intervalDays) + " дн.", row);
    interval->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;color:#555;"
    ).arg(s(14)));
    interval->setFixedWidth(s(90));
    interval->setAlignment(Qt::AlignCenter);
    h->addWidget(interval, 0);

    QLabel *dur = new QLabel(QString::number(task.durationMinutes) + " мин", row);
    dur->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;color:#555;"
    ).arg(s(14)));
    dur->setFixedWidth(s(90));
    dur->setAlignment(Qt::AlignCenter);
    h->addWidget(dur, 0);

    QDate next = task.nextDate.isValid() ? task.nextDate
                                         : computeNextDate(task.lastService, task.intervalDays);

    QLabel *nextLbl = new QLabel(next.toString("dd.MM.yyyy"), row);
    nextLbl->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:700;"
    ).arg(s(14)));
    nextLbl->setAlignment(Qt::AlignCenter);
    nextLbl->setFixedWidth(s(110));

    int daysLeft = QDate::currentDate().daysTo(next);
    if (daysLeft <= 3)
        nextLbl->setStyleSheet("color:#FF0000;font-weight:800;");
    else if (daysLeft < 7)
        nextLbl->setStyleSheet("color:#FF8800;font-weight:800;");
    else
        nextLbl->setStyleSheet("color:#18CF00;font-weight:800;");

    h->addWidget(nextLbl, 0);

    QString assignText = !task.assignedTo.isEmpty() ? task.assignedTo
                        : !originalAssignedUser.isEmpty() ? originalAssignedUser
                        : "—";
    QLabel *assignLbl = new QLabel(row);
    assignLbl->setText(assignLbl->fontMetrics().elidedText(assignText, Qt::ElideRight, s(100)));
    assignLbl->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;color:%2;font-weight:600;"
    ).arg(s(12)).arg(task.assignedTo.isEmpty() ? "#777" : "#2563EB"));
    assignLbl->setAlignment(Qt::AlignCenter);
    assignLbl->setFixedWidth(s(110));
    if (!task.assignedTo.isEmpty())
        assignLbl->setToolTip(task.assignedTo);
    h->addWidget(assignLbl, 0);

    QString currentUser = AppSession::currentUsername();
    QString curRole = getUserRole(currentUser);
    const bool isOffline = (originalStatus.trimmed().toLower() == "offline");
    const QString effectiveAssignee = !task.assignedTo.isEmpty() ? task.assignedTo : originalAssignedUser;
    bool canComplete = !isOffline && ((curRole == "admin" || curRole == "tech") || effectiveAssignee.isEmpty() || effectiveAssignee == currentUser);

    if (canComplete) {
        QPushButton *completeBtn = new QPushButton("Провести", row);
        completeBtn->setFixedWidth(s(130));
        completeBtn->setStyleSheet(QString(
            "QPushButton { background:#0F00DB; color:white; "
            "font-family:Inter; font-size:%1px; font-weight:800; padding:4px 10px; }"
            "QPushButton:hover { background:#1A4ACD; }"
        ).arg(s(13)));
        connect(completeBtn, &QPushButton::clicked, this, [this, taskId, task](){
            if (completeTaskNow(taskId, task)) {
                loadAgv(currentAgvId);
                emit tasksChanged();
                emit DataBus::instance().calendarChanged();
            }
        });
        h->addWidget(completeBtn, 0, Qt::AlignRight);
    } else {
        QLabel *placeLbl = new QLabel("—", row);
        placeLbl->setFixedWidth(s(130));
        placeLbl->setAlignment(Qt::AlignCenter);
        placeLbl->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:#999;").arg(s(14)));
        h->addWidget(placeLbl, 0, Qt::AlignRight);
    }

    tasksLayout->addWidget(row);
}

void AgvSettingsPage::highlightTask(const QString &taskName)
{
    if (!tasksLayout)
        return;

    QScrollArea *scroll = tableWrapper->findChild<QScrollArea *>();
    if (!scroll)
        return;

    for (int i = 0; i < tasksLayout->count(); i++) {
        QWidget *row = tasksLayout->itemAt(i)->widget();
        if (!row) continue;

        QLabel *name = row->findChild<QLabel *>();
        if (!name) continue;

        if (name->text() == taskName) {
            row->setStyleSheet("background:#FFF3CD;border-radius:10px;");
            scroll->ensureWidgetVisible(row);
            return;
        }
    }
}
