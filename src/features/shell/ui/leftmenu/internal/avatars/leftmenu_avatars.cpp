#include "leftmenu.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSqlDatabase>
#include <QSqlQuery>

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

    QPixmap pm;
    pm.loadFromData(bytes);
    avatarCache_.insert(key, pm);
    return pm;
}
