#include "db_users.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QPixmap>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "internal/db_users_internal_state.h"

using namespace DbUsersInternal;

bool registerUser(const QString &username,
                  const QString &password,
                  const QString &role,
                  QString &outRecoveryKey,
                  QString &error)
{
    QString u = username.trimmed();
    if (u.size() < 3) {
        error = "Логин слишком короткий";
        return false;
    }
    if (password.size() < 8) {
        error = "Пароль минимум 8 символов";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        error = "БД недоступна";
        return false;
    }

    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM users WHERE username = :u");
    q.bindValue(":u", u);
    q.exec();
    q.next();
    if (q.value(0).toInt() > 0) {
        error = "Пользователь уже существует";
        return false;
    }

    QString hash = hashPassword(password);
    QString recoveryKey = generateRecoveryKeyInternal();

    q.prepare("INSERT INTO users (username, password_hash, role, permanent_recovery_key) VALUES (:u, :p, :r, :rk)");
    q.bindValue(":u", u);
    q.bindValue(":p", hash);
    q.bindValue(":r", role);
    q.bindValue(":rk", recoveryKey);

    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }

    outRecoveryKey = recoveryKey;
    invalidateUserCaches();
    logAction(u, "register", QString("Пользователь зарегистрирован с ролью %1").arg(role));
    return true;
}

bool loginUser(const QString &username,
               const QString &password,
               UserInfo &outUser,
               QString &error)
{
    QString u = username.trimmed();
    if (u.isEmpty()) {
        error = "Введите логин";
        return false;
    }
    if (password.isEmpty()) {
        error = "Введите пароль";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        error = "БД недоступна";
        return false;
    }

    QSqlQuery q(db);
    q.prepare(R"(
        SELECT id, password_hash, role, is_active, last_login,
               full_name, employee_id, position, department,
               mobile, ext_phone, email, telegram, avatar,
               permanent_recovery_key
        FROM users
        WHERE username = :u
    )");
    q.bindValue(":u", u);

    if (!q.exec() || !q.next()) {
        error = "Неверный логин или пароль";
        logAction(u, "login_failed", "Пользователь не найден");
        return false;
    }

    int id = q.value(0).toInt();
    QString dbHash = q.value(1).toString();
    QString roleValue = q.value(2).toString();
    bool active = q.value(3).toBool();
    QDateTime lastLogin = q.value(4).toDateTime();

    if (!active) {
        error = "Пользователь заблокирован";
        return false;
    }

    QString calcHash = hashPassword(password);
    if (calcHash != dbHash) {
        error = "Неверный логин или пароль";
        logAction(u, "login_failed", "Неверный пароль");
        return false;
    }

    bool expired = lastLogin.isValid() && lastLogin.daysTo(QDateTime::currentDateTime()) > 10;

    QSqlQuery upd(db);
    upd.prepare("UPDATE users SET last_login = NOW() WHERE id = :id");
    upd.bindValue(":id", id);
    upd.exec();

    outUser.id = id;
    outUser.username = u;
    outUser.role = roleValue;
    outUser.isActive = true;
    outUser.expired = expired;

    outUser.fullName = q.value(5).toString();
    outUser.employeeId = q.value(6).toString();
    outUser.position = q.value(7).toString();
    outUser.department = q.value(8).toString();
    outUser.mobile = q.value(9).toString();
    outUser.extPhone = q.value(10).toString();
    outUser.email = q.value(11).toString();
    outUser.telegram = q.value(12).toString();
    outUser.permanentRecoveryKey = q.value(14).toString();

    QByteArray avatarBytes = q.value(13).toByteArray();
    if (!avatarBytes.isEmpty()) {
        QPixmap pm;
        pm.loadFromData(avatarBytes);
        outUser.avatar = pm;
    } else {
        outUser.avatar = QPixmap();
    }

    logAction(u, "login_success", "Успешный вход");
    return true;
}

