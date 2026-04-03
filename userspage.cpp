#include "userspage.h"
#include "databus.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include <QMouseEvent>
#include <QCheckBox>
#include <QDialog>
#include <QFrame>
#include <QTimer>
#include <QMessageBox>
#include <QMap>
#include <QCoreApplication>

#include "app_session.h"
#include "db_users.h"

//
// ======================= CollapsibleSection =======================
//

class CollapsibleSection : public QFrame
{
public:
    enum SectionStyle { StyleDefault, StyleTech, StyleAdmin, StyleViewer };
    CollapsibleSection(const QString &title, bool expandedByDefault,
                      std::function<int(int)> scale, QWidget *parent = nullptr,
                      SectionStyle style = StyleDefault)
        : QFrame(parent), s_(scale), expanded_(expandedByDefault)
    {
        setStyleSheet("QFrame{background:transparent;}");
        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        QString bg, bgHover, textColor;
        switch (style) {
        case StyleTech:   bg = "#22C55E"; bgHover = "#16A34A"; textColor = "#FFFFFF"; break;  // Зелёный
        case StyleAdmin:  bg = "#EF4444"; bgHover = "#DC2626"; textColor = "#FFFFFF"; break;  // Красный
        case StyleViewer: bg = "#6B7280"; bgHover = "#4B5563"; textColor = "#FFFFFF"; break;  // Серый
        default:          bg = "#E8EAED"; bgHover = "#D8DAE0"; textColor = "#1A1A1A"; break;
        }
        headerBtn_ = new QPushButton(this);
        headerBtn_->setCursor(Qt::PointingHandCursor);
        headerBtn_->setStyleSheet(QString(
            "QPushButton{background:%1;border:none;border-radius:8px;text-align:left;"
            "font-family:Inter;font-size:%2px;font-weight:800;color:%3;padding:%4px %5px;}"
            "QPushButton:hover{background:%6;}"
        ).arg(bg).arg(s_(16)).arg(textColor).arg(s_(10)).arg(s_(14)).arg(bgHover));
        headerBtn_->setFixedHeight(s_(44));

        QHBoxLayout *h = new QHBoxLayout(headerBtn_);
        h->setContentsMargins(s_(12), 0, s_(12), 0);
        h->setSpacing(s_(8));
        arrowLbl_ = new QLabel(headerBtn_);
        arrowLbl_->setFixedWidth(s_(20));
        titleLbl_ = new QLabel(title, headerBtn_);
        titleLbl_->setStyleSheet("background:transparent;font:inherit;");
        h->addWidget(arrowLbl_);
        h->addWidget(titleLbl_);
        h->addStretch();

        content_ = new QWidget(this);
        content_->setStyleSheet("background:transparent;");
        contentLayout_ = new QVBoxLayout(content_);
        contentLayout_->setContentsMargins(s_(8), s_(6), 0, s_(8));
        contentLayout_->setSpacing(s_(6));

        root->addWidget(headerBtn_);
        root->addWidget(content_);

        connect(headerBtn_, &QPushButton::clicked, this, [this](){
            expanded_ = !expanded_;
            updateArrow();
            content_->setVisible(expanded_);
        });
        updateArrow();
        content_->setVisible(expanded_);
    }

    QVBoxLayout *contentLayout() { return contentLayout_; }
    void setTitle(const QString &t) { titleLbl_->setText(t); }

private:
    void updateArrow() {
        arrowLbl_->setText(expanded_ ? "▼" : "▶");
        arrowLbl_->setStyleSheet("background:transparent;font-size:14px;color:#555;");
    }
    QPushButton *headerBtn_ = nullptr;
    QLabel *arrowLbl_ = nullptr;
    QLabel *titleLbl_ = nullptr;
    QWidget *content_ = nullptr;
    QVBoxLayout *contentLayout_ = nullptr;
    std::function<int(int)> s_;
    bool expanded_;
};

//
// ======================= ВНУТРЕННИЙ КЛАСС UserItem =======================
//

class UserItem : public QFrame
{
public:
    struct UserData {
        QString username;
        QString fullName;
        QString role;
        QString position;
        QString department;
        QString mobile;
        QString telegram;
        QString lastLogin;
        QString recoveryKey;
        bool    isActive;
        QByteArray avatarBlob;
    };

