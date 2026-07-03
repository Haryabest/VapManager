#include "db_users.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QVariant>

#include "internal/db_users_internal_state.h"
#include "db_tables.h"

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
            :full_name, :position, :department, TRUE, CURRENT_TIMESTAMP
        )
        ON CONFLICT (username) DO UPDATE SET
            role = EXCLUDED.role,
            full_name = EXCLUDED.full_name,
            position = EXCLUDED.position,
            department = EXCLUDED.department,
            is_active = TRUE
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
    return ensureDbTable(db, QStringLiteral("users"), QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS users (
            id SERIAL PRIMARY KEY,
            username VARCHAR(64) NOT NULL UNIQUE,
            password_hash VARCHAR(255) NOT NULL,
            role VARCHAR(16) NOT NULL DEFAULT 'viewer'
                CHECK (role IN ('admin', 'tech', 'viewer')),
            is_active BOOLEAN NOT NULL DEFAULT TRUE,
            last_login TIMESTAMP NULL,
            remember_token VARCHAR(128) NULL,
            active_session_token VARCHAR(128) NULL,
            permanent_recovery_key VARCHAR(32) NULL,
            admin_invite_key VARCHAR(16) NULL,
            admin_invite_key_expire TIMESTAMP NULL,
            tech_invite_key VARCHAR(16) NULL,
            tech_invite_key_expire TIMESTAMP NULL,
            full_name VARCHAR(128) NULL,
            employee_id VARCHAR(16) NULL,
            position VARCHAR(128) NULL,
            department VARCHAR(128) NULL,
            mobile VARCHAR(32) NULL,
            ext_phone VARCHAR(16) NULL,
            email VARCHAR(128) NULL,
            telegram VARCHAR(64) NULL,
            avatar BYTEA NULL,
            created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
        )
    )"));
}
