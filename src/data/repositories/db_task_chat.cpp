#include "db_task_chat.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QSet>

namespace {

QSqlDatabase openMainDb()
{
    return QSqlDatabase::database("main_connection");
}

bool ensureDbAvailable(const QSqlDatabase &db, QString &error)
{
    if (db.isOpen())
        return true;
    error = QStringLiteral("БД недоступна");
    return false;
}

QString normalizedUsername(const QString &username)
{
    return username.trimmed();
}

QString normalizedAgvId(const QString &agvId)
{
    const QString v = agvId.trimmed();
    return v.isEmpty() ? QStringLiteral("—") : v;
}

TaskChatThread rowToThread(const QSqlQuery &q)
{
    TaskChatThread t;
    t.id = q.value(0).toInt();
    t.agvId = q.value(1).toString();
    t.taskId = q.value(2).toInt();
    t.taskName = q.value(3).toString();
    t.createdBy = q.value(4).toString();
    t.recipientUser = q.value(5).toString();
    t.createdAt = q.value(6).toDateTime();
    t.closedAt = q.value(7).toDateTime();
    t.closedBy = q.value(8).toString();
    return t;
}

TaskChatMessage rowToMessage(const QSqlQuery &q)
{
    TaskChatMessage m;
    m.id = q.value(0).toInt();
    m.threadId = q.value(1).toInt();
    m.fromUser = q.value(2).toString();
    m.message = q.value(3).toString();
    m.createdAt = q.value(4).toDateTime();
    return m;
}

}

bool initTaskChatTables()
{
    QSqlDatabase db = openMainDb();
    if (!db.isOpen()) return false;

    QSqlQuery q(db);
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS task_chat_threads (
            id INT AUTO_INCREMENT PRIMARY KEY,
            agv_id VARCHAR(64) NOT NULL,
            task_id INT NULL,
            task_name VARCHAR(255) NULL,
            created_by VARCHAR(64) NOT NULL,
            recipient_user VARCHAR(64) NULL,
            created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
            closed_at DATETIME NULL,
            closed_by VARCHAR(64) NULL,
            INDEX idx_created_by (created_by),
            INDEX idx_recipient (recipient_user),
            INDEX idx_closed (closed_at)
        )
    )")) return false;

    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS task_chat_messages (
            id INT AUTO_INCREMENT PRIMARY KEY,
            thread_id INT NOT NULL,
            from_user VARCHAR(64) NOT NULL,
            message MEDIUMTEXT NOT NULL,
            created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
            INDEX idx_thread (thread_id)
        )
    )")) return false;

    // Для вложений (base64) увеличиваем емкость поля сообщения на существующих БД.
    q.exec("ALTER TABLE task_chat_messages MODIFY COLUMN message MEDIUMTEXT NOT NULL");

    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS task_chat_hidden (
            thread_id INT NOT NULL,
            username VARCHAR(64) NOT NULL,
            created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (thread_id, username),
            INDEX idx_hidden_user (username)
        )
    )")) return false;

    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS task_chat_message_hidden (
            message_id INT NOT NULL,
            username VARCHAR(64) NOT NULL,
            created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (message_id, username),
            INDEX idx_msg_hidden_user (username)
        )
    )")) return false;

    return true;
}

