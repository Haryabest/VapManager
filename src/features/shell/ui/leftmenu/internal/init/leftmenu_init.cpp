#include "leftmenu.h"
#include "app_version.h"
#include "multisectionwidget.h"
#include "listagvinfo.h"
#include "agvsettingspage.h"
#include "addagvdialog.h"
#include "logindialog.h"
#include "db_agv_tasks.h"
#include "db_users.h"
#include "db_task_chat.h"
#include "databus.h"
#include "app_session.h"
#include "accountinfodialog.h"
#include "notifications_logs.h"
#include "taskchatdialog.h"
#include "diag_logger.h"
#include "leftmenu/internal/calendar/leftmenu_calendar_utils.h"
#include "leftmenu/internal/settings/leftmenu_settings_dialogs.h"
#include "leftmenu/internal/stress/leftmenu_stress_utils.h"
#include "db.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateEdit>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

void touchUserPresence(const QString &username);

void leftMenu::initUI()
{
    const QString appVersionText = AppVersion::label();

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(s(5));
    rootLayout->setContentsMargins(s(10), s(10), s(10), s(10));

    // Легкий heartbeat присутствия: обновляем last_login текущего пользователя.
    if (!findChild<QTimer*>("presenceHeartbeatTimer")) {
        QTimer *presenceTimer = new QTimer(this);
        presenceTimer->setObjectName("presenceHeartbeatTimer");
        presenceTimer->setInterval(30000);
        connect(presenceTimer, &QTimer::timeout, this, [this, presenceTimer]() {
            const QString username = AppSession::currentUsername();
            if (username.trimmed().isEmpty())
                return;

            if (qApp->property("manual_switch_account").toBool())
                return;

            if (!isCurrentSessionValid(username)) {
                presenceTimer->stop();
                logAction(username, "session_invalidated", "Сессия завершена: другой вход под этим аккаунтом");
                logoutUser();

                QWidget *mainWindow = window();
                if (mainWindow)
                    mainWindow->hide();

                QMessageBox::warning(
                    nullptr,
                    tr("Сессия завершена"),
                    tr("Выполнен вход под этим аккаунтом в другой копии программы или на другом компьютере.\n"
                       "Войдите снова, если работаете только на этом ПК.")
                );

                LoginDialog dlg(nullptr);
                if (dlg.exec() == QDialog::Accepted) {
                    const UserInfo newUser = dlg.user();
                    AppSession::setCurrentUsername(newUser.username);
                    enableRememberMe(newUser.username);
                    touchUserPresence(newUser.username);
                    if (mainWindow) {
                        mainWindow->show();
                        mainWindow->raise();
                        mainWindow->activateWindow();
                    }
                    presenceTimer->start();
                    return;
                }

                qApp->quit();
                return;
            }

            touchUserPresence(username);
        });
        presenceTimer->start();
    }
    touchUserPresence(AppSession::currentUsername());

    //
    // ======================= ВЕРХНЯЯ ШАПКА =======================
    //
    topRow_ = new QWidget(this);
    QWidget *topRow = topRow_;
    QHBoxLayout *topLayout = new QHBoxLayout(topRow);
    topLayout->setContentsMargins(0,0,0,0);
    topLayout->setSpacing(s(5));

    //
    // ЛЕВАЯ ЧАСТЬ ШАПКИ (ЛОГО)
    //
    QFrame *leftTopHeaderFrame = new QFrame(topRow);
    leftTopHeaderFrame->setStyleSheet("background-color:#F2F3F5;");
    leftTopHeaderFrame->setFixedSize(s(370), s(115));

    QVBoxLayout *leftTopHeaderLayout = new QVBoxLayout(leftTopHeaderFrame);
    leftTopHeaderLayout->setContentsMargins(s(3), s(0), 0, 0);
    leftTopHeaderLayout->setSpacing(0);

    QLabel *iconLabel = new QLabel(leftTopHeaderFrame);
    iconLabel->setPixmap(
        QPixmap(":/new/mainWindowIcons/noback/VAPManagerLogo.png")
            .scaled(s(288), s(92), Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
    iconLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    leftTopHeaderLayout->addStretch();
    leftTopHeaderLayout->addWidget(iconLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    leftTopHeaderLayout->addStretch();

    topLayout->addWidget(leftTopHeaderFrame);

    //
    // ПРАВАЯ ЧАСТЬ ШАПКИ
    //
    QFrame *rightTopHeaderFrame = new QFrame(topRow);
    rightTopHeaderFrame->setStyleSheet("background-color:#F1F2F4;");
    rightTopHeaderFrame->setFixedHeight(s(115));

    QHBoxLayout *rightTopHeaderLayout = new QHBoxLayout(rightTopHeaderFrame);
    rightTopHeaderLayout->setContentsMargins(0,0,0,0);

    QWidget *headerContent = new QWidget(rightTopHeaderFrame);
    QHBoxLayout *headerRow = new QHBoxLayout(headerContent);
    headerRow->setContentsMargins(s(5), s(5), s(5), s(5));

    //
    // Левый текстовый блок
    //
    QWidget *leftTextWidget = new QWidget(headerContent);
    QVBoxLayout *leftTextLayout = new QVBoxLayout(leftTextWidget);
    leftTextLayout->setContentsMargins(0,0,0,0);
    leftTextLayout->setSpacing(0);

    QLabel *titleLabel = new QLabel("Календарь технического обслуживания", leftTextWidget);
    titleLabel->setStyleSheet(QString(
        "font-family:Inter;font-weight:900;font-size:%1px;color:black;padding-left:%2px;"
    ).arg(s(22)).arg(s(9)));

    QLabel *subtitleLabel = new QLabel(
        "Отслеживание графиков обслуживания AGV\nи истории технического обслуживания",
        leftTextWidget
    );
    subtitleLabel->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:bold;color:#B8B8B8;padding-left:%2px;"
    ).arg(s(18)).arg(s(7)));

    leftTextLayout->addStretch();
    leftTextLayout->addWidget(titleLabel);
    leftTextLayout->addWidget(subtitleLabel);
    leftTextLayout->addStretch();

    headerRow->addWidget(leftTextWidget, 1);

    //
    // Правая колонка (поиск, уведомления, юзер)
    //
    QWidget *controls = new QWidget(headerContent);
    QHBoxLayout *controlsRow = new QHBoxLayout(controls);
    controlsRow->setContentsMargins(0,0,0,0);
    controlsRow->setSpacing(s(5));

    //
    // Поиск
    //
    QFrame *searchFrame = new QFrame(controls);
    searchFrame->setFixedSize(s(346), s(64));
    searchFrame->setStyleSheet("background-color:#DFDFDF;border-radius:8px;");

    QHBoxLayout *searchLayout = new QHBoxLayout(searchFrame);
    searchLayout->setContentsMargins(s(12), 0, s(12), 0);

    QLabel *searchIcon = new QLabel(searchFrame);
    searchIcon->setPixmap(
        QPixmap(":/new/mainWindowIcons/noback/lupa.png")
            .scaled(s(31), s(30), Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
    searchIcon->setAlignment(Qt::AlignCenter);

    searchEdit_ = new QLineEdit(searchFrame);
    searchEdit_->setPlaceholderText("Поиск AGV...");
    searchEdit_->setStyleSheet(QString(
        "QLineEdit{background:transparent;border:none;font-family:Inter;font-size:%1px;color:#747474;}"
    ).arg(s(16)));

    connect(searchEdit_, &QLineEdit::textChanged, this, &leftMenu::onSearchTextChanged);

    searchLayout->addWidget(searchIcon);
    searchLayout->addWidget(searchEdit_);

    QToolButton *chatsTopBtn = new QToolButton(controls);
    chatsTopBtn->setFixedSize(s(50), s(50));
    chatsTopBtn->setIcon(QIcon(":/new/mainWindowIcons/noback/logs.png"));
    chatsTopBtn->setIconSize(QSize(s(24), s(24)));
    chatsTopBtn->setStyleSheet(QString(
        "QToolButton{background:#E6E6E6;border-radius:%1px;border:none;}"
        "QToolButton:hover{background:#D5D5D5;}"
    ).arg(s(10)));
    chatsTopBtn->setToolTip("Чаты");
    connect(chatsTopBtn, &QToolButton::clicked, this, [this](){
        showChatsPage();
    });

    //
    // Кнопка уведомлений
    //
    QPushButton *notifBtn = new QPushButton(controls);
    notifBtn->setFixedSize(s(58), s(65));
    notifBtn->setAttribute(Qt::WA_TranslucentBackground);
    notifBtn->setStyleSheet(
        "QPushButton{background-color:transparent;border-radius:8px;border:none;}"
        "QPushButton:hover{background-color:rgba(0,0,0,0.05);} "
        "QPushButton:pressed{border:3px solid #FFD700;}"
    );

    QLabel *bell = new QLabel(notifBtn);
    bell->setAttribute(Qt::WA_TranslucentBackground);
    bell->setStyleSheet("background: transparent;");
    bell->setPixmap(
        QPixmap(":/new/mainWindowIcons/noback/bell.png")
            .scaled(s(37), s(37), Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
    bell->setAlignment(Qt::AlignCenter);

    notifBadge_ = new QLabel(notifBtn);
    notifBadge_->setFixedSize(s(20), s(20));
    notifBadge_->setAlignment(Qt::AlignCenter);
    notifBadge_->setStyleSheet(QString(
        "background:#FF3B30;color:white;font-family:Inter;font-weight:900;"
        "font-size:%1px;border-radius:%2px;"
    ).arg(s(10)).arg(s(10)));
    notifBadge_->hide();

    QHBoxLayout *notifLayout = new QHBoxLayout(notifBtn);
    notifLayout->setContentsMargins(0,0,0,0);
    notifLayout->setSpacing(0);
    notifLayout->addStretch();
    notifLayout->addWidget(bell);
    notifLayout->addStretch();
    notifBadge_->move(s(2), s(2)); // top-left over bell icon
    notifBadge_->raise();

    connect(notifBtn, &QPushButton::clicked, this, [this](){
        showNotificationsPanel();
    });

    //
    // Юзер
    //
    QFrame *userFrame = new QFrame(controls);
    userFrame->setFixedSize(s(65), s(65));
    QHBoxLayout *userLayout = new QHBoxLayout(userFrame);
    userLayout->setContentsMargins(s(5), s(5), s(5), s(5));

    userButton = new QToolButton(userFrame);
    userButton->setFixedSize(s(55), s(55));

    const QString currentUsername = AppSession::currentUsername();
    QPixmap avatarPm = loadUserAvatarFromDb(currentUsername);
    if (!avatarPm.isNull()) {
        QPixmap round = makeRoundPixmap(avatarPm, s(55));
        userButton->setIcon(QIcon(round));
        userButton->setIconSize(QSize(s(55), s(55)));
    } else {
        QString initials = currentUsername.trimmed().left(2).toUpper();
        if (initials.isEmpty())
            initials = "US";
        userButton->setText(initials);
    }
    userButton->setToolTip(currentUsername.isEmpty() ? tr("Пользователь") : currentUsername);

    userButton->setStyleSheet(QString(
        "QToolButton{background-color:#D9D9D9;border-radius:%1px;font-family:Inter;"
        "font-weight:900;font-size:%2px;color:black;border:none;padding:%3px;} "
        "QToolButton:hover{background-color:rgba(173,216,230,76);border:1px solid #ADD8E6;}"
    ).arg(s(27)).arg(s(14)).arg(s(4)));

    userLayout->addWidget(userButton);

    QMenu *userMenu = new QMenu(userButton);
    userMenu->setStyleSheet(QString(
        "QMenu { background-color: white; font-family: Inter; font-size:%1px; }"
        "QMenu::item { padding: %2px %3px; }"
        "QMenu::item:selected { background-color: #E6E6E6; }"
    ).arg(s(14)).arg(s(6)).arg(s(12)));

    QAction *accountInfoAction = userMenu->addAction(
        currentUsername.isEmpty() ? tr("Аккаунт: неизвестно")
                                  : tr("Аккаунт: %1").arg(currentUsername));
    connect(accountInfoAction, &QAction::triggered, this, [this]() {

        QString username = AppSession::currentUsername();
        if (username.isEmpty())
            return;

        QString role = getUserRole(username);
        QString key;
        if (role == "admin") {
            refreshAdminInviteKeyIfNeeded(username);
            key = getAdminInviteKey(username);
        } else if (role == "tech") {
            refreshTechInviteKeyIfNeeded(username);
            key = getTechInviteKey(username);
        }

        // Аватар из базы
        QPixmap avatar = loadUserAvatarFromDb(username);

        AccountInfoDialog dlg(username, role, key, avatar, this);
        dlg.exec();

    });


    userMenu->addSeparator();
    QAction *editProfileAction = userMenu->addAction(tr("Редактировать профиль"));
    QAction *changeAvatarAction = userMenu->addAction(tr("Сменить аватар"));
    QAction *changeLanguageAction = userMenu->addAction(tr("Сменить язык"));
    userMenu->addSeparator();
    QAction *aboutAction = userMenu->addAction(tr("О программе"));
    userMenu->addSeparator();
    QAction *switchAccountAction = userMenu->addAction(tr("Сменить аккаунт"));
    QAction *exitAppAction = userMenu->addAction(tr("Выйти из приложения"));

    auto logMenuClick = [] (const QString &action, const QString &details) {
        logAction(AppSession::currentUsername(), action, details);
    };


    auto openAboutDialog = [this, appVersionText, logMenuClick]() {
        QDialog dlg(this);
        dlg.setWindowTitle("О программе");
        dlg.setModal(true);
        dlg.setFixedSize(s(700), s(430));
        dlg.setStyleSheet(
            "QDialog{background:#F6F8FC;border:1px solid #DCE2EE;border-radius:14px;}"
            "QFrame#aboutCard{background:white;border:1px solid #E5EAF3;border-radius:12px;}"
            "QLabel{font-family:Inter;color:#1A1A1A;}"
            "QLabel#title{font-size:24px;font-weight:900;color:#0F172A;}"
            "QLabel#subtitle{font-size:13px;font-weight:600;color:#64748B;}"
            "QLabel#rowTitle{font-size:13px;font-weight:800;color:#334155;}"
            "QLabel#rowValue{font-size:14px;font-weight:600;color:#0F172A;}"
            "QFrame#line{background:#E7ECF5;border:none;min-height:1px;max-height:1px;}"
            "QPushButton{font-family:Inter;font-size:14px;font-weight:800;border-radius:9px;padding:9px 16px;}"
            "QPushButton#okBtn{background:#0F00DB;color:white;border:none;}"
            "QPushButton#okBtn:hover{background:#1A4ACD;}"
        );

        QVBoxLayout *root = new QVBoxLayout(&dlg);
        root->setContentsMargins(s(16), s(16), s(16), s(16));
        root->setSpacing(s(10));

        QFrame *card = new QFrame(&dlg);
        card->setObjectName("aboutCard");
        QVBoxLayout *v = new QVBoxLayout(card);
        v->setContentsMargins(s(18), s(16), s(18), s(14));
        v->setSpacing(s(10));

        QHBoxLayout *header = new QHBoxLayout();
        header->setContentsMargins(0,0,0,0);
        header->setSpacing(s(12));
        QLabel *logo = new QLabel(card);
        logo->setFixedSize(s(170), s(54));
        logo->setPixmap(QPixmap(":/new/mainWindowIcons/noback/VAPManagerLogo.png")
                        .scaled(logo->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logo->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        QLabel *agvIcon = new QLabel(card);
        agvIcon->setFixedSize(s(42), s(42));
        agvIcon->setPixmap(QPixmap(":/new/mainWindowIcons/noback/agvIcon.png")
                           .scaled(agvIcon->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        agvIcon->setAlignment(Qt::AlignCenter);

        header->addWidget(logo);
        header->addStretch();
        header->addWidget(agvIcon);
        v->addLayout(header);

        QLabel *title = new QLabel("О ПО AGV Manager", card);
        title->setObjectName("title");
        v->addWidget(title);

        QLabel *subtitle = new QLabel("Информация о владельце и разработчике", card);
        subtitle->setObjectName("subtitle");
        v->addWidget(subtitle);

        QFrame *line = new QFrame(card);
        line->setObjectName("line");
        v->addWidget(line);

        auto addInfoRow = [this, v, card](const QString &k, const QString &val) {
            QHBoxLayout *row = new QHBoxLayout();
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(s(12));
            QLabel *kLbl = new QLabel(k, card);
            kLbl->setObjectName("rowTitle");
            kLbl->setMinimumWidth(s(180));
            QLabel *vLbl = new QLabel(val, card);
            vLbl->setObjectName("rowValue");
            vLbl->setWordWrap(true);
            row->addWidget(kLbl);
            row->addWidget(vLbl, 1);
            v->addLayout(row);
        };

        addInfoRow("Организация:", "Горьковский автомобильный завод");
        addInfoRow("Ответственный:", "Ведущий спец. Булькин Дмитрий Олегович");
        addInfoRow("Продукт:", "AGV Manager (панель администрирования)");
        addInfoRow("Версия:", appVersionText);
        addInfoRow("Год:", "2026");

        v->addStretch();

        QPushButton *ok = new QPushButton("Закрыть", card);
        ok->setObjectName("okBtn");
        v->addWidget(ok, 0, Qt::AlignRight);
        connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);

        logMenuClick("user_menu_about", "Открыто окно О программе");
        root->addWidget(card);
        dlg.exec();
    };

    connect(accountInfoAction, &QAction::triggered, this, [logMenuClick](){
        logMenuClick("user_menu_account", "Открыт пункт Аккаунт");
    });
    connect(editProfileAction, &QAction::triggered, this, [this, logMenuClick](){
        logMenuClick("user_menu_edit_profile", "Нажат пункт Редактировать профиль");
        showProfile();
    });
    connect(changeAvatarAction, &QAction::triggered, this, [this, logMenuClick](){
        logMenuClick("user_menu_change_avatar", "Нажат пункт Сменить аватар");
        changeAvatar();
    });

    connect(changeLanguageAction, &QAction::triggered, this, [this, logMenuClick](){
        logMenuClick("user_menu_change_language", "Нажат пункт Сменить язык");
        QDialog langDlg(this);
        langDlg.setWindowTitle(tr("Сменить язык"));
        langDlg.setModal(true);
        langDlg.setFixedSize(320, 120);
        QVBoxLayout *v = new QVBoxLayout(&langDlg);
        QComboBox *cb = new QComboBox(&langDlg);
        cb->addItem(tr("Русский"), "ru");
        cb->addItem("English", "en");
        cb->addItem("中文", "zh");
        QString cfgPath = QCoreApplication::applicationDirPath() + "/config.ini";
        QSettings cfg(cfgPath, QSettings::IniFormat);
        int idx = cb->findData(cfg.value("language", "ru").toString());
        if (idx >= 0) cb->setCurrentIndex(idx);
        v->addWidget(cb);
        QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &langDlg);
        connect(bb, &QDialogButtonBox::accepted, &langDlg, [&](){
            cfg.setValue("language", cb->currentData().toString());
            cfg.sync();
            const int ret = QMessageBox::question(
                &langDlg,
                tr("Язык"),
                tr("Язык сохранен. Перезапустить приложение сейчас?"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);
            if (ret == QMessageBox::Yes) {
                const QString appPath = QCoreApplication::applicationFilePath();
                const QStringList args = QCoreApplication::arguments().mid(1);
                QProcess::startDetached(appPath, args);
                qApp->quit();
                return;
            }
            langDlg.accept();
        });
        connect(bb, &QDialogButtonBox::rejected, &langDlg, &QDialog::reject);
        v->addWidget(bb);
        langDlg.exec();
    });
    connect(switchAccountAction, &QAction::triggered, this, [this](){
        logAction(AppSession::currentUsername(), "user_menu_switch_account", "Нажат пункт Сменить аккаунт");

        QTimer *presenceTimer = findChild<QTimer*>("presenceHeartbeatTimer");
        if (presenceTimer)
            presenceTimer->stop();

        qApp->setProperty("manual_switch_account", true);

        QWidget *mainWindow = window();
        logoutUser();

        if (mainWindow)
            mainWindow->hide();

        LoginDialog dlg(nullptr);
        if (dlg.exec() == QDialog::Accepted) {
            UserInfo newUser = dlg.user();
            AppSession::setCurrentUsername(newUser.username);
            enableRememberMe(newUser.username);
            touchUserPresence(newUser.username);
            logAction(AppSession::currentUsername(), "account_switched", QString("Вход под аккаунтом %1").arg(newUser.username));

            if (mainWindow) {
                mainWindow->show();
                mainWindow->raise();
                mainWindow->activateWindow();
            }
            if (presenceTimer)
                presenceTimer->start();
            qApp->setProperty("manual_switch_account", false);
            
            QTimer::singleShot(0, this, [this]() {
                qreal oldFactor = scaleFactor_;
                scaleFactor_ = 0;
                setScaleFactor(oldFactor);
            });
            return;
        }

        qApp->setProperty("manual_switch_account", false);
        qApp->quit();
    });
    connect(aboutAction, &QAction::triggered, this, [openAboutDialog](){
        openAboutDialog();
    });

    connect(exitAppAction, &QAction::triggered, this, [](){
        logAction(AppSession::currentUsername(), "user_menu_exit_app", "Нажат пункт Выйти из приложения");
        qApp->quit();
    });

    connect(userButton, &QToolButton::clicked, this, [this, userMenu](){
        QPoint pos = this->userButton->mapToGlobal(QPoint(0, this->userButton->height() + s(5)));
        userMenu->popup(pos);
    });



    controlsRow->addWidget(searchFrame);
    controlsRow->addWidget(chatsTopBtn);
    controlsRow->addWidget(notifBtn);
    controlsRow->addWidget(userFrame);

    headerRow->addWidget(controls, 0, Qt::AlignRight);
    rightTopHeaderLayout->addWidget(headerContent);

    topLayout->addWidget(rightTopHeaderFrame);

    rootLayout->addWidget(topRow);

    //
    // ======================= НИЖНЯЯ ПАНЕЛЬ =======================
    //
    bottomRow_ = new QWidget(this);
    QWidget *bottomRow = bottomRow_;
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomRow);
    bottomLayout->setContentsMargins(0, s(5), 0, 0);
    bottomLayout->setSpacing(s(5));

    //
    // ======================= ЛЕВАЯ ПАНЕЛЬ =======================
    //
    QWidget *leftPanel = new QWidget(bottomRow);
    leftPanel->setFixedWidth(s(370));

    QVBoxLayout *leftPanelLayout = new QVBoxLayout(leftPanel);
    leftPanelLayout->setContentsMargins(0,0,0,0);
    leftPanelLayout->setSpacing(s(8));

    //
    // МЕНЮ
    //
    QFrame *leftNavFrame = new QFrame(leftPanel);
    leftNavFrame->setStyleSheet("background-color:#F1F2F4;");
    QVBoxLayout *leftNavLayout = new QVBoxLayout(leftNavFrame);
    leftNavLayout->setContentsMargins(s(10), s(5), s(10), s(5));
    leftNavLayout->setSpacing(0);

    auto makeNavButton = [&](QString text, QString iconPath){
        QPushButton *btn = new QPushButton(text, leftNavFrame);
        QPixmap pix(iconPath);
        if (!pix.isNull())
            btn->setIcon(QIcon(pix.scaled(s(24), s(24), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        btn->setIconSize(QSize(s(24), s(24)));
        btn->setFixedHeight(s(45));
        btn->setStyleSheet(QString(
            "QPushButton{text-align:left;background-color:transparent;padding-left:%1px;font-family:Inter;"
            "font-size:%2px;font-weight:700;border-radius:8px;border:1px solid transparent;} "
            "QPushButton:hover{background-color:rgba(173,216,230,76);border:1px solid #ADD8E6;}"
        ).arg(s(15)).arg(s(14)));
        return btn;
    };

    QPushButton *btnUsers = makeNavButton("Пользователи", ":/new/mainWindowIcons/noback/user.png");
    QPushButton *btnAgv   = makeNavButton("AGV", ":/new/mainWindowIcons/noback/agvIcon.png");
    QPushButton *btnEdit  = makeNavButton("Модель AGV", ":/new/mainWindowIcons/noback/edit.png");
    QPushButton *btnYear  = makeNavButton("Годовой отчёт", ":/new/mainWindowIcons/noback/YearListPrint.png");
    QPushButton *btnSet   = makeNavButton("Настройки", ":/new/mainWindowIcons/noback/agvSetting.png");
    connect(btnUsers, &QPushButton::clicked, this, [this](){
        showUsersPage();
    });

    connect(btnYear, &QPushButton::clicked, this, [this](){
        showAnnualReportDialog();
    });

    connect(btnSet, &QPushButton::clicked, this, [this](){
        LeftMenuDialogs::showAppSettingsDialog(this);
    });

    leftNavLayout->addWidget(btnUsers);
    // === AGV + COUNTER ===
    {
        QWidget *agvRow = new QWidget(leftNavFrame);
        QHBoxLayout *agvRowLayout = new QHBoxLayout(agvRow);
        agvRowLayout->setContentsMargins(0,0,0,0);
        agvRowLayout->setSpacing(s(8));

        agvRowLayout->addWidget(btnAgv);

        agvCounter = new QLabel("0", agvRow);
        agvCounter->setFixedSize(s(26), s(26));
        agvCounter->setAlignment(Qt::AlignCenter);
        agvCounter->setStyleSheet(QString(
            "background:#00C8FF;"
            "color:#0F00DB;"
            "font-family:Inter;"
            "font-weight:900;"
            "font-size:%1px;"
            "border-radius:%2px;"
        ).arg(s(14)).arg(s(13)));

        agvRowLayout->addWidget(agvCounter);

        leftNavLayout->addWidget(agvRow);
    }
    leftNavLayout->addWidget(btnEdit);
    leftNavLayout->addWidget(btnYear);
    leftNavLayout->addWidget(btnSet);
    leftNavLayout->addSpacing(s(10));

    //
    // КНОПКА ДОБАВИТЬ AGV
    //
    QPushButton *addAgvButton = new QPushButton("+ Добавить AGV", leftNavFrame);
    {
        QString role = getUserRole(AppSession::currentUsername());
        if (role == "viewer")
            addAgvButton->hide();
    }
    connect(addAgvButton, &QPushButton::clicked, this, [this](){

        // 1. Открываем диалог добавления
        AddAgvDialog dlg([this](int v){ return s(v); }, this);
        if (dlg.exec() != QDialog::Accepted)
            return;

        // 2. Формируем структуру AgvInfo
        AgvInfo info;
        QString baseName = dlg.result.name.trimmed();

        // последние цифры из серийника
        QString digits;
        QRegularExpression re("\\d+");
        auto it = re.globalMatch(dlg.result.serial);
        while (it.hasNext())
            digits += it.next().captured();

        QString last4 = digits.right(4);
        if (last4.isEmpty()) last4 = "0000";

        QString modelLower = dlg.result.model.toLower();

        QString finalId = QString("%1_%2_%3")
                            .arg(baseName)
                            .arg(last4)
                            .arg(modelLower);

        info.id = finalId;
        info.model = dlg.result.model.toUpper();
        info.serial = dlg.result.serial;
        info.status = dlg.result.status;
        info.task = dlg.result.alias.trimmed();
        info.kilometers = 0;
        info.blueprintPath = ":/new/mainWindowIcons/noback/blueprint.png";
        info.lastActive = QDate::currentDate();

        // 3. Записываем в базу
        if (!insertAgvToDb(info)) {
            qDebug() << "insertAgvToDb: не удалось записать AGV";
            return;
        }
        // 3.1 Копируем задачи модели → agv_tasks
        if (!copyModelTasksToAgv(info.id, info.model)) {
            qDebug() << "copyModelTasksToAgv: ошибка копирования задач для" << info.id;
        }

        // 4. Обновляем список AGV, если он открыт
        agvListDirty_ = true;
        if (listAgvInfo && listAgvInfo->isVisible()) {
            QVector<AgvInfo> agvs = listAgvInfo->loadAgvList();

            listAgvInfo->rebuildList(agvs);
            agvListDirty_ = false;
        }

        // 5. Переключаемся на список AGV
        showAgvList();
    });

    addAgvButton->setFixedHeight(s(40));
    addAgvButton->setStyleSheet(QString(
        "QPushButton{background-color:#0F00DB;color:white;font-family:Inter;font-size:%1px;font-weight:800;"
        "border-radius:10px;border:none;} "
        "QPushButton:hover{background-color:#1A4ACD;}"
    ).arg(s(16)));

    QHBoxLayout *addAgvButtonRow = new QHBoxLayout();
    addAgvButtonRow->addSpacing(s(28));
    addAgvButtonRow->addWidget(addAgvButton);
    addAgvButtonRow->addSpacing(s(28));

    leftNavLayout->addLayout(addAgvButtonRow);
    leftNavLayout->addSpacing(s(5));

    leftPanelLayout->addWidget(leftNavFrame);

    //
    // ======================= СТАТУС СИСТЕМЫ =======================
    //
    QFrame *leftStatusFrame = new QFrame(leftPanel);
    leftStatusFrame->setStyleSheet("background-color:#F1F2F4;");
    leftStatusFrame->setMinimumHeight(s(208));

    QVBoxLayout *leftStatusLayout = new QVBoxLayout(leftStatusFrame);
    leftStatusLayout->setContentsMargins(s(10), s(20), s(10), s(5));
    leftStatusLayout->setSpacing(s(5));

    statusWidget_ = new MultiSectionWidget(leftStatusFrame, scaleFactor_);
    statusWidget_->setScaleFactor(scaleFactor_ * 1.3);
    statusWidget_->setActiveAGVCurrentCount(0);
    statusWidget_->setActiveAGVTotalCount(0);
    statusWidget_->setMaintenanceCurrentCount(0);
    statusWidget_->setMaintenanceTotalCount(0);
    statusWidget_->setErrorCurrentCount(0);
    statusWidget_->setErrorTotalCount(0);
    statusWidget_->setDisabledCurrentCount(0);
    statusWidget_->setDisabledTotalCount(0);

    leftStatusLayout->addWidget(statusWidget_);

    QPushButton *logsButton = new QPushButton("Logs", leftStatusFrame);
    logsButton->setFixedSize(s(120), s(25));
    logsButton->setIcon(QIcon(":/new/mainWindowIcons/noback/logs.png"));
    logsButton->setIconSize(QSize(s(14), s(14)));
    logsButton->setStyleSheet(QString(
        "QPushButton{background-color:transparent;border:none;font-family:Inter;font-size:%1px;font-weight:800;color:black;padding-left:%2px;} "
        "QPushButton:hover{background-color:rgba(173,216,230,76);border-radius:5px;}"
    ).arg(s(12)).arg(s(4)));

    QHBoxLayout *logsRow = new QHBoxLayout();
    logsRow->addStretch();
    logsRow->addWidget(logsButton);
    logsRow->addStretch();
    leftStatusLayout->addLayout(logsRow);
    connect(logsButton, &QPushButton::clicked, this, [this](){
        showLogs();
    });

    QLabel *versionLabel = new QLabel(QString("Версия: %1").arg(appVersionText), leftStatusFrame);
    versionLabel->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:rgb(120,120,120);").arg(s(10)));
    versionLabel->setAlignment(Qt::AlignCenter);
    leftStatusLayout->addWidget(versionLabel);

    leftPanelLayout->addWidget(leftStatusFrame);
    leftPanelLayout->setStretch(0,0);
    leftPanelLayout->setStretch(1,1);

    bottomLayout->addWidget(leftPanel);
    //
    // ======================= ПРАВАЯ ПАНЕЛЬ =======================
    //
    QWidget *rightPanel = new QWidget(bottomRow);
    QVBoxLayout *rightPanelLayout = new QVBoxLayout(rightPanel);
    rightPanelLayout->setContentsMargins(0, s(5), 0, 0);
    rightPanelLayout->setSpacing(s(8));

    QFrame *rightBodyFrame = new QFrame(rightPanel);
    QVBoxLayout *rightBodyLayout = new QVBoxLayout(rightBodyFrame);
    rightBodyLayout->setContentsMargins(0,0,0,0);
    rightBodyLayout->setSpacing(s(5));

    rightPanelLayout->addWidget(rightBodyFrame,1);
    bottomLayout->addWidget(rightPanel,1);

    rootLayout->addWidget(bottomRow,1);

    //
    // ======================= КАЛЕНДАРЬ =======================
    //
    rightCalendarFrame = new QFrame(rightBodyFrame);
    rightCalendarFrame->setStyleSheet("background-color:#F1F2F4;border-radius:12px;");
    rightCalendarFrame->setMinimumHeight(s(300));

    QVBoxLayout *rightCalendarLayout = new QVBoxLayout(rightCalendarFrame);
    rightCalendarLayout_ = rightCalendarLayout;
    rightCalendarLayout->setContentsMargins(s(8),s(8),s(8),s(8));
    rightCalendarLayout->setSpacing(s(8));

    //
    // ======== БЛОК ДЕЙСТВИЙ ДНЯ ========
    //
    calendarActionsFrame = new QFrame(rightCalendarFrame);
    calendarActionsFrame->setStyleSheet("background-color:#F1F2F4;border-radius:8px;");
    calendarActionsFrame->setVisible(false);

    QHBoxLayout *actionsLayout = new QHBoxLayout(calendarActionsFrame);
    actionsLayout->setContentsMargins(s(10), s(10), s(10), s(10));
    actionsLayout->setSpacing(s(10));

    calendarActionsLabel_ = new QLabel("Выберите день или неделю", calendarActionsFrame);
    calendarActionsLabel_->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:700;color:black;"
    ).arg(s(16)));

    QPushButton *btnAdd = new QPushButton("Добавить событие", calendarActionsFrame);
    btnAdd->setStyleSheet(QString(
            "QPushButton{background-color:#0F00DB;color:white;font-family:Inter;font-size:%1px;"
            "font-weight:700;border-radius:8px;padding:%2px %3px;} "
            "QPushButton:hover{background-color:#1A4ACD;}"
    ).arg(s(14)).arg(s(6)).arg(s(12)));

    actionsLayout->addWidget(calendarActionsLabel_);
    actionsLayout->addStretch();
    actionsLayout->addWidget(btnAdd);

    rightCalendarLayout->addWidget(calendarActionsFrame);

    //
    // ======== HEADER КАЛЕНДАРЯ ========
    //
    QWidget *calendarHeader = new QWidget(rightCalendarFrame);
    QVBoxLayout *calendarHeaderMainLayout = new QVBoxLayout(calendarHeader);
    calendarHeaderMainLayout->setContentsMargins(s(10),s(14),s(10),0);
    calendarHeaderMainLayout->setSpacing(s(16));

    QHBoxLayout *topRowLayoutCal = new QHBoxLayout();
    topRowLayoutCal->setContentsMargins(0,0,0,0);
    topRowLayoutCal->setSpacing(s(35));

    monthLabel = new QLabel(monthYearLabelText(selectedMonth_, selectedYear_), calendarHeader);
    monthLabel->setStyleSheet(QString(
        "font-family:Inter;font-weight:900;font-size:%1px;color:black;"
    ).arg(s(26)));
    monthLabel->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);

    topRowLayoutCal->addWidget(monthLabel);
    topRowLayoutCal->addStretch();

    prevMonthBtn_ = new QPushButton("<");
    prevMonthBtn_->setFixedSize(s(32),s(32));
    prevMonthBtn_->setStyleSheet(QString(
        "QPushButton{background-color:#DFDFDF;border-radius:6px;font-size:%1px;} "
        "QPushButton:hover{background-color:#CFCFCF;}"
    ).arg(s(18)));

    nextMonthBtn_ = new QPushButton(">");
    nextMonthBtn_->setFixedSize(s(32),s(32));
    nextMonthBtn_->setStyleSheet(prevMonthBtn_->styleSheet());
    updateCalendarNavButtons();

    connect(prevMonthBtn_,&QPushButton::clicked,this,[this](){ changeMonth(-1); });
    connect(nextMonthBtn_,&QPushButton::clicked,this,[this](){ changeMonth(1); });

    topRowLayoutCal->addWidget(prevMonthBtn_);
    topRowLayoutCal->addWidget(nextMonthBtn_);

    QPushButton *settingsBtn = new QPushButton("Настройки");
    settingsBtn->setFixedSize(s(120),s(38));
    settingsBtn->setStyleSheet(QString(
        "QPushButton{background-color:#DFDFDF;border-radius:8px;font-family:Inter;font-size:%1px;font-weight:700;} "
        "QPushButton:hover{background-color:#CFCFCF;}"
    ).arg(s(16)));

    connect(settingsBtn,&QPushButton::clicked,this,[this](){
        LeftMenuDialogs::CalendarDialogSelection sel;
        if (!LeftMenuDialogs::showCalendarSettingsDialog(this, sel))
            return;

        if (!calendarHighlightTimer) {
            calendarHighlightTimer = new QTimer(this);
            calendarHighlightTimer->setSingleShot(true);
            connect(calendarHighlightTimer, &QTimer::timeout, this, [this](){
                calendarHighlightActive_ = false;
                highlightWeek_ = false;
                selectedWeek_ = 0;
                if (rightCalendarFrame && rightCalendarFrame->isVisible()) {
                    refreshCalendarSelectionVisuals();
                } else {
                    pendingCalendarReload_ = true;
                }
            });
        }

        int y = sel.year, m = sel.month, w = sel.week, d = sel.day;
        // Явный приоритет: выбранный день отменяет неделю (и наоборот), даже если комбо не сбросили вручную.
        if (d != 0)
            w = 0;
        else if (w != 0)
            d = 0;

        calendarHighlightTimer->stop();
        selectedWeek_ = w;
        highlightWeek_ = (w != 0);
        calendarHighlightActive_ = true;

        if (w != 0) {
            int startDay = 1 + (w - 1) * 7;
            const int monthDays = LeftMenuCalendar::daysInMonth(y, m);
            startDay = qBound(1, startDay, monthDays);
            selectedDay_ = QDate(y, m, startDay);
            if (m == selectedMonth_ && y == selectedYear_ && calendarTablePtr) {
                refreshCalendarSelectionVisuals();
            } else {
                setSelectedMonthYear(m, y);
            }
        } else {
            highlightWeek_ = false;
            selectedWeek_ = 0;
            selectDay(y, m, d);
        }

        calendarHighlightTimer->start(20000);
    });

    topRowLayoutCal->addWidget(settingsBtn);
    calendarHeaderMainLayout->addLayout(topRowLayoutCal);

    //
    // ======== ЛЕГЕНДА ========
    //
    QHBoxLayout *legendLayout = new QHBoxLayout();
    legendLayout->setSpacing(s(15));
    legendLayout->setAlignment(Qt::AlignLeft);

    auto makeDot=[&](QString color){
        QLabel *dot=new QLabel();
        dot->setFixedSize(s(12),s(12));
        dot->setStyleSheet(QString("background-color:%1;border-radius:%2px;").arg(color).arg(s(6)));
        return dot;
    };
    auto makeLegendText=[&](QString text){
        QLabel *lbl=new QLabel(text);
        lbl->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:#A39E9E;").arg(s(17)));
        return lbl;
    };

    legendLayout->addWidget(makeDot("#FF0000"));
    legendLayout->addWidget(makeLegendText("Просрочен"));
    legendLayout->addSpacing(s(8));
    legendLayout->addWidget(makeDot("#FF8800"));
    legendLayout->addWidget(makeLegendText("Ближайшие события"));
    legendLayout->addSpacing(s(8));
    legendLayout->addWidget(makeDot("#18CF00"));
    legendLayout->addWidget(makeLegendText("Запланировано"));
    legendLayout->addSpacing(s(8));
    legendLayout->addWidget(makeDot("#00E5FF"));
    legendLayout->addWidget(makeLegendText("Обслужено"));

    calendarHeaderMainLayout->addLayout(legendLayout);

    //
    // ======== ЛИНИЯ ========
    //
    QFrame *calendarDivider = new QFrame(calendarHeader);
    calendarDivider->setFrameShape(QFrame::HLine);
    calendarDivider->setFixedHeight(s(2));
    calendarDivider->setStyleSheet("background-color:rgba(211,211,211,0.8); border:none;");
    calendarHeaderMainLayout->addWidget(calendarDivider);

    rightCalendarLayout->addWidget(calendarHeader);


    //
    // ======================= СЕТКА КАЛЕНДАРЯ (ФИНАЛ + ТЁМНАЯ ШАПКА + ТЕМНЕЕ СТАРЫЕ/НОВЫЕ ДНИ) =======================
    //

    buildCalendarTable();


    //
    // ======================= ПРЕДСТОЯЩЕЕ ТО (с иконками) =======================
    //
    rightUpcomingMaintenanceFrame = new QFrame(rightBodyFrame);
    rightUpcomingMaintenanceFrame->setStyleSheet("background-color:#F1F2F4;border-radius:12px;");
    rightUpcomingMaintenanceFrame->setMinimumHeight(s(130));

    QVBoxLayout *rightUpcomingLayout = new QVBoxLayout(rightUpcomingMaintenanceFrame);
    rightUpcomingLayout->setContentsMargins(s(8), s(8), s(8), s(8));
    rightUpcomingLayout->setSpacing(s(8));

    QLabel *maintenanceTitle = new QLabel("Предстоящее Техническое обслуживание", rightUpcomingMaintenanceFrame);
    maintenanceTitle->setStyleSheet(QString(
        "background:transparent;font-family:Inter;font-size:%1px;font-weight:800;color:black;padding:%2px %3px;"
    ).arg(s(18)).arg(s(10)).arg(s(15)));
    maintenanceTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    rightUpcomingLayout->addWidget(maintenanceTitle);

    QFrame *thickDivider = new QFrame(rightUpcomingMaintenanceFrame);
    thickDivider->setFrameShape(QFrame::HLine);
    thickDivider->setFixedHeight(s(2));
    thickDivider->setStyleSheet("background-color:rgba(211,211,211,0.8); border:none;");
    rightUpcomingLayout->addWidget(thickDivider);

    QScrollArea *upcomingScroll = new QScrollArea(rightUpcomingMaintenanceFrame);
    upcomingScroll->setWidgetResizable(true);
    upcomingScroll->setStyleSheet(
        "QScrollArea { border:none; background:transparent; }"
        "QScrollBar:vertical { width:6px; background:transparent; margin:2px; }"
        "QScrollBar::handle:vertical { background:#C0C0C0; border-radius:3px; }"
        "QScrollBar::handle:vertical:hover { background:#A0A0A0; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height:0; }"
    );

    QWidget *contentContainer = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(contentContainer);
    contentLayout->setContentsMargins(s(10), s(10), s(10), s(10));
    contentLayout->setSpacing(s(8));

    upcomingScroll->setWidget(contentContainer);

    contentLayout->setContentsMargins(s(10), s(10), s(10), s(10));
    contentLayout->setSpacing(s(5));

    auto addMaintenanceItem = [&](const MaintenanceItemData &item){
        QColor bgColor, btnColor;
        QString iconPath;

        if (item.severity == "red") {
            bgColor = QColor(255,0,0,33);
            btnColor = QColor(235,61,61,204);
            iconPath = ":/new/mainWindowIcons/noback/alert.png";
        }
        else if (item.severity == "orange") {
            bgColor = QColor(255,136,0,33);
            btnColor = QColor(255,196,0,204);
            iconPath = ":/new/mainWindowIcons/noback/warning.png";
        }
        else return;

        QFrame *itemFrame = new QFrame(contentContainer);
        itemFrame->setStyleSheet(QString(
            "QFrame{background-color:rgba(%1,%2,%3,%4);border-radius:10px;}"
        ).arg(bgColor.red()).arg(bgColor.green()).arg(bgColor.blue()).arg(bgColor.alpha()));

        QHBoxLayout *itemLayout = new QHBoxLayout(itemFrame);
        itemLayout->setContentsMargins(s(10), s(8), s(10), s(8));
        itemLayout->setSpacing(s(12));

        QLabel *iconLabel = new QLabel(itemFrame);
        iconLabel->setFixedSize(s(32), s(32));
        iconLabel->setPixmap(
            QPixmap(iconPath).scaled(s(32), s(32), Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );
        iconLabel->setStyleSheet("background:transparent;");
        iconLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        itemLayout->addWidget(iconLabel);

        QLabel *textLabel = new QLabel(itemFrame);

        const QString serviceLabel =
            (item.severity == "red") ? "Текущее обслуживание" : "Скоро обслуживание";

        // 🔥 ПЕРВАЯ СТРОКА — НАЗВАНИЕ AGV + тип обслуживания + количество
        QString topLine = QString(
            "<span style='font-weight:800; color:#000000;'>%1 — %2 — %3 задач(и)</span>"
        ).arg(item.agvName).arg(serviceLabel).arg(item.details);


        // 🔥 ВТОРАЯ СТРОКА — дата + название задачи + за кем/общая
        QString assignSuffix = item.assignedInfo.isEmpty() ? "общая" : item.assignedInfo;
        QString bottomLine = QString(
            "<span style='color:#777777;'>%1 — %2 — %3</span>"
        ).arg(item.date.toString("dd.MM.yyyy")).arg(item.type).arg(assignSuffix);

        textLabel->setText(
            topLine +
            "<br style='line-height:200%; font-size:8px;'>" +   // ← ровный увеличенный отступ
            bottomLine
        );

        textLabel->setStyleSheet(QString(
            "background:transparent;font-family:Inter;font-size:%1px;"
        ).arg(s(14)));
        textLabel->setWordWrap(true);

        itemLayout->addWidget(textLabel, 1);

        QPushButton *showBtn = new QPushButton("Показать", itemFrame);
        showBtn->setStyleSheet(QString(
            "QPushButton{background-color:rgba(%1,%2,%3,%4);color:white;font-family:Inter;font-size:%5px;"
            "font-weight:700;border-radius:8px;padding:%6px %7px;border:none;} "
            "QPushButton:hover{background-color:rgba(%8,%9,%10,%11);} "
            "QPushButton:pressed{background-color:rgba(%12,%13,%14,%15);}"
        )
        .arg(btnColor.red()).arg(btnColor.green()).arg(btnColor.blue()).arg(btnColor.alpha())
        .arg(s(13)).arg(s(4)).arg(s(10))
        .arg(btnColor.lighter().red()).arg(btnColor.lighter().green()).arg(btnColor.lighter().blue()).arg(btnColor.lighter().alpha())
        .arg(btnColor.darker().red()).arg(btnColor.darker().green()).arg(btnColor.darker().blue()).arg(btnColor.darker().alpha()));

        connect(showBtn, &QPushButton::clicked, this, [this, item](){
            showAgvDetailInfo(item.agvId);
            if (agvSettingsPage)
                agvSettingsPage->highlightTask(item.type);
        });

        itemLayout->addWidget(showBtn, 0, Qt::AlignVCenter | Qt::AlignRight);

        contentLayout->addWidget(itemFrame);
    };


    QLabel *upcomingPlaceholder = new QLabel(QStringLiteral("Загрузка предстоящего ТО…"), contentContainer);
    upcomingPlaceholder->setObjectName(QStringLiteral("upcomingMaintenancePlaceholder"));
    upcomingPlaceholder->setStyleSheet(QString(
        "background:transparent;font-family:Inter;font-size:%1px;font-weight:700;color:#6B7280;"
    ).arg(s(14)));
    contentLayout->addWidget(upcomingPlaceholder);
    contentLayout->addStretch();

    rightUpcomingLayout->addWidget(upcomingScroll, 1);



    //
    // ======================= НОВЫЙ AGV LIST =======================
    //
    listAgvInfo = new ListAgvInfo([this](int v){ return s(v); }, rightBodyFrame);
    listAgvInfo->setVisible(false);
    connect(listAgvInfo, &ListAgvInfo::openAgvDetails, this, [this](const QString &id){
        showAgvDetailInfo(id);
    });

    // Когда нажали "Назад" в списке AGV → возвращаемся в календарь
    connect(listAgvInfo, &ListAgvInfo::backRequested, this, [this](){
        showCalendar();
    });

    //
    // ======================= СУПЕР‑НАСТРОЙКИ AGV =======================
    //
    agvSettingsPage = new AgvSettingsPage([this](int v){ return s(v); }, rightBodyFrame);
    agvSettingsPage->setVisible(false);
    connect(agvSettingsPage, &AgvSettingsPage::backRequested, this, [this](){
        showAgvList();
    });
    connect(agvSettingsPage, &AgvSettingsPage::tasksChanged,
            this, [this](){
                updateUpcomingMaintenance();
                if (listAgvInfo && listAgvInfo->isVisible()) {
                    QVector<AgvInfo> agvs = listAgvInfo->loadAgvList();
                    listAgvInfo->rebuildList(agvs);
                    agvListDirty_ = false;
                } else {
                    agvListDirty_ = true;
                }
                // Изменили даты/провели обслуживание:
                // если календарь открыт — обновляем сразу, иначе откладываем
                // до следующего открытия экрана календаря.
                if (rightCalendarFrame && rightCalendarFrame->isVisible()) {
                    QTimer::singleShot(0, this, [this](){
                        setSelectedMonthYear(selectedMonth_, selectedYear_);
                    });
                } else {
                    pendingCalendarReload_ = true;
                }
            });
    connect(agvSettingsPage, &AgvSettingsPage::openDelegatorChatRequested,
            this, &leftMenu::openEmbeddedDelegatorChatForAgv);

    //
    // ======================= МОДЕЛИ AGV =======================
    //
    modelListPage = new ModelListPage([this](int v){ return s(v); }, rightBodyFrame);
    modelListPage->setVisible(false);

    connect(modelListPage, &ModelListPage::backRequested, this, [this](){
        showCalendar();
    });

    //
    // ======================= LOGS =======================
    //
    logsPage = new QFrame(rightBodyFrame);
    logsPage->setStyleSheet("background-color:#F1F2F4;border-radius:12px;");
    logsPage->setVisible(false);

    QVBoxLayout *logsRoot = new QVBoxLayout(logsPage);
    logsRoot->setContentsMargins(s(10), s(10), s(10), s(10));
    logsRoot->setSpacing(s(10));

    QWidget *logsHeader = new QWidget(logsPage);
    QHBoxLayout *logsHdr = new QHBoxLayout(logsHeader);
    logsHdr->setContentsMargins(0,0,0,0);
    logsHdr->setSpacing(s(10));

    QPushButton *logsBack = new QPushButton("   Назад", logsHeader);
    logsBack->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    logsBack->setIconSize(QSize(s(24), s(24)));
    logsBack->setFixedSize(s(150), s(50));
    logsBack->setStyleSheet(QString(
        "QPushButton { background-color:#E6E6E6; border-radius:%1px; border:1px solid #C8C8C8;"
        "font-family:Inter; font-size:%2px; font-weight:800; color:black; text-align:left; padding-left:%3px; }"
        "QPushButton:hover { background-color:#D5D5D5; }"
    ).arg(s(10)).arg(s(16)).arg(s(10)));

    QLabel *logsTitle = new QLabel("Logs", logsHeader);
    logsTitle->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
    ).arg(s(26)));
    logsTitle->setAlignment(Qt::AlignCenter);

    QPushButton *logsRefresh = new QPushButton("Обновить", logsHeader);
    logsRefresh->setFixedSize(s(130), s(40));
    logsRefresh->setStyleSheet(QString(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;font-size:%1px;font-weight:800;border-radius:%2px;}"
        "QPushButton:hover{background:#1A4ACD;}"
    ).arg(s(14)).arg(s(8)));

    QString logsUserRole = getUserRole(AppSession::currentUsername());
    bool isTechUser = (logsUserRole == "tech" || logsUserRole == "admin");

    if (isTechUser) {
        logsLoadAllBtn = new QPushButton("Загрузить все", logsHeader);
        logsLoadAllBtn->setFixedSize(s(140), s(40));
        logsLoadAllBtn->setStyleSheet(QString(
            "QPushButton{background:#059669;color:white;font-family:Inter;font-size:%1px;font-weight:800;border-radius:%2px;}"
            "QPushButton:hover{background:#047857;}"
        ).arg(s(14)).arg(s(8)));

        logsExportBtn = new QPushButton("Скачать логи", logsHeader);
        logsExportBtn->setFixedSize(s(170), s(40));
        logsExportBtn->setStyleSheet(QString(
            "QPushButton{background:#6366F1;color:white;font-family:Inter;font-size:%1px;font-weight:800;border-radius:%2px;}"
            "QPushButton:hover{background:#4F46E5;}"
        ).arg(s(14)).arg(s(8)));
    }

    logsHdr->addWidget(logsBack, 0, Qt::AlignLeft);
    logsHdr->addStretch();
    logsHdr->addWidget(logsTitle, 0, Qt::AlignCenter);
    logsHdr->addStretch();
    if (logsLoadAllBtn) logsHdr->addWidget(logsLoadAllBtn, 0, Qt::AlignRight);
    if (logsExportBtn) logsHdr->addWidget(logsExportBtn, 0, Qt::AlignRight);
    logsHdr->addWidget(logsRefresh, 0, Qt::AlignRight);
    logsRoot->addWidget(logsHeader);

    // Log filters
    QWidget *filterRow = new QWidget(logsPage);
    filterRow->setStyleSheet("background:transparent;");
    QHBoxLayout *filterLay = new QHBoxLayout(filterRow);
    filterLay->setContentsMargins(0,0,0,0);
    filterLay->setSpacing(s(8));

    QString filterComboStyle = QString(
        "QComboBox{background:white;border:1px solid #C8C8C8;border-radius:%1px;"
        "font-family:Inter;font-size:%2px;padding:4px 8px;}"
    ).arg(s(6)).arg(s(12));

    auto makeFilterCombo = [&](const QString &placeholder) -> QComboBox* {
        QComboBox *c = new QComboBox(filterRow);
        c->setStyleSheet(filterComboStyle);
        c->setMinimumWidth(s(120));
        c->addItem(placeholder, "");
        return c;
    };

    logFilterUser_ = makeFilterCombo("Пользователь");
    logFilterSource_ = makeFilterCombo("Источник");
    logFilterCategory_ = makeFilterCombo("Категория");
    logFilterTime_ = makeFilterCombo("Период");
    logFilterTime_->addItem("Сегодня", "today");
    logFilterTime_->addItem("Последние 7 дней", "week");
    logFilterTime_->addItem("Последние 30 дней", "month");
    logFilterTime_->addItem("Все", "all");

    filterLay->addWidget(logFilterUser_);
    filterLay->addWidget(logFilterSource_);
    filterLay->addWidget(logFilterCategory_);
    filterLay->addWidget(logFilterTime_);
    filterLay->addStretch();

    auto applyLogFilters = [this](){
        reloadLogs(lastLogsMaxRows_);
    };
    connect(logFilterUser_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyLogFilters);
    connect(logFilterSource_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyLogFilters);
    connect(logFilterCategory_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyLogFilters);
    connect(logFilterTime_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyLogFilters);

    logsRoot->addWidget(filterRow);

    logsTable = new QTableWidget(logsPage);
    logsTable->setColumnCount(5);
    logsTable->setHorizontalHeaderLabels(QStringList()
                                         << "Время" << "Источник" << "Пользователь" << "Категория" << "Детали");
    logsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    logsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    logsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    logsTable->verticalHeader()->setVisible(false);
    logsTable->setWordWrap(true);
    logsTable->setStyleSheet(
        "QTableWidget{background:white;border:1px solid #DADDE3;border-radius:8px;font-family:Inter;}"
        "QHeaderView::section{background:#EDEFF3;font-weight:800;border:none;padding:6px;}"
    );
    logsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    logsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    logsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    logsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    logsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    logsRoot->addWidget(logsTable, 1);

    if (isTechUser) {
        QLabel *techLbl = new QLabel(QStringLiteral("Тех-диагностика (результаты стресс-теста и подробный лог):"), logsPage);
        techLbl->setStyleSheet(QString(QStringLiteral("font-family:Inter;font-size:%1px;font-weight:800;color:#0F172A;"))
                                   .arg(s(12)));
        calendarStressTestBtn_ = new QPushButton(QStringLiteral("Стресс календаря (500)"), logsPage);
        calendarStressTestBtn_->setToolTip(
            QStringLiteral("500 раз подряд переключает месяц вперёд (полная пересборка UI). Результат — в окне ниже и в tech_verbose.log"));
        calendarStressTestBtn_->setStyleSheet(QString(
            "QPushButton{background:#B45309;color:white;font-family:Inter;font-size:%1px;font-weight:800;border-radius:%2px;padding:6px 12px;}"
            "QPushButton:hover{background:#92400E;}"
        ).arg(s(12)).arg(s(8)));
        fullStressAutotestBtn_ = new QPushButton(QStringLiteral("Комплексный тест"), logsPage);
        fullStressAutotestBtn_->setToolTip(
            QStringLiteral("Комплексный тест: много CHECK (БД, навигация, календарь, DataBus, профиль, чаты); "
                          "лимит ~%1 с; итог всегда с отчётом PASS/SKIP. Тяжёлый календарь — оранжевая кнопка.")
                .arg(kLeftMenuStressWallCapMs / 1000));
        fullStressAutotestBtn_->setStyleSheet(QString(
            "QPushButton{background:#7C3AED;color:white;font-family:Inter;font-size:%1px;font-weight:800;border-radius:%2px;padding:6px 12px;}"
            "QPushButton:hover{background:#6D28D9;}"
        ).arg(s(12)).arg(s(8)));
        QHBoxLayout *techRow = new QHBoxLayout();
        techRow->setContentsMargins(0, 0, 0, 0);
        techRow->addWidget(techLbl);
        techRow->addWidget(calendarStressTestBtn_);
        techRow->addWidget(fullStressAutotestBtn_);
        techRow->addStretch();
        logsRoot->addLayout(techRow);

        techDiagLogEdit_ = new QTextEdit(logsPage);
        techDiagLogEdit_->setReadOnly(true);
        techDiagLogEdit_->setMaximumHeight(s(240));
        techDiagLogEdit_->setPlaceholderText(QStringLiteral(
            "Здесь появятся строки TECH_DIAG после стресс-теста и при перелистывании календаря (роли: администратор/разработчик)."));
        techDiagLogEdit_->setStyleSheet(QString(
            "QTextEdit{font-family:Consolas,monospace;font-size:%1px;background:#0F172A;color:#E2E8F0;border:1px solid #334155;border-radius:8px;padding:8px;}"
        ).arg(s(10)));
        for (const QString &line : techDiagRecentLines(500))
            techDiagLogEdit_->append(line);
        setTechDiagLogSink(techDiagLogEdit_);
        connect(calendarStressTestBtn_, &QPushButton::clicked, this, [this]() {
            runCalendarStressTest(500, true, true);
        });
        connect(fullStressAutotestBtn_, &QPushButton::clicked, this, [this]() {
            runFullStressAutotest();
        });
        logsRoot->addWidget(techDiagLogEdit_);
    }

    connect(logsBack, &QPushButton::clicked, this, [this](){
        if (logsTable) logsTable->setRowCount(0);
        showCalendar();
    });
    connect(logsRefresh, &QPushButton::clicked, this, [this](){
        logsStale_ = true;
        reloadingLogs_ = false;
        reloadLogs(lastLogsMaxRows_);
    });
    if (logsLoadAllBtn) {
        connect(logsLoadAllBtn, &QPushButton::clicked, this, [this](){ reloadLogs(0); });
    }
    if (logsExportBtn) {
        connect(logsExportBtn, &QPushButton::clicked, this, [this](){
            QVector<UserInfo> users = getAllUsers(false);
            if (users.isEmpty()) {
                QMessageBox::information(this, "Логи", "Нет пользователей для выбора.");
                return;
            }

            std::sort(users.begin(), users.end(), [](const UserInfo &a, const UserInfo &b) {
                const QString an = a.fullName.trimmed().isEmpty() ? a.username : a.fullName;
                const QString bn = b.fullName.trimmed().isEmpty() ? b.username : b.fullName;
                return an.toLower() < bn.toLower();
            });

            QDialog pick(this);
            pick.setWindowTitle("Выберите пользователей");
            pick.setModal(true);
            pick.setMinimumWidth(520);

            QVBoxLayout *v = new QVBoxLayout(&pick);
            QLabel *lbl = new QLabel("Отметьте пользователей, чьи логи нужно скачать:", &pick);
            v->addWidget(lbl);

            QScrollArea *scroll = new QScrollArea(&pick);
            scroll->setWidgetResizable(true);
            QWidget *host = new QWidget(scroll);
            QVBoxLayout *hostLay = new QVBoxLayout(host);
            hostLay->setContentsMargins(6, 6, 6, 6);
            hostLay->setSpacing(6);

            QVector<QCheckBox*> checks;
            checks.reserve(users.size());
            for (const UserInfo &u : users) {
                const QString display = u.fullName.trimmed().isEmpty()
                                            ? u.username
                                            : QString("%1 (%2)").arg(u.fullName, u.username);
                QCheckBox *cb = new QCheckBox(display, host);
                cb->setProperty("username", u.username);
                cb->setChecked(true);
                checks.push_back(cb);
                hostLay->addWidget(cb);
            }
            hostLay->addStretch();
            scroll->setWidget(host);
            v->addWidget(scroll, 1);

            QHBoxLayout *btnRow = new QHBoxLayout();
            QPushButton *allBtn = new QPushButton("Выбрать всех", &pick);
            QPushButton *noneBtn = new QPushButton("Снять всё", &pick);
            QPushButton *cancelBtn = new QPushButton("Отмена", &pick);
            QPushButton *okBtn = new QPushButton("Скачать", &pick);
            btnRow->addWidget(allBtn);
            btnRow->addWidget(noneBtn);
            btnRow->addStretch();
            btnRow->addWidget(cancelBtn);
            btnRow->addWidget(okBtn);
            v->addLayout(btnRow);

            connect(allBtn, &QPushButton::clicked, &pick, [checks]() {
                for (QCheckBox *cb : checks) cb->setChecked(true);
            });
            connect(noneBtn, &QPushButton::clicked, &pick, [checks]() {
                for (QCheckBox *cb : checks) cb->setChecked(false);
            });
            connect(cancelBtn, &QPushButton::clicked, &pick, &QDialog::reject);
            connect(okBtn, &QPushButton::clicked, &pick, &QDialog::accept);

            if (pick.exec() != QDialog::Accepted)
                return;

            QSet<QString> selectedUsers;
            for (QCheckBox *cb : checks) {
                if (cb->isChecked())
                    selectedUsers.insert(cb->property("username").toString().trimmed());
            }
            if (selectedUsers.isEmpty()) {
                QMessageBox::information(this, "Логи", "Нужно выбрать хотя бы одного пользователя.");
                return;
            }

            QString logsDir = localLogsDirPath();
            if (!QDir().mkpath(logsDir)) {
                QMessageBox::warning(this, "Логи", "Не удалось создать папку для логов.");
                return;
            }

            QString srcPath = localLogFilePath();
            if (!QFile::exists(srcPath)) {
                const QString oldPath = QCoreApplication::applicationDirPath() + "/logs/app.log";
                if (QFile::exists(oldPath))
                    srcPath = oldPath;
            }

            QFile src(srcPath);
            if (!src.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QMessageBox::warning(this, "Логи", "Не удалось открыть локальный файл логов.");
                return;
            }

            const QString outPath = logsDir + QString("/logs_%1.txt")
                .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
            QFile outFile(outPath);
            if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::warning(this, "Логи", "Не удалось создать файл выгрузки.");
                return;
            }

            QTextStream in(&src);
            in.setCodec("UTF-8");
            QTextStream out(&outFile);
            out.setCodec("UTF-8");
            int written = 0;
            while (!in.atEnd()) {
                const QString line = in.readLine();
                const int brOpen = line.indexOf('[');
                const int brClose = line.indexOf(']');
                if (brOpen < 0 || brClose <= brOpen)
                    continue;
                const QString user = line.mid(brOpen + 1, brClose - brOpen - 1).trimmed();
                if (!selectedUsers.contains(user))
                    continue;
                out << line << "\n";
                ++written;
            }
            src.close();
            outFile.close();

            QMessageBox::information(this, "Логи",
                                     QString("Готово. Записано %1 строк.\nФайл: %2")
                                         .arg(written)
                                         .arg(outPath));
        });
    }

    //
    // ======================= ДОБАВЛЯЕМ В ПРАВУЮ ПАНЕЛЬ =======================
    //
    rightBodyLayout->addWidget(rightCalendarFrame, 3);
    rightBodyLayout->addWidget(rightUpcomingMaintenanceFrame, 2);
    rightBodyLayout->addWidget(listAgvInfo, 3);
    rightBodyLayout->addWidget(agvSettingsPage, 3);
    rightBodyLayout->addWidget(modelListPage, 3);
    rightBodyLayout->addWidget(logsPage, 3);

    //
    // ======================= ПЕРЕКЛЮЧЕНИЕ РЕЖИМОВ =======================
    //
    //
    // ======================= USERS PAGE =======================
    //
    usersPage = new UsersPage([this](int v){ return s(v); }, rightBodyFrame);
    usersPage->setVisible(false);
    usersPage->setProperty("loaded_once", false);

    connect(usersPage, &UsersPage::backRequested, this, [this](){
        showCalendar();
    });

    connect(usersPage, &UsersPage::openUserDetailsRequested, this, [this](const QString &username){
        showUserProfilePage(username);
    });

    connect(&DataBus::instance(), &DataBus::userDataChanged,
            this, [this]() {
        avatarCache_.clear();
        if (!usersPage) return;
        usersPage->setProperty("loaded_once", false);
        QTimer::singleShot(0, usersPage, [this]() {
            if (usersPage) usersPage->loadUsers();
        });
    });

    rightBodyLayout->addWidget(usersPage, 3);

    connect(btnAgv, &QPushButton::clicked, this, [this](){
        showAgvList();
    });
    connect(btnEdit, &QPushButton::clicked, this, [this](){
        showModelList();
    });
    // === АВТО-ОБНОВЛЕНИЕ КАЛЕНДАРЯ И ПРЕДСТОЯЩЕГО ТО ===
    connect(&DataBus::instance(), &DataBus::calendarChanged,
            this, [this](){
                invalidateCalendarEventsCache();
                updateUpcomingMaintenance();
                // Во время комплексного теста десятки сигналов подряд иначе ставят в очередь полную пересборку leftMenu (зависание).
                if (stressSuiteRunning_) {
                    pendingCalendarReload_ = true;
                    return;
                }
                if (rightCalendarFrame && rightCalendarFrame->isVisible()) {
                    QTimer::singleShot(0, this, [this](){
                        refreshCalendarMonthLight();
                    });
                } else {
                    pendingCalendarReload_ = true;
                }
            });
    if (listAgvInfo)
    {
        connect(listAgvInfo, &ListAgvInfo::agvListChanged,
                this, &leftMenu::updateAgvCounter);
        connect(listAgvInfo, &ListAgvInfo::agvListChanged, this, [this]() {
            logsStale_ = true;
        });

    }

    // Моментальное обновление счётчика при изменениях через DataBus.
    connect(&DataBus::instance(), &DataBus::agvListChanged,
            this, [this](){
                logsStale_ = true;
                invalidateCalendarEventsCache();
                updateAgvCounter();
                updateSystemStatus();
                agvListDirty_ = true;
                if (stressSuiteRunning_) {
                    pendingCalendarReload_ = true;
                    return;
                }
                if (rightCalendarFrame && rightCalendarFrame->isVisible()) {
                    QTimer::singleShot(0, this, [this](){
                        refreshCalendarMonthLight();
                    });
                } else {
                    pendingCalendarReload_ = true;
                }
            });

    connect(&DataBus::instance(), &DataBus::calendarChanged,
            this, &leftMenu::updateSystemStatus);

    agvCounterTimer = new QTimer(this);
    agvCounterTimer->setSingleShot(true);
    connect(agvCounterTimer, &QTimer::timeout, this, [this](){
        updateAgvCounter();
    });

    scheduleDeferredStartupLoads();

    connect(&DataBus::instance(), &DataBus::notificationsChanged,
            this, &leftMenu::updateNotifBadge);
    connect(&DataBus::instance(), &DataBus::notificationsChanged,
            this, [this]() {
        if (chatsPage && chatsPage->isVisible() && chatsStack_ && chatsStack_->currentIndex() == 0)
            reloadChatsPageList();
    });

    QTimer::singleShot(100, this, &leftMenu::updateNotifBadge);

    notifPollTimer = new QTimer(this);
    notifPollTimer->setInterval(5000);
    connect(notifPollTimer, &QTimer::timeout, this, &leftMenu::updateNotifBadge);
    notifPollTimer->start();

    chatsPollTimer = new QTimer(this);
        // Статус "когда был в сети" обновляем не слишком часто, чтобы не перегружать БД.
        chatsPollTimer->setInterval(180000);
    connect(chatsPollTimer, &QTimer::timeout, this, [this]() {
        // Обновляем список только когда открыт именно список чатов (не сам диалог).
            if (chatsPage && chatsPage->isVisible() && chatsStack_ && chatsStack_->currentIndex() == 0) {
                lastChatsListSignature_.clear();
            reloadChatsPageList();
            }
    });
    chatsPollTimer->start();
}
