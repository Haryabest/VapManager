#include "leftmenu.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlQuery>

namespace {

QString avatarDiskPath(const QString &username)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                        + QStringLiteral("/avatars");
    QDir().mkpath(dir);
    // Имя файла = безопасный логин (заменяем символы, недопустимые в путях Windows)
    QString safe = username.trimmed();
    safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.@-]")), QStringLiteral("_"));
    if (safe.isEmpty())
        safe = QStringLiteral("_anon");
    return dir + QStringLiteral("/") + safe + QStringLiteral(".dat");
}

QPixmap loadAvatarFromDisk(const QString &username)
{
    QPixmap pm;
    QFile f(avatarDiskPath(username));
    if (f.open(QIODevice::ReadOnly)) {
        pm.loadFromData(f.readAll());
        f.close();
    }
    return pm;
}

void saveAvatarToDisk(const QString &username, const QPixmap &pm)
{
    if (pm.isNull())
        return;
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    pm.save(&buf, "PNG");
    buf.close();
    QFile f(avatarDiskPath(username));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(bytes);
        f.close();
    }
}

} // namespace

QPixmap leftMenu::makeRoundPixmap(const QPixmap &src, int size)
{
    if (src.isNull())
        return QPixmap();

    QPixmap out(size, size);
    out.fill(Qt::transparent);

    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    p.setClipPath(path);

    p.drawPixmap(0, 0, size, size, src);
    return out;
}

QPixmap leftMenu::loadUserAvatarFromDb(const QString &username)
{
    const QString key = username.trimmed();
    if (avatarCache_.contains(key))
        return avatarCache_.value(key);

    // 1) Дисковый кеш
    QPixmap pm = loadAvatarFromDisk(key);
    if (!pm.isNull()) {
        avatarCache_.insert(key, pm);
        return pm;
    }

    // 2) БД
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        avatarCache_.insert(key, QPixmap());
        return QPixmap();
    }

    QSqlQuery q(db);
    q.prepare("SELECT avatar FROM users WHERE username = :u");
    q.bindValue(":u", key);

    if (!q.exec() || !q.next()) {
        avatarCache_.insert(key, QPixmap());
        return QPixmap();
    }

    QByteArray bytes = q.value(0).toByteArray();
    if (bytes.isEmpty()) {
        avatarCache_.insert(key, QPixmap());
        return QPixmap();
    }

    pm.loadFromData(bytes);
    avatarCache_.insert(key, pm);
    if (!pm.isNull())
        saveAvatarToDisk(key, pm); // сохраняем на диск для следующих запусков
    return pm;
}

void leftMenu::invalidateAvatarCache(const QString &username)
{
    const QString key = username.trimmed();
    avatarCache_.remove(key);
    QFile::remove(avatarDiskPath(key));
}
