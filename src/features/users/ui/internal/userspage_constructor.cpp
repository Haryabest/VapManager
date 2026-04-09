#include "../userspage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QDialog>
#include <QCheckBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QMessageBox>
#include <QMap>

#include "databus.h"
#include "app_session.h"
#include "db_users.h"

UsersPage::UsersPage(std::function<int(int)> scale, QWidget *parent)
    : QWidget(parent), s(scale)
{
    setStyleSheet("background:#F1F2F4;");
    setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(s(10), s(10), s(10), s(10));
    root->setSpacing(s(12));

    QWidget *header = new QWidget(this);
    header->setStyleSheet("background:transparent;");
    QHBoxLayout *hdr = new QHBoxLayout(header);
    hdr->setContentsMargins(0, 0, 0, 0);
    hdr->setSpacing(s(10));

    QPushButton *back = new QPushButton("   Назад", header);
    back->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    back->setIconSize(QSize(s(24), s(24)));
    back->setFixedSize(s(150), s(50));
    back->setStyleSheet(QString(
        "QPushButton { background-color:#E6E6E6; border-radius:%1px; border:1px solid #C8C8C8;"
        "font-family:Inter; font-size:%2px; font-weight:800; color:black; text-align:left; padding-left:%3px; }"
        "QPushButton:hover { background-color:#D5D5D5; }"
    ).arg(s(10)).arg(s(16)).arg(s(10)));

    connect(back, &QPushButton::clicked, this, [this]() {
        emit backRequested();
    });

    hdr->addWidget(back, 0, Qt::AlignLeft);
    hdr->addStretch();

    QLabel *title = new QLabel("Пользователи", header);
    title->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;background:transparent;"
    ).arg(s(26)));
    title->setAlignment(Qt::AlignCenter);

    hdr->addWidget(title, 0, Qt::AlignCenter);
    hdr->addStretch();

    QString currentRole = getUserRole(AppSession::currentUsername());
    QPushButton *deleteBtn = new QPushButton("Удалить", header);
    deleteBtn->setFixedSize(s(165), s(50));
    deleteBtn->setStyleSheet(QString(
        "QPushButton{ background-color:#FF3B30; border:1px solid #C72B22; border-radius:%1px;"
        "font-family:Inter; font-size:%2px; font-weight:800; color:white; }"
        "QPushButton:hover{background-color:#E4372D;}"
    ).arg(s(10)).arg(s(16)));
    if (currentRole != "admin" && currentRole != "tech")
        deleteBtn->setVisible(false);

    connect(deleteBtn, &QPushButton::clicked, this, [this, currentRole]() {
        QDialog dlg(this);
        dlg.setWindowTitle("Удаление пользователей");
        dlg.setFixedSize(s(460), s(520));
        dlg.setStyleSheet("background:white;border-radius:12px;");

        QVBoxLayout *root = new QVBoxLayout(&dlg);
        root->setContentsMargins(s(16), s(16), s(16), s(16));
        root->setSpacing(s(14));

        QLabel *title = new QLabel("Выберите пользователей для удаления", &dlg);
        title->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
        ).arg(s(20)));
        root->addWidget(title);

        QScrollArea *scroll = new QScrollArea(&dlg);
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");

        QWidget *listWidget = new QWidget(scroll);
        QVBoxLayout *listLayout = new QVBoxLayout(listWidget);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(s(6));

        QVector<QCheckBox *> boxes;
        QMap<QString, QString> userRoles;

        QSqlQuery q(QSqlDatabase::database("main_connection"));
        q.prepare(R"(
            SELECT username, full_name, role
            FROM users
            WHERE username <> :hidden_user
            ORDER BY full_name ASC
        )");
        q.bindValue(":hidden_user", hiddenAutotestUsername());
        q.exec();

        QString current = AppSession::currentUsername();

        while (q.next()) {
            QString username = q.value(0).toString();
            QString full = q.value(1).toString();
            QString role = q.value(2).toString();
            userRoles[username] = role;

            if (currentRole == "admin" && role != "viewer")
                continue;

            QCheckBox *cb = new QCheckBox(QString("%1 — %2").arg(full, username), listWidget);
            cb->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:#1A1A1A;").arg(s(16)));

            if (username == current) {
                cb->setEnabled(false);
                cb->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:#888;").arg(s(16)));
            }

            boxes.push_back(cb);
            listLayout->addWidget(cb);
        }

        listLayout->addStretch();
        scroll->setWidget(listWidget);
        root->addWidget(scroll);

        QHBoxLayout *btns = new QHBoxLayout();
        btns->setSpacing(s(12));

        QPushButton *cancel = new QPushButton("Отмена", &dlg);
        cancel->setFixedHeight(s(44));
        cancel->setStyleSheet(QString(
            "QPushButton{background:#E6E6E6;border-radius:%1px;font-family:Inter;"
            "font-size:%2px;font-weight:700;color:#333;} "
            "QPushButton:hover{background:#D5D5D5;}"
        ).arg(s(8)).arg(s(16)));

        QPushButton *del = new QPushButton("Удалить выбранных", &dlg);
        del->setFixedHeight(s(44));
        del->setStyleSheet(QString(
            "QPushButton{background:#FF3B30;border-radius:%1px;font-family:Inter;"
            "font-size:%2px;font-weight:800;color:white;} "
            "QPushButton:hover{background:#E4372D;}"
        ).arg(s(8)).arg(s(16)));

        btns->addWidget(cancel);
        btns->addWidget(del);
        root->addLayout(btns);

        connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

        connect(del, &QPushButton::clicked, this, [&, this, currentRole, userRoles]() {
            QVector<QString> toDelete;
            for (auto *cb : boxes) {
                if (!cb->isChecked())
                    continue;
                QString username = cb->text().split("—").last().trimmed();
                if (currentRole == "admin" && userRoles.value(username) != "viewer")
                    continue;
                toDelete.push_back(username);
            }

            if (toDelete.isEmpty()) {
                dlg.reject();
                return;
            }

            if (currentRole == "tech") {
                bool hasAdmin = false;
                for (const QString &u : toDelete) {
                    if (userRoles.value(u) == "admin") {
                        hasAdmin = true;
                        break;
                    }
                }
                if (hasAdmin) {
                    auto ret = QMessageBox::question(
                        this,
                        "Подтверждение",
                        "Ты точно хочешь удалить администратора?",
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No
                    );
                    if (ret != QMessageBox::Yes)
                        return;
                }
            }

            QSqlDatabase db = QSqlDatabase::database("main_connection");
            for (const QString &u : toDelete) {
                QSqlQuery updAgv(db);
                updAgv.prepare("UPDATE agv_list SET assigned_user = '' WHERE assigned_user = :u");
                updAgv.bindValue(":u", u);
                updAgv.exec();

                QSqlQuery updTasks(db);
                updTasks.prepare("UPDATE agv_tasks SET assigned_to = '', delegated_by = '' WHERE assigned_to = :u OR delegated_by = :u");
                updTasks.bindValue(":u", u);
                updTasks.exec();

                QSqlQuery updHist(db);
                updHist.prepare("UPDATE agv_task_history SET performed_by = '' WHERE performed_by = :u");
                updHist.bindValue(":u", u);
                updHist.exec();

                QSqlQuery delNotif(db);
                delNotif.prepare("DELETE FROM notifications WHERE target_user = :u");
                delNotif.bindValue(":u", u);
                delNotif.exec();
            }

            QSqlQuery qq(db);
            qq.prepare("DELETE FROM users WHERE username = :u");
            for (const QString &u : toDelete) {
                qq.bindValue(":u", u);
                qq.exec();
            }

            emit DataBus::instance().agvListChanged();
            dlg.accept();
        });

        if (dlg.exec() == QDialog::Accepted)
            loadUsers();
    });

    hdr->addWidget(deleteBtn, 0, Qt::AlignRight);
    root->addWidget(header);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");

    content = new QWidget();
    content->setStyleSheet("background:transparent;");
    layout = new QVBoxLayout(content);
    layout->setSpacing(s(8));
    layout->setContentsMargins(0, 0, 0, 0);

    scroll->setWidget(content);
    root->addWidget(scroll);

    statusRefreshTimer = new QTimer(this);
    statusRefreshTimer->setInterval(60000);
    connect(statusRefreshTimer, &QTimer::timeout, this, &UsersPage::loadUsers);
    statusRefreshTimer->start();

    loadUsers();
}
