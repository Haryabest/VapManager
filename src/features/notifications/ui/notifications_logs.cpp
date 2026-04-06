#include "notifications_logs.h"
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

bool initNotificationsTable()
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return false;

    QSqlQuery q(db);
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS notifications (
            id INT AUTO_INCREMENT PRIMARY KEY,
            target_user VARCHAR(64) NOT NULL,
            title VARCHAR(256) NOT NULL,
            message TEXT,
            is_read TINYINT(1) NOT NULL DEFAULT 0,
            created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
            INDEX idx_target (target_user)
        )
    )")) return false;
    return true;
}

bool initMaintenanceNotificationSentTable()
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return false;
    QSqlQuery q(db);
    return q.exec(R"(
        CREATE TABLE IF NOT EXISTS maintenance_notification_sent (
            agv_id VARCHAR(64) NOT NULL,
            notify_date DATE NOT NULL,
            PRIMARY KEY (agv_id, notify_date)
        )
    )");
}

bool wasMaintenanceNotificationSentToday(const QString &agvId)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return true;
    QSqlQuery q(db);
    q.prepare("SELECT 1 FROM maintenance_notification_sent WHERE agv_id = :id AND notify_date = CURDATE()");
    q.bindValue(":id", agvId);
    if (!q.exec() || !q.next()) return false;
    return true;
}

void markMaintenanceNotificationSentToday(const QString &agvId)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.prepare("INSERT IGNORE INTO maintenance_notification_sent (agv_id, notify_date) VALUES (:id, CURDATE())");
    q.bindValue(":id", agvId);
    q.exec();
}

QVector<Notification> loadNotificationsForUser(const QString &username)
{
    QVector<Notification> list;
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return list;

    QString u = username.trimmed();
    if (u.isEmpty()) return list;

    QSqlQuery q(db);
    q.prepare("SELECT id, target_user, title, message, is_read, created_at "
              "FROM notifications WHERE target_user = :u ORDER BY created_at DESC LIMIT 100");
    q.bindValue(":u", u);
    if (!q.exec()) return list;

    while (q.next()) {
        Notification n;
        n.id = q.value(0).toInt();
        n.targetUser = q.value(1).toString();
        n.title = q.value(2).toString();
        n.message = q.value(3).toString();
        n.isRead = q.value(4).toInt() == 1;
        n.createdAt = q.value(5).toDateTime();
        list.push_back(n);
    }
    return list;
}

int unreadCountForUser(const QString &username)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return 0;

    QString u = username.trimmed();
    if (u.isEmpty()) return 0;

    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM notifications WHERE target_user = :u AND is_read = 0");
    q.bindValue(":u", u);
    if (!q.exec() || !q.next()) return 0;
    return q.value(0).toInt();
}

void markAllReadForUser(const QString &username)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return;
    QString u = username.trimmed();
    if (u.isEmpty()) return;

    QSqlQuery q(db);
    q.prepare("UPDATE notifications SET is_read = 1 WHERE target_user = :u AND is_read = 0");
    q.bindValue(":u", u);
    q.exec();
}

void markNotificationReadById(int notificationId)
{
    if (notificationId <= 0) return;
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare("UPDATE notifications SET is_read = 1 WHERE id = :id AND is_read = 0");
    q.bindValue(":id", notificationId);
    q.exec();
}

void clearAllNotificationsForUser(const QString &username)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return;
    QString u = username.trimmed();
    if (u.isEmpty()) return;

    QSqlQuery q(db);
    q.prepare("DELETE FROM notifications WHERE target_user = :u");
    q.bindValue(":u", u);
    q.exec();
}

void addNotificationForUser(const QString &targetUser,
                            const QString &title,
                            const QString &message)
{
    QString user = targetUser.trimmed();
    if (user.isEmpty()) return;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return;

    initNotificationsTable();

    QSqlQuery q(db);
    q.prepare("INSERT INTO notifications (target_user, title, message) VALUES (:u, :t, :m)");
    q.bindValue(":u", user);
    q.bindValue(":t", title);
    q.bindValue(":m", message);
    if (!q.exec()) {
        qDebug() << "addNotificationForUser failed:" << q.lastError().text();
        qDebug() << "  target_user=" << user << "title=" << title.left(50);
    }
}

QString notificationMessageForDisplay(const QString &storedMessage)
{
    QString s = storedMessage;
    s.remove(QRegularExpression(QStringLiteral("\\[chat:\\d+\\]\\s*")));
    s.remove(QRegularExpression(QStringLiteral("\\s*\\[peer:[^\\]]+\\]\\s*")));
    return s.trimmed();
}

QString notificationPeerUsername(const QString &storedMessage)
{
    static const QRegularExpression re(QStringLiteral("\\[peer:([^\\]]+)\\]"));
    const QRegularExpressionMatch m = re.match(storedMessage);
    if (!m.hasMatch())
        return QString();
    return m.captured(1).trimmed();
}

notifications_logs::notifications_logs(QObject *parent)
    : QObject(parent)
{
    storage = {
        {1, "", "ТО просрочено", "AGV_202 требует немедленного обслуживания",
         QDateTime::currentDateTime().addSecs(-3600), false},
        {2, "", "Скорое ТО", "AGV_505 — плановое обслуживание через 2 дня",
         QDateTime::currentDateTime().addSecs(-7200), false},
        {3, "", "Обслужено", "AGV_101 успешно обслужен",
         QDateTime::currentDateTime().addDays(-1), true}
    };
}

QVector<Notification> notifications_logs::loadAll() const { return storage; }

int notifications_logs::unreadCount() const
{
    int count = 0;
    for (const auto &n : storage)
        if (!n.isRead) count++;
    return count;
}

void notifications_logs::markAllRead()
{
    for (auto &n : storage) n.isRead = true;
}

void notifications_logs::addNotification(const QString &title, const QString &message)
{
    int newId = storage.isEmpty() ? 1 : storage.last().id + 1;
    storage.push_back({newId, "", title, message, QDateTime::currentDateTime(), false});
}
