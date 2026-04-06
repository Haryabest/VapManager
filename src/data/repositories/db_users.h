#pragma once

#include <QString>
#include <QPixmap>
#include <QVector>

// -------------------- UserInfo --------------------
struct UserInfo
{
    int     id = -1;
    QString username;
    QString role;
    bool    isActive = false;
    bool    expired = false;

    QString permanentRecoveryKey;

    QPixmap avatar;          // аватар из БД (QPixmap)

    // Личные данные
    QString fullName;        // ФИО
    QString employeeId;      // Табельный номер
    QString position;        // Должность
    QString department;      // Подразделение
    QString mobile;          // Телефон
    QString extPhone;        // Внутренний номер
    QString email;           // Email
    QString telegram;        // Telegram
};

// -------------------- Работа с таблицей users --------------------
QVector<UserInfo> getAllUsers(bool includeAvatars = true);
QString hiddenAutotestUsername();
bool isHiddenAutotestUser(const QString &username);
bool ensureAutotestChatUser(QString *outUsername = nullptr, QString *outError = nullptr);

bool initUsersTable();

QString localLogsDirPath();
QString localLogFilePath();

void logAction(const QString &username,
               const QString &action,
               const QString &details);

bool hasAnyAdmin();

bool verifyAdminInviteKey(const QString &key, QString &error);
QString getAdminInviteKey(const QString &adminUsername);
void refreshAdminInviteKeyIfNeeded(const QString &adminUsername);

bool hasAnyTech();
bool verifyTechInviteKey(const QString &key, QString &error);
QString getTechInviteKey(const QString &techUsername);
void refreshTechInviteKeyIfNeeded(const QString &techUsername);

bool registerUser(const QString &username,
                  const QString &password,
                  const QString &role,
                  QString &outRecoveryKey,
                  QString &error);

bool loginUser(const QString &username,
               const QString &password,
               UserInfo &outUser,
               QString &error);

bool enableRememberMe(const QString &username);
bool tryAutoLogin(UserInfo &outUser);
bool isCurrentSessionValid(const QString &username);
void logoutUser();

bool verifyPermanentRecoveryKey(const QString &key,
                                QString &outUsername,
                                QString &error);

bool setNewPassword(const QString &username,
                    const QString &newPassword,
                    QString &error);

bool regenerateRecoveryKey(const QString &username,
                           QString &outNewKey,
                           QString &error);

QString getUserRole(const QString &username);

// Профиль
bool loadUserProfile(const QString &username, UserInfo &outUser);
/// ФИО из профиля или логин, если ФИО пустое.
QString userDisplayName(const QString &username);
bool saveUserProfile(const UserInfo &user, QString &error);

// Аватар
QPixmap loadUserAvatarFromDb(const QString &username);
bool saveUserAvatarToDb(const QString &username,
                        const QPixmap &pm,
                        QString &error);

// Обновляет last_login как heartbeat активной сессии (с внутренним троттлингом).
void touchUserPresence(const QString &username);
