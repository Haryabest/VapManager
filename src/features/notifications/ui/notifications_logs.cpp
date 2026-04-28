#include "notifications_logs.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QStringList>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "Winmm.lib")
#endif
namespace {
QString firstExistingSoundPath()
{
    const QStringList candidates = {
        QStringLiteral("C:/Users/Dima/Desktop/sound-messages-odnoklassniki.mp3"),
        QStringLiteral("C:/VapManager/assets/sounds/sound-messages-odnoklassniki.mp3")
    };
    for (const QString &p : candidates) {
        if (QFile::exists(p))
            return QDir::toNativeSeparators(p);
    }
    return QString();
}

QString ensureNotificationSoundPath()
{
    const QString src = QStringLiteral(":/sounds/sound-messages-odnoklassniki.mp3");
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.trimmed().isEmpty())
        return QString();

    QDir dir(base);
    if (!dir.mkpath(QStringLiteral("sounds")))
        return QString();
    const QString dst = dir.filePath(QStringLiteral("sounds/sound-messages-odnoklassniki.mp3"));

    QFile srcFile(src);
    if (!srcFile.exists())
        return QString();
    if (!srcFile.open(QIODevice::ReadOnly))
        return QString();
    const QByteArray bytes = srcFile.readAll();
    srcFile.close();
    if (bytes.isEmpty())
        return QString();

    QFile dstFile(dst);
    bool rewrite = true;
    if (dstFile.exists() && dstFile.open(QIODevice::ReadOnly)) {
        rewrite = (dstFile.readAll() != bytes);
        dstFile.close();
    }
    if (rewrite) {
        QFile::remove(dst);
        if (!dstFile.open(QIODevice::WriteOnly))
            return QString();
        if (dstFile.write(bytes) != bytes.size()) {
            dstFile.close();
            return QString();
        }
        dstFile.close();
    }
    return QDir::toNativeSeparators(dst);
}

#ifdef Q_OS_WIN
QString mciErrorText(MCIERROR code)
{
    if (code == 0)
        return QString();
    wchar_t buf[256] = {0};
    if (::mciGetErrorStringW(code, buf, 255))
        return QString::fromWCharArray(buf);
    return QStringLiteral("MCI error code %1").arg(static_cast<unsigned int>(code));
}
#endif
}


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

void playNotificationSound()
{
#ifdef Q_OS_WIN
    QString soundPath = firstExistingSoundPath();
    if (soundPath.isEmpty())
        soundPath = ensureNotificationSoundPath();
    if (!soundPath.isEmpty()) {
        static QString openedPath;
        static bool openedOk = false;

        // (Re)open if needed.
        if (!openedOk || openedPath != soundPath) {
            mciSendStringW(L"close agv_notify_sound", nullptr, 0, nullptr);
            const QString openCmdAuto = QStringLiteral("open \"%1\" alias agv_notify_sound").arg(soundPath);
            MCIERROR err = mciSendStringW(reinterpret_cast<LPCWSTR>(openCmdAuto.utf16()), nullptr, 0, nullptr);
            if (err != 0) {
                const QString openCmdMpeg = QStringLiteral("open \"%1\" type mpegvideo alias agv_notify_sound").arg(soundPath);
                err = mciSendStringW(reinterpret_cast<LPCWSTR>(openCmdMpeg.utf16()), nullptr, 0, nullptr);
            }
            if (err != 0) {
                openedOk = false;
                qDebug() << "Notification sound open failed:" << mciErrorText(err) << "path=" << soundPath;
            } else {
                openedOk = true;
                openedPath = soundPath;
                // Try to raise volume (0..1000 for MCI).
                mciSendStringW(L"setaudio agv_notify_sound volume to 1000", nullptr, 0, nullptr);
            }
        }

        if (openedOk) {
            mciSendStringW(L"stop agv_notify_sound", nullptr, 0, nullptr);
            mciSendStringW(L"seek agv_notify_sound to start", nullptr, 0, nullptr);
            const MCIERROR playErr = mciSendStringW(L"play agv_notify_sound from 0", nullptr, 0, nullptr);
            if (playErr == 0)
                return;
            qDebug() << "Notification sound play failed:" << mciErrorText(playErr);
        }
    }

    // Non-standard fallback tone if MP3 isn't available.
    Beep(1046, 90);
    Beep(1396, 130);
    return;
#endif
    QApplication::beep();
}

void clearChatNotificationsForThread(const QString &username, int threadId)
{
    QString u = username.trimmed();
    if (u.isEmpty() || threadId <= 0) return;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare("UPDATE notifications SET is_read = 1 WHERE target_user = :u AND message LIKE :pattern");
    q.bindValue(":u", u);
    q.bindValue(":pattern", QString("[chat:%1]%%").arg(threadId));
    q.exec();
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