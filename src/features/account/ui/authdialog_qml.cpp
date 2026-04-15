#include "authdialog_qml.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QRegularExpression>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QQmlContext>
#include <QThread>

AuthDialogQml::AuthDialogQml(QObject *parent)
    : QObject(parent)
{
    m_loginRx = QRegExp("^[A-Za-z0-9]+$");
    m_passRx = QRegExp("^[\\x21-\\x7E]+$");

    m_engine = new QQmlApplicationEngine(this);

    // Регистрация типа UserInfo для QML (если нужно)
    qRegisterMetaType<UserInfo>("UserInfo");
}

AuthDialogQml::~AuthDialogQml()
{
}

int AuthDialogQml::exec()
{
    // Установка контекстного свойства для QML
    m_engine->rootContext()->setContextProperty("authController", this);

    // Загрузка QML
    m_engine->load(QUrl(QStringLiteral("qrc:/qml/pages/AuthDialog.qml")));

    if (m_engine->rootObjects().isEmpty()) {
        return 0;
    }

    m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().first());
    if (!m_window) {
        return 0;
    }

    m_window->show();

    // Запуск event loop (как QDialog::exec)
    m_result = 0;
    m_accepted = false;

    // Ждём, пока окно не закроется
    while (m_window->isVisible()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QThread::msleep(10);
    }

    return m_accepted ? 1 : 0;
}

void AuthDialogQml::onLogin(const QString &login, const QString &password)
{
    QString error;

    // Валидация
    if (!validateLogin(login, error)) {
        setLoginError(error);
        return;
    }

    if (password.isEmpty()) {
        setLoginError("Введите пароль");
        return;
    }

    // Попытка входа
    UserInfo u;
    QString loginError;
    if (!loginUser(login, password, u, loginError)) {
        setLoginError(loginError);
        return;
    }

    // Успешный вход
    m_user = u;
    m_needsPasswordChange = u.expired;

    // Запомнить пользователя
    enableRememberMe(u.username);

    accept();
}

void AuthDialogQml::onRegister(const QString &login, const QString &password,
                               const QString &confirmPassword, const QString &role,
                               const QString &adminKey, const QString &techKey)
{
    QString error;

    // Валидация логина
    if (!validateLogin(login, error)) {
        setRegisterError(error);
        return;
    }

    // Валидация пароля
    if (!validatePassword(password, error)) {
        setRegisterError(error);
        return;
    }

    // Проверка совпадения паролей
    if (password != confirmPassword) {
        setRegisterError("Пароли не совпадают");
        return;
    }

    // Проверка ключей для admin/tech
    if (role == "admin" && hasAnyAdmin()) {
        QString keyError;
        if (!verifyAdminInviteKey(adminKey, keyError)) {
            setRegisterError(keyError);
            return;
        }
    }

    if (role == "tech" && hasAnyTech()) {
        QString keyError;
        if (!verifyTechInviteKey(techKey, keyError)) {
            setRegisterError(keyError);
            return;
        }
    }

    // Регистрация пользователя
    QString recoveryKey;
    QString regError;
    if (!registerUser(login, password, role, recoveryKey, regError)) {
        setRegisterError(regError);
        return;
    }

    // Показываем ключ восстановления
    showRecoveryKeyDialog(login, recoveryKey);

    // Возврат на страницу входа
    // (QML сам обработает pop)
}

void AuthDialogQml::onRecovery(const QString &recoveryKey)
{
    QString key = recoveryKey.trimmed().toUpper();

    if (key.isEmpty()) {
        setRecoveryError("Введите ключ восстановления");
        return;
    }

    QString username;
    QString error;
    if (!verifyPermanentRecoveryKey(key, username, error)) {
        setRecoveryError(error);
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
        setRecoveryError("Аккаунт заблокирован");
        return;
    }

    enableRememberMe(username);
    logAction(username, "login_by_recovery_key", "Вход выполнен по ключу восстановления");

    m_user = u;
    accept();
}

void AuthDialogQml::onRecoveryFromFile()
{
    QString path = QFileDialog::getOpenFileName(
        nullptr,
        "Выберите файл с ключом",
        QString(),
        "Text files (*.txt);;All files (*.*)"
    );

    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setRecoveryError("Не удалось открыть файл");
        return;
    }

    QString content = QString::fromUtf8(f.readAll());
    f.close();

    QString key;
    QRegularExpression rkRe("RK-[A-Za-z0-9]{4}-[A-Za-z0-9]{4}-[A-Za-z0-9]{4}");
    QRegularExpressionMatch m = rkRe.match(content);
    if (m.hasMatch())
        key = m.captured(0);

    if (key.isEmpty()) {
        QStringList lines = content.split('\n');
        for (const QString &lineRaw : lines) {
            QString line = lineRaw.trimmed();
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
        setRecoveryError("В файле не найден корректный ключ");
        return;
    }

    // Вызываем onRecovery с найденным ключом
    onRecovery(key);
}

void AuthDialogQml::onAppClosed()
{
    reject();
}

void AuthDialogQml::setLoginError(const QString &error)
{
    if (m_window) {
        QMetaObject::invokeMethod(m_window, "setLoginError",
                                  Q_ARG(QVariant, error));
    }
}

void AuthDialogQml::setRegisterError(const QString &error)
{
    if (m_window) {
        QMetaObject::invokeMethod(m_window, "setRegisterError",
                                  Q_ARG(QVariant, error));
    }
}

void AuthDialogQml::setRecoveryError(const QString &error)
{
    if (m_window) {
        QMetaObject::invokeMethod(m_window, "setRecoveryError",
                                  Q_ARG(QVariant, error));
    }
}

void AuthDialogQml::showRecoveryKeyDialog(const QString &username, const QString &key)
{
    QMessageBox::information(
        nullptr,
        "Ключ восстановления",
        QString("Ваш аккаунт создан.\n\n"
                "Логин: %1\n\n"
                "Ключ восстановления (сохраните его!):\n%2\n\n"
                "Без этого ключа вы не сможете восстановить доступ к аккаунту.")
            .arg(username, key)
    );
}

bool AuthDialogQml::validateLogin(const QString &login, QString &error)
{
    if (login.trimmed().isEmpty()) {
        error = "Введите логин";
        return false;
    }

    if (!m_loginRx.exactMatch(login)) {
        error = "Логин должен быть на английском и без спецсимволов";
        return false;
    }

    return true;
}

bool AuthDialogQml::validatePassword(const QString &password, QString &error)
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

void AuthDialogQml::accept()
{
    m_accepted = true;
    m_result = 1;
    if (m_window)
        m_window->close();
}

void AuthDialogQml::reject()
{
    m_accepted = false;
    m_result = 0;
    if (m_window)
        m_window->close();
}