int createThread(const QString &agvId, int taskId, const QString &taskName,
                 const QString &createdBy, const QString &recipientUser,
                 const QString &firstMessage, QString &error)
{
    QSqlDatabase db = openMainDb();
    if (!ensureDbAvailable(db, error)) { return 0; }
    if (!initTaskChatTables()) { error = "Не удалось инициализировать таблицы чата"; return 0; }

    QSqlQuery q(db);
    q.prepare("INSERT INTO task_chat_threads (agv_id, task_id, task_name, created_by, recipient_user) "
              "VALUES (:agv, :tid, :tname, :by, :rec)");
    const QString safeAgvId = normalizedAgvId(agvId);
    q.bindValue(":agv", safeAgvId);
    q.bindValue(":tid", taskId > 0 ? taskId : QVariant());
    q.bindValue(":tname", taskName.isEmpty() ? QVariant() : taskName);
    q.bindValue(":by", createdBy);
    q.bindValue(":rec", recipientUser.isEmpty() ? QVariant() : recipientUser);
    if (!q.exec()) {
        error = q.lastError().text();
        return 0;
    }
    int threadId = q.lastInsertId().toInt();
    if (threadId <= 0) {
        error = "Не удалось получить id треда";
        return 0;
    }
    q.prepare("INSERT INTO task_chat_messages (thread_id, from_user, message) VALUES (:tid, :u, :m)");
    q.bindValue(":tid", threadId);
    q.bindValue(":u", createdBy);
    q.bindValue(":m", firstMessage);
    if (!q.exec()) {
        error = q.lastError().text();
        QSqlQuery del(db);
        del.prepare("DELETE FROM task_chat_threads WHERE id = :id");
        del.bindValue(":id", threadId);
        del.exec();
        return 0;
    }
    return threadId;
}

bool addChatMessage(int threadId, const QString &fromUser, const QString &message, QString &error)
{
    QSqlDatabase db = openMainDb();
    if (!ensureDbAvailable(db, error)) { return false; }

    QSqlQuery q(db);
    q.prepare("SELECT 1 FROM task_chat_threads WHERE id = :id");
    q.bindValue(":id", threadId);
    if (!q.exec() || !q.next()) {
        error = "Тред не найден";
        return false;
    }
    // Если чат был исторически закрыт, при новом сообщении считаем его активным.
    QSqlQuery reopen(db);
    reopen.prepare("UPDATE task_chat_threads SET closed_at = NULL, closed_by = NULL WHERE id = :id");
    reopen.bindValue(":id", threadId);
    reopen.exec();
    q.prepare("INSERT INTO task_chat_messages (thread_id, from_user, message) VALUES (:tid, :u, :m)");
    q.bindValue(":tid", threadId);
    q.bindValue(":u", fromUser);
    q.bindValue(":m", message);
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    return true;
}

bool closeThread(int threadId, const QString &closedBy, QString &error)
{
    QSqlDatabase db = openMainDb();
    if (!ensureDbAvailable(db, error)) { return false; }

    QSqlQuery q(db);
    q.prepare("UPDATE task_chat_threads SET closed_at = NOW(), closed_by = :by WHERE id = :id");
    q.bindValue(":by", closedBy);
    q.bindValue(":id", threadId);
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    return true;
}

bool deleteThread(int threadId, QString &error)
{
    QSqlDatabase db = openMainDb();
    if (!ensureDbAvailable(db, error)) { return false; }

    QSqlQuery q(db);
    q.prepare("DELETE FROM task_chat_messages WHERE thread_id = :id");
    q.bindValue(":id", threadId);
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }

    q.prepare("DELETE FROM task_chat_threads WHERE id = :id");
    q.bindValue(":id", threadId);
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    return true;
}

bool hideThreadForUser(int threadId, const QString &username, QString &error)
{
    QSqlDatabase db = openMainDb();
    if (!ensureDbAvailable(db, error)) { return false; }

    const QString u = normalizedUsername(username);
    if (u.isEmpty()) { error = "Пользователь не задан"; return false; }

    QSqlQuery q(db);
    q.prepare("INSERT IGNORE INTO task_chat_hidden (thread_id, username) VALUES (:tid, :u)");
    q.bindValue(":tid", threadId);
    q.bindValue(":u", u);
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    return true;
}

