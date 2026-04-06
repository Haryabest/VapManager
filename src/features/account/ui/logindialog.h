#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QStackedWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QComboBox>
#include "db_users.h"

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);
    UserInfo user() const { return user_; }
    bool needsPasswordChange() const { return needsPasswordChange_; }

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void updatePasswordStrength(const QString &text);
    void onRoleChanged(int index);

private:
    void showRecoveryKeyDialog(const QString &username, const QString &recoveryKey);

    UserInfo user_;
    bool needsPasswordChange_ = false;
    QString recoveryUsername_;

    // login page
    QLineEdit *loginEdit;
    QLineEdit *passEdit;
    QLabel *loginError;

    // registration page
    QLineEdit *regLoginEdit;
    QLineEdit *regPass1Edit;
    QLineEdit *regPass2Edit;
    QComboBox *regRoleCombo = nullptr;
    QWidget *adminKeyRow = nullptr;
    QLineEdit *regAdminKeyEdit = nullptr;
    QWidget *techKeyRow = nullptr;
    QLineEdit *regTechKeyEdit = nullptr;
    QLabel *regError;

    // recovery page
    QLineEdit *recoveryKeyEdit = nullptr;
    QLabel *recoveryError = nullptr;

    // password change page
    QLineEdit *newPass1Edit = nullptr;
    QLineEdit *newPass2Edit = nullptr;
    QLabel *newPassError = nullptr;
    QProgressBar *newPassStrength = nullptr;
    QLabel *newPassStrengthLabel = nullptr;

    QProgressBar *passStrength;
    QLabel *passStrengthLabel;

    QStackedWidget *stack;

    QRegExp loginRx;
    QRegExp passRx;
};

#endif // LOGINDIALOG_H
