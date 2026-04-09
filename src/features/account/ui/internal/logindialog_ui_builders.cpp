#include "../logindialog.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QProgressBar>
#include <QRegExpValidator>
#include <QVBoxLayout>

namespace {

QIcon makePasswordEyeIcon(bool crossed, int size = 18)
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

} // namespace

void LoginDialog::setupPasswordField(QLineEdit *edit)
{
    if (!edit)
        return;

    static const QIcon iconShow = makePasswordEyeIcon(false, 18);
    static const QIcon iconHide = makePasswordEyeIcon(true, 18);

    edit->setEchoMode(QLineEdit::Password);
    edit->setProperty("passwordField", true);

    QAction *toggleAction = new QAction(iconShow, QString(), edit);
    toggleAction->setCheckable(true);
    toggleAction->setToolTip(QObject::tr("Показать пароль"));
    edit->addAction(toggleAction, QLineEdit::TrailingPosition);

    QObject::connect(toggleAction, &QAction::toggled, edit, [edit, toggleAction](bool shown) {
        static const QIcon iconShowInner = makePasswordEyeIcon(false, 18);
        static const QIcon iconHideInner = makePasswordEyeIcon(true, 18);
        edit->setEchoMode(shown ? QLineEdit::Normal : QLineEdit::Password);
        toggleAction->setIcon(shown ? iconHideInner : iconShowInner);
        toggleAction->setToolTip(shown ? QObject::tr("Скрыть пароль") : QObject::tr("Показать пароль"));
    });
}

QWidget *LoginDialog::buildLoginPage()
{
    auto *loginValidator = new QRegExpValidator(loginRx, this);
    auto *passValidator = new QRegExpValidator(passRx, this);

    QWidget *loginPage = new QWidget(stack);
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

    btnLogin_ = new QPushButton("Войти", loginPage);
    btnLogin_->setObjectName("primaryBtn");
    btnLogin_->setMinimumHeight(44);
    loginForm->addWidget(btnLogin_);

    v->addWidget(loginFormPanel);

    QLabel *loginDivider = new QLabel("ДРУГИЕ ДЕЙСТВИЯ", loginPage);
    loginDivider->setObjectName("dividerLabel");
    loginDivider->setAlignment(Qt::AlignCenter);
    v->addWidget(loginDivider);

    QHBoxLayout *loginActionsRow = new QHBoxLayout();
    loginActionsRow->setContentsMargins(0, 0, 0, 0);
    loginActionsRow->setSpacing(10);

    btnReg_ = new QPushButton("Регистрация", loginPage);
    btnReg_->setObjectName("secondaryBtn");
    btnReg_->setMinimumHeight(42);
    loginActionsRow->addWidget(btnReg_, 1);

    btnRecovery_ = new QPushButton("Вход по ключу", loginPage);
    btnRecovery_->setObjectName("ghostBtn");
    btnRecovery_->setMinimumHeight(42);
    loginActionsRow->addWidget(btnRecovery_, 1);

    v->addLayout(loginActionsRow);
    v->addStretch();

    return loginPage;
}

QWidget *LoginDialog::buildRegisterPage()
{
    auto *loginValidator = new QRegExpValidator(loginRx, this);
    auto *passValidator = new QRegExpValidator(passRx, this);

    QWidget *regPage = new QWidget(stack);
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

    QLabel *regLoginLabel = new QLabel("Логин", regPage);
    regLoginLabel->setObjectName("fieldLabel");
    regForm->addWidget(regLoginLabel);

    regLoginEdit = new QLineEdit(regPage);
    regLoginEdit->setPlaceholderText("Логин");
    regLoginEdit->setValidator(loginValidator);
    regLoginEdit->setMinimumHeight(42);
    regForm->addWidget(regLoginEdit);

    QLabel *regPass1Label = new QLabel("Пароль", regPage);
    regPass1Label->setObjectName("fieldLabel");
    regForm->addWidget(regPass1Label);

    regPass1Edit = new QLineEdit(regPage);
    regPass1Edit->setPlaceholderText("Пароль");
    regPass1Edit->setValidator(passValidator);
    regPass1Edit->setMinimumHeight(42);
    setupPasswordField(regPass1Edit);
    regForm->addWidget(regPass1Edit);

    QLabel *regPass2Label = new QLabel("Повторите пароль", regPage);
    regPass2Label->setObjectName("fieldLabel");
    regForm->addWidget(regPass2Label);

    regPass2Edit = new QLineEdit(regPage);
    regPass2Edit->setPlaceholderText("Повторите пароль");
    regPass2Edit->setValidator(passValidator);
    regPass2Edit->setMinimumHeight(42);
    setupPasswordField(regPass2Edit);
    regForm->addWidget(regPass2Edit);

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

    QLabel *regRoleLabel = new QLabel("Роль", regPage);
    regRoleLabel->setObjectName("fieldLabel");
    regForm->addWidget(regRoleLabel);

    regRoleCombo = new QComboBox(regPage);
    regRoleCombo->addItem("Пользователь", "viewer");
    regRoleCombo->addItem("Администратор", "admin");
    regRoleCombo->addItem("Техник", "tech");
    regRoleCombo->setMinimumHeight(42);
    regForm->addWidget(regRoleCombo);

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

    regError = new QLabel(regPage);
    regError->setObjectName("errorLabel");
    regError->setWordWrap(true);
    regForm->addWidget(regError);

    btnRegOk_ = new QPushButton("Создать аккаунт", regPage);
    btnRegOk_->setObjectName("primaryBtn");
    btnRegOk_->setMinimumHeight(44);

    btnBack_ = new QPushButton("Назад", regPage);
    btnBack_->setObjectName("secondaryBtn");
    btnBack_->setMinimumHeight(42);

    QHBoxLayout *regActionsRow = new QHBoxLayout();
    regActionsRow->setContentsMargins(0, 0, 0, 0);
    regActionsRow->setSpacing(10);
    regActionsRow->addWidget(btnBack_, 1);
    regActionsRow->addWidget(btnRegOk_, 2);
    regForm->addLayout(regActionsRow);

    r->addWidget(regFormPanel);
    r->addStretch();

    return regPage;
}

QWidget *LoginDialog::buildRecoveryPage()
{
    QWidget *recoveryPage = new QWidget(stack);
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

    btnRecoveryFromFile_ = new QPushButton("Вставить ключ из файла", recoveryPage);
    btnRecoveryFromFile_->setObjectName("secondaryBtn");
    btnRecoveryFromFile_->setMinimumHeight(40);
    recLayout->addWidget(btnRecoveryFromFile_);

    btnRecoveryOk_ = new QPushButton("Войти", recoveryPage);
    btnRecoveryOk_->setObjectName("primaryBtn");
    btnRecoveryOk_->setMinimumHeight(42);
    recLayout->addWidget(btnRecoveryOk_);

    btnRecoveryBack_ = new QPushButton("Назад", recoveryPage);
    btnRecoveryBack_->setObjectName("secondaryBtn");
    btnRecoveryBack_->setMinimumHeight(40);
    recLayout->addWidget(btnRecoveryBack_);

    return recoveryPage;
}

QWidget *LoginDialog::buildPasswordChangePage()
{
    auto *passValidator = new QRegExpValidator(passRx, this);

    QWidget *passChangePage = new QWidget(stack);
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

    btnChangePass_ = new QPushButton("Сменить пароль", passChangePage);
    btnChangePass_->setObjectName("primaryBtn");
    btnChangePass_->setMinimumHeight(42);
    pcLayout->addWidget(btnChangePass_);

    return passChangePage;
}
