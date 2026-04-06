#include "agvsettingspage.h"
#include "db_agv_tasks.h"
#include "db_users.h"
#include "databus.h"
#include "app_session.h"
#include "notifications_logs.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QScrollArea>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDriver>
#include <QLineEdit>
#include <QCalendarWidget>
#include <QDialog>
#include <QDebug>
#include <QFrame>
#include <QResizeEvent>
#include <QIntValidator>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QCheckBox>
#include <QPixmap>
#include <QSqlDatabase>
#include <QMouseEvent>
#include <QTimer>
#include <QMessageBox>
#include <QDateTime>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QSizePolicy>
#include <QFontMetrics>

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
        calendar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        calendar->setStyleSheet(
            "QCalendarWidget QWidget#qt_calendar_navigationbar { color:black; }"
            "QCalendarWidget QToolButton { "
            "   height:40px; width:120px; font-family:Inter; font-size:20px; font-weight:700; "
            "   background:#F1F2F4; border-radius:8px; color:black; "
            "} "
            "QCalendarWidget QToolButton:hover { background:#E6E6E6; }"
            "QCalendarWidget QSpinBox { "
            "   font-family:Inter; font-size:18px; font-weight:600; "
            "   background:white; border:1px solid #C8C8C8; border-radius:6px; padding:4px; color:black; "
            "} "
            "QCalendarWidget QAbstractItemView:enabled { "
            "   font-family:Inter; font-size:18px; color:black; background:white; "
            "   selection-background-color:#0F00DB; selection-color:white; "
            "} "
        );

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
    QCalendarWidget *calendar;
};

