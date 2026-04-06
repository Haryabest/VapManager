#include "db_users.h"
#include "diag_logger.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDir>
#include <QUuid>
#include <QBuffer>
#include <QStandardPaths>
#include <QHash>


static QString appSalt()
{
    return QStringLiteral("CHANGE_THIS_SALT_123456789");
}

static QString hashPassword(const QString &password)
{
    QByteArray data = (appSalt() + password).toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

static QString randomToken()
{
    QByteArray bytes;
    bytes.append(QString::number(QDateTime::currentMSecsSinceEpoch()));
    bytes.append(QByteArray::number(qrand()));
    QByteArray hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

static QString generateShortKey()
{
    QString uuid = QUuid::createUuid().toString().remove("{").remove("}").remove("-");
    return uuid.left(8).toUpper();
}

static QString generateRecoveryKeyInternal()
{
    QString uuid = QUuid::createUuid().toString().remove("{").remove("}").remove("-");
    return QString("RK-%1-%2-%3")
        .arg(uuid.mid(0, 4).toUpper())
        .arg(uuid.mid(4, 4).toUpper())
        .arg(uuid.mid(8, 4).toUpper());
}

namespace {

const QString kHiddenAutotestUsername = QStringLiteral("__autotest_chat_peer__");
const QString kHiddenAutotestFullName = QStringLiteral("Autotest Chat Peer");

struct CachedUserProfile {
    UserInfo user;
    QDateTime cachedAt;
};

const int kUserCacheTtlMs = 10000;
QHash<QString, QString> s_roleCache;
QHash<QString, CachedUserProfile> s_profileCache;
QHash<QString, QPixmap> s_avatarCache;
QDateTime s_roleCacheAt;
QDateTime s_profileCacheAt;
QDateTime s_avatarCacheAt;
QVector<UserInfo> s_allUsersWithAvatarsCache;
QVector<UserInfo> s_allUsersNoAvatarsCache;
QDateTime s_allUsersWithAvatarsCacheAt;
QDateTime s_allUsersNoAvatarsCacheAt;

bool cacheFresh(const QDateTime &ts)
{
    return ts.isValid() && ts.msecsTo(QDateTime::currentDateTime()) <= kUserCacheTtlMs;
}

void invalidateUserCaches(const QString &username = QString())
{
    s_allUsersWithAvatarsCache.clear();
    s_allUsersNoAvatarsCache.clear();
    s_allUsersWithAvatarsCacheAt = QDateTime();
    s_allUsersNoAvatarsCacheAt = QDateTime();

    if (username.trimmed().isEmpty()) {
        s_roleCache.clear();
        s_profileCache.clear();
        s_avatarCache.clear();
        s_roleCacheAt = QDateTime();
        s_profileCacheAt = QDateTime();
        s_avatarCacheAt = QDateTime();
        return;
    }

    const QString key = username.trimmed();
    s_roleCache.remove(key);
    s_profileCache.remove(key);
    s_avatarCache.remove(key);
}

}

QString hiddenAutotestUsername()
{
    return kHiddenAutotestUsername;
}

bool isHiddenAutotestUser(const QString &username)
{
    return username.trimmed().compare(kHiddenAutotestUsername, Qt::CaseInsensitive) == 0;
}

bool ensureAutotestChatUser(QString *outUsername, QString *outError)
{
    if (outUsername)
        *outUsername = kHiddenAutotestUsername;
    if (outError)
        outError->clear();

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        if (outError)
            *outError = QStringLiteral("БД недоступна");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO users (
            username, password_hash, role, permanent_recovery_key,
            full_name, position, department, is_active, last_login
        )
        VALUES (
            :u, :p, 'viewer', :rk,
            :full_name, :position, :department, 1, NOW()
        )
        ON DUPLICATE KEY UPDATE
            role = VALUES(role),
            full_name = VALUES(full_name),
            position = VALUES(position),
            department = VALUES(department),
            is_active = 1
    )");
    q.bindValue(":u", kHiddenAutotestUsername);
    q.bindValue(":p", hashPassword(QStringLiteral("AUTOTEST_CHAT_PEER_2026")));
    q.bindValue(":rk", QStringLiteral("AUTO-CHAT-HIDDEN"));
    q.bindValue(":full_name", kHiddenAutotestFullName);
    q.bindValue(":position", QStringLiteral("service"));
    q.bindValue(":department", QStringLiteral("autotest"));
    if (!q.exec()) {
        if (outError)
            *outError = q.lastError().text();
        return false;
    }

    invalidateUserCaches(kHiddenAutotestUsername);
    return true;
}

bool initUsersTable()
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return false;

    QSqlQuery q(db);
    bool ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS users (
            id INT AUTO_INCREMENT PRIMARY KEY,
            username VARCHAR(64) NOT NULL UNIQUE,
            password_hash VARCHAR(255) NOT NULL,
            role ENUM('admin','tech','viewer') NOT NULL DEFAULT 'viewer',
            is_active TINYINT(1) NOT NULL DEFAULT 1,
            last_login DATETIME NULL,
            remember_token VARCHAR(128) NULL,
            permanent_recovery_key VARCHAR(32) NULL,
            admin_invite_key VARCHAR(16) NULL,
            admin_invite_key_expire DATETIME NULL,
            tech_invite_key VARCHAR(16) NULL,
            tech_invite_key_expire DATETIME NULL,
            full_name VARCHAR(128) NULL,
            employee_id VARCHAR(16) NULL,
            position VARCHAR(128) NULL,
            department VARCHAR(128) NULL,
            mobile VARCHAR(32) NULL,
            ext_phone VARCHAR(16) NULL,
            email VARCHAR(128) NULL,
            telegram VARCHAR(64) NULL,
            avatar LONGBLOB NULL,
            created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
        )
    )");

    // На случай старой схемы — мягкие ALTER'ы (игнор ошибок)
    if (ok) {
        // В старых дампах users может быть latin1 — из-за этого кириллица в ФИО/должности/подразделении ломается.
        // Мягко конвертируем таблицу (ошибки игнорируются).
        q.exec("ALTER TABLE users CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci");

        q.exec("ALTER TABLE users ADD COLUMN permanent_recovery_key VARCHAR(32) NULL");
        q.exec("ALTER TABLE users ADD COLUMN admin_invite_key VARCHAR(16) NULL");
        q.exec("ALTER TABLE users ADD COLUMN admin_invite_key_expire DATETIME NULL");
        q.exec("ALTER TABLE users ADD COLUMN tech_invite_key VARCHAR(16) NULL");
        q.exec("ALTER TABLE users ADD COLUMN tech_invite_key_expire DATETIME NULL");
        q.exec("ALTER TABLE users ADD COLUMN full_name VARCHAR(128) NULL");
        q.exec("ALTER TABLE users ADD COLUMN employee_id VARCHAR(16) NULL");
        q.exec("ALTER TABLE users ADD COLUMN position VARCHAR(128) NULL");
        q.exec("ALTER TABLE users ADD COLUMN department VARCHAR(128) NULL");
        q.exec("ALTER TABLE users ADD COLUMN mobile VARCHAR(32) NULL");
        q.exec("ALTER TABLE users ADD COLUMN ext_phone VARCHAR(16) NULL");
        q.exec("ALTER TABLE users ADD COLUMN email VARCHAR(128) NULL");
        q.exec("ALTER TABLE users ADD COLUMN telegram VARCHAR(64) NULL");
        q.exec("ALTER TABLE users ADD COLUMN avatar LONGBLOB NULL");
    }

    return ok;
}

