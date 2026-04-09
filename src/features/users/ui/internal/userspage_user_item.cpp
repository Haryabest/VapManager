#include "userspage_user_item.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDateTime>

#include "app_session.h"
#include "db_users.h"

namespace UsersPageInternal {

UserItem::UserItem(const UserData &data, std::function<int(int)> scale, QWidget *parent)
    : QFrame(parent), data_(data), s_(scale)
{
    setObjectName("userItem");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    setStyleSheet(
        "#userItem{background:white;border-radius:10px;border:1px solid #E0E0E0;}"
        "#userItem:hover{background:#F7F7F7;}"
    );

    build();
}

QString UserItem::username() const
{
    return data_.username;
}

bool UserItem::isExpanded() const
{
    return details_ && details_->isVisible();
}

void UserItem::setExpanded(bool expanded)
{
    if (!details_ || !arrow_)
        return;

    details_->setVisible(expanded);
    arrow_->setPixmap(QPixmap(
        expanded
            ? ":/new/mainWindowIcons/noback/arrow_up.png"
            : ":/new/mainWindowIcons/noback/arrow_down.png"
    ).scaled(s_(18), s_(18)));
}

void UserItem::updateData(const UserData &data)
{
    const bool wasExpanded = isExpanded();
    data_ = data;
    rebuildUI();
    setExpanded(wasExpanded);
}

bool UserItem::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == this && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);

        QWidget *clicked = childAt(me->pos());
        if (clicked && qobject_cast<QPushButton *>(clicked))
            return QFrame::eventFilter(obj, event);

        const bool nowExpanded = !isExpanded();
        setExpanded(nowExpanded);
        if (onExpandedChanged)
            onExpandedChanged(data_.username, nowExpanded);

        return true;
    }

    return QFrame::eventFilter(obj, event);
}

void UserItem::rebuildUI()
{
    QLayoutItem *child;
    while ((child = layout()->takeAt(0)) != nullptr) {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }
    build();
}