AgvSettingsPage::AgvSettingsPage(std::function<int(int)> scale, QWidget *parent)
    : QWidget(parent), s(scale)
{
    setStyleSheet("QLabel { background: transparent; color:#000; }");

    rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(s(10), s(10), s(10), s(10));
    rootLayout->setSpacing(s(10));

    // ===== Верхняя панель =====
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
                    // Автоделегирование: все задачи AGV — assigned_to и delegated_by
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

                    // Уведомление при закреплении — в тексте ФИО; [peer:логин] для чата (не показывается в UI)
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

    rootLayout->addLayout(top);

    // ===== Информация об AGV =====
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
    auto valueStyleEdit = [&](QLineEdit *e){
        e->setReadOnly(false);
        e->setStyleSheet(QString(
            "QLineEdit{font-family:Inter;font-size:%1px;font-weight:700;color:#111;"
            "border:1px solid #C8C8C8;border-radius:%2px;background:white;padding:4px 8px;}"
            "QLineEdit:focus{border:1px solid #0F00DB;}"
        ).arg(s(15)).arg(s(6)));
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
    auto statusStyleEdit = [&](QComboBox *c){
        c->setEnabled(true);
        c->setStyleSheet(QString(
            "QComboBox{font-family:Inter;font-size:%1px;font-weight:700;color:#111;"
            "border:1px solid #C8C8C8;border-radius:%2px;background:white;padding:4px 8px;}"
            "QComboBox:hover{border:1px solid #0F00DB;}"
        ).arg(s(15)).arg(s(6)));
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

    // ===== Таблица задач (обёртка) =====
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

    // ===== Нижняя панель =====
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

    // === АВТО-ОБНОВЛЕНИЕ ЗАДАЧ AGV ===
    connect(&DataBus::instance(), &DataBus::agvTasksChanged,
            this, [this](const QString &id){
        if (id == currentAgvId) {
            loadAgv(currentAgvId);
            emit tasksChanged();
        }

            });

    }


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
    // Важно: не подрезаем к "сегодня", иначе теряется реальная просрочка
    // и цвет статуса перестает зависеть от введенной даты последнего ТО.
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
        } else
            blueprintLabel->setPixmap(QPixmap());
    }

    const bool isOffline = (originalStatus.trimmed().toLower() == "offline");

    // Закреплён за
    QString curRole = getUserRole(AppSession::currentUsername());
    QString currentUser = AppSession::currentUsername();
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

    // Для отображения берем фактическую дату из БД, чтобы просроченные задачи
    // не "подтягивались" к сегодняшнему дню и не теряли красный статус.
    QDate next = task.nextDate.isValid() ? task.nextDate
                                         : computeNextDate(task.lastService, task.intervalDays);
    QLabel *nextLbl = new QLabel(next.toString("dd.MM.yyyy"), row);
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

    // Показываем assigned_to; если пусто — владелец AGV (assigned_user)
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
    // Может провести: админ/техник; или задача назначена ему (assigned_to или владелец AGV)
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

    // Автоделегирование: если изменился assigned_user, обновить все задачи AGV
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
            db.commit(); // явный commit при работе без транзакции
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
        idx = 2; // после infoFrame

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

    // Делегированную задачу может провести только пользователь, которому она назначена
    // (assigned_to или владелец AGV при пустом assigned_to)
    QString currentUser = AppSession::currentUsername();
    QString effectiveAssignee = !task.assignedTo.isEmpty() ? task.assignedTo : originalAssignedUser;
    if (!effectiveAssignee.isEmpty() && currentUser != effectiveAssignee) {
        QMessageBox::warning(this, "Задача",
                             "Выполнить эту задачу может только пользователь, которому она назначена.");
        return false;
    }

    const QDate performedDate = QDate::currentDate();
    // Следующую дату считаем от даты фактического проведения:
    // если работа проведена 25.03 (в середине срока), то next = 25.03 + interval,
    // а не от исходной плановой даты 11.04.
    const QDate nextDate = computeNextDate(performedDate, task.intervalDays);

    const bool useTransaction = db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions);
    if (useTransaction && !db.transaction()) {
        // На некоторых конфигурациях драйвера транзакция может не стартовать —
        // продолжаем без неё, чтобы проведение задачи всё равно работало.
        qDebug() << "completeTaskNow: transaction start failed, fallback to non-transaction mode:"
                 << db.lastError().text();
    }

    QSqlQuery upd(db);
    // Делегирование: если проводит владелец AGV (assigned_user) — не трогаем assigned_to/delegated_by.
    // Если проводит человек с ручным делегированием — возвращаем задачу владельцу AGV (единоразовое делегирование).
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

    // Уведомляем делегировавшего пользователя, что задача выполнена
    if (!task.delegatedBy.isEmpty() && task.delegatedBy != currentUser) {
        const QString whoDid = userDisplayName(currentUser);
        addNotificationForUser(
            task.delegatedBy,
            "Задача выполнена",
            QString("Задача \"%1\" для AGV %2 выполнена: %3 (%4) [peer:%5]")
                .arg(task.taskName, currentAgvId, whoDid, performedDate.toString("dd.MM.yyyy"), currentUser));
        emit DataBus::instance().notificationsChanged();
    }
    // Если AGV назначена пользователю — уведомляем того, кто назначил AGV
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

    // Delegation: assign to user (admins only)
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
            if (u.role != "viewer") continue; // делегировать только пользователям (viewer)
            QString display = u.fullName.isEmpty() ? u.username : QString("%1 (%2)").arg(u.fullName, u.username);
            assignCombo->addItem(display, u.username);
        }
        // Автоделегирование: если AGV закреплена за кем-то, новые задачи по умолчанию назначаются ему
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
        // Автоделегирование: если не выбрано вручную, но AGV закреплена — назначаем владельцу AGV
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

    // Assign combo for edit form
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
            if (u.role != "viewer") continue; // делегировать только пользователям (viewer)
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
    emit tasksChanged();   // ★ ДОБАВЛЕНО
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
    emit tasksChanged();   // ★ ДОБАВЛЕНО
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
    undoTimer->start(10000); // 10 секунд
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
    emit tasksChanged();   // ★ ДОБАВЛЕНО

}