    std::function<void(const QString &)> onOpenDetails;

    UserItem(const UserData &u, std::function<int(int)> scale, QWidget *parent = nullptr)
        : QFrame(parent), data(u), s(scale)
    {
        setObjectName("userItem");
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        setStyleSheet(
            "#userItem{background:white;border-radius:10px;border:1px solid #E0E0E0;}"
            "#userItem:hover{background:#F7F7F7;}"
        );

        build();
    }

    QString username() const { return data.username; }

    void updateData(const UserData &u)
    {
        data = u;
        rebuildUI();
    }

private:
    UserData data;
    std::function<int(int)> s;
    QWidget *header = nullptr;
    QWidget *details = nullptr;
    QLabel  *arrow = nullptr;

    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (obj == this && event->type() == QEvent::MouseButtonRelease)
        {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);

            QWidget *clicked = childAt(me->pos());
            if (clicked && qobject_cast<QPushButton*>(clicked))
                return QFrame::eventFilter(obj, event);

            bool vis = details->isVisible();
            details->setVisible(!vis);

            arrow->setPixmap(QPixmap(
                vis ?
                ":/new/mainWindowIcons/noback/arrow_down.png" :
                ":/new/mainWindowIcons/noback/arrow_up.png"
            ).scaled(s(18), s(18)));

