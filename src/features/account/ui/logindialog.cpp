#include "logindialog.h"
#include "db_users.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegExpValidator>
#include <QClipboard>
#include <QApplication>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QAction>
#include <QPainter>
#include <QPen>
#include <QRegularExpression>

static QIcon loadIconScaled(const QString &path, int size = 20)
{
    QPixmap pm(path);
    if (pm.isNull())
        return QIcon();

    pm = pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return QIcon(pm);
}

static QIcon makePasswordEyeIcon(bool crossed, int size = 18)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor stroke(79, 95, 129);
    QPen pen(stroke, qMax(1.6, size * 0.10), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const QRectF eyeRect(size * 0.10, size * 0.26, size * 0.80, size * 0.48);
    p.drawEllipse(eyeRect);

    p.setBrush(stroke);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(size * 0.40, size * 0.40, size * 0.20, size * 0.20));

    if (crossed) {
        QPen slash(QColor(220, 38, 38), qMax(1.8, size * 0.12), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(slash);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(size * 0.18, size * 0.84), QPointF(size * 0.84, size * 0.18));
    }

    p.end();
    return QIcon(pm);
}


LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Вход в систему");
    setModal(true);
    setMinimumSize(460, 700);
    resize(520, 800);
    setSizeGripEnabled(true);

    setStyleSheet(
        "QDialog { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #F4F7FF, stop:1 #EEF3FB); }"
        "QFrame#authCard { background: #FFFFFF; border: 1px solid #DCE5F4; border-radius: 18px; }"
        "QFrame#heroPanel { background: #F1F6FF; border: 1px solid #D6E2FF; border-radius: 12px; }"
        "QFrame#formPanel { background: #F8FAFF; border: 1px solid #DEE6F6; border-radius: 12px; }"
        "QLabel#heroTitle { font-family: Inter; font-size: 15px; font-weight: 800; color: #15327A; }"
        "QLabel#heroSubTitle { font-family: Inter; font-size: 12px; font-weight: 600; color: #4B5F93; }"
        "QLabel#titleLabel { font-family: Inter; font-size: 25px; font-weight: 900; color: #0F2438; }"
        "QLabel#subTitleLabel { font-family: Inter; font-size: 13px; font-weight: 500; color: #5D6E82; }"
        "QLabel#fieldLabel { font-family: Inter; font-size: 13px; font-weight: 800; color: #1F334A; }"
        "QLabel#hintLabel { font-family: Inter; font-size: 12px; font-weight: 600; color: #63789C; }"
        "QLabel#dividerLabel { font-family: Inter; font-size: 11px; font-weight: 700; color: #8FA2C8; }"
        "QLabel#errorLabel { font-family: Inter; font-size: 12px; font-weight: 600; color: #D6284B; }"
        "QLineEdit {"
        "  background: #FFFFFF;"
        "  border: 1px solid #D3DDF0;"
        "  border-radius: 11px;"
        "  padding: 11px 12px;"
        "  font-family: Inter;"
        "  font-size: 14px;"
        "  color: #0F172A;"
        "}"
        "QLineEdit[passwordField=\"true\"] { padding-right: 42px; }"
        "QLineEdit:focus { border: 1px solid #335CFF; background: #FFFFFF; }"
        "QComboBox {"
        "  background: #FFFFFF;"
        "  border: 1px solid #D3DDF0;"
        "  border-radius: 11px;"
        "  padding: 11px 12px;"
        "  font-family: Inter;"
        "  font-size: 14px;"
        "  color: #0F172A;"
        "}"
        "QComboBox:focus { border: 1px solid #335CFF; background: #FFFFFF; }"
        "QComboBox::drop-down { border: none; width: 30px; }"
        "QComboBox::down-arrow { image: none; border-left: 5px solid transparent; border-right: 5px solid transparent; border-top: 6px solid #64748B; }"
        "QPushButton {"
        "  font-family: Inter;"
        "  font-size: 14px;"
        "  font-weight: 800;"
        "  border-radius: 11px;"
        "  padding: 11px 14px;"
        "  border: 1px solid transparent;"
        "}"
        "QPushButton#primaryBtn { background: #1F46FF; color: white; }"
        "QPushButton#primaryBtn:hover { background: #143AF3; }"
        "QPushButton#secondaryBtn { background: #EBF1FF; color: #1B3A9A; border-color: #C9D7FB; }"
        "QPushButton#secondaryBtn:hover { background: #DFE8FF; }"
        "QPushButton#ghostBtn { background: #FFFFFF; color: #52607A; border-color: #D6E0F0; }"
        "QPushButton#ghostBtn:hover { background: #F3F7FF; }"
        "QProgressBar {"
        "  border: none;"
        "  border-radius: 3px;"
        "  background: #E9EEF6;"
        "  text-align: center;"
        "  color: transparent;"
        "  height: 6px;"
        "}"
        "QProgressBar::chunk { border-radius: 3px; background: #9CA3AF; }"
    );

    loginRx = QRegExp("^[A-Za-z0-9]+$");
    passRx  = QRegExp("^[\\x21-\\x7E]+$");

    auto *loginValidator = new QRegExpValidator(loginRx, this);
    auto *passValidator  = new QRegExpValidator(passRx, this);

    const QIcon iconShow = makePasswordEyeIcon(false, 18);
    const QIcon iconHide = makePasswordEyeIcon(true, 18);
    auto setupPasswordField = [iconShow, iconHide](QLineEdit *edit) {
        if (!edit)
            return;
        edit->setEchoMode(QLineEdit::Password);
        edit->setProperty("passwordField", true);

        QAction *toggleAction = new QAction(iconShow, QString(), edit);
        toggleAction->setCheckable(true);
        toggleAction->setToolTip(QObject::tr("Показать пароль"));
        edit->addAction(toggleAction, QLineEdit::TrailingPosition);

        QObject::connect(toggleAction, &QAction::toggled, edit, [edit, toggleAction, iconShow, iconHide](bool shown) {
            edit->setEchoMode(shown ? QLineEdit::Normal : QLineEdit::Password);
            toggleAction->setIcon(shown ? iconHide : iconShow);
            toggleAction->setToolTip(shown ? QObject::tr("Скрыть пароль") : QObject::tr("Показать пароль"));
        });
    };

    QVBoxLayout *main = new QVBoxLayout(this);
    main->setContentsMargins(24, 20, 24, 20);

    QFrame *authCard = new QFrame(this);
    authCard->setObjectName("authCard");
    authCard->setMaximumWidth(560);
    authCard->setMinimumWidth(430);
    QVBoxLayout *cardLayout = new QVBoxLayout(authCard);
    cardLayout->setContentsMargins(28, 24, 28, 24);
    cardLayout->setSpacing(14);

    stack = new QStackedWidget(authCard);

    // =========== LOGIN PAGE ===========
    QWidget *loginPage = new QWidget(authCard);
    loginPage->setObjectName("loginPage");
    QVBoxLayout *v = new QVBoxLayout(loginPage);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(14);

    QFrame *loginHero = new QFrame(loginPage);
    loginHero->setObjectName("heroPanel");
    QVBoxLayout *loginHeroLayout = new QVBoxLayout(loginHero);
    loginHeroLayout->setContentsMargins(14, 12, 14, 12);
    loginHeroLayout->setSpacing(2);
    QLabel *loginHeroTitle = new QLabel("AGV Manager · Авторизация", loginHero);
    loginHeroTitle->setObjectName("heroTitle");
    QLabel *loginHeroSubTitle = new QLabel("Безопасный вход в рабочий аккаунт", loginHero);
    loginHeroSubTitle->setObjectName("heroSubTitle");
    loginHeroLayout->addWidget(loginHeroTitle);
    loginHeroLayout->addWidget(loginHeroSubTitle);
    v->addWidget(loginHero);

    QLabel *lblTitle = new QLabel("Вход в аккаунт", loginPage);
    lblTitle->setObjectName("titleLabel");
    QLabel *lbl = new QLabel("Введите логин и пароль, чтобы продолжить работу", loginPage);
    lbl->setObjectName("subTitleLabel");
    lbl->setWordWrap(true);
    v->addWidget(lblTitle);
    v->addWidget(lbl);
    v->addSpacing(2);

    QFrame *loginFormPanel = new QFrame(loginPage);
    loginFormPanel->setObjectName("formPanel");
    QVBoxLayout *loginForm = new QVBoxLayout(loginFormPanel);
    loginForm->setContentsMargins(14, 14, 14, 14);
    loginForm->setSpacing(10);

    QLabel *loginFieldLabel = new QLabel("Логин", loginPage);
    loginFieldLabel->setObjectName("fieldLabel");
    loginForm->addWidget(loginFieldLabel);

    loginEdit = new QLineEdit(loginPage);
    loginEdit->setPlaceholderText("Логин");
    loginEdit->setValidator(loginValidator);
    loginEdit->setMinimumHeight(44);
    loginForm->addWidget(loginEdit);

    QLabel *passFieldLabel = new QLabel("Пароль", loginPage);
    passFieldLabel->setObjectName("fieldLabel");
    loginForm->addWidget(passFieldLabel);

    passEdit = new QLineEdit(loginPage);
    passEdit->setPlaceholderText("Пароль");
    passEdit->setValidator(passValidator);
    passEdit->setMinimumHeight(44);
    setupPasswordField(passEdit);
    loginForm->addWidget(passEdit);

    QLabel *loginHint = new QLabel("Логин: латиница и цифры. Пароль: не менее 8 символов.", loginPage);
    loginHint->setObjectName("hintLabel");
    loginHint->setWordWrap(true);
    loginForm->addWidget(loginHint);

    loginError = new QLabel(loginPage);
    loginError->setObjectName("errorLabel");
    loginError->setWordWrap(true);
    loginForm->addWidget(loginError);

    QPushButton *btnLogin = new QPushButton("Войти", loginPage);
    btnLogin->setObjectName("primaryBtn");
    btnLogin->setMinimumHeight(44);
    loginForm->addWidget(btnLogin);

    v->addWidget(loginFormPanel);

    QLabel *loginDivider = new QLabel("ДРУГИЕ ДЕЙСТВИЯ", loginPage);
    loginDivider->setObjectName("dividerLabel");
    loginDivider->setAlignment(Qt::AlignCenter);
    v->addWidget(loginDivider);

    QHBoxLayout *loginActionsRow = new QHBoxLayout();
    loginActionsRow->setContentsMargins(0, 0, 0, 0);
    loginActionsRow->setSpacing(10);

    QPushButton *btnReg = new QPushButton("Регистрация", loginPage);
    btnReg->setObjectName("secondaryBtn");
    btnReg->setMinimumHeight(42);
    loginActionsRow->addWidget(btnReg, 1);

    QPushButton *btnRecovery = new QPushButton("Вход по ключу", loginPage);
    btnRecovery->setObjectName("ghostBtn");
    btnRecovery->setMinimumHeight(42);
    loginActionsRow->addWidget(btnRecovery, 1);

    v->addLayout(loginActionsRow);
    v->addStretch();

    stack->addWidget(loginPage);

    // =========== REGISTER PAGE ===========
    QWidget *regPage = new QWidget(authCard);
    regPage->setObjectName("regPage");
    QVBoxLayout *r = new QVBoxLayout(regPage);
    r->setContentsMargins(0, 0, 0, 0);
    r->setSpacing(14);

    QFrame *regHero = new QFrame(regPage);
    regHero->setObjectName("heroPanel");
    QVBoxLayout *regHeroLayout = new QVBoxLayout(regHero);
    regHeroLayout->setContentsMargins(14, 12, 14, 12);
    regHeroLayout->setSpacing(2);
    QLabel *regHeroTitle = new QLabel("Создание аккаунта", regHero);
    regHeroTitle->setObjectName("heroTitle");
    QLabel *regHeroSubTitle = new QLabel("Заполните данные и сохраните ключ восстановления", regHero);
    regHeroSubTitle->setObjectName("heroSubTitle");
    regHeroLayout->addWidget(regHeroTitle);
    regHeroLayout->addWidget(regHeroSubTitle);
    r->addWidget(regHero);

    // Заголовки
    QLabel *rlTitle = new QLabel("Регистрация", regPage);
    rlTitle->setObjectName("titleLabel");
    QLabel *rl = new QLabel("Укажите логин, пароль и роль пользователя", regPage);
    rl->setObjectName("subTitleLabel");
    rl->setWordWrap(true);
    r->addWidget(rlTitle);
    r->addWidget(rl);
    r->addSpacing(2);

    QFrame *regFormPanel = new QFrame(regPage);
    regFormPanel->setObjectName("formPanel");
    QVBoxLayout *regForm = new QVBoxLayout(regFormPanel);
    regForm->setContentsMargins(14, 14, 14, 14);
    regForm->setSpacing(10);

    // Логин
    QLabel *regLoginLabel = new QLabel("Логин", regPage);
    regLoginLabel->setObjectName("fieldLabel");
    regForm->addWidget(regLoginLabel);

    regLoginEdit = new QLineEdit(regPage);
    regLoginEdit->setPlaceholderText("Логин");
    regLoginEdit->setValidator(loginValidator);
    regLoginEdit->setMinimumHeight(42);
    regForm->addWidget(regLoginEdit);

    // Пароль 1
    QLabel *regPass1Label = new QLabel("Пароль", regPage);
    regPass1Label->setObjectName("fieldLabel");
    regForm->addWidget(regPass1Label);

    regPass1Edit = new QLineEdit(regPage);
    regPass1Edit->setPlaceholderText("Пароль");
    regPass1Edit->setValidator(passValidator);
    regPass1Edit->setMinimumHeight(42);
    setupPasswordField(regPass1Edit);
    regForm->addWidget(regPass1Edit);

    // Пароль 2
    QLabel *regPass2Label = new QLabel("Повторите пароль", regPage);
    regPass2Label->setObjectName("fieldLabel");
    regForm->addWidget(regPass2Label);

    regPass2Edit = new QLineEdit(regPage);
    regPass2Edit->setPlaceholderText("Повторите пароль");
    regPass2Edit->setValidator(passValidator);
    regPass2Edit->setMinimumHeight(42);
    setupPasswordField(regPass2Edit);
    regForm->addWidget(regPass2Edit);

    // Индикатор надёжности — ПОД ВТОРЫМ ПОЛЕМ
    passStrength = new QProgressBar(regPage);
    passStrength->setRange(0, 100);
    passStrength->setValue(0);
    passStrength->setTextVisible(false);
    passStrength->setFixedHeight(6);

    passStrengthLabel = new QLabel("Надёжность: —", regPage);
    passStrengthLabel->setObjectName("subTitleLabel");
    passStrengthLabel->setStyleSheet("color:#6B7280;font-weight:700;");

    QHBoxLayout *strengthRow = new QHBoxLayout();
    strengthRow->setContentsMargins(0, 0, 0, 0);
    strengthRow->setSpacing(10);
    strengthRow->addWidget(passStrength, 1);
    strengthRow->addWidget(passStrengthLabel, 0, Qt::AlignVCenter);

    regForm->addLayout(strengthRow);

    connect(regPass1Edit, &QLineEdit::textChanged, this, &LoginDialog::updatePasswordStrength);

    // Роль
    QLabel *regRoleLabel = new QLabel("Роль", regPage);
    regRoleLabel->setObjectName("fieldLabel");
    regForm->addWidget(regRoleLabel);

    regRoleCombo = new QComboBox(regPage);
    regRoleCombo->addItem("Пользователь", "viewer");
    regRoleCombo->addItem("Администратор", "admin");
    regRoleCombo->addItem("Техник", "tech");
    regRoleCombo->setMinimumHeight(42);
    regForm->addWidget(regRoleCombo);

    // Ключ администратора
    adminKeyRow = new QWidget(regPage);
    adminKeyRow->setVisible(false);

    QVBoxLayout *adminKeyLayout = new QVBoxLayout(adminKeyRow);
    adminKeyLayout->setContentsMargins(0, 0, 0, 0);
    adminKeyLayout->setSpacing(4);

    QLabel *regAdminKeyLabel = new QLabel("Ключ от администратора", adminKeyRow);
    regAdminKeyLabel->setObjectName("fieldLabel");
    adminKeyLayout->addWidget(regAdminKeyLabel);

    regAdminKeyEdit = new QLineEdit(adminKeyRow);
    regAdminKeyEdit->setPlaceholderText("Запросите ключ у действующего админа");
    regAdminKeyEdit->setMinimumHeight(42);
    adminKeyLayout->addWidget(regAdminKeyEdit);

    regForm->addWidget(adminKeyRow);

    // Ключ техника
    techKeyRow = new QWidget(regPage);
    techKeyRow->setVisible(false);

    QVBoxLayout *techKeyLayout = new QVBoxLayout(techKeyRow);
    techKeyLayout->setContentsMargins(0, 0, 0, 0);
    techKeyLayout->setSpacing(4);

    QLabel *regTechKeyLabel = new QLabel("Ключ от техника", techKeyRow);
    regTechKeyLabel->setObjectName("fieldLabel");
    techKeyLayout->addWidget(regTechKeyLabel);

    regTechKeyEdit = new QLineEdit(techKeyRow);
    regTechKeyEdit->setPlaceholderText("Запросите ключ у действующего техника");
    regTechKeyEdit->setMinimumHeight(42);
    techKeyLayout->addWidget(regTechKeyEdit);

    regForm->addWidget(techKeyRow);

    connect(regRoleCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onRoleChanged(int)));

    // Ошибки
    regError = new QLabel(regPage);
    regError->setObjectName("errorLabel");
    regError->setWordWrap(true);
    regForm->addWidget(regError);

    // Кнопки
    QPushButton *btnRegOk = new QPushButton("Создать аккаунт", regPage);
    btnRegOk->setObjectName("primaryBtn");
    btnRegOk->setMinimumHeight(44);

    QPushButton *btnBack = new QPushButton("Назад", regPage);
    btnBack->setObjectName("secondaryBtn");
    btnBack->setMinimumHeight(42);

    QHBoxLayout *regActionsRow = new QHBoxLayout();
    regActionsRow->setContentsMargins(0, 0, 0, 0);
    regActionsRow->setSpacing(10);
    regActionsRow->addWidget(btnBack, 1);
    regActionsRow->addWidget(btnRegOk, 2);
    regForm->addLayout(regActionsRow);

    r->addWidget(regFormPanel);
    r->addStretch();

    stack->addWidget(regPage);








    // =========== RECOVERY PAGE ===========
    QWidget *recoveryPage = new QWidget(authCard);
    QVBoxLayout *recLayout = new QVBoxLayout(recoveryPage);
    recLayout->setContentsMargins(0, 0, 0, 0);
    recLayout->setSpacing(10);

    QLabel *recTitle = new QLabel("Восстановление доступа", recoveryPage);
    recTitle->setObjectName("titleLabel");
    QLabel *recSubtitle = new QLabel("Введите ключ восстановления, полученный при регистрации", recoveryPage);
    recSubtitle->setObjectName("subTitleLabel");
    recSubtitle->setWordWrap(true);
    recLayout->addWidget(recTitle);
    recLayout->addWidget(recSubtitle);
    recLayout->addSpacing(8);

    QLabel *recKeyLabel = new QLabel("Ключ восстановления", recoveryPage);
    recKeyLabel->setObjectName("fieldLabel");
    recLayout->addWidget(recKeyLabel);

    recoveryKeyEdit = new QLineEdit(recoveryPage);
    recoveryKeyEdit->setPlaceholderText("RK-XXXX-XXXX-XXXX");
    recoveryKeyEdit->setMinimumHeight(42);
    recLayout->addWidget(recoveryKeyEdit);

    recoveryError = new QLabel(recoveryPage);
    recoveryError->setObjectName("errorLabel");
    recoveryError->setWordWrap(true);
    recLayout->addWidget(recoveryError);

    QPushButton *btnRecoveryFromFile = new QPushButton("Вставить ключ из файла", recoveryPage);
    btnRecoveryFromFile->setObjectName("secondaryBtn");
    btnRecoveryFromFile->setMinimumHeight(40);
    recLayout->addWidget(btnRecoveryFromFile);

    QPushButton *btnRecoveryOk = new QPushButton("Войти", recoveryPage);
    btnRecoveryOk->setObjectName("primaryBtn");
    btnRecoveryOk->setMinimumHeight(42);
    recLayout->addWidget(btnRecoveryOk);

    QPushButton *btnRecoveryBack = new QPushButton("Назад", recoveryPage);
    btnRecoveryBack->setObjectName("secondaryBtn");
    btnRecoveryBack->setMinimumHeight(40);
    recLayout->addWidget(btnRecoveryBack);

    stack->addWidget(recoveryPage);

    // =========== PASSWORD CHANGE PAGE ===========
    QWidget *passChangePage = new QWidget(authCard);
    QVBoxLayout *pcLayout = new QVBoxLayout(passChangePage);
    pcLayout->setContentsMargins(0, 0, 0, 0);
    pcLayout->setSpacing(10);

    QLabel *pcTitle = new QLabel("Смена пароля", passChangePage);
    pcTitle->setObjectName("titleLabel");
    QLabel *pcSubtitle = new QLabel("Установите новый пароль для вашего аккаунта", passChangePage);
    pcSubtitle->setObjectName("subTitleLabel");
    pcSubtitle->setWordWrap(true);
    pcLayout->addWidget(pcTitle);
    pcLayout->addWidget(pcSubtitle);
    pcLayout->addSpacing(8);

    QLabel *newPass1Label = new QLabel("Новый пароль", passChangePage);
    newPass1Label->setObjectName("fieldLabel");
    pcLayout->addWidget(newPass1Label);

    newPass1Edit = new QLineEdit(passChangePage);
    newPass1Edit->setPlaceholderText("Новый пароль");
    newPass1Edit->setValidator(passValidator);
    newPass1Edit->setMinimumHeight(42);
    setupPasswordField(newPass1Edit);
    pcLayout->addWidget(newPass1Edit);

    newPassStrength = new QProgressBar(passChangePage);
    newPassStrength->setRange(0, 100);
    newPassStrength->setValue(0);
    newPassStrength->setTextVisible(false);
    newPassStrength->setFixedHeight(6);

    newPassStrengthLabel = new QLabel("Надёжность: —", passChangePage);
    newPassStrengthLabel->setObjectName("subTitleLabel");
    newPassStrengthLabel->setStyleSheet("color:#6B7280;font-weight:700;");

    QHBoxLayout *newStrengthRow = new QHBoxLayout();
    newStrengthRow->setContentsMargins(0, 0, 0, 0);
    newStrengthRow->setSpacing(10);
    newStrengthRow->addWidget(newPassStrength, 1);
    newStrengthRow->addWidget(newPassStrengthLabel, 0, Qt::AlignVCenter);
    pcLayout->addLayout(newStrengthRow);

    QLabel *newPass2Label = new QLabel("Повторите пароль", passChangePage);
    newPass2Label->setObjectName("fieldLabel");
    pcLayout->addWidget(newPass2Label);

    newPass2Edit = new QLineEdit(passChangePage);
    newPass2Edit->setPlaceholderText("Повторите новый пароль");
    newPass2Edit->setValidator(passValidator);
    newPass2Edit->setMinimumHeight(42);
    setupPasswordField(newPass2Edit);
    pcLayout->addWidget(newPass2Edit);

    newPassError = new QLabel(passChangePage);
    newPassError->setObjectName("errorLabel");
    newPassError->setWordWrap(true);
    pcLayout->addWidget(newPassError);

    QPushButton *btnChangePass = new QPushButton("Сменить пароль", passChangePage);
    btnChangePass->setObjectName("primaryBtn");
    btnChangePass->setMinimumHeight(42);
    pcLayout->addWidget(btnChangePass);

    stack->addWidget(passChangePage);

    cardLayout->addWidget(stack);
    main->addStretch();
    main->addWidget(authCard, 0, Qt::AlignHCenter);
    main->addStretch();

    // ========================================================
    // СОБЫТИЯ И ОБРАБОТКИ
    // ========================================================

    connect(btnLogin, SIGNAL(clicked()), this, SLOT(onLoginClicked()));
    connect(btnReg, SIGNAL(clicked()), this, SLOT(onRegisterClicked()));

    connect(btnRecovery, &QPushButton::clicked, this, [this]() {
        stack->setCurrentIndex(2);
    });

    connect(btnRecoveryBack, &QPushButton::clicked, this, [this]() {
        recoveryError->clear();
        recoveryKeyEdit->clear();
        stack->setCurrentIndex(0);
    });

    connect(recoveryKeyEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        recoveryError->clear();
    });

    auto loginByRecoveryKey = [this](const QString &rawKey) {
        recoveryError->clear();
        const QString key = rawKey.trimmed().toUpper();

        if (key.isEmpty()) {
            recoveryError->setText("Введите ключ восстановления");
            return;
        }

        QString username;
        QString error;
        if (!verifyPermanentRecoveryKey(key, username, error)) {
            recoveryError->setText(error);
            return;
        }

        UserInfo u;
        u.username = username;
        u.isActive = true;

        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare("SELECT id, role, is_active FROM users WHERE username = :u");
            q.bindValue(":u", username);
            if (q.exec() && q.next()) {
                u.id = q.value(0).toInt();
                u.role = q.value(1).toString();
                u.isActive = q.value(2).toInt() == 1;
            }
        }

        if (!u.isActive) {
            recoveryError->setText("Аккаунт заблокирован");
            return;
        }

        enableRememberMe(username);
        user_ = u;
        logAction(username, "login_by_recovery_key", "Вход выполнен по ключу восстановления");
        recoveryKeyEdit->clear();
        accept();
    };

    connect(btnRecoveryOk, &QPushButton::clicked, this, [this, loginByRecoveryKey]() {
        loginByRecoveryKey(recoveryKeyEdit->text());
    });

    connect(btnRecoveryFromFile, &QPushButton::clicked, this, [this, loginByRecoveryKey]() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            "Выберите файл с ключом",
            QString(),
            "Text files (*.txt);;All files (*.*)"
        );
        if (path.isEmpty())
            return;

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            recoveryError->setText("Не удалось открыть файл");
            return;
        }

        const QString content = QString::fromUtf8(f.readAll());
        f.close();

        QString key;
        const QRegularExpression rkRe("RK-[A-Za-z0-9]{4}-[A-Za-z0-9]{4}-[A-Za-z0-9]{4}");
        const QRegularExpressionMatch m = rkRe.match(content);
        if (m.hasMatch())
            key = m.captured(0);

        if (key.isEmpty()) {
            const QStringList lines = content.split('\n');
            for (const QString &lineRaw : lines) {
                const QString line = lineRaw.trimmed();
                if (line.isEmpty())
                    continue;
                if (line.startsWith("Ключ:", Qt::CaseInsensitive)) {
                    key = line.section(':', 1).trimmed();
                    break;
                }
                if (line.startsWith("Recovery key:", Qt::CaseInsensitive)) {
                    key = line.section(':', 1).trimmed();
                    break;
                }
            }
        }

        if (key.isEmpty()) {
            recoveryError->setText("В файле не найден корректный ключ");
            return;
        }

        recoveryKeyEdit->setText(key.toUpper());
        loginByRecoveryKey(key);
    });

    connect(newPass1Edit, &QLineEdit::textChanged, this, [this](const QString &text) {
        newPassError->clear();

        int score = 0;
        const int len = text.length();
        const bool hasLower = text.contains(QRegExp("[a-z]"));
        const bool hasUpper = text.contains(QRegExp("[A-Z]"));
        const bool hasDigit = text.contains(QRegExp("[0-9]"));
        const bool hasSpecial = text.contains(QRegExp("[^A-Za-z0-9]"));

        int classes = 0;
        if (hasLower) ++classes;
        if (hasUpper) ++classes;
        if (hasDigit) ++classes;
        if (hasSpecial) ++classes;

        if (len >= 6) score += 20;
        if (len >= 8) score += 15;
        if (len >= 10) score += 15;
        if (len >= 12) score += 15;
        score += classes * 10;
        if (classes >= 3 && len >= 8) score += 10;
        if (classes <= 1 && len < 10) score -= 10;
        if (score < 0) score = 0;
        if (score > 100) score = 100;

        newPassStrength->setValue(score >= 85 ? 100 : score);

        if (text.isEmpty()) {
            newPassStrengthLabel->setText("Надёжность: —");
            newPassStrengthLabel->setStyleSheet("color:#6B7280;font-weight:700;");
            newPassStrength->setStyleSheet("QProgressBar{border:none;border-radius:3px;background:#E9EEF6;height:6px;}QProgressBar::chunk{background:#9CA3AF;border-radius:3px;}");
        } else if (score < 35) {
            newPassStrengthLabel->setText("Надёжность: Слабый");
            newPassStrengthLabel->setStyleSheet("color:#DC2626;font-weight:700;");
            newPassStrength->setStyleSheet("QProgressBar{border:none;border-radius:3px;background:#FEE2E2;height:6px;}QProgressBar::chunk{background:#EA4335;border-radius:3px;}");
        } else if (score < 60) {
            newPassStrengthLabel->setText("Надёжность: Средний");
            newPassStrengthLabel->setStyleSheet("color:#D97706;font-weight:700;");
            newPassStrength->setStyleSheet("QProgressBar{border:none;border-radius:3px;background:#FEF3C7;height:6px;}QProgressBar::chunk{background:#FBBC04;border-radius:3px;}");
        } else if (score < 85) {
            newPassStrengthLabel->setText("Надёжность: Надёжный");
            newPassStrengthLabel->setStyleSheet("color:#15803D;font-weight:700;");
            newPassStrength->setStyleSheet("QProgressBar{border:none;border-radius:3px;background:#DCFCE7;height:6px;}QProgressBar::chunk{background:#34A853;border-radius:3px;}");
        } else {
            newPassStrengthLabel->setText("Надёжность: Отличный");
            newPassStrengthLabel->setStyleSheet("color:#166534;font-weight:800;");
            newPassStrength->setStyleSheet("QProgressBar{border:none;border-radius:3px;background:#D1FAE5;height:6px;}QProgressBar::chunk{background:#0F9D58;border-radius:3px;}");
        }
    });

    connect(newPass2Edit, &QLineEdit::textChanged, this, [this](const QString &) {
        newPassError->clear();
    });

    connect(btnChangePass, &QPushButton::clicked, this, [this]() {
        newPassError->clear();
        QString p1 = newPass1Edit->text();
        QString p2 = newPass2Edit->text();

        if (!passRx.exactMatch(p1)) {
            newPassError->setText("Пароль: только английские буквы, цифры и спецсимволы");
            return;
        }

        if (p1.length() < 8) {
            newPassError->setText("Пароль должен быть минимум 8 символов");
            return;
        }

        if (p1 != p2) {
            newPassError->setText("Пароли не совпадают");
            return;
        }

        QString error;
        if (!setNewPassword(recoveryUsername_, p1, error)) {
            newPassError->setText(error);
            return;
        }

        QString newRecoveryKey;
        if (!regenerateRecoveryKey(recoveryUsername_, newRecoveryKey, error)) {
            newPassError->setText("Пароль изменён, но не удалось создать новый ключ: " + error);
        }

        UserInfo u;
        u.username = recoveryUsername_;
        u.isActive = true;

        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare("SELECT id, role FROM users WHERE username = :u");
            q.bindValue(":u", recoveryUsername_);
            if (q.exec() && q.next()) {
                u.id = q.value(0).toInt();
                u.role = q.value(1).toString();
            }
        }

        enableRememberMe(recoveryUsername_);
        user_ = u;
        logAction(recoveryUsername_, "password_changed_via_recovery", "Пароль изменён через ключ восстановления");

        if (!newRecoveryKey.isEmpty()) {
            showRecoveryKeyDialog(recoveryUsername_, newRecoveryKey);
        }

        accept();
    });

    connect(loginEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        loginError->clear();
    });
    connect(passEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        loginError->clear();
    });
    connect(regLoginEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        regError->clear();
    });
    connect(regPass1Edit, &QLineEdit::textChanged, this, [this](const QString &) {
        regError->clear();
    });
    connect(regPass2Edit, &QLineEdit::textChanged, this, [this](const QString &) {
        regError->clear();
    });

    connect(btnBack, &QPushButton::clicked, this, [=]() {
        regError->clear();
        stack->setCurrentIndex(0);
    });

    connect(btnRegOk, &QPushButton::clicked, this, [=]() {
        regError->clear();
        QString login = regLoginEdit->text().trimmed();
        QString p1 = regPass1Edit->text();
        QString p2 = regPass2Edit->text();

        if (!loginRx.exactMatch(login)) {
            regError->setText("Логин: только латиница и цифры");
            return;
        }

        if (!passRx.exactMatch(p1)) {
            regError->setText("Пароль: только английские буквы, цифры и спецсимволы");
            return;
        }

        if (p1 != p2) {
            regError->setText("Пароли не совпадают");
            return;
        }

        QString role = regRoleCombo->currentData().toString();

        if (role == "admin" && hasAnyAdmin()) {
            QString adminKey = regAdminKeyEdit->text().trimmed();
            QString keyError;
            if (!verifyAdminInviteKey(adminKey, keyError)) {
                regError->setText(keyError);
                return;
            }
        }
        if (role == "tech" && hasAnyTech()) {
            QString techKey = regTechKeyEdit->text().trimmed();
            QString keyError;
            if (!verifyTechInviteKey(techKey, keyError)) {
                regError->setText(keyError);
                return;
            }
        }

        QString recoveryKey;
        QString error;
        if (!registerUser(login, p1, role, recoveryKey, error)) {
            regError->setText(error);
            return;
        }

        regLoginEdit->clear();
        regPass1Edit->clear();
        regPass2Edit->clear();
        if (regAdminKeyEdit) regAdminKeyEdit->clear();
        if (regTechKeyEdit) regTechKeyEdit->clear();
        regRoleCombo->setCurrentIndex(0);
        regError->clear();
        passStrength->setValue(0);
        passStrengthLabel->setText("Надёжность: —");
        passStrengthLabel->setStyleSheet("color:#6B7280;font-weight:700;");
        passStrength->setStyleSheet(
            "QProgressBar{border:none;border-radius:3px;background:#E9EEF6;text-align:center;color:transparent;height:6px;}"
            "QProgressBar::chunk{background:#9CA3AF;border-radius:3px;}"
        );

        showRecoveryKeyDialog(login, recoveryKey);

        stack->setCurrentIndex(0);
    });
}