void UserItem::build()
{
    QHBoxLayout *root = new QHBoxLayout(this);
    root->setContentsMargins(s_(12), s_(10), s_(12), s_(10));
    root->setSpacing(s_(12));

    QWidget *leftCol = new QWidget(this);
    QVBoxLayout *left = new QVBoxLayout(leftCol);
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(s_(6));

    header_ = new QWidget(leftCol);
    header_->setCursor(Qt::PointingHandCursor);
    this->installEventFilter(this);

    QHBoxLayout *h = new QHBoxLayout(header_);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(s_(10));

    QLabel *avatar = new QLabel(header_);
    avatar->setFixedSize(s_(42), s_(42));

    QPixmap pm;
    if (!data_.avatarBlob.isEmpty())
        pm.loadFromData(data_.avatarBlob);

    QPixmap final(s_(42), s_(42));
    final.fill(Qt::transparent);

    {
        QPainter p(&final);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        QPainterPath path;
        path.addEllipse(0, 0, s_(42), s_(42));
        p.setClipPath(path);

        if (!pm.isNull()) {
            QPixmap scaled = pm.scaled(
                s_(42),
                s_(42),
                Qt::KeepAspectRatioByExpanding,
                Qt::SmoothTransformation
            );
            p.drawPixmap(0, 0, scaled);
        } else {
            p.setBrush(QColor("#888"));
            p.setPen(Qt::NoPen);
            p.drawEllipse(0, 0, s_(42), s_(42));

            p.setPen(Qt::white);
            QFont f("Inter");
            f.setPointSize(s_(16));
            f.setBold(true);
            p.setFont(f);

            QString initials;
            for (const QString &part : data_.fullName.split(" ", Qt::SkipEmptyParts))
                initials += part.left(1).toUpper();

            p.drawText(final.rect(), Qt::AlignCenter, initials.left(2));
        }
    }

    avatar->setPixmap(final);

    QString roleText;
    QString roleColor;
    if (data_.role == "admin") {
        roleText = "[admin]";
        roleColor = "#FF3B30";
    } else if (data_.role == "tech") {
        roleText = "[tech]";
        roleColor = "#18CF00";
    } else {
        roleText = "[user]";
        roleColor = "#777777";
    }

    QLabel *title = new QLabel(
        QString("%1 — %2").arg(data_.fullName).arg(data_.username),
        header_
    );
    title->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
    ).arg(s_(16)));

    QLabel *roleLabel = new QLabel(roleText, header_);
    roleLabel->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:%2;"
    ).arg(s_(16)).arg(roleColor));

    arrow_ = new QLabel(header_);
    arrow_->setPixmap(QPixmap(":/new/mainWindowIcons/noback/arrow_down.png").scaled(s_(18), s_(18)));

    h->addWidget(avatar);
    h->addWidget(title);
    h->addWidget(roleLabel);
    h->addStretch();
    h->addWidget(arrow_);

    left->addWidget(header_);

    QString formattedLastLogin;
    {
        QString src = data_.lastLogin.trimmed();
        QDateTime dt;

        const QStringList strFormats = {
            "yyyy-MM-dd hh:mm:ss",
            "yyyy-MM-dd hh:mm",
            "yyyy/MM/dd hh:mm:ss",
            "dd.MM.yyyy hh:mm:ss"
        };

        for (const QString &f : strFormats) {
            dt = QDateTime::fromString(src, f);
            if (dt.isValid())
                break;
        }

        if (!dt.isValid())
            dt = QDateTime::fromString(src, Qt::ISODate);
        if (!dt.isValid())
            dt = QDateTime::fromString(src, Qt::ISODateWithMs);

        formattedLastLogin = dt.isValid() ? dt.toString("dd.MM.yy — hh:mm") : src;
    }

    QLabel *lastUnderTitle = new QLabel("Активность: " + formattedLastLogin, leftCol);
    lastUnderTitle->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:600;color:#333;"
    ).arg(s_(14)));
    lastUnderTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    left->addWidget(lastUnderTitle);

    details_ = new QWidget(leftCol);
    details_->setVisible(false);

    QVBoxLayout *d = new QVBoxLayout(details_);
    d->setContentsMargins(s_(5), s_(5), s_(5), s_(5));
    d->setSpacing(s_(6));

    auto makeRow = [&](const QString &label, const QString &value) {
        QLabel *l = new QLabel(QString("<b>%1:</b> %2").arg(label, value), details_);
        l->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;color:#1A1A1A;"
        ).arg(s_(14)));
        return l;
    };

    d->addWidget(makeRow("Должность", data_.position));
    d->addWidget(makeRow("Телефон", data_.mobile));
    d->addWidget(makeRow("Telegram", data_.telegram));

    static QString cachedRoleForUser;
    static QString cachedRole;
    const QString currentUser = AppSession::currentUsername();
    if (cachedRoleForUser != currentUser) {
        cachedRoleForUser = currentUser;
        cachedRole = getUserRole(currentUser);
    }

    if (cachedRole == "tech")
        d->addWidget(makeRow("Recovery key", data_.recoveryKey));

    left->addWidget(details_);

    QWidget *rightCol = new QWidget(this);
    rightCol->setMinimumWidth(s_(180));
    QVBoxLayout *right = new QVBoxLayout(rightCol);
    right->setContentsMargins(s_(8), s_(4), s_(2), s_(4));
    right->setSpacing(s_(10));

    qint64 lastLoginSecs = -1;
    {
        QDateTime dt;
        QString src = data_.lastLogin.trimmed();
        for (const QString &f : QStringList{
                 "yyyy-MM-dd hh:mm:ss",
                 "yyyy-MM-dd hh:mm",
                 "yyyy/MM/dd hh:mm:ss",
                 "dd.MM.yyyy hh:mm:ss"
             }) {
            dt = QDateTime::fromString(src, f);
            if (dt.isValid())
                break;
        }
        if (!dt.isValid())
            dt = QDateTime::fromString(src, Qt::ISODate);
        if (dt.isValid())
            lastLoginSecs = dt.secsTo(QDateTime::currentDateTime());
    }

    QString dotColor;
    if (!data_.isActive || lastLoginSecs < 0)
        dotColor = "#999999";
    else if (lastLoginSecs < 600)
        dotColor = "#18CF00";
    else if (lastLoginSecs < 86400)
        dotColor = "#F59E0B";
    else if (lastLoginSecs < 172800)
        dotColor = "#EF4444";
    else
        dotColor = "#999999";

    QLabel *statusDot = new QLabel(rightCol);
    statusDot->setFixedSize(s_(22), s_(22));
    statusDot->setStyleSheet(QString("background:%1;border-radius:%2px;").arg(dotColor).arg(s_(11)));

    QPushButton *detailsBtn = new QPushButton("Подробнее", rightCol);
    detailsBtn->setStyleSheet(QString(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;"
        "font-size:%1px;font-weight:700;border-radius:%2px;padding:%3px %4px;} "
        "QPushButton:hover{background:#1A4ACD;}"
    ).arg(s_(14)).arg(s_(8)).arg(s_(5)).arg(s_(12)));
    detailsBtn->setMinimumHeight(s_(38));

    QObject::connect(detailsBtn, &QPushButton::clicked, [this]() {
        if (onOpenDetails)
            onOpenDetails(data_.username);
    });

    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(s_(8));
    statusLayout->addWidget(statusDot, 0, Qt::AlignRight);
    statusLayout->addWidget(detailsBtn, 0, Qt::AlignRight);

    right->addStretch();
    right->addLayout(statusLayout);
    right->addStretch();

    root->addWidget(leftCol, 1);
    root->addWidget(rightCol, 0, Qt::AlignRight);
}

} // namespace UsersPageInternal
