#pragma once
#include <QObject>
#include <QVector>
#include <QDateTime>

struct Notification {
    int id;
    QString targetUser;
    QString title;
    QString message;
    QDateTime createdAt;
    bool isRead;
};

bool initNotificationsTable();
bool initMaintenanceNotificationSentTable();
bool wasMaintenanceNotificationSentToday(const QString &agvId);
void markMaintenanceNotificationSentToday(const QString &agvId);
QVector<Notification> loadNotificationsForUser(const QString &username);
int unreadCountForUser(const QString &username);
void markAllReadForUser(const QString &username);
void markNotificationReadById(int notificationId);
void clearAllNotificationsForUser(const QString &username);
void addNotificationForUser(const QString &targetUser,
                            const QString &title,
                            const QString &message);

/// Убрать служебные метки [chat:N] и [peer:login] для показа пользователю.
QString notificationMessageForDisplay(const QString &storedMessage);
/// Логин собеседника из хвоста сообщения [peer:username] (для чата из уведомления).
QString notificationPeerUsername(const QString &storedMessage);

class notifications_logs : public QObject
{
    Q_OBJECT
public:
    explicit notifications_logs(QObject *parent = nullptr);

    QVector<Notification> loadAll() const;
    int unreadCount() const;
    void markAllRead();
    void addNotification(const QString &title, const QString &message);

private:
    QVector<Notification> storage;
};
