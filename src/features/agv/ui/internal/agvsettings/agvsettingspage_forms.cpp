#include "agvsettingspage.h"

#include "app_session.h"
#include "databus.h"
#include "db_users.h"
#include "notifications_logs.h"

#include <QCalendarWidget>
#include <QComboBox>
#include <QDate>
#include <QDebug>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVBoxLayout>

class DatePickDialog : public QDialog
{
public:
    DatePickDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("Выбор даты");
        setModal(true);
        setMinimumSize(460, 460);
        resize(520, 520);
        setSizeGripEnabled(true);

        setStyleSheet(
            "QDialog { background:white; border-radius:12px; }"
            "QPushButton { font-family:Inter; font-size:18px; font-weight:600; "
            "border-radius:10px; padding:10px 20px; }"
            "QPushButton#okBtn { background:#0F00DB; color:white; }"
            "QPushButton#okBtn:hover { background:#1A4ACD; }"
            "QPushButton#cancelBtn { background:#E6E6E6; color:black; }"
            "QPushButton#cancelBtn:hover { background:#D5D5D5; }"
        );

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(20, 20, 20, 20);
        root->setSpacing(20);

        calendar = new QCalendarWidget(this);
        calendar->setGridVisible(true);
        calendar->setMinimumHeight(320);

        root->addWidget(calendar);

        QHBoxLayout *btns = new QHBoxLayout();
        btns->addStretch();

        QPushButton *cancel = new QPushButton("Отмена", this);
        cancel->setObjectName("cancelBtn");

        QPushButton *ok = new QPushButton("ОК", this);
        ok->setObjectName("okBtn");

        btns->addWidget(cancel);
        btns->addWidget(ok);

        root->addLayout(btns);

        connect(ok, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    }

    QDate selected() const { return calendar->selectedDate(); }

private:
    QCalendarWidget *calendar = nullptr;
};

void AgvSettingsPage::showForm(QWidget *form)
{
    if (formWrapper) {
        rootLayout->removeWidget(formWrapper);
        formWrapper->deleteLater();
        formWrapper = nullptr;
    }

    formWrapper = form;
    tableWrapper->hide();

    int idx = rootLayout->indexOf(tableWrapper);
    if (idx < 0)
        idx = 2;

    rootLayout->insertWidget(idx, formWrapper, 1);
}

void AgvSettingsPage::closeForm()
{
    if (formWrapper) {
        rootLayout->removeWidget(formWrapper);
        formWrapper->deleteLater();
        formWrapper = nullptr;
    }
    tableWrapper->show();
    addFormOpened = false;
    QString curRole = getUserRole(AppSession::currentUsername());
    bool canEditTasks = (curRole == "admin" || curRole == "tech");
    addTaskBtn->setEnabled(true);
    editModeBtn->setEnabled(true);
    addTaskBtn->setVisible(canEditTasks);
    editModeBtn->setVisible(canEditTasks);
    deleteSelectedBtn->show();
    if (!recentlyDeleted.isEmpty())
        undoDeleteBtn->show();
    else
        undoDeleteBtn->hide();
    if (historyTasksBtn)
        historyTasksBtn->show();
}