QString localLogsDirPath()
{
    QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (docs.trimmed().isEmpty())
        docs = QCoreApplication::applicationDirPath();
    return docs + "/VapManagerLogs";
}

QString localLogFilePath()
{
    return localLogsDirPath() + "/app.log";
}

static const int MAX_LOG_LINES = 100000;

static void trimLogFileIfNeeded()
{
    const QString path = localLogFilePath();
    QFile f(path);
    if (!f.exists())
        return;
    
    qint64 lineCount = 0;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        in.setCodec("UTF-8");
        while (!in.atEnd()) {
            in.readLine();
            ++lineCount;
        }
        f.close();
    }
    
    if (lineCount <= MAX_LOG_LINES)
        return;
    
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    
    QTextStream in(&f);
    in.setCodec("UTF-8");
    QStringList allLines;
    while (!in.atEnd())
        allLines.append(in.readLine());
    f.close();
    
    const int linesToKeep = MAX_LOG_LINES / 2;
    QStringList trimmed = allLines.mid(allLines.size() - linesToKeep);
    
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&f);
        out.setCodec("UTF-8");
        for (const QString &line : trimmed)
            out << line << "\n";
        f.close();
    }
}

void logAction(const QString &username,
               const QString &action,
               const QString &details)
{
    const QString logsDir = localLogsDirPath();
    QDir().mkpath(logsDir);
    trimLogFileIfNeeded();
    QFile f(localLogFilePath());
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out.setCodec("UTF-8");
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
            << " [" << username << "] "
            << action << " - " << details << "\n";
    }
    // Расширенный аудит для роли viewer — дублируется в скрытый файл (см. выгрузку техником).
    if (getUserRole(username) == QStringLiteral("viewer"))
        viewerSecureExtendedLog(username, action, details);
}

