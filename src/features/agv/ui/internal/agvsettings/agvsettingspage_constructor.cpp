#include "agvsettingspage.h"

#include "app_session.h"
#include "databus.h"
#include "db_users.h"
#include "db_agv_errors.h"
#include "notifications_logs.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QDateEdit>
#include <QTimeEdit>
#include <QFormLayout>


AgvSettingsPage::AgvSettingsPage(std::function<int(int)> scale, QWidget *parent)
    : QWidget(parent), s(scale)
{
    setStyleSheet("QLabel { background: transparent; color:#000; }");

    rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(s(10), s(10), s(10), s(10));
    rootLayout->setSpacing(s(10));

    QHBoxLayout *top = new QHBoxLayout();

    backButton = new QPushButton("   Назад", this);
    backButton->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    backButton->setIconSize(QSize(s(24), s(24)));
    backButton->setFixedSize(s(150), s(50));
    backButton->setStyleSheet(QString(
        "QPushButton { background:#E6E6E6; border-radius:%1px; border:1px solid #C8C8C8;"
        "font-family:Inter; font-size:%2px; font-weight:800; color:black; text-align:left; padding-left:%3px; }"
        "QPushButton:hover { background:#D5D5D5; }"
    ).arg(s(10)).arg(s(16)).arg(s(10)));

    connect(backButton, &QPushButton::clicked, this, [this](){
        if (formWrapper || addFormOpened) {
            closeForm();
            loadAgv(currentAgvId);
            emit tasksChanged();
        } else {
            emit backRequested();
        }
    });

    titleLabel = new QLabel("Информация об AGV", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QString(
        "font-family:Inter; font-size:%1px; font-weight:900; color:#1A1A1A;"
    ).arg(s(26)));

    top->addWidget(backButton);
    top->addStretch();
    top->addWidget(titleLabel, 0, Qt::AlignCenter);
    top->addStretch();

    pinAgvBtn = new QPushButton("Закрепить за", this);
    pinAgvBtn->setStyleSheet(QString(
        "QPushButton { background:#E8E8E8; color:#000; font-family:Inter; font-size:%1px; font-weight:800; "
        "padding:8px 16px; border:1px solid #C8C8C8; } QPushButton:hover { background:#D8D8D8; }"
    ).arg(s(14)));
    pinAgvBtn->setFixedHeight(s(40));
    connect(pinAgvBtn, &QPushButton::clicked, this, [this](){
        QString role = getUserRole(AppSession::currentUsername());
        if (role != "admin" && role != "tech") return;
        QDialog dlg(this);
        dlg.setWindowTitle("Закрепить AGV за пользователем");
        dlg.setModal(true);
        dlg.setFixedWidth(360);
        QVBoxLayout *v = new QVBoxLayout(&dlg);
        QComboBox *cb = new QComboBox(&dlg);
        cb->addItem("— Нет —", "");
        QVector<UserInfo> allUsers = getAllUsers(false);
        for (const UserInfo &u : allUsers) {
            if (u.role != "viewer") continue;
            QString display = u.fullName.isEmpty() ? u.username : QString("%1 (%2)").arg(u.fullName, u.username);
            cb->addItem(display, u.username);
        }
        int idx = cb->findData(originalAssignedUser);
        if (idx >= 0) cb->setCurrentIndex(idx);
        v->addWidget(new QLabel("Пользователь:", &dlg));
        v->addWidget(cb);
        QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        v->addWidget(bb);
        if (dlg.exec() == QDialog::Accepted) {
            QString newUser = cb->currentData().toString();
            QSqlDatabase db = QSqlDatabase::database("main_connection");
            if (db.isOpen()) {
                QString whoAssigned = AppSession::currentUsername();
                QDateTime whenAssigned = QDateTime::currentDateTime();

                QSqlQuery q(db);
                if (newUser.isEmpty()) {
                    q.prepare("UPDATE agv_list SET assigned_user = '', assigned_by = '' WHERE agv_id = :id");
                } else {
                    q.prepare("UPDATE agv_list SET assigned_user = :u, assigned_by = :by WHERE agv_id = :id");
                    q.bindValue(":u", newUser);
                    q.bindValue(":by", whoAssigned);
                }
                q.bindValue(":id", currentAgvId);

                if (q.exec()) {
                    QSqlQuery updTasks(db);
                    if (newUser.isEmpty()) {
                        updTasks.prepare("UPDATE agv_tasks SET assigned_to = '', delegated_by = '' WHERE agv_id = :id");
                    } else {
                        updTasks.prepare("UPDATE agv_tasks SET assigned_to = :u, delegated_by = :by WHERE agv_id = :id");
                        updTasks.bindValue(":u", newUser);
                        updTasks.bindValue(":by", whoAssigned);
                    }
                    updTasks.bindValue(":id", currentAgvId);
                    updTasks.exec();

                    if (!newUser.isEmpty()) {
                        const QString whoDisplay = userDisplayName(whoAssigned);
                        addNotificationForUser(newUser,
                            "AGV закреплена за вами",
                            QString("За вами закреплена AGV %1. Закрепил: %2, %3 [peer:%4]")
                                .arg(currentAgvId, whoDisplay, whenAssigned.toString("dd.MM.yyyy HH:mm"), whoAssigned));
                        emit DataBus::instance().notificationsChanged();
                    }

                    originalAssignedUser = newUser;
                    originalAssignedBy = whoAssigned;
                    assignedUserLabel->setText(newUser.isEmpty() ? "—" : newUser);
                    if (pinAgvBtn) pinAgvBtn->setText(newUser.isEmpty() ? "Закрепить за" : QString("Закреплён за %1").arg(newUser));
                    emit DataBus::instance().agvListChanged();
                }
            }
        }
    });
    top->addWidget(pinAgvBtn, 0, Qt::AlignRight);

    QPushButton *addErrorBtn = new QPushButton(QStringLiteral("Добавить ошибку"), this);
    addErrorBtn->setStyleSheet(QString(
        "QPushButton { background:#FDE68A; color:#111; font-family:Inter; font-size:%1px; font-weight:900; "
        "padding:8px 14px; border:1px solid #F59E0B; border-radius:%2px; } "
        "QPushButton:hover { background:#FCD34D; }"
    ).arg(s(14)).arg(s(10)));
    addErrorBtn->setFixedHeight(s(40));
    connect(addErrorBtn, &QPushButton::clicked, this, [this]() {
        if (currentAgvId.trimmed().isEmpty())
            return;

        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("Добавить ошибку AGV"));
        dlg.setModal(true);
        dlg.setMinimumWidth(s(460));
        QVBoxLayout *root = new QVBoxLayout(&dlg);

        QFormLayout *form = new QFormLayout();
        form->setLabelAlignment(Qt::AlignLeft);
        form->setFormAlignment(Qt::AlignTop);

        QDateEdit *dateEdit = new QDateEdit(QDate::currentDate(), &dlg);
        dateEdit->setCalendarPopup(true);
        dateEdit->setDisplayFormat("dd.MM.yyyy");

        QLineEdit *typeEdit = new QLineEdit(&dlg);
        typeEdit->setPlaceholderText(QStringLiteral("Тип (например: Электрика)"));
        typeEdit->setMaxLength(64);

        QLineEdit *titleEdit = new QLineEdit(&dlg);
        titleEdit->setPlaceholderText(QStringLiteral("Название ошибки"));
        titleEdit->setMaxLength(256);

        QWidget *timeRow = new QWidget(&dlg);
        QHBoxLayout *timeL = new QHBoxLayout(timeRow);
        timeL->setContentsMargins(0, 0, 0, 0);
        timeL->setSpacing(s(8));
        QTimeEdit *fromEdit = new QTimeEdit(QTime(12, 0), timeRow);
        fromEdit->setDisplayFormat("HH:mm");
        QTimeEdit *toEdit = new QTimeEdit(QTime(14, 0), timeRow);
        toEdit->setDisplayFormat("HH:mm");
        QLabel *minsLbl = new QLabel(timeRow);
        minsLbl->setStyleSheet(QString("font-family:Inter;font-size:%1px;font-weight:800;color:#334155;").arg(s(13)));

        auto computeMinutes = [fromEdit, toEdit]() -> int {
            const QTime a = fromEdit->time();
            const QTime b = toEdit->time();
            int m = a.secsTo(b) / 60;
            if (m < 0) m += 24 * 60; // если через полночь
            return qMax(0, m);
        };
        auto refreshMins = [minsLbl, computeMinutes]() {
            minsLbl->setText(QStringLiteral("= %1 мин").arg(computeMinutes()));
        };
        refreshMins();
        connect(fromEdit, &QTimeEdit::timeChanged, &dlg, [refreshMins](const QTime &) { refreshMins(); });
        connect(toEdit, &QTimeEdit::timeChanged, &dlg, [refreshMins](const QTime &) { refreshMins(); });

        timeL->addWidget(new QLabel(QStringLiteral("с"), timeRow));
        timeL->addWidget(fromEdit);
        timeL->addWidget(new QLabel(QStringLiteral("до"), timeRow));
        timeL->addWidget(toEdit);
        timeL->addStretch();
        timeL->addWidget(minsLbl);

        form->addRow(QStringLiteral("Дата:"), dateEdit);
        form->addRow(QStringLiteral("Тип:"), typeEdit);
        form->addRow(QStringLiteral("Название:"), titleEdit);
        form->addRow(QStringLiteral("Диапазон времени:"), timeRow);
        root->addLayout(form);

        QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        bb->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Сохранить"));
        bb->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));
        root->addWidget(bb);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        connect(bb, &QDialogButtonBox::accepted, &dlg, [&dlg]() { dlg.accept(); });

        if (dlg.exec() != QDialog::Accepted)
            return;

        const QString type = typeEdit->text().trimmed();
        const QString title = titleEdit->text().trimmed();
        if (type.isEmpty() || title.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("Заполните «Тип» и «Название»."));
            return;
        }
        QString err;
        const int mins = computeMinutes();
        const QString who = AppSession::currentUsername();
        if (!addAgvErrorLog(currentAgvId, dateEdit->date(), type, title, fromEdit->time(), toEdit->time(), mins, who, &err)) {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), err.isEmpty() ? QStringLiteral("Не удалось записать ошибку") : err);
            return;
        }
        QMessageBox::information(this, QStringLiteral("Готово"), QStringLiteral("Ошибка записана."));
    });
    top->addWidget(addErrorBtn, 0, Qt::AlignRight);

    rootLayout->addLayout(top);

    QFrame *infoFrame = new QFrame(this);
    infoFrame->setStyleSheet("background:white;");
    QVBoxLayout *infoLayout = new QVBoxLayout(infoFrame);
    infoLayout->setContentsMargins(s(15), s(15), s(15), s(15));
    infoLayout->setSpacing(s(6));

    auto titleStyle = [&](QLabel *l){
        l->setStyleSheet(QString(
            "font-family:Inter; font-size:%1px; font-weight:700; color:#555;"
        ).arg(s(14)));
    };
    auto valueStyleReadonly = [&](QLineEdit *e){
        e->setReadOnly(true);
        e->setStyleSheet(QString(
            "QLineEdit{font-family:Inter;font-size:%1px;font-weight:800;color:#111;"
            "border:none;background:transparent;padding:0;}"
            "QLineEdit:focus{border:none;}"
        ).arg(s(16)));
    };
    auto statusStyleReadonly = [&](QComboBox *c){
        c->setEnabled(false);
        c->setStyleSheet(QString(
            "QComboBox{font-family:Inter;font-size:%1px;font-weight:800;color:#111;"
            "border:none;background:transparent;padding:0;}"
            "QComboBox::drop-down{border:none;width:0px;}"
            "QComboBox::down-arrow{image:none;}"
        ).arg(s(16)));
    };

    idLabel = new QLabel("ID:", this); titleStyle(idLabel);
    idEdit = new QLineEdit(this); valueStyleReadonly(idEdit);

    modelLabel = new QLabel("Модель:", this); titleStyle(modelLabel);
    modelEdit = new QLineEdit(this); valueStyleReadonly(modelEdit);

    serialLabel = new QLabel("S/N:", this); titleStyle(serialLabel);
    serialEdit = new QLineEdit(this); valueStyleReadonly(serialEdit);

    statusLabel = new QLabel("Статус:", this); titleStyle(statusLabel);
    statusCombo = new QComboBox(this);
    statusCombo->addItem("online");
    statusCombo->addItem("offline");
    statusCombo->addItem("working");
    statusStyleReadonly(statusCombo);

    kmLabel = new QLabel("Пробег (км):", this); titleStyle(kmLabel);
    kmEdit = new QLineEdit(this); valueStyleReadonly(kmEdit);
    kmEdit->setValidator(new QIntValidator(0, 100000000, kmEdit));

    currentTaskLabel = new QLabel("Текущая задача:", this); titleStyle(currentTaskLabel);
    currentTaskEdit = new QLineEdit(this); valueStyleReadonly(currentTaskEdit);
    currentTaskEdit->setMaxLength(40);

    auto addInfoRow = [&](QLabel *label, QWidget *field){
        QHBoxLayout *row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(s(6));
        label->setMinimumWidth(s(135));
        row->addWidget(label, 0, Qt::AlignVCenter);
        row->addWidget(field, 1, Qt::AlignVCenter);
        infoLayout->addLayout(row);
    };

    addInfoRow(modelLabel, modelEdit);
    addInfoRow(idLabel, idEdit);
    addInfoRow(serialLabel, serialEdit);
    addInfoRow(statusLabel, statusCombo);
    addInfoRow(kmLabel, kmEdit);
    addInfoRow(currentTaskLabel, currentTaskEdit);

    assignedUserLabel = new QLabel("—", infoFrame);
    assignedUserLabel->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:800;color:#111;background:transparent;"
    ).arg(s(16)));
    assignedUserCombo = new QComboBox(infoFrame);
    assignedUserCombo->setStyleSheet(QString(
        "QComboBox{font-family:Inter;font-size:%1px;font-weight:700;border:1px solid #C8C8C8;"
        "border-radius:%2px;background:white;padding:4px 8px;} QComboBox:hover{border:1px solid #0F00DB;}"
    ).arg(s(15)).arg(s(6)));
    assignedUserCombo->addItem("— Нет —", "");
    assignedUserCombo->hide();
    {
        QHBoxLayout *ar = new QHBoxLayout();
        ar->setContentsMargins(0, 0, 0, 0);
        ar->setSpacing(s(6));
        QLabel *al = new QLabel("Закреплён за:", infoFrame);
        titleStyle(al);
        al->setMinimumWidth(s(135));
        ar->addWidget(al, 0, Qt::AlignVCenter);
        ar->addWidget(assignedUserLabel, 1, Qt::AlignVCenter);
        ar->addWidget(assignedUserCombo, 1, Qt::AlignVCenter);
        infoLayout->addLayout(ar);
    }
    infoLayout->addSpacing(s(2));

    QHBoxLayout *lastActiveRow = new QHBoxLayout();
    lastActiveRow->setContentsMargins(0, 0, 0, 0);
    lastActiveRow->setSpacing(s(8));
    lastActiveRow->addStretch();

    blueprintLabel = new QLabel(this);
    blueprintLabel->setAlignment(Qt::AlignCenter);

    editAgvBtn = new QPushButton("Редактировать данные", infoFrame);
    saveAgvBtn = new QPushButton("Сохранить", infoFrame);
    cancelAgvBtn = new QPushButton("Отмена", infoFrame);

    editAgvBtn->setStyleSheet(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;font-size:15px;font-weight:800;border-radius:8px;padding:6px 14px;}"
        "QPushButton:hover{background:#1A4ACD;}"
    );
    saveAgvBtn->setStyleSheet(
        "QPushButton{background:#28A745;color:white;font-family:Inter;font-size:15px;font-weight:800;border-radius:8px;padding:6px 14px;}"
        "QPushButton:hover{background:#2EC24F;}"
    );
    cancelAgvBtn->setStyleSheet(
        "QPushButton{background:#F7F7F7;border:1px solid #C8C8C8;color:#222;font-family:Inter;font-size:15px;font-weight:700;border-radius:8px;padding:6px 14px;}"
        "QPushButton:hover{background:#EDEDED;}"
    );
    saveAgvBtn->hide();
    cancelAgvBtn->hide();

    lastActiveRow->addWidget(editAgvBtn, 0, Qt::AlignVCenter);
    lastActiveRow->addWidget(cancelAgvBtn, 0, Qt::AlignVCenter);
    lastActiveRow->addWidget(saveAgvBtn, 0, Qt::AlignVCenter);
    infoLayout->addLayout(lastActiveRow);

    infoLayout->addWidget(blueprintLabel);

    connect(editAgvBtn, &QPushButton::clicked, this, &AgvSettingsPage::enterAgvEditMode);
    connect(cancelAgvBtn, &QPushButton::clicked, this, &AgvSettingsPage::leaveAgvEditMode);
    connect(saveAgvBtn, &QPushButton::clicked, this, [this](){
        if (saveAgvInfo())
            leaveAgvEditMode();
    });

    connect(idEdit, &QLineEdit::textChanged, this, [this](const QString &){ refreshAgvEditButtons(); });
    connect(serialEdit, &QLineEdit::textChanged, this, [this](const QString &){ refreshAgvEditButtons(); });
    connect(statusCombo, &QComboBox::currentTextChanged, this, [this](const QString &){ refreshAgvEditButtons(); });
    connect(kmEdit, &QLineEdit::textChanged, this, [this](const QString &){ refreshAgvEditButtons(); });
    connect(currentTaskEdit, &QLineEdit::textChanged, this, [this](const QString &){ refreshAgvEditButtons(); });
    connect(assignedUserCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){ refreshAgvEditButtons(); });

    rootLayout->addWidget(infoFrame);

    tableWrapper = new QWidget(this);
    QVBoxLayout *tableWrapLayout = new QVBoxLayout(tableWrapper);
    tableWrapLayout->setContentsMargins(0,0,0,0);
    tableWrapLayout->setSpacing(0);

    buildTasksHeader();
    tableWrapLayout->addWidget(headerRow);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("border:none;background:transparent;");

    QWidget *tasksContainer = new QWidget(scroll);
    tasksLayout = new QVBoxLayout(tasksContainer);
    tasksLayout->setContentsMargins(0,0,0,0);
    tasksLayout->setSpacing(s(6));

    scroll->setWidget(tasksContainer);
    tableWrapLayout->addWidget(scroll, 1);

    rootLayout->addWidget(tableWrapper, 1);

    QHBoxLayout *bottomBar = new QHBoxLayout();
    bottomBar->setContentsMargins(0, 0, 0, 0);
    bottomBar->setSpacing(s(10));

    addTaskBtn = new QPushButton("+ Добавить задачу", this);
    addTaskBtn->setFixedSize(s(260), s(45));
    addTaskBtn->setStyleSheet(QString(
        "QPushButton { background-color:#0F00DB; border-radius:%1px; font-family:Inter; font-size:%2px; font-weight:800; color:white; }"
        "QPushButton:hover { background-color:#1A4ACD; }"
    ).arg(s(10)).arg(s(16)));

    connect(addTaskBtn, &QPushButton::clicked, this, [this](){
        if (!addFormOpened)
            openAddTaskForm();
    });

    QPushButton *writeTaskBtn = new QPushButton("Перейти в диалог", this);
    writeTaskBtn->setFixedSize(s(220), s(45));
    writeTaskBtn->setStyleSheet(QString(
        "QPushButton { background-color:#0369A1; border-radius:%1px; font-family:Inter; font-size:%2px; font-weight:800; color:white; }"
        "QPushButton:hover { background-color:#0284C7; }"
    ).arg(s(10)).arg(s(14)));
    connect(writeTaskBtn, &QPushButton::clicked, this, [this](){
        emit openDelegatorChatRequested(currentAgvId);
    });

    deleteSelectedBtn = new QPushButton("Удалить выбранные", this);
    deleteSelectedBtn->setFixedSize(s(220), s(40));
    deleteSelectedBtn->setStyleSheet(QString(
        "QPushButton { background:#FF3B30; border-radius:%1px; font-family:Inter; font-size:%2px; font-weight:800; color:white; }"
        "QPushButton:hover { background:#E13228; }"
    ).arg(s(8)).arg(s(14)));
    deleteSelectedBtn->hide();

    connect(deleteSelectedBtn, &QPushButton::clicked, this, &AgvSettingsPage::deleteSelectedTasks);

    undoDeleteBtn = new QPushButton("Вернуть", this);
    undoDeleteBtn->setFixedSize(s(160), s(40));
    undoDeleteBtn->setStyleSheet(QString(
        "QPushButton { background:#F1C40F; border-radius:%1px; font-family:Inter; font-size:%2px; font-weight:800; color:black; }"
        "QPushButton:hover { background:#D4AC0D; }"
    ).arg(s(8)).arg(s(14)));
    undoDeleteBtn->hide();

    undoTimer = new QTimer(this);
    undoTimer->setSingleShot(true);

    connect(undoDeleteBtn, &QPushButton::clicked, this, [this](){
        restoreDeletedTasks();
    });

    connect(undoTimer, &QTimer::timeout, this, [this](){
        cancelUndo();
    });

    editModeBtn = new QPushButton("Редактировать", this);
    editModeBtn->setFixedSize(s(180), s(40));
    editModeBtn->setStyleSheet(QString(
        "QPushButton { background:#E6E6E6; border-radius:%1px; font-family:Inter; font-size:%2px; font-weight:800; color:black; }"
        "QPushButton:hover { background:#D5D5D5; }"
    ).arg(s(8)).arg(s(14)));

    connect(editModeBtn, &QPushButton::clicked, this, &AgvSettingsPage::toggleEditMode);

    historyTasksBtn = new QPushButton("История задач", this);
    historyTasksBtn->setFixedSize(s(190), s(40));
    historyTasksBtn->setStyleSheet(QString(
        "QPushButton { background:#E6E6E6; border-radius:%1px; font-family:Inter; font-size:%2px; font-weight:800; color:black; }"
        "QPushButton:hover { background:#D5D5D5; }"
    ).arg(s(8)).arg(s(14)));
    connect(historyTasksBtn, &QPushButton::clicked, this, &AgvSettingsPage::openTaskHistoryDialog);

    bottomBar->addWidget(addTaskBtn, 0, Qt::AlignLeft);
    bottomBar->addWidget(writeTaskBtn, 0, Qt::AlignLeft);
    bottomBar->addStretch();
    bottomBar->addWidget(historyTasksBtn, 0, Qt::AlignRight);
    bottomBar->addWidget(undoDeleteBtn, 0, Qt::AlignRight);
    bottomBar->addWidget(deleteSelectedBtn, 0, Qt::AlignRight);
    bottomBar->addWidget(editModeBtn, 0, Qt::AlignRight);

    rootLayout->addLayout(bottomBar);

    ensureTaskHistoryTable();

    connect(&DataBus::instance(), &DataBus::agvTasksChanged,
            this, [this](const QString &id){
        if (id == currentAgvId) {
            loadAgv(currentAgvId);
            emit tasksChanged();
        }
    });
}
