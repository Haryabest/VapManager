#include "registerpage_controller.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QRegularExpression>

RegisterPageController::RegisterPageController(QObject *parent)
    : QObject(parent)
{
    m_loginRx = QRegExp("^[A-Za-z0-9]+$");
    m_passRx = QRegExp("^[\\x21-\\x7E]+$");
}

RegisterPageController::~RegisterPageController()
{
}

void RegisterPageController::show(QQuickView *view)
{
    m_view = view;
    m_view->setSource(QUrl("qrc:/qml/pages/RegisterPage.qml"));

    if (m_view->rootObject()) {
        setRootItem(m_view->rootObject());
    }
}

void RegisterPageController::setRootItem(QQuickItem *item)
{
    m_rootItem = item;
    if (!m_rootItem)
        return;

    // Подключение сигналов QML к C++ слотам
    connect(m_rootItem, SIGNAL(registerClicked(QString,QString,QString,QString,QString,QString)),
            this, SLOT(onRegisterClicked(QString,QString,QString,QString,QString,QString)));

    connect(m_rootItem, SIGNAL(backClicked()),
            this, SLOT(onBackClicked()));

    connect(m_rootItem, SIGNAL(password1Edited(QString)),
            this, SLOT(onPassword1Changed(QString)));

    connect(m_rootItem, SIGNAL(loginEdited(QString)),
            this, SLOT(onLoginTextChanged(QString)));

    connect(m_rootItem, SIGNAL(password2Edited(QString)),
            this, SLOT(onPassword2TextChanged(QString)));

    connect(m_rootItem, SIGNAL(roleIndexChanged(int)),
            this, SLOT(onRoleChanged(int)));
}

void RegisterPageController::onRegisterClicked(const QString &login, const QString &password,
                                               const QString &confirmPassword, const QString &role,
                                               const QString &adminKey, const QString &techKey)
{
    QString error;

    // Валидация логина
    if (!validateLogin(login, error)) {
        if (m_rootItem) {
            m_rootItem->setProperty("errorMessage", error);
        }
        return;
    }

    // Валидация пароля
    if (!validatePassword(password, error)) {
        if (m_rootItem) {
            m_rootItem->setProperty("errorMessage", error);
        }
        return;
    }

    // Проверка совпадения паролей
    if (password != confirmPassword) {
        if (m_rootItem) {
            m_rootItem->setProperty("errorMessage", "Пароли не совпадают");
        }
        return;
    }

    // Проверка ключей для admin/tech
    if (role == "admin" && hasAnyAdmin()) {
        QString keyError;
        if (!verifyAdminInviteKey(adminKey, keyError)) {
            if (m_rootItem) {
                m_rootItem->setProperty("errorMessage", keyError);
            }
            return;
        }
    }

    if (role == "tech" && hasAnyTech()) {
        QString keyError;
        if (!verifyTechInviteKey(techKey, keyError)) {
            if (m_rootItem) {
                m_rootItem->setProperty("errorMessage", keyError);
            }
            return;
        }
    }

    // Регистрация пользователя
    QString recoveryKey;
    QString regError;
    if (!registerUser(login, password, role, recoveryKey, regError)) {
        if (m_rootItem) {
            m_rootItem->setProperty("errorMessage", regError);
        }
        return;
    }

    // Успешная регистрация
    clearForm();

    // Показываем ключ восстановления
    emit requestShowRecoveryKey(login, recoveryKey);
    emit requestGoBack();
}

void RegisterPageController::onBackClicked()
{
    clearForm();
    emit requestGoBack();
}

void RegisterPageController::onPassword1Changed(const QString &text)
{
    updatePasswordStrength(text);
}

void RegisterPageController::onLoginTextChanged(const QString &text)
{
    Q_UNUSED(text);
    // Можно очистить ошибку при изменении
    if (m_rootItem) {
        m_rootItem->setProperty("errorMessage", "");
    }
}

void RegisterPageController::onPassword2TextChanged(const QString &text)
{
    Q_UNUSED(text);
    // Можно очистить ошибку при изменении
    if (m_rootItem) {
        m_rootItem->setProperty("errorMessage", "");
    }
}

void RegisterPageController::onRoleChanged(int index)
{
    Q_UNUSED(index);
    // Логика показа полей для adminKey/techKey
    if (!m_rootItem) return;

    // TODO: Получить данные из combo box и показать нужные поля
    // m_rootItem->setProperty("adminKeyVisible", needAdminKey);
    // m_rootItem->setProperty("techKeyVisible", needTechKey);
}

void RegisterPageController::updatePasswordStrength(const QString &text)
{
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

    // Обновляем UI
    if (!m_rootItem) return;

    m_rootItem->setProperty("passwordStrength", score);

    QString strengthText;
    QString color;

    if (text.isEmpty()) {
        strengthText = "Надёжность: —";
        color = "#6B7280";
    } else if (score < 35) {
        strengthText = "Надёжность: Слабый";
        color = "#DC2626";
    } else if (score < 60) {
        strengthText = "Надёжность: Средний";
        color = "#D97706";
    } else if (score < 85) {
        strengthText = "Надёжность: Надёжный";
        color = "#15803D";
    } else {
        strengthText = "Надёжность: Отличный";
        color = "#166534";
    }

    m_rootItem->setProperty("passwordStrengthText", strengthText);
    m_rootItem->setProperty("passwordStrengthColor", color);
}

bool RegisterPageController::validateLogin(const QString &login, QString &error)
{
    if (login.trimmed().isEmpty()) {
        error = "Введите логин";
        return false;
    }

    if (!m_loginRx.exactMatch(login)) {
        error = "Логин: только латиница и цифры";
        return false;
    }

    return true;
}

bool RegisterPageController::validatePassword(const QString &password, QString &error)
{
    if (password.isEmpty()) {
        error = "Введите пароль";
        return false;
    }

    if (!m_passRx.exactMatch(password)) {
        error = "Пароль: только английские буквы, цифры и спецсимволы";
        return false;
    }

    if (password.length() < 8) {
        error = "Пароль должен быть минимум 8 символов";
        return false;
    }

    return true;
}

void RegisterPageController::clearForm()
{
    if (!m_rootItem) return;

    m_rootItem->setProperty("loginText", "");
    m_rootItem->setProperty("password1Text", "");
    m_rootItem->setProperty("password2Text", "");
    m_rootItem->setProperty("errorMessage", "");
    m_rootItem->setProperty("passwordStrength", 0);
    m_rootItem->setProperty("passwordStrengthText", "Надёжность: —");
    m_rootItem->setProperty("passwordStrengthColor", "#6B7280");
}