void AgvSettingsPage::openAddTaskForm()
{
    addFormOpened = true;
    addTaskBtn->hide();
    editModeBtn->hide();
    deleteSelectedBtn->hide();
    undoDeleteBtn->hide();
    if (historyTasksBtn)
        historyTasksBtn->hide();

    QFrame *form = new QFrame(this);
    form->setStyleSheet("background:white;border-radius:10px;");
    form->setMinimumHeight(s(320));

    QVBoxLayout *v = new QVBoxLayout(form);
    v->setContentsMargins(s(15), s(10), s(15), s(10));
    v->setSpacing(s(10));

    int labelSize = s(19);
    int editSize  = s(18);

    QString editStyle = QString(
        "font-family:Inter;"
        "font-size:%1px;"
        "font-weight:500;"
        "color:#000;"
        "border:1px solid #C8C8C8;"
        "border-radius:8px;"
        "padding:8px 10px;"
    ).arg(editSize);

    QString labelStyle = QString(
        "font-family:Inter;"
        "font-size:%1px;"
        "font-weight:600;"
        "color:#1A1A1A;"
    ).arg(labelSize);

    QLabel *nameLbl = new QLabel("Название:", form);
    nameLbl->setStyleSheet(labelStyle);

    QLineEdit *name = new QLineEdit(form);
    name->setMaxLength(50);
    name->setStyleSheet(editStyle);

    QLabel *daysLbl = new QLabel("Периодичность (дни):", form);
    daysLbl->setStyleSheet(labelStyle);

    QLineEdit *days = new QLineEdit(form);
    days->setMaxLength(3);
    days->setValidator(new QIntValidator(0, 999, days));
    days->setStyleSheet(editStyle);

    connect(days, &QLineEdit::textChanged, this, [=](){
        QString t = days->text();
        t.remove(' ');
        if (t != days->text())
            days->setText(t);
    });

    QLabel *minLbl = new QLabel("Минуты:", form);
    minLbl->setStyleSheet(labelStyle);

    QLineEdit *minutes = new QLineEdit(form);
    minutes->setMaxLength(4);
    minutes->setValidator(new QIntValidator(0, 9999, minutes));
    minutes->setStyleSheet(editStyle);

    connect(minutes, &QLineEdit::textChanged, this, [=](){
        QString t = minutes->text();
        t.remove(' ');
        if (t != minutes->text())
            minutes->setText(t);
    });

    QLabel *hoursLbl = new QLabel("В часах: 0 ч", form);
    hoursLbl->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:600;color:#0077CC;"
    ).arg(s(16)));

    connect(minutes, &QLineEdit::textChanged, this, [=](){
        bool ok = false;
        int m = minutes->text().toInt(&ok);
        if (!ok || m <= 0) {
            hoursLbl->setText("В часах: 0 ч");
            return;
        }
        double h = m / 60.0;
        hoursLbl->setText("В часах: " + QString::number(h, 'f', 2) + " ч");
    });

    QLabel *assignLbl = new QLabel("Назначить:", form);
    assignLbl->setStyleSheet(labelStyle);

    QComboBox *assignCombo = new QComboBox(form);
    assignCombo->setStyleSheet(editStyle);
    assignCombo->addItem("— Не назначено —", "");

    QString curRole = getUserRole(AppSession::currentUsername());
    bool canAssign = (curRole == "admin" || curRole == "tech");

    if (canAssign) {
        QVector<UserInfo> allUsers = getAllUsers(false);
        for (const UserInfo &u : allUsers) {
            if (u.role != "viewer") continue;
            QString display = u.fullName.isEmpty() ? u.username : QString("%1 (%2)").arg(u.fullName, u.username);
            assignCombo->addItem(display, u.username);
        }
        if (!originalAssignedUser.isEmpty()) {
            int idx = assignCombo->findData(originalAssignedUser);
            if (idx >= 0) assignCombo->setCurrentIndex(idx);
        }
    }
    assignLbl->setVisible(canAssign);
    assignCombo->setVisible(canAssign);

    QLabel *lastLbl = new QLabel("Последнее обслуживание:", form);
    lastLbl->setStyleSheet(labelStyle);

    QHBoxLayout *dateRow = new QHBoxLayout();

    QLineEdit *manual = new QLineEdit(form);
    manual->setPlaceholderText("дд.мм.гггг");
    manual->setStyleSheet(editStyle + "font-size:20px; height:48px;");
    manual->setMinimumHeight(48);
    manual->setMaxLength(10);

    QRegularExpression rx("^\\d{0,2}(\\.\\d{0,2}(\\.\\d{0,4})?)?$");
    manual->setValidator(new QRegularExpressionValidator(rx, manual));

    connect(manual, &QLineEdit::textChanged, this, [manual]() {
        QString t = manual->text();
        QString digits;
        for (QChar c : t)
            if (c.isDigit())
                digits += c;

        QString out;
        if (digits.length() >= 1) out += digits.mid(0, 2);
        if (digits.length() >= 3) out += "." + digits.mid(2, 2);
        if (digits.length() >= 5) out += "." + digits.mid(4, 4);

        if (out != t)
            manual->setText(out);
    });

    QPushButton *pickDateBtn = new QPushButton("Выбрать дату", form);
    pickDateBtn->setStyleSheet(QString(
        "background:#0F00DB;color:white;font-family:Inter;font-size:%1px;font-weight:700;"
        "border-radius:%2px;padding:%3px %4px;"
    ).arg(s(20)).arg(s(10)).arg(s(10)).arg(s(20)));
    pickDateBtn->setMinimumHeight(48);
    pickDateBtn->setMinimumWidth(180);

    dateRow->addWidget(manual);
    dateRow->addWidget(pickDateBtn);

    QPushButton *save = new QPushButton("Сохранить", form);
    save->setStyleSheet(QString(
        "background:#28A745;color:white;font-family:Inter;font-size:%1px;font-weight:800;"
        "border-radius:%2px;padding:%3px %4px;"
    ).arg(s(18)).arg(s(10)).arg(s(10)).arg(s(20)));
    save->setMinimumHeight(50);

    QPushButton *cancel = new QPushButton("Отменить", form);
    cancel->setStyleSheet(QString(
        "background:#E6E6E6;color:black;font-family:Inter;font-size:%1px;font-weight:800;"
        "border-radius:%2px;padding:%3px %4px;"
    ).arg(s(18)).arg(s(10)).arg(s(10)).arg(s(20)));
    cancel->setMinimumHeight(50);

    connect(pickDateBtn, &QPushButton::clicked, this, [=](){
        DatePickDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            manual->setText(dlg.selected().toString("dd.MM.yyyy"));
        }
    });

    connect(cancel, &QPushButton::clicked, this, [this](){
        closeForm();
    });

    connect(save, &QPushButton::clicked, this, [=](){
        QString baseEditStyle = editStyle;

        auto markError = [&](QLineEdit *e){
            e->setStyleSheet(baseEditStyle + "border:2px solid #FF3B30;");
        };
        auto clearError = [&](QLineEdit *e){
            e->setStyleSheet(baseEditStyle);
        };

        bool okAll = true;

        if (name->text().trimmed().isEmpty()) {
            markError(name);
            okAll = false;
        } else clearError(name);

        if (days->text().isEmpty()) {
            markError(days);
            okAll = false;
        } else clearError(days);

        if (minutes->text().isEmpty()) {
            markError(minutes);
            okAll = false;
        } else clearError(minutes);

        if (!okAll)
            return;

        QDate lastService;

        QString manualText = manual->text().trimmed();
        if (!manualText.isEmpty()) {
            QDate d = QDate::fromString(manualText, "dd.MM.yyyy");
            if (d.isValid())
                lastService = d;
        }

        if (!lastService.isValid())
            lastService = QDate::currentDate();

        int interval = days->text().toInt();
        QDate nextDate = computeNextDate(lastService, interval);

        QString assignedUser = assignCombo->currentData().toString();
        QString delegatedBy;
        if (!assignedUser.isEmpty())
            delegatedBy = AppSession::currentUsername();
        if (assignedUser.isEmpty() && !originalAssignedUser.isEmpty()) {
            assignedUser = originalAssignedUser;
            delegatedBy = originalAssignedBy;
        }

        QSqlDatabase db = QSqlDatabase::database("main_connection");
        QSqlQuery q(db);
        q.prepare("INSERT INTO agv_tasks (agv_id, task_name, task_description, interval_days, duration_minutes, is_default, next_date, assigned_to, delegated_by) "
                  "VALUES (:id, :n, :dsc, :d, :m, 0, :next, :assign, :delegated_by)");
        q.bindValue(":id", currentAgvId);
        q.bindValue(":n", name->text());
        q.bindValue(":dsc", QVariant(QVariant::String));
        q.bindValue(":d", interval);
        q.bindValue(":m", minutes->text().toInt());
        q.bindValue(":next", nextDate.toString("yyyy-MM-dd"));
        q.bindValue(":assign", assignedUser);
        q.bindValue(":delegated_by", delegatedBy);
        if (!q.exec()) {
            qDebug() << "AgvSettingsPage::openAddTaskForm: insert failed:" << q.lastError().text();
        }

        if (!assignedUser.isEmpty()) {
            const QString whoUser = AppSession::currentUsername();
            const QString whoDisplay = userDisplayName(whoUser);
            addNotificationForUser(
                assignedUser,
                "Задача делегирована",
                QString("Вам делегирована задача \"%1\" для AGV %2. Делегировал: %3 [peer:%4]")
                    .arg(name->text(), currentAgvId, whoDisplay, whoUser));
        } else {
            const QString whoUser = AppSession::currentUsername();
            const QString whoDisplay = userDisplayName(whoUser);
            QVector<UserInfo> allUsers = getAllUsers(false);
            for (const UserInfo &u : allUsers) {
                if (u.role != "admin" && u.role != "tech") {
                    addNotificationForUser(
                        u.username,
                        "Новая задача",
                        QString("Доступна задача «%1» для AGV %2. Добавил: %3 [peer:%4]")
                            .arg(name->text(), currentAgvId, whoDisplay, whoUser));
                }
            }
        }
        emit DataBus::instance().notificationsChanged();

        closeForm();
        loadAgv(currentAgvId);
        emit tasksChanged();
    });

    v->addWidget(nameLbl);
    v->addWidget(name);
    v->addWidget(daysLbl);
    v->addWidget(days);
    v->addWidget(minLbl);
    v->addWidget(minutes);
    v->addWidget(hoursLbl);
    v->addWidget(assignLbl);
    v->addWidget(assignCombo);
    v->addWidget(lastLbl);
    v->addLayout(dateRow);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addWidget(cancel);
    btnRow->addStretch();
    btnRow->addWidget(save);

    v->addLayout(btnRow);

    showForm(form);
}

