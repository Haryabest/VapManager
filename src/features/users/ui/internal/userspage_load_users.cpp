#include "../userspage.h"

#include <QCoreApplication>
#include <QLayoutItem>
#include <QLabel>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QVBoxLayout>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QThread>

#include "db.h"
#include "userspage_collapsible_section.h"
#include "userspage_user_item.h"
#include "userspage_user_types.h"
#include "db_users.h"

using namespace UsersPageInternal;

namespace {

QString workerConnectionName()
{
    return QStringLiteral("users_page_worker_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);
}

bool openWorkerDatabase(QSqlDatabase &db, QString &connName)
{
    connName = workerConnectionName();
    if (QSqlDatabase::contains(connName))
        QSqlDatabase::removeDatabase(connName);

    db = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), connName);
    db.setHostName(getDbHost());
    db.setPort(getDbPort());
    db.setDatabaseName(getDbName());
    db.setUserName(getDbUser());
    db.setPassword(getDbPassword());
    db.setConnectOptions(QStringLiteral("connect_timeout=5"));
    return db.open();
}

void closeWorkerDatabase(const QString &connName)
{
    if (connName.isEmpty())
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(connName);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(connName);
}

QVector<UserData> fetchUsersList(const QString &hiddenUser, bool limitRows)
{
    QVector<UserData> users;
    users.reserve(256);

    QSqlDatabase db;
    QString connName;
    if (!openWorkerDatabase(db, connName))
        return users;

    QSqlQuery q(db);
    QString sql = QStringLiteral(
        R"(SELECT username, full_name, role, position, department,
               mobile, telegram, last_login, permanent_recovery_key,
               is_active
        FROM users
        WHERE username <> :hidden_user
        ORDER BY full_name ASC)"
    );
    if (limitRows)
        sql += QStringLiteral(" LIMIT 200");

    q.prepare(sql);
    q.bindValue(":hidden_user", hiddenUser);

    if (!q.exec()) {
        closeWorkerDatabase(connName);
        return users;
    }

    while (q.next()) {
        UserData u;
        u.username = q.value(0).toString();

        QString full = q.value(1).toString().trimmed();
        if (full.isEmpty())
            full = u.username;

        u.fullName = full;
        u.role = q.value(2).toString();
        u.position = q.value(3).toString();
        u.department = q.value(4).toString();
        u.mobile = q.value(5).toString();
        u.telegram = q.value(6).toString();
        u.lastLogin = q.value(7).toString();
        u.recoveryKey = q.value(8).toString();
        u.isActive = q.value(9).toInt() == 1;

        users.push_back(u);
    }

    closeWorkerDatabase(connName);
    return users;
}

QHash<QString, QByteArray> fetchUserAvatars(const QString &hiddenUser)
{
    QHash<QString, QByteArray> avatars;

    QSqlDatabase db;
    QString connName;
    if (!openWorkerDatabase(db, connName))
        return avatars;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT username, avatar FROM users "
        "WHERE username <> :hidden_user AND avatar IS NOT NULL"
    ));
    q.bindValue(":hidden_user", hiddenUser);

    if (q.exec()) {
        while (q.next()) {
            const QByteArray blob = q.value(1).toByteArray();
            if (!blob.isEmpty())
                avatars.insert(q.value(0).toString(), blob);
        }
    }

    closeWorkerDatabase(connName);
    return avatars;
}

QVector<UserData> fetchUsersListOnMainThread(const QString &hiddenUser, bool limitRows)
{
    QVector<UserData> users;
    users.reserve(256);

    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen())
        return users;

    QSqlQuery q(db);
    QString sql = QStringLiteral(
        R"(SELECT username, full_name, role, position, department,
               mobile, telegram, last_login, permanent_recovery_key,
               is_active
        FROM users
        WHERE username <> :hidden_user
        ORDER BY full_name ASC)"
    );
    if (limitRows)
        sql += QStringLiteral(" LIMIT 200");

    q.prepare(sql);
    q.bindValue(":hidden_user", hiddenUser);

    if (!q.exec())
        return users;

    while (q.next()) {
        UserData u;
        u.username = q.value(0).toString();

        QString full = q.value(1).toString().trimmed();
        if (full.isEmpty())
            full = u.username;

        u.fullName = full;
        u.role = q.value(2).toString();
        u.position = q.value(3).toString();
        u.department = q.value(4).toString();
        u.mobile = q.value(5).toString();
        u.telegram = q.value(6).toString();
        u.lastLogin = q.value(7).toString();
        u.recoveryKey = q.value(8).toString();
        u.isActive = q.value(9).toInt() == 1;

        users.push_back(u);
    }

    return users;
}

} // namespace

