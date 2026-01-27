    #include "notifications_logs.h"
    #include <QDebug>

    notifications_logs::notifications_logs(QObject *parent)
        : QObject(parent)
    {
        // Псевдо-данные (как будто SELECT * FROM notifications)
        storage = {
            {1, "ТО просрочено", "AGV_202 требует немедленного обслуживания",
             QDateTime::currentDateTime().addSecs(-3600), false},

            {2, "Скорое ТО", "AGV_505 — плановое обслуживание через 2 дня",
             QDateTime::currentDateTime().addSecs(-7200), false},

            {3, "Обслужено", "AGV_101 успешно обслужен",
             QDateTime::currentDateTime().addDays(-1), true}
        };
    }

    QVector<Notification> notifications_logs::loadAll() const
    {
        return storage;
    }

    int notifications_logs::unreadCount() const
    {
        int count = 0;
        for (const auto &n : storage)
            if (!n.isRead)
                count++;
        return count;
    }

    void notifications_logs::markAllRead()
    {
        for (auto &n : storage)
            n.isRead = true;

        qDebug() << "[notifications_logs] markAllRead() called";
    }

    void notifications_logs::addNotification(const QString &title, const QString &message)
    {
        int newId = storage.isEmpty() ? 1 : storage.last().id + 1;

        storage.push_back({
            newId,
            title,
            message,
            QDateTime::currentDateTime(),
            false
        });

        qDebug() << "[notifications_logs] added:" << title << message;
    }
