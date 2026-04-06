#include "AccountInfoDialog.h"
#include "db_agv_tasks.h"
#include <QPainter>
#include <QPainterPath>

AccountInfoDialog::AccountInfoDialog(const QString &username,
                                     const QString &role,
                                     const QString &inviteKey,
                                     const QPixmap &avatarFromDb,
                                     QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Аккаунт");
    setModal(true);
    setMinimumWidth(420);
    setStyleSheet("background:#F5F7FB;");

    QVBoxLayout *main = new QVBoxLayout(this);
    main->setContentsMargins(20, 20, 20, 20);
    main->setSpacing(15);

    //
    // ===== ЛОГИН + КРУГЛЫЙ АВАТАР =====
    //
    QHBoxLayout *userRow = new QHBoxLayout();
    userRow->setSpacing(12);

    QLabel *icon = new QLabel(this);

    // --- КРУГЛЫЙ АВАТАР ---
    int size = 48;
    QPixmap avatar = avatarFromDb;

    if (avatar.isNull()) {
        avatar = QPixmap(":/icons/default_user.png");
    }

    QPixmap scaled = avatar.isNull() ? QPixmap() : avatar.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    QPixmap circle(size, size);
    circle.fill(Qt::transparent);

    {
        QPainter p(&circle);
        p.setRenderHint(QPainter::Antialiasing, true);

        QPainterPath path;
        path.addEllipse(0, 0, size, size);
        p.setClipPath(path);

        if (!scaled.isNull())
            p.drawPixmap(0, 0, scaled);
    }

    icon->setPixmap(circle);
    icon->setFixedSize(size, size);

    QLabel *userLabel = new QLabel(username, this);
    userLabel->setStyleSheet(
        "font-family:Inter;"
        "font-size:22px;"
        "font-weight:900;"
        "color:#0F172A;"
    );

    userRow->addWidget(icon);
    userRow->addWidget(userLabel);
    userRow->addStretch();
    main->addLayout(userRow);

    //
    // ===== РОЛЬ =====
    //
    QString roleDisplay = role;
    if (role == "admin") roleDisplay = "Администратор";
    else if (role == "tech") roleDisplay = "Техник";
    else if (role == "viewer") roleDisplay = "Пользователь";
    QLabel *roleLabel = new QLabel("Роль: " + roleDisplay, this);
    roleLabel->setStyleSheet(
        "font-family:Inter;"
        "font-size:14px;"
        "font-weight:700;"
        "color:#475569;"
    );
    main->addWidget(roleLabel);

    //
    // ===== AGV ЗАКРЕПЛЁННЫЕ ЗА ПОЛЬЗОВАТЕЛЕМ =====
    //
    QStringList agvList = getAgvIdsAssignedToUser(username);
    if (!agvList.isEmpty()) {
        QLabel *agvTitle = new QLabel("AGV закреплены за:", this);
        agvTitle->setStyleSheet(
            "font-family:Inter;"
            "font-size:16px;"
            "font-weight:800;"
            "color:#0C4A6E;"
        );
        main->addWidget(agvTitle);

        QLabel *agvListLbl = new QLabel(agvList.join(", "), this);
        agvListLbl->setWordWrap(true);
        agvListLbl->setStyleSheet(
            "font-family:Inter;"
            "font-size:14px;"
            "color:#0369A1;"
        );
        main->addWidget(agvListLbl);
    }

    //
    // ===== БЛОК КЛЮЧА (только для admin/tech) =====
    //
    bool isTech = (role == "tech");
    bool hasInviteKey = (role == "admin" || role == "tech");
    QString keyTitleText = isTech ? "🔑 Ключ техника" : "🔑 Ключ администратора";
    if (hasInviteKey) {
        QLabel *keyTitle = new QLabel(keyTitleText, this);
        keyTitle->setStyleSheet(
            "font-family:Inter;"
            "font-size:16px;"
            "font-weight:800;"
            "color:#0C4A6E;"
        );
        main->addWidget(keyTitle);

        QString hintText = isTech
            ? "Этот ключ нужен для регистрации новых техников."
            : "Этот ключ нужен для регистрации новых администраторов.";
        QLabel *hint = new QLabel(hintText, this);
        hint->setWordWrap(true);
        hint->setStyleSheet(
            "font-family:Inter;"
            "font-size:12px;"
            "color:#0369A1;"
        );
        main->addWidget(hint);

        QLineEdit *keyEdit = new QLineEdit(this);
        keyEdit->setText(inviteKey);
        keyEdit->setReadOnly(true);
        keyEdit->setStyleSheet(
            "QLineEdit {"
            "   background:#FFFFFF;"
            "   border:1px solid #BAE6FD;"
            "   border-radius:8px;"
            "   font-family:Consolas;"
            "   font-size:18px;"
            "   font-weight:700;"
            "   color:#0C4A6E;"
            "   padding:10px;"
            "}"
        );
        main->addWidget(keyEdit);

        QPushButton *copyBtn = new QPushButton("Копировать", this);
        copyBtn->setStyleSheet(
            "QPushButton {"
            "   background:#0EA5E9;"
            "   color:white;"
            "   font-family:Inter;"
            "   font-size:14px;"
            "   font-weight:700;"
            "   border:none;"
            "   border-radius:8px;"
            "   padding:10px 16px;"
            "}"
            "QPushButton:hover { background:#0284C7; }"
        );

        connect(copyBtn, &QPushButton::clicked, this, [keyEdit, copyBtn]() {
            QApplication::clipboard()->setText(keyEdit->text());
            copyBtn->setText("Скопировано!");
            QTimer::singleShot(2000, copyBtn, [copyBtn]() {
                copyBtn->setText("Копировать");
            });
        });

        main->addWidget(copyBtn);
    }

    //
    // ===== КНОПКА ЗАКРЫТЬ =====
    //
    QPushButton *closeBtn = new QPushButton("Закрыть", this);
    closeBtn->setStyleSheet(
        "QPushButton {"
        "   background:#E6E6E6;"
        "   border-radius:8px;"
        "   border:1px solid #C8C8C8;"
        "   font-family:Inter;"
        "   font-size:14px;"
        "   font-weight:700;"
        "   padding:8px 16px;"
        "}"
        "QPushButton:hover { background:#D5D5D5; }"
    );
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    main->addWidget(closeBtn, 0, Qt::AlignRight);
}