void UsersPage::prefetchUsers()
{
    if (prefetchPending_ || !prefetchedUsers_.isEmpty())
        return;

    prefetchPending_ = true;
    const QString hiddenUser = hiddenAutotestUsername();
    const bool limitRows = QCoreApplication::instance()
        && QCoreApplication::instance()->property("autotest_running").toBool();

    auto *watcher = new QFutureWatcher<QVector<UserData>>(this);
    connect(watcher, &QFutureWatcher<QVector<UserData>>::finished, this, [this, watcher]() {
        prefetchedUsers_ = watcher->result();
        prefetchPending_ = false;
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([hiddenUser, limitRows]() {
        return fetchUsersList(hiddenUser, limitRows);
    }));
}

void UsersPage::populateUsersList(const QVector<UserData> &users)
{
    userItems_.clear();

    if (users.isEmpty()) {
        QLabel *empty = new QLabel(QStringLiteral("Здесь ничего нет"), content);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#555;"
        ).arg(s(28)));

        layout->addStretch();
        layout->addWidget(empty, 0, Qt::AlignCenter);
        layout->addStretch();
        return;
    }

    QVector<UserData> admins;
    QVector<UserData> viewers;
    QVector<UserData> techs;
    for (const auto &u : users) {
        if (u.role == QStringLiteral("admin"))
            admins.append(u);
        else if (u.role == QStringLiteral("tech"))
            techs.append(u);
        else
            viewers.append(u);
    }

    auto addSection = [&](const QString &title,
                          const QVector<UserData> &list,
                          CollapsibleSection::SectionStyle style) {
        if (list.isEmpty())
            return;

        CollapsibleSection *sec = new CollapsibleSection(
            QString("%1 (%2)").arg(title).arg(list.size()),
            true,
            s,
            content,
            style
        );

        for (const auto &u : list) {
            UserItem *item = new UserItem(u, s, showRecoveryKeys_, sec);
            item->onOpenDetails = [this](const QString &username) {
                emit openUserDetailsRequested(username);
            };
            item->onExpandedChanged = [this](const QString &username, bool expanded) {
                if (expanded)
                    expandedUsers_.insert(username);
                else
                    expandedUsers_.remove(username);
            };
            item->setExpanded(expandedUsers_.contains(u.username));
            sec->contentLayout()->addWidget(item);
            userItems_.insert(u.username, item);
        }

        layout->addWidget(sec);
    };

    addSection(QStringLiteral("Разработчики"), techs, CollapsibleSection::StyleTech);
    addSection(QStringLiteral("Администраторы"), admins, CollapsibleSection::StyleAdmin);
    addSection(QStringLiteral("Пользователи"), viewers, CollapsibleSection::StyleViewer);
    layout->addStretch();
}

void UsersPage::beginAvatarLoad()
{
    if (!avatarWatcher_ || userItems_.isEmpty())
        return;

    pendingAvatarGeneration_ = loadGeneration_;
    const QString hiddenUser = hiddenAutotestUsername();
    avatarWatcher_->setFuture(QtConcurrent::run(fetchUserAvatars, hiddenUser));
}

void UsersPage::loadUsers()
{
    if (!isVisible())
        return;
    if (!layout || !content)
        return;

    if (loadingUsers_)
        return;

    loadingUsers_ = true;
    ++loadGeneration_;

    if (content)
        content->setUpdatesEnabled(false);

    QLayoutItem *child;
    while ((child = layout->takeAt(0)) != nullptr) {
        if (child->widget())
            delete child->widget();
        delete child;
    }
    userItems_.clear();

    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen()) {
        if (content) {
            content->setUpdatesEnabled(true);
            content->update();
        }
        loadingUsers_ = false;
        return;
    }

    const QString hiddenUser = hiddenAutotestUsername();
    const bool limitRows = QCoreApplication::instance()
        && QCoreApplication::instance()->property("autotest_running").toBool();

    QVector<UserData> users = prefetchedUsers_;
    prefetchedUsers_.clear();
    if (users.isEmpty())
        users = fetchUsersListOnMainThread(hiddenUser, limitRows);

    populateUsersList(users);

    if (content) {
        content->setUpdatesEnabled(true);
        content->update();
    }
    loadingUsers_ = false;

    beginAvatarLoad();
}
