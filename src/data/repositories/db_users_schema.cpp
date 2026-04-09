#include "db_users.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QVariant>

#include "internal/db_users_internal_state.h"

using namespace DbUsersInternal;

QString hiddenAutotestUsername()
{
    return hiddenAutotestUsernameConst();
}

bool isHiddenAutotestUser(const QString &username)
{
    return username.trimmed().compare(hiddenAutotestUsernameConst(), Qt::CaseInsensitive) == 0;
}

bool ensureAutotestChatUser(QString *outUsername, QString *outError)
{
    if (outUsername)
        *outUsername = hiddenAutotestUsernameConst();
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
    q.bindValue(":u", hiddenAutotestUsernameConst());
    q.bindValue(":p", hashPassword(QStringLiteral("AUTOTEST_CHAT_PEER_2026")));
    q.bindValue(":rk", QStringLiteral("AUTO-CHAT-HIDDEN"));
    q.bindValue(":full_name", hiddenAutotestFullNameConst());
    q.bindValue(":position", QStringLiteral("service"));
    q.bindValue(":department", QStringLiteral("autotest"));
    if (!q.exec()) {
        if (outError)
            *outError = q.lastError().text();
        return false;
    }

    invalidateUserCaches(hiddenAutotestUsernameConst());
    return true;
}

bool initUsersTable()
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return false;

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
            active_session_token VARCHAR(128) NULL,
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

    if (ok) {
        q.exec("ALTER TABLE users CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci");
        q.exec("ALTER TABLE users ADD COLUMN permanent_recovery_key VARCHAR(32) NULL");
        q.exec("ALTER TABLE users ADD COLUMN active_session_token VARCHAR(128) NULL");
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