            return true;
        }

        return QFrame::eventFilter(obj, event);
    }

    void rebuildUI()
    {
        QLayoutItem *child;
        while ((child = layout()->takeAt(0)) != nullptr) {
            if (child->widget())
                child->widget()->deleteLater();
            delete child;
        }
        build();
    }

    void build()
    {
        QHBoxLayout *root = new QHBoxLayout(this);
        root->setContentsMargins(s(12), s(10), s(12), s(10));
        root->setSpacing(s(12));

        QWidget *leftCol = new QWidget(this);
        QVBoxLayout *left = new QVBoxLayout(leftCol);
        left->setContentsMargins(0, 0, 0, 0);
        left->setSpacing(s(6));

        //
        // === HEADER ===
        //
        header = new QWidget(leftCol);
        header->setCursor(Qt::PointingHandCursor);
        this->installEventFilter(this);

        QHBoxLayout *h = new QHBoxLayout(header);
        h->setContentsMargins(0,0,0,0);
        h->setSpacing(s(10));

        //
        // === АВАТАР ===
        //
        QLabel *avatar = new QLabel(header);
        avatar->setFixedSize(s(42), s(42));

        QPixmap pm;
        if (!data.avatarBlob.isEmpty())
            pm.loadFromData(data.avatarBlob);

        QPixmap final(s(42), s(42));
        final.fill(Qt::transparent);

        {
            QPainter p(&final);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);

            QPainterPath path;
            path.addEllipse(0, 0, s(42), s(42));
            p.setClipPath(path);

            if (!pm.isNull()) {
                QPixmap scaled = pm.scaled(s(42), s(42),
                                           Qt::KeepAspectRatioByExpanding,
                                           Qt::SmoothTransformation);
                p.drawPixmap(0, 0, scaled);
            } else {
                p.setBrush(QColor("#888"));
                p.setPen(Qt::NoPen);
                p.drawEllipse(0, 0, s(42), s(42));

                p.setPen(Qt::white);
                QFont f("Inter");
                f.setPointSize(s(16));
                f.setBold(true);
                p.setFont(f);

                QString initials;
                for (const QString &part : data.fullName.split(" ", Qt::SkipEmptyParts))
                    initials += part.left(1).toUpper();

                p.drawText(final.rect(), Qt::AlignCenter, initials.left(2));
            }
        }

        avatar->setPixmap(final);

        //
        // === РОЛЬ (цветная) ===
        //
        QString roleText;
        QString roleColor;

        if (data.role == "admin") {
            roleText = "[admin]";
            roleColor = "#FF3B30";
        } else if (data.role == "tech") {
            roleText = "[tech]";
            roleColor = "#18CF00";
        } else {
            roleText = "[user]";
            roleColor = "#777777";
        }

        QLabel *title = new QLabel(
            QString("%1 — %2").arg(data.fullName).arg(data.username),
            header
        );
        title->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
        ).arg(s(16)));

        QLabel *roleLabel = new QLabel(roleText, header);
        roleLabel->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:%2;"
        ).arg(s(16)).arg(roleColor));

        arrow = new QLabel(header);
        arrow->setPixmap(QPixmap(":/new/mainWindowIcons/noback/arrow_down.png")
                         .scaled(s(18), s(18)));

        h->addWidget(avatar);
        h->addWidget(title);
        h->addWidget(roleLabel);
        h->addStretch();
        h->addWidget(arrow);

        left->addWidget(header);

        //
        // === АКТИВНОСТЬ ПОД ФИО ===
        //
        QString formattedLastLogin;
        {
            QString src = data.lastLogin.trimmed();
            QDateTime dt;

            const QStringList strFormats = {
                "yyyy-MM-dd hh:mm:ss",
                "yyyy-MM-dd hh:mm",
                "yyyy/MM/dd hh:mm:ss",
                "dd.MM.yyyy hh:mm:ss"
            };

            for (const QString &f : strFormats) {
                dt = QDateTime::fromString(src, f);
                if (dt.isValid()) break;
            }

            if (!dt.isValid()) dt = QDateTime::fromString(src, Qt::ISODate);
            if (!dt.isValid()) dt = QDateTime::fromString(src, Qt::ISODateWithMs);

            formattedLastLogin = dt.isValid() ? dt.toString("dd.MM.yy — hh:mm") : src;
        }

        QLabel *lastUnderTitle = new QLabel("Активность: " + formattedLastLogin, leftCol);
        lastUnderTitle->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:600;color:#333;"
        ).arg(s(14)));
        lastUnderTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        left->addWidget(lastUnderTitle);

        //
        // === DETAILS (РАСКРЫВАЮЩИЙСЯ) ===
        //
        details = new QWidget(leftCol);
        details->setVisible(false);

        QVBoxLayout *d = new QVBoxLayout(details);
        d->setContentsMargins(s(5), s(5), s(5), s(5));
        d->setSpacing(s(6));

        auto makeRow = [&](QString label, QString value){
            QLabel *l = new QLabel(QString("<b>%1:</b> %2").arg(label, value), details);
            l->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;color:#1A1A1A;"
            ).arg(s(14)));
            return l;
        };

        d->addWidget(makeRow("Должность", data.position));
        d->addWidget(makeRow("Телефон", data.mobile));
        d->addWidget(makeRow("Telegram", data.telegram));
        // Критично для производительности на удаленной БД:
        // раньше здесь был SQL-запрос role для КАЖДОГО пользователя (N+1).
        // Держим роль текущего пользователя в простом кеше по currentUsername.
        static QString cachedRoleForUser;
        static QString cachedRole;
        const QString currentUser = AppSession::currentUsername();
        if (cachedRoleForUser != currentUser) {
            cachedRoleForUser = currentUser;
            cachedRole = getUserRole(currentUser);
        }
        const QString currentRole = cachedRole;

        if (currentRole == "tech") {
            d->addWidget(makeRow("Recovery key", data.recoveryKey));
        }

        left->addWidget(details);

        //
        // === ПРАВАЯ КОЛОНКА (ТОЧКА + КНОПКА) ===
        //
        QWidget *rightCol = new QWidget(this);
        rightCol->setMinimumWidth(s(180));
        QVBoxLayout *right = new QVBoxLayout(rightCol);
        right->setContentsMargins(s(8), s(4), s(2), s(4));
        right->setSpacing(s(10));

        qint64 lastLoginSecs = -1;
        {
            QDateTime dt;
            QString src = data.lastLogin.trimmed();
            for (const QString &f : QStringList{"yyyy-MM-dd hh:mm:ss", "yyyy-MM-dd hh:mm", "yyyy/MM/dd hh:mm:ss", "dd.MM.yyyy hh:mm:ss"}) {
                dt = QDateTime::fromString(src, f);
                if (dt.isValid()) break;
            }
            if (!dt.isValid()) dt = QDateTime::fromString(src, Qt::ISODate);
            if (dt.isValid()) lastLoginSecs = dt.secsTo(QDateTime::currentDateTime());
        }
        QString dotColor;
        if (!data.isActive) {
            dotColor = "#999999";
        } else if (lastLoginSecs < 0) {
            dotColor = "#999999";
        } else if (lastLoginSecs < 600) {
            dotColor = "#18CF00";
        } else if (lastLoginSecs < 7200) {
            dotColor = "#F59E0B";
        } else if (lastLoginSecs < 86400) {
            dotColor = "#F59E0B";
        } else if (lastLoginSecs < 172800) {
            dotColor = "#EF4444";
        } else {
            dotColor = "#999999";
        }
        QLabel *statusDot = new QLabel(rightCol);
        statusDot->setFixedSize(s(22), s(22));
        statusDot->setStyleSheet(QString(
            "background:%1;border-radius:%2px;"
        ).arg(dotColor).arg(s(11)));

        QPushButton *detailsBtn = new QPushButton("Подробнее", rightCol);
        detailsBtn->setStyleSheet(QString(
            "QPushButton{background:#0F00DB;color:white;font-family:Inter;"
            "font-size:%1px;font-weight:700;border-radius:%2px;padding:%3px %4px;} "
            "QPushButton:hover{background:#1A4ACD;}"
        ).arg(s(14)).arg(s(8)).arg(s(5)).arg(s(12)));
        detailsBtn->setMinimumHeight(s(38));

        QObject::connect(detailsBtn, &QPushButton::clicked, [this](){
            if (onOpenDetails)
                onOpenDetails(data.username);
        });

        QHBoxLayout *statusLayout = new QHBoxLayout();
        statusLayout->setContentsMargins(0,0,0,0);
        statusLayout->setSpacing(s(8));
        statusLayout->addWidget(statusDot, 0, Qt::AlignRight);
        statusLayout->addWidget(detailsBtn, 0, Qt::AlignRight);

        right->addStretch();
        right->addLayout(statusLayout);
        right->addStretch();

        root->addWidget(leftCol, 1);
        root->addWidget(rightCol, 0, Qt::AlignRight);
    }
};
//
// ======================= UsersPage =======================
//