bool hideMessageForUser(int messageId, const QString &username, QString &error)
{
    QSqlDatabase db = openMainDb();
    if (!ensureDbAvailable(db, error)) { return false; }
    const QString u = normalizedUsername(username);
    if (u.isEmpty()) { error = "Пользователь не задан"; return false; }
    QSqlQuery q(db);
    q.prepare("INSERT IGNORE INTO task_chat_message_hidden (message_id, username) VALUES (:mid, :u)");
    q.bindValue(":mid", messageId);
    q.bindValue(":u", u);
    if (!q.exec()) { error = q.lastError().text(); return false; }
    return true;
}

bool deleteMessage(int messageId, const QString &actingUser, QString &error)
{
    QSqlDatabase db = openMainDb();
    if (!ensureDbAvailable(db, error)) { return false; }
    QSqlQuery q(db);
    q.prepare("SELECT from_user FROM task_chat_messages WHERE id = :id");
    q.bindValue(":id", messageId);
    if (!q.exec() || !q.next()) { error = "Сообщение не найдено"; return false; }
    QString author = q.value(0).toString();
    QString role;
    { QSqlQuery r(db); r.prepare("SELECT role FROM users WHERE username = :u"); r.bindValue(":u", actingUser); if (r.exec() && r.next()) role = r.value(0).toString(); }
    bool canDelete = (actingUser == author) || (role == "admin" || role == "tech");
    if (!canDelete) { error = "Нет прав удалить сообщение"; return false; }
    q.prepare("UPDATE task_chat_messages SET message = :m WHERE id = :id");
    q.bindValue(":m", QStringLiteral("Сообщение удалено"));
    q.bindValue(":id", messageId);
    if (!q.exec()) { error = q.lastError().text(); return false; }
    return true;
}

QVector<TaskChatThread> getThreadsForUser(const QString &username)
{
    QVector<TaskChatThread> list;
    QSqlDatabase db = openMainDb();
    if (!db.isOpen()) return list;

    const QString u = normalizedUsername(username);
    if (u.isEmpty()) return list;

    QSqlQuery q(db);
    q.prepare(
        "SELECT t.id, t.agv_id, t.task_id, t.task_name, t.created_by, t.recipient_user, t.created_at, t.closed_at, t.closed_by "
        "FROM task_chat_threads t "
        "LEFT JOIN ("
        "  SELECT thread_id, MAX(created_at) AS last_msg_at "
        "  FROM task_chat_messages "
        "  GROUP BY thread_id"
        ") lm ON lm.thread_id = t.id "
        "WHERE (t.created_by = :u OR t.recipient_user = :u) "
        "AND NOT EXISTS (SELECT 1 FROM task_chat_hidden h WHERE h.thread_id = t.id AND h.username = :u) "
        "ORDER BY COALESCE(lm.last_msg_at, t.created_at) DESC");
    q.bindValue(":u", u);
    if (!q.exec()) return list;
    while (q.next())
        list.append(rowToThread(q));
    return list;
}

QVector<TaskChatThread> getThreadsForAdmin(const QString &adminUsername)
{
    // Privacy rule: admins see only chats where they are sender or recipient.
    return getThreadsForUser(adminUsername);
}

int getThreadBetweenUsers(const QString &user1, const QString &user2, const QString &optionalAgvId)
{
    const QString u1 = normalizedUsername(user1);
    const QString u2 = normalizedUsername(user2);
    if (u1.isEmpty() || u2.isEmpty()) return 0;

    QSqlDatabase db = openMainDb();
    if (!db.isOpen()) return 0;

    QSqlQuery q(db);
    QString sql =
        "SELECT id FROM task_chat_threads "
        "WHERE ((created_by = :u1 AND recipient_user = :u2) OR (created_by = :u2 AND recipient_user = :u1)) ";
    if (!optionalAgvId.trimmed().isEmpty())
        sql += "AND agv_id = :agv ";
    sql += "ORDER BY created_at DESC LIMIT 1";

    q.prepare(sql);
    q.bindValue(":u1", u1);
    q.bindValue(":u2", u2);
    if (!optionalAgvId.trimmed().isEmpty())
        q.bindValue(":agv", normalizedAgvId(optionalAgvId));
    if (!q.exec() || !q.next()) return 0;
    return q.value(0).toInt();
}

