#pragma once

#include <QFrame>
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
    UserItem(const UserData &data, std::function<int(int)> scale, QWidget *parent = nullptr);

    std::function<void(const QString &)> onOpenDetails;
    std::function<void(const QString &, bool)> onExpandedChanged;

    QString username() const;
    bool isExpanded() const;
    void setExpanded(bool expanded);
    void updateData(const UserData &data);

private:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void rebuildUI();
    void build();

    UserData data_;
    std::function<int(int)> s_;
    QWidget *header_ = nullptr;
    QWidget *details_ = nullptr;
    QLabel *arrow_ = nullptr;
};

} // namespace UsersPageInternal
