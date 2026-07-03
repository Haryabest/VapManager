#include "userspage_user_types.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

namespace UsersPageInternal {

UserData loadUserData(const QString &username)
{
    UserData u;

    QSqlQuery q(QSqlDatabase::database("main_connection"));
    q.prepare(R"(
        SELECT username, full_name, role, position, department,
               mobile, telegram, last_login, permanent_recovery_key,
               is_active, avatar
        FROM users
        WHERE username = :u
    )");
    q.bindValue(":u", username);
    q.exec();

    if (q.next()) {
        u.username = q.value(0).toString();
        u.fullName = q.value(1).toString();
        u.role = q.value(2).toString();
        u.position = q.value(3).toString();
        u.department = q.value(4).toString();
        u.mobile = q.value(5).toString();
        u.telegram = q.value(6).toString();
        u.lastLogin = q.value(7).toString();
        u.recoveryKey = q.value(8).toString();
        u.isActive = q.value(9).toInt() == 1;
        u.avatarBlob = q.value(10).toByteArray();
    }

    return u;
}

} // namespace UsersPageInternal
