#include "db_users.h"

#include <QBuffer>
#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "internal/db_users_internal_state.h"

using namespace DbUsersInternal;

QPixmap loadUserAvatarFromDb(const QString &username)
{
    const QString key = username.trimmed();
    if (key.isEmpty())
        return QPixmap();

    if (cacheFresh(avatarCacheAt()) && avatarCache().contains(key))
        return avatarCache().value(key);

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return QPixmap();

    QSqlQuery q(db);
    q.prepare("SELECT avatar FROM users WHERE username = :u");
    q.bindValue(":u", key);

    if (!q.exec() || !q.next())
        return QPixmap();

    QByteArray bytes = q.value(0).toByteArray();
    if (bytes.isEmpty()) {
        avatarCache().insert(key, QPixmap());
        avatarCacheAt() = QDateTime::currentDateTime();
        return QPixmap();
    }

    QPixmap pm;
    pm.loadFromData(bytes);
    avatarCache().insert(key, pm);
    avatarCacheAt() = QDateTime::currentDateTime();
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
    if (!db.isOpen()) {
        error = "БД недоступна";
        return false;
    }

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
    if (q.exec())
        s_lastTouchAt.insert(u, now);
}

QVector<UserInfo> getAllUsers(bool includeAvatars)
{
    if (includeAvatars && cacheFresh(allUsersWithAvatarsCacheAt()))
        return allUsersWithAvatarsCache();
    if (!includeAvatars && cacheFresh(allUsersNoAvatarsCacheAt()))
        return allUsersNoAvatarsCache();

    QVector<UserInfo> list;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return list;

    QSqlQuery query(db);
    query.prepare(includeAvatars
                      ? "SELECT id, username, full_name, role, position, mobile, telegram, is_active, avatar FROM users"
                      : "SELECT id, username, full_name, role, position, mobile, telegram, is_active FROM users");
    if (!query.exec())
        return list;

    while (query.next()) {
        UserInfo u;
        u.id = query.value("id").toInt();
        u.username = query.value("username").toString();
        if (isHiddenAutotestUser(u.username))
            continue;

        u.fullName = query.value("full_name").toString();
        u.role = query.value("role").toString();
        u.position = query.value("position").toString();
        u.mobile = query.value("mobile").toString();
        u.telegram = query.value("telegram").toString();
        u.isActive = query.value("is_active").toInt() == 1;

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
        allUsersWithAvatarsCache() = list;
        allUsersWithAvatarsCacheAt() = QDateTime::currentDateTime();
    } else {
        allUsersNoAvatarsCache() = list;
        allUsersNoAvatarsCacheAt() = QDateTime::currentDateTime();
    }

    return list;
}
