#pragma once
#include <QObject>
#include <QVector>
#include <QDateTime>

struct Notification {
    int id;
    QString title;
    QString message;
    QDateTime createdAt;
    bool isRead;
};

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
    QVector<Notification> storage; // псевдо-БД
};
