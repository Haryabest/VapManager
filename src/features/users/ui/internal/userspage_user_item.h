#pragma once

#include <QFrame>
#include <QPixmap>
#include <functional>

#include "userspage_user_types.h"

class QWidget;
class QLabel;
class QObject;
class QEvent;

namespace UsersPageInternal {

class UserItem : public QFrame
{
public:
    UserItem(const UserData &data,
             std::function<int(int)> scale,
             bool showRecoveryKey,
             QWidget *parent = nullptr);

    std::function<void(const QString &)> onOpenDetails;
    std::function<void(const QString &, bool)> onExpandedChanged;

    QString username() const;
    bool isExpanded() const;
    void setExpanded(bool expanded);
    void updateData(const UserData &data);
    void setAvatarPixmap(const QPixmap &pm);

private:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void rebuildUI();
    void build();
    QPixmap makeAvatarPixmap(const QPixmap &source) const;

    UserData data_;
    std::function<int(int)> s_;
    bool showRecoveryKey_ = false;
    QWidget *header_ = nullptr;
    QWidget *details_ = nullptr;
    QLabel *arrow_ = nullptr;
    QLabel *avatarLabel_ = nullptr;
};

} // namespace UsersPageInternal