bool enableRememberMe(const QString &username)
{
    const QString rememberToken = randomToken();
    const QString sessionToken = randomToken();

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    QSqlQuery q(db);
    q.prepare("UPDATE users SET remember_token = :rt, active_session_token = :st WHERE username = :u");
    q.bindValue(":rt", rememberToken);
    q.bindValue(":st", sessionToken);
    q.bindValue(":u", username);
    if (!q.exec())
        return false;

    QDir().mkpath("config");
    const bool rememberSaved = writeTextFile(rememberTokenFilePath(), rememberToken);
    const bool sessionSaved = writeTextFile(sessionTokenFilePath(), sessionToken);
    return rememberSaved && sessionSaved;
}

bool tryAutoLogin(UserInfo &outUser)
{
    QString token = readTextFileTrimmed(rememberTokenFilePath());
    if (token.isEmpty())
        return false;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT id, username, role, is_active, last_login,
               full_name, employee_id, position, department,
               mobile, ext_phone, email, telegram, avatar,
               permanent_recovery_key
        FROM users
        WHERE remember_token = :t
    )");
    q.bindValue(":t", token);

    if (!q.exec() || !q.next())
        return false;

    outUser.id = q.value(0).toInt();
    outUser.username = q.value(1).toString();
    outUser.role = q.value(2).toString();
    outUser.isActive = q.value(3).toBool();

    QDateTime lastLogin = q.value(4).toDateTime();
    outUser.expired = lastLogin.isValid() && lastLogin.daysTo(QDateTime::currentDateTime()) > 10;

    outUser.fullName = q.value(5).toString();
    outUser.employeeId = q.value(6).toString();
    outUser.position = q.value(7).toString();
    outUser.department = q.value(8).toString();
    outUser.mobile = q.value(9).toString();
    outUser.extPhone = q.value(10).toString();
    outUser.email = q.value(11).toString();
    outUser.telegram = q.value(12).toString();
    outUser.permanentRecoveryKey = q.value(14).toString();

    QByteArray avatarBytes = q.value(13).toByteArray();
    if (!avatarBytes.isEmpty()) {
        QPixmap pm;
        pm.loadFromData(avatarBytes);
        outUser.avatar = pm;
    } else {
        outUser.avatar = QPixmap();
    }

    return true;
}

bool isCurrentSessionValid(const QString &username)
{
    const QString u = username.trimmed();
    if (u.isEmpty())
        return false;

    const QString sessionToken = readTextFileTrimmed(sessionTokenFilePath());
    if (sessionToken.isEmpty())
        return false;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare("SELECT 1 FROM users WHERE username = :u AND active_session_token = :st AND is_active = 1 LIMIT 1");
    q.bindValue(":u", u);
    q.bindValue(":st", sessionToken);
    if (!q.exec())
        return false;

    return q.next();
}

void logoutUser()
{
    QFile::remove(rememberTokenFilePath());
    QFile::remove(sessionTokenFilePath());
}

bool verifyPermanentRecoveryKey(const QString &key, QString &outUsername, QString &error)
{
    if (key.trimmed().isEmpty()) {
        error = "Введите ключ восстановления";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        error = "БД недоступна";
        return false;
    }

    QSqlQuery q(db);
    q.prepare("SELECT username FROM users WHERE permanent_recovery_key = :k");
    q.bindValue(":k", key.trimmed().toUpper());

    if (!q.exec() || !q.next()) {
        error = "Неверный ключ восстановления";
        return false;
    }

    outUsername = q.value(0).toString();
    return true;
}

bool setNewPassword(const QString &username, const QString &newPassword, QString &error)
{
    if (newPassword.size() < 8) {
        error = "Пароль минимум 8 символов";
        return false;
    }

    QString hash = hashPassword(newPassword);

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    QSqlQuery q(db);
    q.prepare("UPDATE users SET password_hash = :p WHERE username = :u");
    q.bindValue(":p", hash);
    q.bindValue(":u", username);

    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }

    logAction(username, "password_reset", "Пароль успешно изменён");
    return true;
}

bool regenerateRecoveryKey(const QString &username, QString &outNewKey, QString &error)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        error = "БД недоступна";
        return false;
    }

    QString newKey = generateRecoveryKeyInternal();

    QSqlQuery q(db);
    q.prepare("UPDATE users SET permanent_recovery_key = :k WHERE username = :u");
    q.bindValue(":k", newKey);
    q.bindValue(":u", username);

    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }

    outNewKey = newKey;
    logAction(username, "recovery_key_regenerated", "Ключ восстановления пересоздан");
    return true;
}