void AgvSettingsPage::openEditTaskForm(const QString &taskId, const AgvTask &task)
{
    if (addFormOpened)
        return;

    addFormOpened = true;
    addTaskBtn->hide();
    editModeBtn->hide();
    deleteSelectedBtn->hide();
    undoDeleteBtn->hide();
    if (historyTasksBtn)
        historyTasksBtn->hide();

    QFrame *form = new QFrame(this);
    form->setStyleSheet("background:white;border-radius:10px;");
    form->setMinimumHeight(s(320));

    QVBoxLayout *v = new QVBoxLayout(form);
    v->setContentsMargins(s(15), s(10), s(15), s(10));
    v->setSpacing(s(10));

    int labelSize = s(19);
    int editSize  = s(18);

    QString editStyle = QString(
        "font-family:Inter;"
        "font-size:%1px;"
        "font-weight:500;"
        "color:#000;"
        "border:1px solid #C8C8C8;"
        "border-radius:8px;"
        "padding:8px 10px;"
    ).arg(editSize);

    QString labelStyle = QString(
        "font-family:Inter;"
        "font-size:%1px;"
        "font-weight:600;"
        "color:#1A1A1A;"
    ).arg(labelSize);

    QLabel *nameLbl = new QLabel("Название:", form);
    nameLbl->setStyleSheet(labelStyle);

    QLineEdit *name = new QLineEdit(form);
    name->setMaxLength(50);
    name->setStyleSheet(editStyle);
    name->setText(task.taskName);

    QLabel *daysLbl = new QLabel("Периодичность (дни):", form);
    daysLbl->setStyleSheet(labelStyle);

    QLineEdit *days = new QLineEdit(form);
    days->setMaxLength(3);
    days->setValidator(new QIntValidator(0, 999, days));
    days->setStyleSheet(editStyle);
    days->setText(QString::number(task.intervalDays));

    connect(days, &QLineEdit::textChanged, this, [=](){
        QString t = days->text();
        t.remove(' ');
        if (t != days->text())
            days->setText(t);
    });

    QLabel *minLbl = new QLabel("Минуты:", form);
    minLbl->setStyleSheet(labelStyle);

    QLineEdit *minutes = new QLineEdit(form);
    minutes->setMaxLength(4);
    minutes->setValidator(new QIntValidator(0, 9999, minutes));
    minutes->setStyleSheet(editStyle);
    minutes->setText(QString::number(task.durationMinutes));

    connect(minutes, &QLineEdit::textChanged, this, [=](){
        QString t = minutes->text();
        t.remove(' ');
        if (t != minutes->text())
            minutes->setText(t);
    });

    QLabel *hoursLbl = new QLabel("В часах: 0 ч", form);
    hoursLbl->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:600;color:#0077CC;"
    ).arg(s(16)));

    connect(minutes, &QLineEdit::textChanged, this, [=](){
        bool ok = false;
        int m = minutes->text().toInt(&ok);
        if (!ok || m <= 0) {
            hoursLbl->setText("В часах: 0 ч");
            return;
        }
        double h = m / 60.0;
        hoursLbl->setText("В часах: " + QString::number(h, 'f', 2) + " ч");
    });

    QLabel *editAssignLbl = new QLabel("Назначить:", form);
    editAssignLbl->setStyleSheet(labelStyle);

    QComboBox *editAssignCombo = new QComboBox(form);
    editAssignCombo->setStyleSheet(editStyle);
    editAssignCombo->addItem("— Не назначено —", "");

    QString editCurRole = getUserRole(AppSession::currentUsername());
    bool editCanAssign = (editCurRole == "admin" || editCurRole == "tech");

    if (editCanAssign) {
        QVector<UserInfo> allUsers = getAllUsers(false);
        for (const UserInfo &u : allUsers) {
            if (u.role != "viewer") continue;
            QString display = u.fullName.isEmpty() ? u.username : QString("%1 (%2)").arg(u.fullName, u.username);
            editAssignCombo->addItem(display, u.username);
        }
        int idx = editAssignCombo->findData(task.assignedTo);
        if (idx >= 0) editAssignCombo->setCurrentIndex(idx);
    }
    editAssignLbl->setVisible(editCanAssign);
    editAssignCombo->setVisible(editCanAssign);

    QLabel *lastLbl = new QLabel("Последнее обслуживание:", form);
    lastLbl->setStyleSheet(labelStyle);

    QHBoxLayout *dateRow = new QHBoxLayout();

    QLineEdit *manual = new QLineEdit(form);
    manual->setPlaceholderText("дд.мм.гггг");
    manual->setStyleSheet(editStyle + "font-size:20px; height:48px;");
    manual->setMinimumHeight(48);
    manual->setMaxLength(10);

    if (task.lastService.isValid())
        manual->setText(task.lastService.toString("dd.MM.yyyy"));

    QRegularExpression rx("^\\d{0,2}(\\.\\d{0,2}(\\.\\d{0,4})?)?$");
    manual->setValidator(new QRegularExpressionValidator(rx, manual));

    connect(manual, &QLineEdit::textChanged, this, [manual]() {
        QString t = manual->text();
        QString digits;
        for (QChar c : t)
            if (c.isDigit())
                digits += c;

        QString out;
        if (digits.length() >= 1) out += digits.mid(0, 2);
        if (digits.length() >= 3) out += "." + digits.mid(2, 2);
        if (digits.length() >= 5) out += "." + digits.mid(4, 4);

        if (out != t)
            manual->setText(out);
    });

    QPushButton *pickDateBtn = new QPushButton("Выбрать дату", form);
    pickDateBtn->setStyleSheet(QString(
        "background:#0F00DB;color:white;font-family:Inter;font-size:%1px;font-weight:700;"
        "border-radius:%2px;padding:%3px %4px;"
    ).arg(s(20)).arg(s(10)).arg(s(10)).arg(s(20)));
    pickDateBtn->setMinimumHeight(48);
    pickDateBtn->setMinimumWidth(180);

    dateRow->addWidget(manual);
    dateRow->addWidget(pickDateBtn);

    QPushButton *save = new QPushButton("Сохранить", form);
    save->setStyleSheet(QString(
        "background:#28A745;color:white;font-family:Inter;font-size:%1px;font-weight:800;"
        "border-radius:%2px;padding:%3px %4px;"
    ).arg(s(18)).arg(s(10)).arg(s(10)).arg(s(20)));
    save->setMinimumHeight(50);

    QPushButton *cancel = new QPushButton("Отменить", form);
    cancel->setStyleSheet(QString(
        "background:#E6E6E6;color:black;font-family:Inter;font-size:%1px;font-weight:800;"
        "border-radius:%2px;padding:%3px %4px;"
    ).arg(s(18)).arg(s(10)).arg(s(10)).arg(s(20)));
    cancel->setMinimumHeight(50);

    connect(pickDateBtn, &QPushButton::clicked, this, [=](){
        DatePickDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            manual->setText(dlg.selected().toString("dd.MM.yyyy"));
        }
    });

    connect(cancel, &QPushButton::clicked, this, [this](){
        closeForm();
        loadAgv(currentAgvId);
        emit tasksChanged();
    });

    connect(save, &QPushButton::clicked, this, [=](){
        QString baseEditStyle = editStyle;

        auto markError = [&](QLineEdit *e){
            e->setStyleSheet(baseEditStyle + "border:2px solid #FF3B30;");
        };
        auto clearError = [&](QLineEdit *e){
            e->setStyleSheet(baseEditStyle);
        };

        bool okAll = true;

        if (name->text().trimmed().isEmpty()) {
            markError(name);
            okAll = false;
        } else clearError(name);

        if (days->text().isEmpty()) {
            markError(days);
            okAll = false;
        } else clearError(days);

        if (minutes->text().isEmpty()) {
            markError(minutes);
            okAll = false;
        } else clearError(minutes);

        if (!okAll)
            return;

        QDate lastService;

        QString manualText = manual->text().trimmed();
        if (!manualText.isEmpty()) {
            QDate d = QDate::fromString(manualText, "dd.MM.yyyy");
            if (d.isValid())
                lastService = d;
        }

        if (!lastService.isValid())
            lastService = QDate::currentDate();

        int interval = days->text().toInt();
        QDate nextDate = computeNextDate(lastService, interval);

        QString editAssignedUser = editAssignCombo->currentData().toString();
        QString newDelegatedBy;
        if (editAssignedUser.isEmpty()) {
            newDelegatedBy.clear();
        } else if (editAssignedUser != task.assignedTo) {
            newDelegatedBy = AppSession::currentUsername();
        } else {
            newDelegatedBy = task.delegatedBy;
        }

        QSqlDatabase db = QSqlDatabase::database("main_connection");
        QSqlQuery q(db);
        q.prepare("UPDATE agv_tasks SET task_name = :n, task_description = :dsc, interval_days = :d, "
                  "duration_minutes = :m, next_date = :next, assigned_to = :assign, delegated_by = :delegated_by WHERE id = :tid");
        q.bindValue(":n", name->text());
        q.bindValue(":dsc", QVariant(QVariant::String));
        q.bindValue(":d", interval);
        q.bindValue(":m", minutes->text().toInt());
        q.bindValue(":next", nextDate.toString("yyyy-MM-dd"));
        q.bindValue(":assign", editAssignedUser);
        q.bindValue(":delegated_by", newDelegatedBy);
        q.bindValue(":tid", taskId.toInt());
        if (!q.exec()) {
            qDebug() << "AgvSettingsPage::openEditTaskForm: update failed:" << q.lastError().text();
        }

        if (editAssignedUser != task.assignedTo) {
            if (!editAssignedUser.isEmpty()) {
                const QString whoUser = AppSession::currentUsername();
                const QString whoDisplay = userDisplayName(whoUser);
                addNotificationForUser(
                    editAssignedUser,
                    "Задача делегирована",
                    QString("Вам делегирована задача \"%1\" для AGV %2. Делегировал: %3 [peer:%4]")
                        .arg(name->text(), currentAgvId, whoDisplay, whoUser));
            } else {
                const QString whoUser = AppSession::currentUsername();
                const QString whoDisplay = userDisplayName(whoUser);
                QVector<UserInfo> allUsers = getAllUsers(false);
                for (const UserInfo &u : allUsers) {
                    if (u.role != "admin" && u.role != "tech") {
                        addNotificationForUser(
                            u.username,
                            "Задача назначена",
                            QString("Доступна задача «%1» для AGV %2. Добавил: %3 [peer:%4]")
                                .arg(name->text(), currentAgvId, whoDisplay, whoUser));
                    }
                }
            }
            emit DataBus::instance().notificationsChanged();
        }

        closeForm();
        loadAgv(currentAgvId);
        emit tasksChanged();
    });

    v->addWidget(nameLbl);
    v->addWidget(name);
    v->addWidget(daysLbl);
    v->addWidget(days);
    v->addWidget(minLbl);
    v->addWidget(minutes);
    v->addWidget(hoursLbl);
    v->addWidget(editAssignLbl);
    v->addWidget(editAssignCombo);
    v->addWidget(lastLbl);
    v->addLayout(dateRow);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addWidget(cancel);
    btnRow->addStretch();
    btnRow->addWidget(save);

    v->addLayout(btnRow);

    showForm(form);
}