QVector<TaskChatMessage> getMessagesForThread(int threadId)
{
    return getMessagesForThread(threadId, QString());
}

QVector<TaskChatMessage> getMessagesForThread(int threadId, const QString &currentUser)
{
    QVector<TaskChatMessage> list;
    QSqlDatabase db = openMainDb();
    if (!db.isOpen()) return list;

    QSqlQuery q(db);
    if (normalizedUsername(currentUser).isEmpty()) {
        q.prepare("SELECT id, thread_id, from_user, message, created_at "
                  "FROM task_chat_messages WHERE thread_id = :id ORDER BY created_at ASC");
        q.bindValue(":id", threadId);
    } else {
        q.prepare("SELECT m.id, m.thread_id, m.from_user, m.message, m.created_at "
                  "FROM task_chat_messages m "
                  "LEFT JOIN task_chat_message_hidden h ON h.message_id = m.id AND h.username = :u "
                  "WHERE m.thread_id = :id AND h.message_id IS NULL "
                  "ORDER BY m.created_at ASC");
        q.bindValue(":u", normalizedUsername(currentUser));
        q.bindValue(":id", threadId);
    }
    if (!q.exec()) return list;

    while (q.next())
        list.append(rowToMessage(q));
    return list;
}

QVector<TaskChatMessage> getMessagesForThreadLastN(int threadId, const QString &currentUser, int limit)
{
    QVector<TaskChatMessage> list;
    QSqlDatabase db = openMainDb();
    if (!db.isOpen() || limit <= 0) return list;

    const int safeLimit = qBound(1, limit, 10000);
    QSqlQuery q(db);
    if (normalizedUsername(currentUser).isEmpty()) {
        q.prepare(QString("SELECT id, thread_id, from_user, message, created_at FROM ("
                  "SELECT id, thread_id, from_user, message, created_at FROM task_chat_messages "
                  "WHERE thread_id = :id ORDER BY id DESC LIMIT %1"
                  ") AS t ORDER BY id ASC").arg(safeLimit));
        q.bindValue(":id", threadId);
    } else {
        q.prepare(QString("SELECT id, thread_id, from_user, message, created_at FROM ("
                  "SELECT m.id, m.thread_id, m.from_user, m.message, m.created_at "
                  "FROM task_chat_messages m "
                  "LEFT JOIN task_chat_message_hidden h ON h.message_id = m.id AND h.username = :u "
                  "WHERE m.thread_id = :id AND h.message_id IS NULL "
                  "ORDER BY m.id DESC LIMIT %1"
                  ") AS t ORDER BY id ASC").arg(safeLimit));
        q.bindValue(":u", normalizedUsername(currentUser));
        q.bindValue(":id", threadId);
    }
    if (!q.exec()) return list;

    while (q.next())
        list.append(rowToMessage(q));
    return list;
}

QVector<TaskChatMessage> getMessagesForThreadOlderThan(int threadId, const QString &currentUser, int beforeId, int limit)
{
    QVector<TaskChatMessage> list;
    QSqlDatabase db = openMainDb();
    if (!db.isOpen() || beforeId <= 0 || limit <= 0) return list;

    const int safeLimit = qBound(1, limit, 10000);
    QSqlQuery q(db);
    if (normalizedUsername(currentUser).isEmpty()) {
        q.prepare(QString("SELECT id, thread_id, from_user, message, created_at FROM ("
                  "SELECT id, thread_id, from_user, message, created_at FROM task_chat_messages "
                  "WHERE thread_id = :id AND id < :before ORDER BY id DESC LIMIT %1"
                  ") AS t ORDER BY id ASC").arg(safeLimit));
        q.bindValue(":id", threadId);
        q.bindValue(":before", beforeId);
    } else {
        q.prepare(QString("SELECT id, thread_id, from_user, message, created_at FROM ("
                  "SELECT m.id, m.thread_id, m.from_user, m.message, m.created_at "
                  "FROM task_chat_messages m "
                  "LEFT JOIN task_chat_message_hidden h ON h.message_id = m.id AND h.username = :u "
                  "WHERE m.thread_id = :id AND m.id < :before AND h.message_id IS NULL "
                  "ORDER BY m.id DESC LIMIT %1"
                  ") AS t ORDER BY id ASC").arg(safeLimit));
        q.bindValue(":u", normalizedUsername(currentUser));
        q.bindValue(":id", threadId);
        q.bindValue(":before", beforeId);
    }
    if (!q.exec()) return list;

    while (q.next())
        list.append(rowToMessage(q));
    return list;
}

