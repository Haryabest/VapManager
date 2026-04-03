#pragma once

#include <QWidget>
#include <QVector>
#include <functional>

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

private:
    std::function<int(int)> s;

    QWidget     *content = nullptr;
    QVBoxLayout *layout  = nullptr;
    QPushButton *addBtn  = nullptr;
    QTimer      *statusRefreshTimer = nullptr;
    bool        loadingUsers_ = false;
};
