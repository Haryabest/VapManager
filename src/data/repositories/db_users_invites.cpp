#include "db_users.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

#include "internal/db_users_internal_state.h"

using namespace DbUsersInternal;

bool hasAnyAdmin()
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM users WHERE role = 'admin'");
    if (!q.exec() || !q.next())
        return false;

    return q.value(0).toInt() > 0;
}

bool verifyAdminInviteKey(const QString &key, QString &error)
{
    if (key.trimmed().isEmpty()) {
        error = "Введите ключ администратора";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        error = "БД недоступна";
        return false;
    }

    QSqlQuery q(db);
    q.prepare("SELECT username FROM users WHERE role = 'admin' AND admin_invite_key = :k AND admin_invite_key_expire > NOW()");
    q.bindValue(":k", key.trimmed().toUpper());

    if (!q.exec() || !q.next()) {
        error = "Неверный или просроченный ключ";
        return false;
    }

    return true;
}

QString getAdminInviteKey(const QString &adminUsername)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return QString();

    QSqlQuery q(db);
    q.prepare("SELECT admin_invite_key, admin_invite_key_expire FROM users WHERE username = :u AND role = 'admin'");
    q.bindValue(":u", adminUsername);

    if (!q.exec() || !q.next())
        return QString();

    QString key = q.value(0).toString();
    QDateTime expire = q.value(1).toDateTime();

    if (key.isEmpty() || !expire.isValid() || expire <= QDateTime::currentDateTime())
        return QString();

    return key;
}

void refreshAdminInviteKeyIfNeeded(const QString &adminUsername)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return;

    QSqlQuery q(db);
    q.prepare("SELECT admin_invite_key_expire FROM users WHERE username = :u AND role = 'admin'");
    q.bindValue(":u", adminUsername);

    if (!q.exec() || !q.next())
        return;

    QDateTime expire = q.value(0).toDateTime();
    if (!expire.isValid() || expire <= QDateTime::currentDateTime()) {
        QString newKey = generateShortKey();
        QSqlQuery upd(db);
        upd.prepare("UPDATE users SET admin_invite_key = :k, admin_invite_key_expire = DATE_ADD(NOW(), INTERVAL 10 MINUTE) WHERE username = :u");
        upd.bindValue(":k", newKey);
        upd.bindValue(":u", adminUsername);
        upd.exec();
    }
}

bool hasAnyTech()
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM users WHERE role = 'tech'");
    if (!q.exec() || !q.next())
        return false;

    return q.value(0).toInt() > 0;
}

bool verifyTechInviteKey(const QString &key, QString &error)
{
    if (key.trimmed().isEmpty()) {
        error = "Введите ключ техника";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        error = "БД недоступна";
        return false;
    }

    QSqlQuery q(db);
    q.prepare("SELECT username FROM users WHERE role = 'tech' AND tech_invite_key = :k AND tech_invite_key_expire > NOW()");
    q.bindValue(":k", key.trimmed().toUpper());

    if (!q.exec() || !q.next()) {
        error = "Неверный или просроченный ключ техника";
        return false;
    }

    return true;
}

QString getTechInviteKey(const QString &techUsername)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return QString();

    QSqlQuery q(db);
    q.prepare("SELECT tech_invite_key, tech_invite_key_expire FROM users WHERE username = :u AND role = 'tech'");
    q.bindValue(":u", techUsername);

    if (!q.exec() || !q.next())
        return QString();

    QString key = q.value(0).toString();
    QDateTime expire = q.value(1).toDateTime();

    if (key.isEmpty() || !expire.isValid() || expire <= QDateTime::currentDateTime())
        return QString();

    return key;
}

void refreshTechInviteKeyIfNeeded(const QString &techUsername)
{
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return;

    QSqlQuery q(db);
    q.prepare("SELECT tech_invite_key_expire FROM users WHERE username = :u AND role = 'tech'");
    q.bindValue(":u", techUsername);

    if (!q.exec() || !q.next())
        return;

    QDateTime expire = q.value(0).toDateTime();
    if (!expire.isValid() || expire <= QDateTime::currentDateTime()) {
        QString newKey = generateShortKey();
        QSqlQuery upd(db);
        upd.prepare("UPDATE users SET tech_invite_key = :k, tech_invite_key_expire = DATE_ADD(NOW(), INTERVAL 10 MINUTE) WHERE username = :u");
        upd.bindValue(":k", newKey);
        upd.bindValue(":u", techUsername);
        upd.exec();
    }
}
