#include "db_users_internal_state.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QPixmap>
#include <QUuid>

namespace DbUsersInternal {

namespace {

const QString kHiddenAutotestUsername = QStringLiteral("__autotest_chat_peer__");
const QString kHiddenAutotestFullName = QStringLiteral("Autotest Chat Peer");

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

} // namespace

const QString &hiddenAutotestUsernameConst()
{
    return kHiddenAutotestUsername;
}

const QString &hiddenAutotestFullNameConst()
{
    return kHiddenAutotestFullName;
}

bool cacheFresh(const QDateTime &ts)
{
    return ts.isValid() && ts.msecsTo(QDateTime::currentDateTime()) <= kUserCacheTtlMs;
}

void invalidateUserCaches(const QString &username)
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

QHash<QString, QString> &roleCache() { return s_roleCache; }
QDateTime &roleCacheAt() { return s_roleCacheAt; }

QHash<QString, CachedUserProfile> &profileCache() { return s_profileCache; }
QDateTime &profileCacheAt() { return s_profileCacheAt; }

QHash<QString, QPixmap> &avatarCache() { return s_avatarCache; }
QDateTime &avatarCacheAt() { return s_avatarCacheAt; }

QVector<UserInfo> &allUsersWithAvatarsCache() { return s_allUsersWithAvatarsCache; }
QVector<UserInfo> &allUsersNoAvatarsCache() { return s_allUsersNoAvatarsCache; }
QDateTime &allUsersWithAvatarsCacheAt() { return s_allUsersWithAvatarsCacheAt; }
QDateTime &allUsersNoAvatarsCacheAt() { return s_allUsersNoAvatarsCacheAt; }

QString appSalt()
{
    return QStringLiteral("CHANGE_THIS_SALT_123456789");
}

QString hashPassword(const QString &password)
{
    QByteArray data = (appSalt() + password).toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

QString rememberTokenFilePath()
{
    return QStringLiteral("config/remember_me.txt");
}

QString sessionTokenFilePath()
{
    return QStringLiteral("config/session_token.txt");
}

bool writeTextFile(const QString &path, const QString &value)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    f.write(value.toUtf8());
    return true;
}

QString readTextFileTrimmed(const QString &path)
{
    QFile f(path);
    if (!f.exists())
        return QString();
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll()).trimmed();
}

QString randomToken()
{
    QByteArray bytes;
    bytes.append(QString::number(QDateTime::currentMSecsSinceEpoch()));
    bytes.append(QByteArray::number(qrand()));
    QByteArray hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

QString generateShortKey()
{
    QString uuid = QUuid::createUuid().toString().remove("{").remove("}").remove("-");
    return uuid.left(8).toUpper();
}

QString generateRecoveryKeyInternal()
{
    QString uuid = QUuid::createUuid().toString().remove("{").remove("}").remove("-");
    return QString("RK-%1-%2-%3")
        .arg(uuid.mid(0, 4).toUpper())
        .arg(uuid.mid(4, 4).toUpper())
        .arg(uuid.mid(8, 4).toUpper());
}

} // namespace DbUsersInternal
