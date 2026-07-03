#include "../userspage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QDialog>
#include <QCheckBox>
#include <QFrame>
#include <QLineEdit>
#include <QTextEdit>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QMessageBox>
#include <QMap>

#include "databus.h"
#include "app_session.h"
#include "db_users.h"
#include "notifications_logs.h"

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
    const bool canManageUsers = (currentRole == QStringLiteral("admin") || currentRole == QStringLiteral("tech"));

    QPushButton *broadcastBtn = new QPushButton(QStringLiteral("Создать рассылку"), header);
    broadcastBtn->setFixedSize(s(210), s(50));
    broadcastBtn->setStyleSheet(QString(
        "QPushButton{background-color:#0F00DB;border:1px solid #0B00A8;border-radius:%1px;"
        "font-family:Inter;font-size:%2px;font-weight:800;color:white;}"
        "QPushButton:hover{background-color:#1A4ACD;}"
    ).arg(s(10)).arg(s(16)));
    broadcastBtn->setVisible(canManageUsers);

    connect(broadcastBtn, &QPushButton::clicked, this, [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("Рассылка уведомлений"));
        dlg.setMinimumSize(s(520), s(620));
        dlg.setStyleSheet(QStringLiteral(
            "QDialog{background:#F7FAFF;border:1px solid #E2E8F0;border-radius:12px;}"));

        QVBoxLayout *dlgRoot = new QVBoxLayout(&dlg);
        dlgRoot->setContentsMargins(s(16), s(16), s(16), s(16));
        dlgRoot->setSpacing(s(12));

        QLabel *dlgTitle = new QLabel(QStringLiteral("Новая рассылка"), &dlg);
        dlgTitle->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;background:transparent;"
        ).arg(s(20)));
        dlgRoot->addWidget(dlgTitle);

        QLabel *subjectLbl = new QLabel(QStringLiteral("Тема"), &dlg);
        subjectLbl->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:700;color:#334155;background:transparent;"
        ).arg(s(14)));
        dlgRoot->addWidget(subjectLbl);

        QLineEdit *subjectEdit = new QLineEdit(&dlg);
        subjectEdit->setPlaceholderText(QStringLiteral("Заголовок уведомления"));
        subjectEdit->setStyleSheet(QString(
            "QLineEdit{background:white;border:1px solid #DCE2EE;border-radius:%1px;"
            "font-family:Inter;font-size:%2px;padding:%3px %4px;}"
        ).arg(s(8)).arg(s(14)).arg(s(8)).arg(s(10)));
        dlgRoot->addWidget(subjectEdit);

        QLabel *bodyLbl = new QLabel(QStringLiteral("Текст"), &dlg);
        bodyLbl->setStyleSheet(subjectLbl->styleSheet());
        dlgRoot->addWidget(bodyLbl);

        QTextEdit *bodyEdit = new QTextEdit(&dlg);
        bodyEdit->setPlaceholderText(QStringLiteral("Текст сообщения для выбранных пользователей"));
        bodyEdit->setMinimumHeight(s(120));
        bodyEdit->setStyleSheet(QString(
            "QTextEdit{background:white;border:1px solid #DCE2EE;border-radius:%1px;"
            "font-family:Inter;font-size:%2px;padding:%3px;}"
        ).arg(s(8)).arg(s(14)).arg(s(8)));
        dlgRoot->addWidget(bodyEdit);

        QCheckBox *allUsersCb = new QCheckBox(QStringLiteral("Всем пользователям"), &dlg);
        allUsersCb->setStyleSheet(QString(
            "QCheckBox{font-family:Inter;font-size:%1px;font-weight:800;color:#1A1A1A;background:transparent;}"
        ).arg(s(15)));
        dlgRoot->addWidget(allUsersCb);

        QScrollArea *scroll = new QScrollArea(&dlg);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet(QStringLiteral(
            "QScrollArea{border:none;background:transparent;}"
            "QScrollArea > QWidget > QWidget{background:transparent;}"));
        scroll->viewport()->setStyleSheet(QStringLiteral("background:transparent;"));

        QWidget *listWidget = new QWidget(scroll);
        listWidget->setAutoFillBackground(false);
        listWidget->setStyleSheet(QStringLiteral("background:transparent;"));
        QVBoxLayout *listLayout = new QVBoxLayout(listWidget);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(s(6));

        QVector<QPair<QString, QCheckBox *>> userBoxes;
        QSqlQuery q(QSqlDatabase::database(QStringLiteral("main_connection")));
        q.prepare(QStringLiteral(
            "SELECT username, full_name FROM users "
            "WHERE is_active = TRUE AND username <> :hidden_user "
            "ORDER BY full_name ASC"));
        q.bindValue(QStringLiteral(":hidden_user"), hiddenAutotestUsername());
        if (q.exec()) {
            while (q.next()) {
                const QString username = q.value(0).toString().trimmed();
                const QString full = q.value(1).toString().trimmed();
                if (username.isEmpty())
                    continue;
                const QString label = full.isEmpty() ? username : QStringLiteral("%1 — %2").arg(full, username);
                QCheckBox *cb = new QCheckBox(label, listWidget);
                cb->setProperty("username", username);
                cb->setStyleSheet(QString(
                    "QCheckBox{font-family:Inter;font-size:%1px;color:#1A1A1A;background:transparent;}"
                ).arg(s(15)));
                userBoxes.push_back({username, cb});
                listLayout->addWidget(cb);
            }
        }
        listLayout->addStretch();
        scroll->setWidget(listWidget);
        dlgRoot->addWidget(scroll, 1);

        connect(allUsersCb, &QCheckBox::toggled, &dlg, [&](bool checked) {
            for (const auto &entry : userBoxes) {
                entry.second->blockSignals(true);
                entry.second->setChecked(checked);
                entry.second->blockSignals(false);
            }
        });
        allUsersCb->setChecked(true);
        for (const auto &entry : userBoxes)
            entry.second->setChecked(true);

        for (const auto &entry : userBoxes) {
            connect(entry.second, &QCheckBox::toggled, &dlg, [&, allUsersCb](bool /*checked*/) {
                if (allUsersCb->isChecked()) {
                    bool allChecked = true;
                    for (const auto &e : userBoxes) {
                        if (!e.second->isChecked()) {
                            allChecked = false;
                            break;
                        }
                    }
                    if (!allChecked) {
                        allUsersCb->blockSignals(true);
                        allUsersCb->setChecked(false);
                        allUsersCb->blockSignals(false);
                    }
                    return;
                }
                bool allChecked = true;
                for (const auto &e : userBoxes) {
                    if (!e.second->isChecked()) {
                        allChecked = false;
                        break;
                    }
                }
                if (allChecked && !userBoxes.isEmpty()) {
                    allUsersCb->blockSignals(true);
                    allUsersCb->setChecked(true);
                    allUsersCb->blockSignals(false);
                }
            });
        }

        QHBoxLayout *btns = new QHBoxLayout();
        btns->setSpacing(s(12));
        QPushButton *cancel = new QPushButton(QStringLiteral("Отмена"), &dlg);
        cancel->setFixedHeight(s(44));
        cancel->setStyleSheet(QString(
            "QPushButton{background:#E6E6E6;border-radius:%1px;font-family:Inter;"
            "font-size:%2px;font-weight:700;color:#333;}"
            "QPushButton:hover{background:#D5D5D5;}"
        ).arg(s(8)).arg(s(16)));

        QPushButton *send = new QPushButton(QStringLiteral("Отправить"), &dlg);
        send->setFixedHeight(s(44));
        send->setStyleSheet(QString(
            "QPushButton{background:#0F00DB;border-radius:%1px;font-family:Inter;"
            "font-size:%2px;font-weight:800;color:white;}"
            "QPushButton:hover{background:#1A4ACD;}"
        ).arg(s(8)).arg(s(16)));

        btns->addWidget(cancel);
        btns->addWidget(send);
        dlgRoot->addLayout(btns);

        connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
        connect(send, &QPushButton::clicked, &dlg, [&]() {
            const QString title = subjectEdit->text().trimmed();
            const QString body = bodyEdit->toPlainText().trimmed();
            if (title.isEmpty() || body.isEmpty()) {
                QMessageBox::warning(&dlg,
                                     QStringLiteral("Рассылка"),
                                     QStringLiteral("Заполните тему и текст сообщения."));
                return;
            }

            QStringList recipients;
            if (allUsersCb->isChecked()) {
                for (const auto &entry : userBoxes)
                    recipients.append(entry.first);
            } else {
                for (const auto &entry : userBoxes) {
                    if (entry.second->isChecked())
                        recipients.append(entry.first);
                }
            }

            if (recipients.isEmpty()) {
                QMessageBox::warning(&dlg,
                                     QStringLiteral("Рассылка"),
                                     QStringLiteral("Выберите хотя бы одного получателя или «Всем пользователям»."));
                return;
            }

            const QString sender = AppSession::currentUsername().trimmed();

            for (const QString &user : recipients) {
                const QString storedMessage = QStringLiteral("[broadcast][from:%1]\n%2")
                                                  .arg(sender, body);
                addNotificationForUser(user, title, storedMessage);
            }

            if (!sender.isEmpty() && !recipients.contains(sender, Qt::CaseInsensitive)) {
                addNotificationForUser(
                    sender,
                    QStringLiteral("Рассылка отправлена"),
                    QStringLiteral("Тема «%1» — доставлено %2 пользователям.")
                        .arg(title)
                        .arg(recipients.size()));
            }

            logAction(AppSession::currentUsername(),
                      QStringLiteral("notification_broadcast"),
                      QStringLiteral("Рассылка «%1» — %2 получателей")
                          .arg(title)
                          .arg(recipients.size()));

            DataBus::instance().triggerNotificationsChanged();
            dlg.accept();
        });

        if (dlg.exec() == QDialog::Accepted) {
            QMessageBox::information(this,
                                     QStringLiteral("Рассылка"),
                                     QStringLiteral("Уведомления отправлены."));
        }
    });

    QPushButton *deleteBtn = new QPushButton("Удалить", header);
    deleteBtn->setFixedSize(s(165), s(50));
    deleteBtn->setStyleSheet(QString(
        "QPushButton{ background-color:#FF3B30; border:1px solid #C72B22; border-radius:%1px;"
        "font-family:Inter; font-size:%2px; font-weight:800; color:white; }"
        "QPushButton:hover{background-color:#E4372D;}"
    ).arg(s(10)).arg(s(16)));
    if (!canManageUsers)
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

    hdr->addWidget(broadcastBtn, 0, Qt::AlignRight);
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