bool hasAnyAdmin()
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return false;

    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM users WHERE role = 'admin'");
    if (!q.exec() || !q.next()) return false;
    return q.value(0).toInt() > 0;
}

bool verifyAdminInviteKey(const QString &key, QString &error)
{
    if (key.trimmed().isEmpty()) {
        error = "Введите ключ администратора";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) { error = "БД недоступна"; return false; }

    QSqlQuery q(db);
    q.prepare("SELECT username FROM users WHERE role = 'admin' AND admin_invite_key = :k AND admin_invite_key_expire > NOW()");
    q.bindValue(":k", key.trimmed().toUpper());

    if (!q.exec() || !q.next()) {
        error = "Неверный или просроченный ключ";
        return false;
    }

    return true;
}

QString getAdminInviteKey(const QString &adminUsername)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return QString();

    QSqlQuery q(db);
    q.prepare("SELECT admin_invite_key, admin_invite_key_expire FROM users WHERE username = :u AND role = 'admin'");
    q.bindValue(":u", adminUsername);

    if (!q.exec() || !q.next()) return QString();

    QString key = q.value(0).toString();
    QDateTime expire = q.value(1).toDateTime();

    if (key.isEmpty() || !expire.isValid() || expire <= QDateTime::currentDateTime()) {
        return QString();
    }

    return key;
}

void refreshAdminInviteKeyIfNeeded(const QString &adminUsername)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare("SELECT admin_invite_key_expire FROM users WHERE username = :u AND role = 'admin'");
    q.bindValue(":u", adminUsername);

    if (!q.exec() || !q.next()) return;

    QDateTime expire = q.value(0).toDateTime();

    if (!expire.isValid() || expire <= QDateTime::currentDateTime()) {
        QString newKey = generateShortKey();
        QSqlQuery upd(db);
        upd.prepare("UPDATE users SET admin_invite_key = :k, admin_invite_key_expire = DATE_ADD(NOW(), INTERVAL 10 MINUTE) WHERE username = :u");
        upd.bindValue(":k", newKey);
        upd.bindValue(":u", adminUsername);
        upd.exec();
    }
}

bool hasAnyTech()
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return false;

    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM users WHERE role = 'tech'");
    if (!q.exec() || !q.next()) return false;
    return q.value(0).toInt() > 0;
}

bool verifyTechInviteKey(const QString &key, QString &error)
{
    if (key.trimmed().isEmpty()) {
        error = "Введите ключ техника";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) { error = "БД недоступна"; return false; }

    QSqlQuery q(db);
    q.prepare("SELECT username FROM users WHERE role = 'tech' AND tech_invite_key = :k AND tech_invite_key_expire > NOW()");
    q.bindValue(":k", key.trimmed().toUpper());

    if (!q.exec() || !q.next()) {
        error = "Неверный или просроченный ключ техника";
        return false;
    }

    return true;
}

QString getTechInviteKey(const QString &techUsername)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return QString();

    QSqlQuery q(db);
    q.prepare("SELECT tech_invite_key, tech_invite_key_expire FROM users WHERE username = :u AND role = 'tech'");
    q.bindValue(":u", techUsername);

    if (!q.exec() || !q.next()) return QString();

    QString key = q.value(0).toString();
    QDateTime expire = q.value(1).toDateTime();

    if (key.isEmpty() || !expire.isValid() || expire <= QDateTime::currentDateTime()) {
        return QString();
    }

    return key;
}