void LoginDialog::onLoginClicked()
{
    loginError->clear();
    QString error;
    UserInfo u;

    if (!loginRx.exactMatch(loginEdit->text())) {
        loginError->setText("Логин должен быть на английском и без спецсимволов");
        return;
    }

    if (!loginUser(loginEdit->text(), passEdit->text(), u, error)) {
        loginError->setText(error);
        return;
    }

    enableRememberMe(u.username);
    user_ = u;
    accept();
}

void LoginDialog::onRegisterClicked()
{
    stack->setCurrentIndex(1);
}

void LoginDialog::updatePasswordStrength(const QString &text)
{
    int score = 0;
    const int len = text.length();

    const bool hasLower = text.contains(QRegExp("[a-z]"));
    const bool hasUpper = text.contains(QRegExp("[A-Z]"));
    const bool hasDigit = text.contains(QRegExp("[0-9]"));
    const bool hasSpecial = text.contains(QRegExp("[^A-Za-z0-9]"));

    int classes = 0;
    if (hasLower)   ++classes;
    if (hasUpper)   ++classes;
    if (hasDigit)   ++classes;
    if (hasSpecial) ++classes;

    if (len >= 6)  score += 20;
    if (len >= 8)  score += 15;
    if (len >= 10) score += 15;
    if (len >= 12) score += 15;

    score += classes * 10; // до 40 баллов за разнообразие

    if (classes >= 3 && len >= 8)
        score += 10; // бонус за действительно смешанный пароль

    if (classes <= 1 && len < 10)
        score -= 10; // штраф за простой короткий пароль

    if (score < 0) score = 0;
    if (score > 100) score = 100;

    const int uiScore = (score >= 85) ? 100 : score;
    passStrength->setValue(uiScore);

    if (text.isEmpty()) {
        passStrengthLabel->setText("Надёжность: —");
        passStrengthLabel->setStyleSheet("color:#6B7280;font-weight:700;");
        passStrength->setStyleSheet(
            "QProgressBar{border:none;border-radius:3px;background:#E9EEF6;text-align:center;color:transparent;height:6px;}"
            "QProgressBar::chunk{background:#9CA3AF;border-radius:3px;}"
        );
    } else if (score < 35) {
        passStrengthLabel->setText("Надёжность: Слабый");
        passStrengthLabel->setStyleSheet("color:#DC2626;font-weight:700;");
        passStrength->setStyleSheet(
            "QProgressBar{border:none;border-radius:3px;background:#FEE2E2;text-align:center;color:transparent;height:6px;}"
            "QProgressBar::chunk{background:#EA4335;border-radius:3px;}"
        );
    } else if (score < 60) {
        passStrengthLabel->setText("Надёжность: Средний");
        passStrengthLabel->setStyleSheet("color:#D97706;font-weight:700;");
        passStrength->setStyleSheet(
            "QProgressBar{border:none;border-radius:3px;background:#FEF3C7;text-align:center;color:transparent;height:6px;}"
            "QProgressBar::chunk{background:#FBBC04;border-radius:3px;}"
        );
    } else if (score < 85) {
        passStrengthLabel->setText("Надёжность: Надёжный");
        passStrengthLabel->setStyleSheet("color:#15803D;font-weight:700;");
        passStrength->setStyleSheet(
            "QProgressBar{border:none;border-radius:3px;background:#DCFCE7;text-align:center;color:transparent;height:6px;}"
            "QProgressBar::chunk{background:#34A853;border-radius:3px;}"
        );
    } else {
        passStrengthLabel->setText("Надёжность: Отличный");
        passStrengthLabel->setStyleSheet("color:#166534;font-weight:800;");
        passStrength->setStyleSheet(
            "QProgressBar{border:none;border-radius:3px;background:#D1FAE5;text-align:center;color:transparent;height:6px;}"
            "QProgressBar::chunk{background:#0F9D58;border-radius:3px;}"
        );
    }
}

