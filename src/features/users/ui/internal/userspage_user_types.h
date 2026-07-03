#pragma once

#include <QString>
#include <QByteArray>

namespace UsersPageInternal {

struct UserData {
    QString username;
    QString fullName;
    QString role;
    QString position;
    QString department;
    QString mobile;
    QString telegram;
    QString lastLogin;
    QString recoveryKey;
    bool isActive = false;
    QByteArray avatarBlob;
};

UserData loadUserData(const QString &username);

} // namespace UsersPageInternal
