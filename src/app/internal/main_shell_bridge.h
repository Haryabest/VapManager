#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class MainShellBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentUsername READ currentUsername NOTIFY sessionChanged)
    Q_PROPERTY(QString currentUserRole READ currentUserRole NOTIFY sessionChanged)

public:
    explicit MainShellBridge(QObject *parent = nullptr);

    QString currentUsername() const;
    QString currentUserRole() const;

    Q_INVOKABLE QVariantMap loadSystemStatus() const;
    Q_INVOKABLE QVariantList loadCalendarEvents(int month, int year) const;
    Q_INVOKABLE QVariantList loadUpcomingMaintenance(int month, int year) const;
    Q_INVOKABLE int unreadNotificationsCount() const;

    Q_INVOKABLE QVariantMap loadCurrentUserProfile() const;
    Q_INVOKABLE bool saveCurrentUserProfile(const QVariantMap &profile);

    Q_INVOKABLE QString loadUserAvatar() const;
    Q_INVOKABLE bool saveUserAvatar(const QString &avatarData);

    Q_INVOKABLE bool switchAccount();
    Q_INVOKABLE void changeAvatar();
    Q_INVOKABLE void changeLanguage();
    Q_INVOKABLE void showAboutDialog();
    Q_INVOKABLE void showSettingsDialog();

signals:
    void sessionChanged();
    void profileUpdated();
    void avatarChanged();

private:
    void refreshRole();
    QString m_role;
};