#ifndef REGISTERPAGE_CONTROLLER_H
#define REGISTERPAGE_CONTROLLER_H

#include <QObject>
#include <QQuickView>
#include <QQuickItem>
#include "db_users.h"

class RegisterPageController : public QObject
{
    Q_OBJECT
public:
    explicit RegisterPageController(QObject *parent = nullptr);
    ~RegisterPageController();

    // Показать страницу регистрации
    void show(QQuickView *view);

    // Слот для установки root QML item
    void setRootItem(QQuickItem *item);

signals:
    // Сигналы для навигации и взаимодействия
    void requestGoBack();
    void requestShowRecoveryKey(const QString &username, const QString &recoveryKey);
    void requestLoginSuccess(const UserInfo &user);

public slots:
    // Слоты, вызываемые из QML
    void onRegisterClicked(const QString &login, const QString &password,
                          const QString &confirmPassword, const QString &role,
                          const QString &adminKey, const QString &techKey);
    void onBackClicked();
    void onPassword1Changed(const QString &text);
    void onLoginTextChanged(const QString &text);
    void onPassword2TextChanged(const QString &text);
    void onRoleChanged(int index);

private:
    void updatePasswordStrength(const QString &text);
    bool validateLogin(const QString &login, QString &error);
    bool validatePassword(const QString &password, QString &error);
    void clearForm();

    QQuickView *m_view = nullptr;
    QQuickItem *m_rootItem = nullptr;

    // Валидаторы
    QRegExp m_loginRx;
    QRegExp m_passRx;
};

#endif // REGISTERPAGE_CONTROLLER_H