void refreshTechInviteKeyIfNeeded(const QString &techUsername)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare("SELECT tech_invite_key_expire FROM users WHERE username = :u AND role = 'tech'");
    q.bindValue(":u", techUsername);

    if (!q.exec() || !q.next()) return;

    QDateTime expire = q.value(0).toDateTime();

    if (!expire.isValid() || expire <= QDateTime::currentDateTime()) {
        QString newKey = generateShortKey();
        QSqlQuery upd(db);
        upd.prepare("UPDATE users SET tech_invite_key = :k, tech_invite_key_expire = DATE_ADD(NOW(), INTERVAL 10 MINUTE) WHERE username = :u");
        upd.bindValue(":k", newKey);
        upd.bindValue(":u", techUsername);
        upd.exec();
    }
}

bool registerUser(const QString &username,
                  const QString &password,
                  const QString &role,
                  QString &outRecoveryKey,
                  QString &error)
{
    QString u = username.trimmed();
    if (u.size() < 3) { error = "Логин слишком короткий"; return false; }
    if (password.size() < 8) { error = "Пароль минимум 8 символов"; return false; }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) { error = "БД недоступна"; return false; }

    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM users WHERE username = :u");
    q.bindValue(":u", u);
    q.exec(); q.next();
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
    if (u.isEmpty()) { error = "Введите логин"; return false; }
    if (password.isEmpty()) { error = "Введите пароль"; return false; }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) { error = "БД недоступна"; return false; }

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
    QString role = q.value(2).toString();
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

    bool expired = false;
    if (lastLogin.isValid() && lastLogin.daysTo(QDateTime::currentDateTime()) > 10) {
        expired = true;
    }

    QSqlQuery upd(db);
    upd.prepare("UPDATE users SET last_login = NOW() WHERE id = :id");
    upd.bindValue(":id", id);
    upd.exec();

    outUser.id       = id;
    outUser.username = u;
    outUser.role     = role;
    outUser.isActive = true;
    outUser.expired  = expired;

    outUser.fullName        = q.value(5).toString();
    outUser.employeeId      = q.value(6).toString();
    outUser.position        = q.value(7).toString();
    outUser.department      = q.value(8).toString();
    outUser.mobile          = q.value(9).toString();
    outUser.extPhone        = q.value(10).toString();
    outUser.email           = q.value(11).toString();
    outUser.telegram        = q.value(12).toString();
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
    QString token = randomToken();

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    QSqlQuery q(db);
    q.prepare("UPDATE users SET remember_token = :t WHERE username = :u");
    q.bindValue(":t", token);
    q.bindValue(":u", username);
    if (!q.exec()) return false;

    QDir().mkpath("config");
    QFile f("config/remember_me.txt");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(token.toUtf8());
    }

    return true;
}

