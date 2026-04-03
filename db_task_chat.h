#pragma once

#include <QString>
#include <QVector>
#include <QDateTime>
#include <QHash>

struct TaskChatThread {
    int id = 0;
    QString agvId;
    int taskId = 0;
    QString taskName;
    QString createdBy;
    QString recipientUser;  // private recipient username
    QDateTime createdAt;
    QDateTime closedAt;
    QString closedBy;
    bool isClosed() const { return closedAt.isValid(); }
};

struct TaskChatMessage {
    int id = 0;
    int threadId = 0;
    QString fromUser;
    QString message;
    QDateTime createdAt;
};

bool initTaskChatTables();
int createThread(const QString &agvId, int taskId, const QString &taskName,
                 const QString &createdBy, const QString &recipientUser,
                 const QString &firstMessage, QString &error);
bool addChatMessage(int threadId, const QString &fromUser, const QString &message, QString &error);
bool closeThread(int threadId, const QString &closedBy, QString &error);
bool deleteThread(int threadId, QString &error);
bool hideThreadForUser(int threadId, const QString &username, QString &error);
/// Скрыть сообщение только для пользователя (удалить у себя)
bool hideMessageForUser(int messageId, const QString &username, QString &error);
/// Удалить сообщение для всех (только админ или автор сообщения)
bool deleteMessage(int messageId, const QString &actingUser, QString &error);
QVector<TaskChatThread> getThreadsForUser(const QString &username);
QVector<TaskChatThread> getThreadsForAdmin(const QString &adminUsername);
/// Найти тред между двумя пользователями (участники: user1 и user2). optionalAgvId — при желании фильтр по agv_id.
int getThreadBetweenUsers(const QString &user1, const QString &user2, const QString &optionalAgvId = QString());
QVector<TaskChatMessage> getMessagesForThread(int threadId);
/// Как getMessagesForThread, но исключает сообщения, скрытые для currentUser
QVector<TaskChatMessage> getMessagesForThread(int threadId, const QString &currentUser);
/// Последние limit сообщений треда (для пагинации)
QVector<TaskChatMessage> getMessagesForThreadLastN(int threadId, const QString &currentUser, int limit);
/// До limit сообщений старше beforeId (для подгрузки при скролле вверх)
QVector<TaskChatMessage> getMessagesForThreadOlderThan(int threadId, const QString &currentUser, int beforeId, int limit);
/// Все сообщения с id >= fromId (для обновления при уже загруженной истории)
QVector<TaskChatMessage> getMessagesForThreadFrom(int threadId, const QString &currentUser, int fromId);
TaskChatThread getThreadById(int threadId);
QHash<int, TaskChatThread> getThreadsByIds(const QVector<int> &threadIds);