void AgvSettingsPage::openTaskHistoryDialog()
{
    if (currentAgvId.trimmed().isEmpty())
        return;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        QMessageBox::warning(this, "История задач", "База данных не открыта.");
        return;
    }
    if (!ensureTaskHistoryTable()) {
        QMessageBox::warning(this, "История задач", "Таблица истории задач недоступна.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("История задач");
    dlg.setModal(true);
    dlg.resize(s(900), s(520));
    dlg.setStyleSheet(
        "QDialog{background:#F7F8FA;}"
        "QTableWidget{background:white;border:1px solid #D9DDE3;border-radius:8px;}"
        "QHeaderView::section{background:#ECEFF3;color:#1A1A1A;font-weight:800;padding:6px;border:none;}"
        "QPushButton{background:#0F00DB;color:white;border:none;border-radius:8px;padding:8px 14px;font-weight:800;}"
        "QPushButton:hover{background:#1A4ACD;}"
    );

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(s(12), s(12), s(12), s(12));
    root->setSpacing(s(10));

    QTableWidget *tbl = new QTableWidget(&dlg);
    tbl->setColumnCount(5);
    tbl->setHorizontalHeaderLabels(QStringList() << "Дата проведения"
                                                 << "Задача"
                                                 << "Интервал"
                                                 << "Следующая дата"
                                                 << "Кем проведено");
    tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tbl->verticalHeader()->setVisible(false);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    tbl->setSelectionMode(QAbstractItemView::SingleSelection);

    QSqlQuery q(db);
    q.prepare("SELECT completed_at, task_name, interval_days, next_date_after, performed_by "
              "FROM agv_task_history WHERE agv_id = :id ORDER BY completed_ts DESC");
    q.bindValue(":id", currentAgvId);
    if (!q.exec()) {
        QMessageBox::warning(this, "История задач", "Ошибка чтения истории: " + q.lastError().text());
        return;
    }

    int row = 0;
    while (q.next()) {
        tbl->insertRow(row);
        tbl->setItem(row, 0, new QTableWidgetItem(q.value(0).toDate().toString("dd.MM.yyyy")));
        tbl->setItem(row, 1, new QTableWidgetItem(q.value(1).toString()));
        tbl->setItem(row, 2, new QTableWidgetItem(QString::number(q.value(2).toInt()) + " дней"));
        tbl->setItem(row, 3, new QTableWidgetItem(q.value(3).toDate().toString("dd.MM.yyyy")));
        tbl->setItem(row, 4, new QTableWidgetItem(q.value(4).toString()));
        ++row;
    }

    if (row == 0) {
        tbl->insertRow(0);
        QTableWidgetItem *empty = new QTableWidgetItem("История пока пуста");
        empty->setFlags(empty->flags() & ~Qt::ItemIsSelectable);
        tbl->setItem(0, 0, empty);
        tbl->setSpan(0, 0, 1, 5);
    }

    QPushButton *closeBtn = new QPushButton("Закрыть", &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    root->addWidget(tbl, 1);
    root->addWidget(closeBtn, 0, Qt::AlignRight);
    dlg.exec();
}

void AgvSettingsPage::highlightTask(const QString &taskName)
{
    if (!tasksLayout)
        return;

    // Находим scroll area
    QScrollArea *scroll = tableWrapper->findChild<QScrollArea *>();
    if (!scroll)
        return;

    for (int i = 0; i < tasksLayout->count(); i++) {
        QWidget *row = tasksLayout->itemAt(i)->widget();
        if (!row) continue;

        QLabel *name = row->findChild<QLabel *>();
        if (!name) continue;

        if (name->text() == taskName) {

            // Подсветка строки
            row->setStyleSheet("background:#FFF3CD;border-radius:10px;");

            // Скроллим к ней
            scroll->ensureWidgetVisible(row);

            return;
        }
    }
}

