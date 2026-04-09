#include "../userspage.h"

#include <QCoreApplication>
#include <QLayoutItem>
#include <QLabel>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QVBoxLayout>

#include "userspage_collapsible_section.h"
#include "userspage_user_item.h"
#include "userspage_user_types.h"
#include "db_users.h"

using namespace UsersPageInternal;

void UsersPage::loadUsers()
{
    if (!isVisible())
        return;
    if (!layout || !content)
        return;

    if (loadingUsers_)
        return;

    loadingUsers_ = true;
    if (content)
        content->setUpdatesEnabled(false);

    QLayoutItem *child;
    while ((child = layout->takeAt(0)) != nullptr) {
        if (child->widget())
            delete child->widget();
        delete child;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        if (content) {
            content->setUpdatesEnabled(true);
            content->update();
        }
        loadingUsers_ = false;
        return;
    }

    QSqlQuery q(db);
    QString sql = QStringLiteral(
        R"(SELECT username, full_name, role, position, department,
               mobile, telegram, last_login, permanent_recovery_key,
               is_active, avatar
        FROM users
        WHERE username <> :hidden_user
        ORDER BY full_name ASC)"
    );
    if (QCoreApplication::instance()
        && QCoreApplication::instance()->property("autotest_running").toBool()) {
        sql += QStringLiteral(" LIMIT 200");
    }

    q.prepare(sql);
    q.bindValue(":hidden_user", hiddenAutotestUsername());

    if (!q.exec()) {
        if (content) {
            content->setUpdatesEnabled(true);
            content->update();
        }
        loadingUsers_ = false;
        return;
    }

    QVector<UserData> users;
    users.reserve(256);

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
        u.avatarBlob = q.value(10).toByteArray();

        users.push_back(u);
    }

    if (users.isEmpty()) {
        QLabel *empty = new QLabel("Здесь ничего нет", content);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#555;"
        ).arg(s(28)));

        layout->addStretch();
        layout->addWidget(empty, 0, Qt::AlignCenter);
        layout->addStretch();

        if (content) {
            content->setUpdatesEnabled(true);
            content->update();
        }
        loadingUsers_ = false;
        return;
    }

    QVector<UserData> admins;
    QVector<UserData> viewers;
    QVector<UserData> techs;
    for (const auto &u : users) {
        if (u.role == "admin")
            admins.append(u);
        else if (u.role == "tech")
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
            UserItem *item = new UserItem(u, s, sec);
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
        }

        layout->addWidget(sec);
    };

    addSection("Техники", techs, CollapsibleSection::StyleTech);
    addSection("Администраторы", admins, CollapsibleSection::StyleAdmin);
    addSection("Пользователи", viewers, CollapsibleSection::StyleViewer);

    layout->addStretch();

    if (content) {
        content->setUpdatesEnabled(true);
        content->update();
    }
    loadingUsers_ = false;
}
