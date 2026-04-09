#include "db_users.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "internal/db_users_internal_state.h"

using namespace DbUsersInternal;

QString getUserRole(const QString &username)
{
    const QString key = username.trimmed();
    if (key.isEmpty())
        return "unknown";

    if (cacheFresh(roleCacheAt()) && roleCache().contains(key))
        return roleCache().value(key);

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
        roleCache().insert(key, role);
        roleCacheAt() = QDateTime::currentDateTime();
        return role;
    }

    return "unknown";
}

bool loadUserProfile(const QString &username, UserInfo &outUser)
{
    const QString key = username.trimmed();
    if (key.isEmpty())
        return false;

    if (cacheFresh(profileCacheAt()) && profileCache().contains(key)) {
        outUser = profileCache().value(key).user;
        return true;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    auto runSelect = [&q, &outUser]() -> bool {
        if (!q.exec() || !q.next())
            return false;

        outUser.id = q.value(0).toInt();
        outUser.username = q.value(1).toString();
        outUser.role = q.value(2).toString();
        outUser.isActive = q.value(3).toBool();

        outUser.fullName = q.value(4).toString();
        outUser.employeeId = q.value(5).toString();
        outUser.position = q.value(6).toString();
        outUser.department = q.value(7).toString();
        outUser.mobile = q.value(8).toString();
        outUser.extPhone = q.value(9).toString();
        outUser.email = q.value(10).toString();
        outUser.telegram = q.value(11).toString();
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
    profileCache().insert(canon, cached);
    if (canon.compare(key, Qt::CaseInsensitive) != 0)
        profileCache().insert(key, cached);
    profileCacheAt() = cached.cachedAt;

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
    if (!db.isOpen()) {
        error = "БД недоступна";
        return false;
    }

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
    q.bindValue(":fio", user.fullName);
    q.bindValue(":emp", user.employeeId);
    q.bindValue(":pos", user.position);
    q.bindValue(":dep", user.department);
    q.bindValue(":mob", user.mobile);
    q.bindValue(":ext", user.extPhone);
    q.bindValue(":mail", user.email);
    q.bindValue(":tg", user.telegram);
    q.bindValue(":u", user.username);

    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }

    invalidateUserCaches(user.username);
    logAction(user.username, "profile_saved_db", "Профиль обновлён в БД");
    return true;
}