UsersPage::UsersPage(std::function<int(int)> scale, QWidget *parent)
    : QWidget(parent), s(scale)
{
    setStyleSheet("background:#F1F2F4;");
    setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(s(10), s(10), s(10), s(10));
    root->setSpacing(s(12));

    //
    // === HEADER ===
    //
    QWidget *header = new QWidget(this);
    header->setStyleSheet("background:transparent;");
    QHBoxLayout *hdr = new QHBoxLayout(header);
    hdr->setContentsMargins(0,0,0,0);
    hdr->setSpacing(s(10));

    //
    // КНОПКА НАЗАД
    //
    QPushButton *back = new QPushButton("   Назад", header);
    back->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    back->setIconSize(QSize(s(24), s(24)));
    back->setFixedSize(s(150), s(50));
    back->setStyleSheet(QString(
        "QPushButton { background-color:#E6E6E6; border-radius:%1px; border:1px solid #C8C8C8;"
        "font-family:Inter; font-size:%2px; font-weight:800; color:black; text-align:left; padding-left:%3px; }"
        "QPushButton:hover { background-color:#D5D5D5; }"
    ).arg(s(10)).arg(s(16)).arg(s(10)));

    connect(back, &QPushButton::clicked, this, [this](){
        emit backRequested();
    });

    hdr->addWidget(back, 0, Qt::AlignLeft);
    hdr->addStretch();

    //
    // ЗАГОЛОВОК
    //
    QLabel *title = new QLabel("Пользователи", header);
    title->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;background:transparent;"
    ).arg(s(26)));
    title->setAlignment(Qt::AlignCenter);

    hdr->addWidget(title, 0, Qt::AlignCenter);
    hdr->addStretch();

    //
    // КНОПКА УДАЛЕНИЯ (только admin и tech)
    //
    QString currentRole = getUserRole(AppSession::currentUsername());
    QPushButton *deleteBtn = new QPushButton("Удалить", header);
    deleteBtn->setFixedSize(s(165), s(50));
    deleteBtn->setStyleSheet(QString(
        "QPushButton{ background-color:#FF3B30; border:1px solid #C72B22; border-radius:%1px;"
        "font-family:Inter; font-size:%2px; font-weight:800; color:white; }"
        "QPushButton:hover{background-color:#E4372D;}"
    ).arg(s(10)).arg(s(16)));
    if (currentRole != "admin" && currentRole != "tech") {
        deleteBtn->setVisible(false);
    }

    connect(deleteBtn, &QPushButton::clicked, this, [this, currentRole](){

        QDialog dlg(this);
        dlg.setWindowTitle("Удаление пользователей");
        dlg.setFixedSize(s(460), s(520));
        dlg.setStyleSheet("background:white;border-radius:12px;");

        QVBoxLayout *root = new QVBoxLayout(&dlg);
        root->setContentsMargins(s(16), s(16), s(16), s(16));
        root->setSpacing(s(14));

        //
        // === ЗАГОЛОВОК ===
        //
        QLabel *title = new QLabel("Выберите пользователей для удаления", &dlg);
        title->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
        ).arg(s(20)));
        root->addWidget(title);

        //
        // === СКРОЛЛ С ЧЕКБОКСАМИ ===
        //
        QScrollArea *scroll = new QScrollArea(&dlg);
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");

        QWidget *listWidget = new QWidget(scroll);
        QVBoxLayout *listLayout = new QVBoxLayout(listWidget);
        listLayout->setContentsMargins(0,0,0,0);
        listLayout->setSpacing(s(6));

        QVector<QCheckBox*> boxes;
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

            // Админ может удалять только пользователей (viewer), не админов и техов
            if (currentRole == "admin" && role != "viewer")
                continue;

            QCheckBox *cb = new QCheckBox(
                QString("%1 — %2").arg(full, username),
                listWidget
            );

            cb->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;color:#1A1A1A;"
            ).arg(s(16)));

            if (username == current) {
                cb->setEnabled(false);
                cb->setStyleSheet(QString(
                    "font-family:Inter;font-size:%1px;color:#888;"
                ).arg(s(16)));
            }

            boxes.push_back(cb);
            listLayout->addWidget(cb);
        }

        listLayout->addStretch();
        scroll->setWidget(listWidget);
        root->addWidget(scroll);

        //
        // === КНОПКИ ===
        //
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

        //
        // === ЛОГИКА ===
        //
        connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

        connect(del, &QPushButton::clicked, this, [&, this, currentRole, userRoles](){
            QVector<QString> toDelete;

            for (auto *cb : boxes) {
                if (cb->isChecked()) {
                    QString text = cb->text();
                    QString username = text.split("—").last().trimmed();
                    // Админ не может удалять админов и техов (в списке их нет, но на всякий случай)
                    if (currentRole == "admin" && userRoles.value(username) != "viewer")
                        continue;
                    toDelete.push_back(username);
                }
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
                    auto ret = QMessageBox::question(this, "Подтверждение",
                        "Ты точно хочешь удалить администратора?",
                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                    if (ret != QMessageBox::Yes) return;
                }
            }

            QSqlDatabase db = QSqlDatabase::database("main_connection");
            for (const QString &u : toDelete) {
                // Вернуть AGV в общие: снять закрепление
                QSqlQuery updAgv(db);
                updAgv.prepare("UPDATE agv_list SET assigned_user = '' WHERE assigned_user = :u");
                updAgv.bindValue(":u", u);
                updAgv.exec();

                // Убрать назначение/делегирование в задачах
                QSqlQuery updTasks(db);
                updTasks.prepare("UPDATE agv_tasks SET assigned_to = '', delegated_by = '' WHERE assigned_to = :u OR delegated_by = :u");
                updTasks.bindValue(":u", u);
                updTasks.exec();

                // Очистить performed_by в истории (анонимизировать)
                QSqlQuery updHist(db);
                updHist.prepare("UPDATE agv_task_history SET performed_by = '' WHERE performed_by = :u");
                updHist.bindValue(":u", u);
                updHist.exec();

                // Удалить уведомления пользователя
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

            // Обновить UI списка AGV
            emit DataBus::instance().agvListChanged();

            dlg.accept();
        });

        //
        // === ПОСЛЕ ЗАКРЫТИЯ ===
        //
        if (dlg.exec() == QDialog::Accepted) {
            loadUsers();
        }
    });


    hdr->addWidget(deleteBtn, 0, Qt::AlignRight);

    root->addWidget(header);

    //
    // ===== СКРОЛЛ =====
    //
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");

    content = new QWidget();
    content->setStyleSheet("background:transparent;");
    layout = new QVBoxLayout(content);
    layout->setSpacing(s(8));
    layout->setContentsMargins(0,0,0,0);

    scroll->setWidget(content);
    root->addWidget(scroll);

    //
    // Автообновление статуса (реже, чтобы не грузить удаленную БД).
    //
    statusRefreshTimer = new QTimer(this);
    statusRefreshTimer->setInterval(60000);
    connect(statusRefreshTimer, &QTimer::timeout, this, &UsersPage::loadUsers);
    statusRefreshTimer->start();

    loadUsers();
}
//
// ======================= ЗАГРУЗКА ПОЛЬЗОВАТЕЛЕЙ =======================
//

