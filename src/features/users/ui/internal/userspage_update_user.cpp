#include "../userspage.h"

#include "userspage_user_item.h"
#include "userspage_user_types.h"

using namespace UsersPageInternal;

void UsersPage::updateUserInList(const QString &username)
{
    UserData u = loadUserData(username);

    QList<UserItem *> items = content->findChildren<UserItem *>();
    for (UserItem *item : items) {
        if (item->username() == username) {
            item->updateData(u);
            return;
        }
    }
}
