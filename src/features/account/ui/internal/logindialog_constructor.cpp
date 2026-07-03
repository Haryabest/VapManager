#include "../logindialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

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
        "QLineEdit { background: #FFFFFF; border: 1px solid #D3DDF0; border-radius: 11px; padding: 11px 12px; font-family: Inter; font-size: 14px; color: #0F172A; }"
        "QLineEdit[passwordField=\"true\"] { padding-right: 42px; }"
        "QLineEdit:focus { border: 1px solid #335CFF; background: #FFFFFF; }"
        "QComboBox { background: #FFFFFF; border: 1px solid #D3DDF0; border-radius: 11px; padding: 11px 12px; font-family: Inter; font-size: 14px; color: #0F172A; }"
        "QComboBox:focus { border: 1px solid #335CFF; background: #FFFFFF; }"
        "QComboBox::drop-down { border: none; width: 30px; }"
        "QComboBox::down-arrow { image: none; border-left: 5px solid transparent; border-right: 5px solid transparent; border-top: 6px solid #64748B; }"
        "QPushButton { font-family: Inter; font-size: 14px; font-weight: 800; border-radius: 11px; padding: 11px 14px; border: 1px solid transparent; }"
        "QPushButton#primaryBtn { background: #1F46FF; color: white; }"
        "QPushButton#primaryBtn:hover { background: #143AF3; }"
        "QPushButton#secondaryBtn { background: #EBF1FF; color: #1B3A9A; border-color: #C9D7FB; }"
        "QPushButton#secondaryBtn:hover { background: #DFE8FF; }"
        "QPushButton#ghostBtn { background: #FFFFFF; color: #52607A; border-color: #D6E0F0; }"
        "QPushButton#ghostBtn:hover { background: #F3F7FF; }"
        "QProgressBar { border: none; border-radius: 3px; background: #E9EEF6; text-align: center; color: transparent; height: 6px; }"
        "QProgressBar::chunk { border-radius: 3px; background: #9CA3AF; }"
    );

    loginRx = QRegExp("^[A-Za-z0-9]+$");
    passRx = QRegExp("^[\\x21-\\x7E]+$");

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
    stack->addWidget(buildLoginPage());
    stack->addWidget(buildRegisterPage());
    stack->addWidget(buildRecoveryPage());
    stack->addWidget(buildPasswordChangePage());

    cardLayout->addWidget(stack);
    main->addStretch();
    main->addWidget(authCard, 0, Qt::AlignHCenter);
    main->addStretch();

    wireSignals();
}