void LoginDialog::onRoleChanged(int index)
{
    QString role = regRoleCombo->itemData(index).toString();
    bool needAdminKey = (role == "admin" && hasAnyAdmin());
    bool needTechKey = (role == "tech" && hasAnyTech());
    adminKeyRow->setVisible(needAdminKey);
    techKeyRow->setVisible(needTechKey);
}

void LoginDialog::showRecoveryKeyDialog(const QString &username, const QString &recoveryKey)
{
    QDialog dlg(this);
    dlg.setWindowTitle("Ключ восстановления");
    dlg.setModal(true);
    dlg.setFixedSize(460, 370);
    dlg.setStyleSheet(
        "QDialog { background: #F5F7FB; }"
        "QFrame#keyCard { background: white; border: 1px solid #E4E8F0; border-radius: 14px; }"
        "QLabel#title { font-family: Inter; font-size: 20px; font-weight: 900; color: #0F172A; }"
        "QLabel#subtitle { font-family: Inter; font-size: 13px; font-weight: 500; color: #5B6475; }"
        "QLabel#keyLabel { font-family: Consolas, monospace; font-size: 15px; font-weight: 700; color: #0F172A; background: #F1F5F9; padding: 14px 16px; border-radius: 8px; }"
        "QPushButton { font-family: Inter; font-size: 14px; font-weight: 700; border-radius: 8px; padding: 10px 16px; }"
        "QPushButton#copyBtn { background: #0F00DB; color: white; border: none; }"
        "QPushButton#copyBtn:hover { background: #1A4ACD; }"
        "QPushButton#saveBtn { background: #EDF1FF; color: #182B7A; border: 1px solid #CFD8F4; }"
        "QPushButton#saveBtn:hover { background: #E3E9FB; }"
        "QPushButton#okBtn { background: #10B981; color: white; border: none; }"
        "QPushButton#okBtn:hover { background: #059669; }"
    );

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(0);

    QFrame *keyCard = new QFrame(&dlg);
    keyCard->setObjectName("keyCard");
    QVBoxLayout *cardLayout = new QVBoxLayout(keyCard);
    cardLayout->setContentsMargins(20, 16, 20, 16);
    cardLayout->setSpacing(10);

    QLabel *title = new QLabel("Сохраните ключ восстановления!", keyCard);
    title->setObjectName("title");
    cardLayout->addWidget(title);

    QLabel *subtitle = new QLabel(QString("Аккаунт: %1\nЭтот ключ потребуется для восстановления доступа.").arg(username), keyCard);
    subtitle->setObjectName("subtitle");
    subtitle->setWordWrap(true);
    cardLayout->addWidget(subtitle);

    cardLayout->addSpacing(6);

    QLabel *keyLabel = new QLabel(recoveryKey, keyCard);
    keyLabel->setObjectName("keyLabel");
    keyLabel->setAlignment(Qt::AlignCenter);
    keyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cardLayout->addWidget(keyLabel);

    cardLayout->addSpacing(10);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    QPushButton *copyBtn = new QPushButton("Копировать", keyCard);
    copyBtn->setObjectName("copyBtn");
    connect(copyBtn, &QPushButton::clicked, this, [recoveryKey, copyBtn]() {
        QApplication::clipboard()->setText(recoveryKey);
        copyBtn->setText("Скопировано!");
    });
    btnRow->addWidget(copyBtn);

    QPushButton *saveBtn = new QPushButton("Сохранить в файл", keyCard);
    saveBtn->setObjectName("saveBtn");
    connect(saveBtn, &QPushButton::clicked, this, [&dlg, username, recoveryKey]() {
        QString path = QFileDialog::getSaveFileName(&dlg, "Сохранить ключ", QString("recovery_key_%1.txt").arg(username), "Text files (*.txt)");
        if (!path.isEmpty()) {
            QFile f(path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&f);
                out << "=== Ключ восстановления ===" << "\n";
                out << "Аккаунт: " << username << "\n";
                out << "Ключ: " << recoveryKey << "\n";
                out << "Дата: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
                f.close();
            }
        }
    });
    btnRow->addWidget(saveBtn);

    cardLayout->addLayout(btnRow);

    QPushButton *okBtn = new QPushButton("Готово", keyCard);
    okBtn->setObjectName("okBtn");
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    cardLayout->addWidget(okBtn);

    root->addWidget(keyCard);
    dlg.exec();
}

void LoginDialog::closeEvent(QCloseEvent *event)
{
    if (stack->currentIndex() == 3) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void LoginDialog::keyPressEvent(QKeyEvent *event)
{
    if (stack->currentIndex() == 3 && event->key() == Qt::Key_Escape) {
        event->ignore();
        return;
    }
    QDialog::keyPressEvent(event);
}