QVector<TaskChatMessage> getMessagesForThreadFrom(int threadId, const QString &currentUser, int fromId)
{
    QVector<TaskChatMessage> list;
    QSqlDatabase db = openMainDb();
    if (!db.isOpen() || fromId <= 0) return list;

    QSqlQuery q(db);
    if (normalizedUsername(currentUser).isEmpty()) {
        q.prepare("SELECT id, thread_id, from_user, message, created_at FROM task_chat_messages "
                  "WHERE thread_id = :id AND id >= :from ORDER BY id ASC");
        q.bindValue(":id", threadId);
        q.bindValue(":from", fromId);
    } else {
        q.prepare("SELECT m.id, m.thread_id, m.from_user, m.message, m.created_at "
                  "FROM task_chat_messages m "
                  "LEFT JOIN task_chat_message_hidden h ON h.message_id = m.id AND h.username = :u "
                  "WHERE m.thread_id = :id AND m.id >= :from AND h.message_id IS NULL "
                  "ORDER BY m.id ASC");
        q.bindValue(":u", normalizedUsername(currentUser));
        q.bindValue(":id", threadId);
        q.bindValue(":from", fromId);
    }
    if (!q.exec()) return list;

    while (q.next())
        list.append(rowToMessage(q));
    return list;
}

TaskChatThread getThreadById(int threadId)
{
    TaskChatThread t;
    QSqlDatabase db = openMainDb();
    if (!db.isOpen()) return t;

    QSqlQuery q(db);
    q.prepare("SELECT id, agv_id, task_id, task_name, created_by, recipient_user, created_at, closed_at, closed_by "
              "FROM task_chat_threads WHERE id = :id");
    q.bindValue(":id", threadId);
    if (!q.exec() || !q.next()) return t;
    return rowToThread(q);
}

QHash<int, TaskChatThread> getThreadsByIds(const QVector<int> &threadIds)
{
    QHash<int, TaskChatThread> result;
    if (threadIds.isEmpty())
        return result;

    QSqlDatabase db = openMainDb();
    if (!db.isOpen()) return result;

    QVector<int> uniqueIds;
    uniqueIds.reserve(threadIds.size());
    QSet<int> seen;
    for (int id : threadIds) {
        if (id > 0 && !seen.contains(id)) {
            seen.insert(id);
            uniqueIds.append(id);
        }
    }
    if (uniqueIds.isEmpty())
        return result;

    QStringList placeholders;
    placeholders.reserve(uniqueIds.size());
    for (int i = 0; i < uniqueIds.size(); ++i)
        placeholders << QString(":id%1").arg(i);

    QSqlQuery q(db);
    q.prepare(QString("SELECT id, agv_id, task_id, task_name, created_by, recipient_user, created_at, closed_at, closed_by "
                      "FROM task_chat_threads WHERE id IN (%1)").arg(placeholders.join(",")));
    for (int i = 0; i < uniqueIds.size(); ++i)
        q.bindValue(placeholders[i], uniqueIds[i]);
    if (!q.exec())
        return result;

    while (q.next()) {
        const TaskChatThread t = rowToThread(q);
        result.insert(t.id, t);
    }
    return result;
}