void UsersPage::loadUsers()
{
    // Не дергаем БД, если страница пользователей сейчас скрыта.
    if (!isVisible())
        return;
    if (!layout || !content)
        return;

    if (loadingUsers_) return;
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
        ORDER BY full_name ASC)");
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

    QVector<UserItem::UserData> users;
    users.reserve(256);

    while (q.next()) {
        UserItem::UserData u;
        u.username    = q.value(0).toString();

        QString full = q.value(1).toString().trimmed();
        if (full.isEmpty())
            full = u.username;

        u.fullName    = full;
        u.role        = q.value(2).toString();
        u.position    = q.value(3).toString();
        u.department  = q.value(4).toString();
        u.mobile      = q.value(5).toString();
        u.telegram    = q.value(6).toString();
        u.lastLogin   = q.value(7).toString();
        u.recoveryKey = q.value(8).toString();
        u.isActive    = q.value(9).toInt() == 1;
        u.avatarBlob  = q.value(10).toByteArray();

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

    QVector<UserItem::UserData> admins, viewers, techs;
    for (const auto &u : users) {
        if (u.role == "admin") admins.append(u);
        else if (u.role == "tech") techs.append(u);
        else viewers.append(u);
    }

    auto addSection = [&](const QString &title, const QVector<UserItem::UserData> &list,
                         CollapsibleSection::SectionStyle style) {
        if (list.isEmpty()) return;
        CollapsibleSection *sec = new CollapsibleSection(
            QString("%1 (%2)").arg(title).arg(list.size()), true, s, content, style);
        for (const auto &u : list) {
            UserItem *item = new UserItem(u, s, sec);
            item->onOpenDetails = [this](const QString &username){
                emit openUserDetailsRequested(username);
            };
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

//
// ======================= ЗАГРУЗКА ДАННЫХ ОДНОГО ПОЛЬЗОВАТЕЛЯ =======================
//

UserItem::UserData loadUserData(const QString &username)
{
    UserItem::UserData u;

    QSqlQuery q(QSqlDatabase::database("main_connection"));
    q.prepare(R"(
        SELECT username, full_name, role, position, department,
               mobile, telegram, last_login, permanent_recovery_key,
               is_active, avatar
        FROM users
        WHERE username = :u
    )");
    q.bindValue(":u", username);
    q.exec();

    if (q.next()) {
        u.username    = q.value(0).toString();
        u.fullName    = q.value(1).toString();
        u.role        = q.value(2).toString();
        u.position    = q.value(3).toString();
        u.department  = q.value(4).toString();
        u.mobile      = q.value(5).toString();
        u.telegram    = q.value(6).toString();
        u.lastLogin   = q.value(7).toString();
        u.recoveryKey = q.value(8).toString();
        u.isActive    = q.value(9).toInt() == 1;
        u.avatarBlob  = q.value(10).toByteArray();
    }

    return u;
}

//
// ======================= МГНОВЕННОЕ ОБНОВЛЕНИЕ ПОЛЬЗОВАТЕЛЯ =======================
//

void UsersPage::updateUserInList(const QString &username)
{
    UserItem::UserData u = loadUserData(username);

    QList<UserItem*> items = content->findChildren<UserItem*>();
    for (UserItem *item : items) {
        if (item->username() == username) {
            item->updateData(u);
            return;
        }
    }
}
