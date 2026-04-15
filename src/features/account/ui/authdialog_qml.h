#ifndef AUTHDIALOG_QML_H
#define AUTHDIALOG_QML_H

#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include "db_users.h"

class AuthDialogQml : public QObject
{
    Q_OBJECT
public:
    explicit AuthDialogQml(QObject *parent = nullptr);
    ~AuthDialogQml();

    // Показать диалог авторизации (блокирующий, как QDialog::exec)
    int exec();

    // Получить пользователя после успешной авторизации
    UserInfo user() const { return m_user; }
    bool needsPasswordChange() const { return m_needsPasswordChange; }

signals:
    void loginRequested(const QString &login, const QString &password);
    void registerRequested(const QString &login, const QString &password,
                          const QString &confirmPassword, const QString &role,
                          const QString &adminKey, const QString &techKey);
    void recoveryRequested(const QString &recoveryKey);
    void recoveryFromFileRequested();

public slots:
    // Слоты, вызываемые из QML
    void onLogin(const QString &login, const QString &password);
    void onRegister(const QString &login, const QString &password,
                   const QString &confirmPassword, const QString &role,
                   const QString &adminKey, const QString &techKey);
    void onRecovery(const QString &recoveryKey);
    void onRecoveryFromFile();
    void onAppClosed();

    // Установка свойств в QML
    void setLoginError(const QString &error);
    void setRegisterError(const QString &error);
    void setRecoveryError(const QString &error);
    void showRecoveryKeyDialog(const QString &username, const QString &key);

private:
    bool validateLogin(const QString &login, QString &error);
    bool validatePassword(const QString &password, QString &error);
    void accept();
    void reject();

    QQmlApplicationEngine *m_engine = nullptr;
    QQuickWindow *m_window = nullptr;

    UserInfo m_user;
    bool m_needsPasswordChange = false;
    bool m_accepted = false;
    int m_result = 0; // 0 = rejected, 1 = accepted

    // Валидаторы
    QRegExp m_loginRx;
    QRegExp m_passRx;
};

#endif // AUTHDIALOG_QML_H