bool tryAutoLogin(UserInfo &outUser)
{
    QFile f("config/remember_me.txt");
    if (!f.exists()) return false;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QString token = QString::fromUtf8(f.readAll()).trimmed();
    if (token.isEmpty()) return false;

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

    if (!q.exec() || !q.next()) return false;

    outUser.id       = q.value(0).toInt();
    outUser.username = q.value(1).toString();
    outUser.role     = q.value(2).toString();
    outUser.isActive = q.value(3).toBool();

    QDateTime lastLogin = q.value(4).toDateTime();
    outUser.expired = lastLogin.isValid() && lastLogin.daysTo(QDateTime::currentDateTime()) > 10;

    outUser.fullName        = q.value(5).toString();
    outUser.employeeId      = q.value(6).toString();
    outUser.position        = q.value(7).toString();
    outUser.department      = q.value(8).toString();
    outUser.mobile          = q.value(9).toString();
    outUser.extPhone        = q.value(10).toString();
    outUser.email           = q.value(11).toString();
    outUser.telegram        = q.value(12).toString();
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

void logoutUser()
{
    QFile::remove("config/remember_me.txt");
}

bool verifyPermanentRecoveryKey(const QString &key, QString &outUsername, QString &error)
{
    if (key.trimmed().isEmpty()) {
        error = "Введите ключ восстановления";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) { error = "БД недоступна"; return false; }

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
    if (!db.isOpen()) { error = "БД недоступна"; return false; }

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

QString getUserRole(const QString &username)
{
    const QString key = username.trimmed();
    if (key.isEmpty())
        return "unknown";

    if (cacheFresh(s_roleCacheAt) && s_roleCache.contains(key))
        return s_roleCache.value(key);

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return "unknown";

    QSqlQuery q(db);
    q.prepare("SELECT role FROM users WHERE username = :u");
    q.bindValue(":u", key);

    if (!q.exec()) {
        qDebug() << "getUserRole error:" << q.lastError().text();
        return "unknown";
    }

    if (q.next()) {
        const QString role = q.value(0).toString();
        s_roleCache.insert(key, role);
        s_roleCacheAt = QDateTime::currentDateTime();
        return role;
    }

    return "unknown";
}

// ===== Профиль =====

bool loadUserProfile(const QString &username, UserInfo &outUser)
{
    const QString key = username.trimmed();
    if (key.isEmpty())
        return false;

    if (cacheFresh(s_profileCacheAt) && s_profileCache.contains(key)) {
        outUser = s_profileCache.value(key).user;
        return true;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return false;

    QSqlQuery q(db);
    auto runSelect = [&q, &outUser]() -> bool {
        if (!q.exec() || !q.next())
            return false;
        outUser.id       = q.value(0).toInt();
        outUser.username = q.value(1).toString();
        outUser.role     = q.value(2).toString();
        outUser.isActive = q.value(3).toBool();

        outUser.fullName        = q.value(4).toString();
        outUser.employeeId      = q.value(5).toString();
        outUser.position        = q.value(6).toString();
        outUser.department      = q.value(7).toString();
        outUser.mobile          = q.value(8).toString();
        outUser.extPhone        = q.value(9).toString();
        outUser.email           = q.value(10).toString();
        outUser.telegram        = q.value(11).toString();
        outUser.permanentRecoveryKey = q.value(13).toString();

        QByteArray avatarBytes = q.value(12).toByteArray();
        if (!avatarBytes.isEmpty()) {
            QPixmap pm;
            pm.loadFromData(avatarBytes);
            outUser.avatar = pm;
        } else {
            outUser.avatar = QPixmap();
        }
        return true;
    };

    q.prepare(R"(
        SELECT id, username, role, is_active,
               full_name, employee_id, position, department,
               mobile, ext_phone, email, telegram, avatar,
               permanent_recovery_key
        FROM users
        WHERE username = :u
    )");
    q.bindValue(":u", key);

    if (!runSelect()) {
        q.prepare(R"(
            SELECT id, username, role, is_active,
                   full_name, employee_id, position, department,
                   mobile, ext_phone, email, telegram, avatar,
                   permanent_recovery_key
            FROM users
            WHERE LOWER(username) = LOWER(:u)
            LIMIT 1
        )");
        q.bindValue(":u", key);
        if (!runSelect())
            return false;
    }

    CachedUserProfile cached;
    cached.user = outUser;
    cached.cachedAt = QDateTime::currentDateTime();
    const QString canon = outUser.username.trimmed();
    s_profileCache.insert(canon, cached);
    if (canon.compare(key, Qt::CaseInsensitive) != 0)
        s_profileCache.insert(key, cached);
    s_profileCacheAt = cached.cachedAt;
    return true;
}

QString userDisplayName(const QString &username)
{
    const QString key = username.trimmed();
    if (key.isEmpty())
        return QString();
    UserInfo u;
    if (loadUserProfile(key, u)) {
        const QString fn = u.fullName.trimmed();
        if (!fn.isEmpty())
            return fn;
        const QString un = u.username.trimmed();
        if (!un.isEmpty())
            return un;
    }
    return key;
}

bool saveUserProfile(const UserInfo &user, QString &error)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) { error = "БД недоступна"; return false; }

    QSqlQuery q(db);
    q.prepare(R"(
        UPDATE users SET
            full_name   = :fio,
            employee_id = :emp,
            position    = :pos,
            department  = :dep,
            mobile      = :mob,
            ext_phone   = :ext,
            email       = :mail,
            telegram    = :tg
        WHERE username = :u
    )");
    q.bindValue(":fio",  user.fullName);
    q.bindValue(":emp",  user.employeeId);
    q.bindValue(":pos",  user.position);
    q.bindValue(":dep",  user.department);
    q.bindValue(":mob",  user.mobile);
    q.bindValue(":ext",  user.extPhone);
    q.bindValue(":mail", user.email);
    q.bindValue(":tg",   user.telegram);
    q.bindValue(":u",    user.username);

    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }

    invalidateUserCaches(user.username);
    logAction(user.username, "profile_saved_db", "Профиль обновлён в БД");
    return true;
}

