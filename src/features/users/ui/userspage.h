#pragma once

#include <QWidget>
#include <QVector>
#include <QSet>
#include <QHash>
#include <QByteArray>
#include <QString>
#include <QFutureWatcher>
#include <functional>

#include "internal/userspage_user_types.h"

namespace UsersPageInternal {
class UserItem;
}

class QVBoxLayout;
class QWidget;
class QPushButton;
class QTimer;

class UsersPage : public QWidget
{
    Q_OBJECT
public:
    explicit UsersPage(std::function<int(int)> scale, QWidget *parent = nullptr);

signals:
    void backRequested();
    void openUserDetailsRequested(const QString &username);
    void addUserRequested();
    void userUpdated(const QString &username);
    void deleteUserRequested(const QString &username);

public slots:
    void loadUsers();
    void updateUserInList(const QString &username);
    void prefetchUsers();
    void invalidatePrefetch() { prefetchedUsers_.clear(); }

private:
    void populateUsersList(const QVector<UsersPageInternal::UserData> &users);
    void beginAvatarLoad();

    std::function<int(int)> s;

    QWidget     *content = nullptr;
    QVBoxLayout *layout  = nullptr;
    QPushButton *addBtn  = nullptr;
    QTimer      *statusRefreshTimer = nullptr;
    QFutureWatcher<QHash<QString, QByteArray>> *avatarWatcher_ = nullptr;
    bool        loadingUsers_ = false;
    bool        showRecoveryKeys_ = false;
    int         loadGeneration_ = 0;
    int         pendingAvatarGeneration_ = 0;
    QSet<QString> expandedUsers_;
    QHash<QString, UsersPageInternal::UserItem *> userItems_;
    QVector<UsersPageInternal::UserData> prefetchedUsers_;
    bool        prefetchPending_ = false;
};
