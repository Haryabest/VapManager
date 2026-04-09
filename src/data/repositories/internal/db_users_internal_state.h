#pragma once

#include "../db_users.h"

#include <QDateTime>
#include <QHash>
#include <QVector>

namespace DbUsersInternal {

struct CachedUserProfile {
    UserInfo user;
    QDateTime cachedAt;
};

const QString &hiddenAutotestUsernameConst();
const QString &hiddenAutotestFullNameConst();

bool cacheFresh(const QDateTime &ts);
void invalidateUserCaches(const QString &username = QString());

QHash<QString, QString> &roleCache();
QDateTime &roleCacheAt();

QHash<QString, CachedUserProfile> &profileCache();
QDateTime &profileCacheAt();

QHash<QString, QPixmap> &avatarCache();
QDateTime &avatarCacheAt();

QVector<UserInfo> &allUsersWithAvatarsCache();
QVector<UserInfo> &allUsersNoAvatarsCache();
QDateTime &allUsersWithAvatarsCacheAt();
QDateTime &allUsersNoAvatarsCacheAt();

QString appSalt();
QString hashPassword(const QString &password);

QString rememberTokenFilePath();
QString sessionTokenFilePath();
bool writeTextFile(const QString &path, const QString &value);
QString readTextFileTrimmed(const QString &path);

QString randomToken();
QString generateShortKey();
QString generateRecoveryKeyInternal();

} // namespace DbUsersInternal