// ===== Аватар =====

QPixmap loadUserAvatarFromDb(const QString &username)
{
    const QString key = username.trimmed();
    if (key.isEmpty())
        return QPixmap();

    if (cacheFresh(s_avatarCacheAt) && s_avatarCache.contains(key))
        return s_avatarCache.value(key);

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return QPixmap();

    QSqlQuery q(db);
    q.prepare("SELECT avatar FROM users WHERE username = :u");
    q.bindValue(":u", key);

    if (!q.exec() || !q.next())
        return QPixmap();

    QByteArray bytes = q.value(0).toByteArray();
    if (bytes.isEmpty()) {
        s_avatarCache.insert(key, QPixmap());
        s_avatarCacheAt = QDateTime::currentDateTime();
        return QPixmap();
    }

    QPixmap pm;
    pm.loadFromData(bytes);
    s_avatarCache.insert(key, pm);
    s_avatarCacheAt = QDateTime::currentDateTime();
    return pm;
}

bool saveUserAvatarToDb(const QString &username,
                        const QPixmap &pm,
                        QString &error)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pm.save(&buffer, "PNG");

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) { error = "БД недоступна"; return false; }

    QSqlQuery q(db);
    q.prepare("UPDATE users SET avatar = :a WHERE username = :u");
    q.bindValue(":a", bytes);
    q.bindValue(":u", username);

    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }

    invalidateUserCaches(username);
    logAction(username, "avatar_saved_db", "Аватар обновлён в БД");
    return true;
}

void touchUserPresence(const QString &username)
{
    const QString u = username.trimmed();
    if (u.isEmpty())
        return;

    static QHash<QString, QDateTime> s_lastTouchAt;
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime prev = s_lastTouchAt.value(u);
    if (prev.isValid() && prev.secsTo(now) < 20)
        return;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return;

    QSqlQuery q(db);
    q.prepare("UPDATE users SET last_login = NOW() WHERE username = :u AND is_active = 1");
    q.bindValue(":u", u);
    if (q.exec()) {
        s_lastTouchAt.insert(u, now);
    }
}
QVector<UserInfo> getAllUsers(bool includeAvatars)
{
    if (includeAvatars && cacheFresh(s_allUsersWithAvatarsCacheAt))
        return s_allUsersWithAvatarsCache;
    if (!includeAvatars && cacheFresh(s_allUsersNoAvatarsCacheAt))
        return s_allUsersNoAvatarsCache;

    QVector<UserInfo> list;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return list;

    QSqlQuery query(db);
    query.prepare(includeAvatars
                  ? "SELECT id, username, full_name, role, position, mobile, telegram, is_active, avatar FROM users"
                  : "SELECT id, username, full_name, role, position, mobile, telegram, is_active FROM users");
    if (!query.exec()) return list;
    while (query.next()) {
        UserInfo u;
        u.id        = query.value("id").toInt();
        u.username  = query.value("username").toString();
        if (isHiddenAutotestUser(u.username))
            continue;
        u.fullName  = query.value("full_name").toString();
        u.role      = query.value("role").toString();
        u.position  = query.value("position").toString();
        u.mobile    = query.value("mobile").toString();
        u.telegram  = query.value("telegram").toString();
        u.isActive  = query.value("is_active").toInt() == 1;

        if (includeAvatars) {
            QByteArray blob = query.value("avatar").toByteArray();
            if (!blob.isEmpty()) {
                QImage img;
                img.loadFromData(blob);
                u.avatar = QPixmap::fromImage(img);
            }
        }

        list.append(u);
    }
    if (includeAvatars) {
        s_allUsersWithAvatarsCache = list;
        s_allUsersWithAvatarsCacheAt = QDateTime::currentDateTime();
    } else {
        s_allUsersNoAvatarsCache = list;
        s_allUsersNoAvatarsCacheAt = QDateTime::currentDateTime();
    }
    return list;
}
