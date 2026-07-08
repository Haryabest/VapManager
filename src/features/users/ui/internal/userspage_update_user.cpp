#include "../userspage.h"

#include "userspage_user_item.h"
#include "userspage_user_types.h"

using namespace UsersPageInternal;

void UsersPage::updateUserInList(const QString &username)
{
    UserData u = loadUserData(username);

    UserItem *item = userItems_.value(username);
    if (!item) {
        QList<UserItem *> items = content->findChildren<UserItem *>();
        for (UserItem *candidate : items) {
            if (candidate->username() == username) {
                item = candidate;
                break;
            }
        }
    }

    if (!item)
        return;

    item->updateData(u);
    if (!u.avatarBlob.isEmpty()) {
        QPixmap pm;
        if (pm.loadFromData(u.avatarBlob))
            item->setAvatarPixmap(pm);
    }
}
