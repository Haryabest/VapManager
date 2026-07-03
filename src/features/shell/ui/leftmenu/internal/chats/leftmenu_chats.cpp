#include "leftmenu.h"

#include "app_session.h"
#include "db_task_chat.h"
#include "db_users.h"
#include "notifications_logs.h"
#include "taskchatdialog.h"
#include "databus.h"

#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <algorithm>
#include <QDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

class NotificationToast : public QFrame
{
public:
    NotificationToast(leftMenu *menu, const Notification &n, QWidget *parent)
        : QFrame(parent), menu_(menu), notif_(n)
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setCursor(Qt::PointingHandCursor);
        setStyleSheet(
            "QFrame{background:#1F2937;border:1px solid #374151;border-radius:12px;}"
            "QLabel{background:transparent;}");
        setFixedWidth(360);

        QVBoxLayout *lay = new QVBoxLayout(this);
        lay->setContentsMargins(14, 12, 14, 12);
        lay->setSpacing(4);

        QLabel *titleLbl = new QLabel(n.title, this);
        titleLbl->setStyleSheet("color:#F9FAFB;font-family:Inter;font-size:14px;font-weight:800;");
        titleLbl->setWordWrap(true);
        lay->addWidget(titleLbl);

        QLabel *bodyLbl = new QLabel(notificationMessageForDisplay(n.message), this);
        bodyLbl->setStyleSheet("color:#D1D5DB;font-family:Inter;font-size:12px;font-weight:600;");
        bodyLbl->setWordWrap(true);
        lay->addWidget(bodyLbl);

        QLabel *tapLbl = new QLabel(QStringLiteral("Нажмите, чтобы открыть"), this);
        tapLbl->setStyleSheet("color:#93C5FD;font-family:Inter;font-size:11px;font-weight:700;");
        lay->addWidget(tapLbl);
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && menu_) {
            menu_->openNotificationTarget(notif_);
            close();
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    leftMenu *menu_ = nullptr;
    Notification notif_;
};

void showNotificationToast(leftMenu *menu, const Notification &n)
{
    if (!menu)
        return;
    QWidget *top = menu->window();
    if (!top)
        return;

    auto *toast = new NotificationToast(menu, n, top);
    toast->adjustSize();
    const QPoint globalTopRight = top->mapToGlobal(QPoint(top->width() - toast->width() - 24, 72));
    toast->move(globalTopRight);
    toast->show();
    toast->raise();
    QTimer::singleShot(8000, toast, &QWidget::close);
}

void flashTaskbarOnNewNotification(QWidget *owner)
{
#ifdef Q_OS_WIN
    QWidget *top = owner ? owner->window() : nullptr;
    if (!top || !top->isWindow()) {
        const auto tops = QApplication::topLevelWidgets();
        for (QWidget *w : tops) {
            if (w && w->isVisible() && w->isWindow()) {
                top = w;
                break;
            }
        }
    }
    if (!top)
        return;
    HWND hwnd = reinterpret_cast<HWND>(top->winId());
    if (!hwnd)
        return;
    FLASHWINFO fi{};
    fi.cbSize = sizeof(FLASHWINFO);
    fi.hwnd = hwnd;
    // Persistent taskbar highlight until user activates app window.
    fi.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
    fi.uCount = 0;
    fi.dwTimeout = 0;
    FlashWindowEx(&fi);
#else
    Q_UNUSED(owner);
#endif
}

void stopTaskbarFlash(QWidget *owner)
{
#ifdef Q_OS_WIN
    QWidget *top = owner ? owner->window() : nullptr;
    if (!top || !top->isWindow()) {
        const auto tops = QApplication::topLevelWidgets();
        for (QWidget *w : tops) {
            if (w && w->isVisible() && w->isWindow()) {
                top = w;
                break;
            }
        }
    }
    if (!top)
        return;
    HWND hwnd = reinterpret_cast<HWND>(top->winId());
    if (!hwnd)
        return;
    FLASHWINFO fi{};
    fi.cbSize = sizeof(FLASHWINFO);
    fi.hwnd = hwnd;
    fi.dwFlags = FLASHW_STOP;
    fi.uCount = 0;
    fi.dwTimeout = 0;
    FlashWindowEx(&fi);
#else
    Q_UNUSED(owner);
#endif
}
QString highlightSearchMatch(const QString &text, const QString &query);  // fwd

QStringList loadChatSearchHistory()
{
    QSettings s(QStringLiteral("VapManager"), QStringLiteral("VapManager"));
    return s.value(QStringLiteral("chat/search_history")).toStringList();
}

void saveChatSearchHistory(const QStringList &history)
{
    QSettings s(QStringLiteral("VapManager"), QStringLiteral("VapManager"));
    s.setValue(QStringLiteral("chat/search_history"), history);
    s.sync();
}

void pushChatSearchHistory(const QString &rawQuery)
{
    const QString q = rawQuery.trimmed();
    if (q.isEmpty())
        return;
    QStringList hist = loadChatSearchHistory();
    hist.removeAll(q);
    hist.prepend(q);
    while (hist.size() > 8)
        hist.removeLast();
    saveChatSearchHistory(hist);
}

QString highlightSearchMatch(const QString &text, const QString &query)
{
    const QString esc = text.toHtmlEscaped();
    if (query.isEmpty())
        return esc;
    const QString q = query.toHtmlEscaped();
    if (q.isEmpty())
        return esc;
    // Find all match positions on the escaped string, then rebuild from left to right
    // using a regex with capture to preserve original case.
    QRegularExpression re(QRegularExpression::escape(q), QRegularExpression::CaseInsensitiveOption);
    QString out;
    int last = 0;
    auto it = re.globalMatch(esc);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        out += esc.mid(last, m.capturedStart() - last);
        out += QStringLiteral("<span style=\"background:#FEF08A;color:#0F172A;font-weight:800;\">")
             + m.captured(0)
             + QStringLiteral("</span>");
        last = m.capturedEnd();
    }
    out += esc.mid(last);
    return out;
}

QString makeChatListSignature(const QVector<TaskChatThread> &threads)
{
    QStringList parts;
    parts.reserve(threads.size());
    for (const TaskChatThread &t : threads) {
        parts.append(QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
                         .arg(t.id)
                         .arg(t.createdAt.toString(Qt::ISODate))
                         .arg(t.closedAt.toString(Qt::ISODate))
                         .arg(t.createdBy)
                         .arg(t.recipientUser)
                         .arg(t.lastMessageAt.toString(Qt::ISODate))
                         .arg(t.unreadCount));
    }
    return parts.join(QLatin1Char('\n'));
}

struct ChatPeerMeta
{
    QString displayName;
    QPixmap avatar;
    bool isActive = false;
    QDateTime lastLogin;
};

bool chatListPeerShowsOnline(bool isActive, const QDateTime &lastLogin)
{
    if (!isActive || !lastLogin.isValid())
        return false;
    const qint64 secs = lastLogin.secsTo(QDateTime::currentDateTime());
    return secs >= 0 && secs < 180;
}

QString formatLastSeenText(bool isActive, const QDateTime &lastLogin)
{
    if (chatListPeerShowsOnline(isActive, lastLogin))
        return QStringLiteral("в сети");

    if (!lastLogin.isValid())
        return QStringLiteral("не в сети");

    const qint64 secs = lastLogin.secsTo(QDateTime::currentDateTime());
    if (secs < 60)
        return QStringLiteral("был в сети только что");
    if (secs < 3600)
        return QStringLiteral("был в сети %1 мин назад").arg(qMax<qint64>(1, secs / 60));
    if (secs < 86400)
        return QStringLiteral("был в сети %1 ч назад").arg(qMax<qint64>(1, secs / 3600));
    if (secs < 172800)
        return QStringLiteral("был в сети вчера");
    return QStringLiteral("был в сети %1").arg(lastLogin.toString(QStringLiteral("dd.MM.yyyy hh:mm")));
}

QHash<QString, ChatPeerMeta> loadChatPeerMeta(const QSet<QString> &usernames)
{
    QHash<QString, ChatPeerMeta> result;
    if (usernames.isEmpty())
        return result;

    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen())
        return result;

    QStringList names = usernames.values();
    names.removeAll(QString());
    if (names.isEmpty())
        return result;

    QStringList placeholders;
    placeholders.reserve(names.size());
    for (int i = 0; i < names.size(); ++i)
        placeholders.append(QStringLiteral("?"));

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT username, full_name, avatar, is_active, last_login FROM users WHERE username IN (%1)")
                  .arg(placeholders.join(QStringLiteral(","))));
    for (const QString &name : names)
        q.addBindValue(name);

    if (!q.exec())
        return result;

    while (q.next()) {
        const QString username = q.value(0).toString().trimmed();
        const QString fullName = q.value(1).toString().trimmed();
        if (username.isEmpty())
            continue;

        ChatPeerMeta meta;
        meta.displayName = fullName.isEmpty() ? username : fullName;

        const QByteArray avatarBytes = q.value(2).toByteArray();
        if (!avatarBytes.isEmpty()) {
            QPixmap pm;
            pm.loadFromData(avatarBytes);
            meta.avatar = pm;
        }
        meta.isActive = q.value(3).toInt() == 1;
        meta.lastLogin = q.value(4).toDateTime();

        result.insert(username, meta);
    }
    return result;
}

} // namespace

void leftMenu::showBroadcastNotificationDetails(const QString &senderLogin,
                                                const QString &subject,
                                                const QString &body,
                                                int notificationId)
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Рассылка"));
    dlg.setFixedWidth(s(500));
    dlg.setMaximumHeight(s(580));
    dlg.setStyleSheet(QStringLiteral(
        "QDialog{background:#F7FAFF;border:1px solid #E2E8F0;border-radius:12px;}"));

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(s(20), s(20), s(20), s(20));
    root->setSpacing(s(14));

    QLabel *titleLbl = new QLabel(QStringLiteral("Сообщение рассылки"), &dlg);
    titleLbl->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#0F172A;background:transparent;"
    ).arg(s(20)));
    root->addWidget(titleLbl);

    const QString fieldStyle = QString(
        "font-family:Inter;font-size:%1px;font-weight:600;color:#0F172A;background:#FFFFFF;"
        "border:1px solid #E2E8F0;border-radius:%2px;padding:%3px %4px;"
    ).arg(s(15)).arg(s(8)).arg(s(10)).arg(s(12));

    auto addLabel = [&](const QString &label) {
        QLabel *lbl = new QLabel(label, &dlg);
        lbl->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:700;color:#64748B;background:transparent;"
        ).arg(s(13)));
        root->addWidget(lbl);
    };

    auto addPlainField = [&](const QString &value) {
        QLabel *val = new QLabel(value.isEmpty() ? QStringLiteral("—") : value, &dlg);
        val->setWordWrap(true);
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        val->setStyleSheet(fieldStyle);
        root->addWidget(val);
    };

    auto addScrollableField = [&](const QString &value, int maxLines) {
        QTextEdit *val = new QTextEdit(&dlg);
        val->setReadOnly(true);
        val->setPlainText(value.isEmpty() ? QStringLiteral("—") : value);
        val->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        val->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        val->setLineWrapMode(QTextEdit::WidgetWidth);
        val->setStyleSheet(fieldStyle);
        QFont f = val->font();
        f.setFamily(QStringLiteral("Inter"));
        f.setPixelSize(s(15));
        val->setFont(f);
        const int lineHeight = QFontMetrics(f).lineSpacing();
        val->setFixedHeight(lineHeight * maxLines + s(20));
        root->addWidget(val);
    };

    QString fromText;
    if (senderLogin.isEmpty()) {
        fromText = QStringLiteral("—");
    } else {
        const QString displayName = userDisplayName(senderLogin);
        fromText = (displayName.isEmpty() || displayName == senderLogin)
                       ? senderLogin
                       : QStringLiteral("%1 — %2").arg(displayName, senderLogin);
    }

    addLabel(QStringLiteral("От кого"));
    addPlainField(fromText);
    addLabel(QStringLiteral("Тема"));
    addScrollableField(subject, 3);
    addLabel(QStringLiteral("Текст"));
    addScrollableField(body, 15);

    QPushButton *closeBtn = new QPushButton(QStringLiteral("Закрыть"), &dlg);
    closeBtn->setFixedHeight(s(44));
    closeBtn->setStyleSheet(QString(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;font-size:%1px;"
        "font-weight:700;border:none;border-radius:%2px;padding:0 %3px;}"
        "QPushButton:hover{background:#1A4ACD;}"
    ).arg(s(15)).arg(s(8)).arg(s(18)));
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    root->addWidget(closeBtn, 0, Qt::AlignRight);

    dlg.exec();

    if (notificationId > 0) {
        markNotificationReadById(notificationId);
        updateNotifBadge();
    }
}

void leftMenu::showNotificationsPanel()
{
    const QString currentUser = AppSession::currentUsername();
    QDialog dlg(this);
    dlg.setWindowTitle("Уведомления");
    dlg.setFixedSize(s(520), s(560));
    dlg.setStyleSheet("background:#F5F7FB;");

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(s(16), s(16), s(16), s(16));
    root->setSpacing(s(10));

    QLabel *title = new QLabel("Уведомления", &dlg);
    title->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#0F172A;"
    ).arg(s(22)));
    root->addWidget(title);

    QWidget *dndRow = new QWidget(&dlg);
    QHBoxLayout *dndLay = new QHBoxLayout(dndRow);
    dndLay->setContentsMargins(0, 0, 0, 0);
    dndLay->setSpacing(s(6));
    QLabel *dndLbl = new QLabel(dndRow);
    const QString dndText = notificationDndStatusText();
    dndLbl->setText(dndText.isEmpty() ? QStringLiteral("Уведомления активны") : dndText);
    dndLbl->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:700;color:%2;background:transparent;"
    ).arg(s(11)).arg(dndText.isEmpty() ? QStringLiteral("#64748B") : QStringLiteral("#B45309")));
    dndLay->addWidget(dndLbl, 1);
    auto makeDndBtn = [&](const QString &text) {
        QPushButton *b = new QPushButton(text, dndRow);
        b->setStyleSheet(QString(
            "QPushButton{background:#E2E8F0;color:#1E293B;font-family:Inter;font-size:%1px;"
            "font-weight:700;border:none;border-radius:%2px;padding:%3px %4px;}"
            "QPushButton:hover{background:#CBD5E1;}"
        ).arg(s(10)).arg(s(6)).arg(s(4)).arg(s(8)));
        return b;
    };
    QPushButton *dndHourBtn = makeDndBtn(QStringLiteral("1 час"));
    QPushButton *dndShiftBtn = makeDndBtn(QStringLiteral("До смены"));
    QPushButton *dndOffBtn = makeDndBtn(QStringLiteral("Вкл."));
    connect(dndHourBtn, &QPushButton::clicked, &dlg, [&]() {
        setNotificationDndForOneHour();
        dndLbl->setText(notificationDndStatusText());
    });
    connect(dndShiftBtn, &QPushButton::clicked, &dlg, [&]() {
        setNotificationDndUntilEndOfShift();
        dndLbl->setText(notificationDndStatusText());
    });
    connect(dndOffBtn, &QPushButton::clicked, &dlg, [&]() {
        clearNotificationDnd();
        dndLbl->setText(QStringLiteral("Уведомления активны"));
    });
    dndLay->addWidget(dndHourBtn);
    dndLay->addWidget(dndShiftBtn);
    dndLay->addWidget(dndOffBtn);
    root->addWidget(dndRow);

    QScrollArea *scroll = new QScrollArea(&dlg);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");

    QWidget *host = new QWidget(scroll);
    host->setStyleSheet("background:transparent;");
    QVBoxLayout *listLayout = new QVBoxLayout(host);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(s(8));

    QVector<Notification> notifs = loadNotificationsForUser(currentUser);

    if (notifs.isEmpty()) {
        QLabel *empty = new QLabel("Нет уведомлений", host);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:700;color:#888;"
        ).arg(s(16)));
        listLayout->addStretch();
        listLayout->addWidget(empty, 0, Qt::AlignCenter);
        listLayout->addStretch();
    } else {
        for (const Notification &n : notifs) {
            QFrame *card = new QFrame(host);
            card->setProperty("notificationId", n.id);
            card->setStyleSheet(QString(
                "QFrame{background:%1;border:none;}"
            ).arg(n.isRead ? "#FFFFFF" : "#EFF6FF"));

            QVBoxLayout *cardL = new QVBoxLayout(card);
            cardL->setContentsMargins(s(12), s(8), s(12), s(8));
            cardL->setSpacing(s(4));

            QHBoxLayout *topRow = new QHBoxLayout();
            QLabel *titleLbl = new QLabel(n.title, card);
            titleLbl->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;font-weight:800;color:#0F172A;background:transparent;"
            ).arg(s(14)));

            QLabel *timeLbl = new QLabel(n.createdAt.toString("dd.MM.yy hh:mm"), card);
            timeLbl->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;color:#94A3B8;background:transparent;"
            ).arg(s(11)));

            topRow->addWidget(titleLbl);
            topRow->addStretch();
            topRow->addWidget(timeLbl);
            cardL->addLayout(topRow);

            QLabel *msgLbl = new QLabel(notificationMessageForDisplay(n.message), card);
            msgLbl->setWordWrap(true);
            msgLbl->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;color:#475569;background:transparent;"
            ).arg(s(13)));
            cardL->addWidget(msgLbl);

            auto parseAgvId = [&](const QString &text) -> QString {
                QRegularExpression re("AGV\\s*№?\\s*([A-Za-z0-9_\\-]+)");
                QRegularExpressionMatch m = re.match(text);
                return m.hasMatch() ? m.captured(1).trimmed() : QString();
            };
            auto parseSenderLogin = [&](const QString &text) -> QString {
                QRegularExpression re("\\(([^\\)]+)\\)");
                QRegularExpressionMatch m = re.match(text);
                if (m.hasMatch()) return m.captured(1).trimmed();
                return QString();
            };
            auto parseChatId = [&](const QString &text) -> int {
                QRegularExpression re("\\[chat:(\\d+)\\]");
                QRegularExpressionMatch m = re.match(text);
                return m.hasMatch() ? m.captured(1).toInt() : 0;
            };

            const bool chatEligible = (n.title == "AGV закреплена за вами" || n.title == "Задача делегирована");
            const bool chatNotification = (n.title.contains("чат", Qt::CaseInsensitive) ||
                                           n.title == "Новое сообщение по задаче" ||
                                           n.title == "Ответ в чате по задаче");
            if (chatEligible) {
                QString agvId = parseAgvId(n.message);
                QString recipient = notificationPeerUsername(n.message);
                if (recipient.isEmpty())
                    recipient = parseSenderLogin(n.message);
                QPushButton *chatBtn = new QPushButton("Начать чат", card);
                chatBtn->setStyleSheet(QString(
                    "QPushButton{background:#0B89FF;color:white;font-family:Inter;font-size:%1px;"
                    "font-weight:700;border:none;border-radius:%2px;padding:%3px %4px;}"
                    "QPushButton:hover{background:#0A75D6;}"
                ).arg(s(12)).arg(s(7)).arg(s(6)).arg(s(12)));
                chatBtn->setIcon(QIcon(":/new/mainWindowIcons/noback/user.png"));
                chatBtn->setIconSize(QSize(s(14), s(14)));
                connect(chatBtn, &QPushButton::clicked, &dlg, [this, currentUser, agvId, recipient, &dlg, n]() {
                    if (agvId.isEmpty() || recipient.isEmpty()) {
                        QMessageBox::warning(this, "Чат",
                                             "Не удалось определить AGV или отправителя из уведомления.");
                        return;
                    }
                    QString err;
                    int tid = TaskChatDialog::ensureThreadWithUser(currentUser, recipient, QString(), &err);
                    if (tid <= 0) {
                        QMessageBox::warning(this, "Чат", err.isEmpty() ? QStringLiteral("Не удалось открыть чат") : err);
                        return;
                    }
                    TaskChatDialog::markNextMessageSpecial(tid, QStringLiteral("AGV %1").arg(agvId));
                    markNotificationReadById(n.id);
                    updateNotifBadge();
                    activeChatThreadId_ = tid;
                    activeChatPeer_ = recipient;
                    showChatsPage();
                    dlg.accept();
                });
                cardL->addWidget(chatBtn, 0, Qt::AlignLeft);
            }
            const bool taskNotifOpenChat =
                (n.title == QStringLiteral("Задача выполнена") ||
                 n.title == QStringLiteral("Задача назначена") ||
                 n.title == QStringLiteral("Новая задача") ||
                 n.title == QStringLiteral("Задача делегирована"));
            if (taskNotifOpenChat) {
                const QString peerU = notificationPeerUsername(n.message);
                if (!peerU.isEmpty()) {
                    const QString agvIdTask = parseAgvId(n.message);
                    QString tname;
                    {
                        static const QRegularExpression reTn(QStringLiteral("Задача\\s+\"([^\"]+)\""));
                        static const QRegularExpression reAvail(QStringLiteral("Доступна задача «([^»]+)»"));
                        static const QRegularExpression reDel(QStringLiteral("делегирована задача\\s+\"([^\"]+)\""),
                                                             QRegularExpression::CaseInsensitiveOption);
                        QRegularExpressionMatch m = reTn.match(n.message);
                        if (m.hasMatch()) tname = m.captured(1).trimmed();
                        if (tname.isEmpty()) {
                            m = reAvail.match(n.message);
                            if (m.hasMatch()) tname = m.captured(1).trimmed();
                        }
                        if (tname.isEmpty()) {
                            m = reDel.match(n.message);
                            if (m.hasMatch()) tname = m.captured(1).trimmed();
                        }
                    }
                    QString ctx;
                    if (!tname.isEmpty() && !agvIdTask.isEmpty())
                        ctx = QStringLiteral("По задаче «%1», AGV %2").arg(tname, agvIdTask);
                    else if (!agvIdTask.isEmpty())
                        ctx = QStringLiteral("Чат по AGV %1").arg(agvIdTask);
                    else if (!tname.isEmpty())
                        ctx = QStringLiteral("По задаче «%1»").arg(tname);
                    else
                        ctx = QStringLiteral("По уведомлению");
                    card->setProperty("openChatPeerUser", peerU);
                    card->setProperty("openChatAgvHint", agvIdTask);
                    card->setProperty("openChatContextText", ctx);
                    card->setCursor(Qt::PointingHandCursor);
                    card->installEventFilter(this);
                    QLabel *tapHint = new QLabel(QStringLiteral("Нажмите, чтобы открыть чат"), card);
                    tapHint->setStyleSheet(QString(
                        "font-family:Inter;font-size:%1px;font-weight:600;color:#64748B;background:transparent;"
                    ).arg(s(11)));
                    cardL->addWidget(tapHint);
                }
            }
            if (chatNotification) {
                int chatId = parseChatId(n.message);
                if (chatId > 0) {
                    card->setProperty("openChatThreadId", chatId);
                    if (n.title == QStringLiteral("Новое сообщение по задаче")) {
                        static const QRegularExpression ra(QStringLiteral("AGV\\s+([A-Za-z0-9_\\-]+)"));
                        const QRegularExpressionMatch mx = ra.match(n.message);
                        if (mx.hasMatch())
                            card->setProperty("openChatAgvHintForThread", mx.captured(1).trimmed());
                    }
                } else {
                    card->setProperty("openChatsPageOnClick", true);
                }
                card->setCursor(Qt::PointingHandCursor);
                card->installEventFilter(this);
            }
            if (isBroadcastNotification(n.message)) {
                card->setProperty("openBroadcastDetails", true);
                card->setProperty("broadcastFromUser", broadcastNotificationSender(n.message));
                card->setProperty("broadcastSubject", n.title);
                card->setProperty("broadcastBody", broadcastNotificationBody(n.message));
                card->setCursor(Qt::PointingHandCursor);
                card->installEventFilter(this);
                QLabel *tapHint = new QLabel(QStringLiteral("Нажмите, чтобы прочитать"), card);
                tapHint->setStyleSheet(QString(
                    "font-family:Inter;font-size:%1px;font-weight:600;color:#64748B;background:transparent;"
                ).arg(s(11)));
                cardL->addWidget(tapHint);
            }

            auto isMutedPeer = [&](const QString &peer) -> bool {
                QSettings s("VapManager", "VapManager");
                return s.value(QString("chat/mute/%1/%2").arg(currentUser.trimmed(), peer.trimmed()), false).toBool();
            };

            bool mutedChatNotif = false;
            if (!n.isRead) {
                const int chatId = parseChatId(n.message);
                if (chatId > 0) {
                    TaskChatThread t = getThreadById(chatId);
                    QString other = (t.createdBy == currentUser) ? t.recipientUser : t.createdBy;
                    if (!other.trimmed().isEmpty() && isMutedPeer(other))
                        mutedChatNotif = true;
                }
            }

            if (!n.isRead && !mutedChatNotif) {
                QLabel *badge = new QLabel("Новое", card);
                badge->setStyleSheet(QString(
                    "font-family:Inter;font-size:%1px;font-weight:700;color:#2563EB;background:transparent;"
                ).arg(s(11)));
                cardL->addWidget(badge);
            }

            listLayout->addWidget(card);
        }
        listLayout->addStretch();
    }

    scroll->setWidget(host);
    root->addWidget(scroll, 1);

    QPushButton *markReadBtn = new QPushButton("Отметить все как прочитанные", &dlg);
    markReadBtn->setStyleSheet(QString(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;font-size:%1px;"
        "font-weight:700;border:none;border-radius:%2px;padding:%3px %4px;}"
        "QPushButton:hover{background:#1A4ACD;}"
    ).arg(s(13)).arg(s(8)).arg(s(8)).arg(s(16)));
    connect(markReadBtn, &QPushButton::clicked, &dlg, [&](){
        markAllReadForUser(currentUser);
        updateNotifBadge();
        dlg.accept();
    });
    root->addWidget(markReadBtn);

    QHBoxLayout *bottomBtns = new QHBoxLayout();
    QPushButton *clearBtn = new QPushButton("Очистить уведомления", &dlg);
    clearBtn->setStyleSheet(QString(
        "QPushButton{background:#E6E6E6;border-radius:%1px;border:1px solid #C8C8C8;"
        "font-family:Inter;font-size:%2px;font-weight:700;padding:%3px %4px;}"
        "QPushButton:hover{background:#D5D5D5;}"
    ).arg(s(8)).arg(s(13)).arg(s(6)).arg(s(14)));
    connect(clearBtn, &QPushButton::clicked, &dlg, [&](){
        clearAllNotificationsForUser(currentUser);
        updateNotifBadge();
        DataBus::instance().triggerNotificationsChanged();
        dlg.accept();
    });
    bottomBtns->addWidget(clearBtn, 0, Qt::AlignLeft);
    bottomBtns->addStretch();
    QPushButton *closeBtn = new QPushButton("Закрыть", &dlg);
    closeBtn->setStyleSheet(QString(
        "QPushButton{background:#E6E6E6;border-radius:%1px;border:1px solid #C8C8C8;"
        "font-family:Inter;font-size:%2px;font-weight:700;padding:%3px %4px;}"
        "QPushButton:hover{background:#D5D5D5;}"
    ).arg(s(8)).arg(s(13)).arg(s(6)).arg(s(14)));
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    bottomBtns->addWidget(closeBtn);

    root->addLayout(bottomBtns);

    dlg.exec();
    updateNotifBadge();
}

void leftMenu::updateNotifBadge()
{
    if (!notifBadge_) return;
    const QString currentUser = AppSession::currentUsername();
    const QVector<Notification> notifs = loadUnreadNotificationsForUser(currentUser);
    auto parseChatId = [&](const QString &text) -> int {
        QRegularExpression re("\\[chat:(\\d+)\\]");
        QRegularExpressionMatch m = re.match(text);
        return m.hasMatch() ? m.captured(1).toInt() : 0;
    };
    auto isMutedPeer = [&](const QString &peer) -> bool {
        QSettings s("VapManager", "VapManager");
        return s.value(QString("chat/mute/%1/%2").arg(currentUser.trimmed(), peer.trimmed()), false).toBool();
    };
    QVector<int> chatIds;
    chatIds.reserve(notifs.size());
    for (const Notification &n : notifs) {
        const int chatId = parseChatId(n.message);
        if (chatId > 0)
            chatIds.push_back(chatId);
    }
    const QHash<int, TaskChatThread> threadMap = chatIds.isEmpty()
        ? QHash<int, TaskChatThread>()
        : getThreadsByIds(chatIds);
    const int unreadAnyCount = notifs.size();
    int count = 0;
    int unreadChatCount = 0;
    for (const Notification &n : notifs) {
        const int chatId = parseChatId(n.message);
        if (chatId > 0) {
            const TaskChatThread t = threadMap.value(chatId);
            QString other = (t.createdBy == currentUser) ? t.recipientUser : t.createdBy;
            if (!other.trimmed().isEmpty() && isMutedPeer(other))
                continue;
            unreadChatCount++;
        }
        count++;
    }
    if (lastUnreadAnyNotifCount_ >= 0 && unreadAnyCount > lastUnreadAnyNotifCount_) {
        if (!isNotificationDndActive()) {
            playNotificationSound();
            flashTaskbarOnNewNotification(this);
            if (!notifs.isEmpty()) {
                const Notification latest = notifs.constFirst();
                showNotificationToast(this, latest);
            }
        }
        if (chatsPage && chatsPage->isVisible() && chatsStack_ && chatsStack_->currentIndex() == 0) {
            lastChatsListSignature_.clear();
            reloadChatsPageList();
        }
    }
    lastUnreadChatNotifCount_ = unreadChatCount;
    lastUnreadAnyNotifCount_ = unreadAnyCount;
    if (unreadAnyCount == 0)
        stopTaskbarFlash(this);
    if (count > 0) {
        notifBadge_->setText(count > 99 ? "99+" : QString::number(count));
        notifBadge_->show();
    } else {
        notifBadge_->hide();
    }
}

void leftMenu::showUserProfilePage(const QString &username)
{
    activePage_ = ActivePage::UserProfile;
    activeUsername_ = username;
    UserInfo info;
    if (!loadUserProfile(username, info)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось загрузить данные пользователя.");
        return;
    }

    hideAllPages();

    QWidget *profileParent = rightCalendarFrame ? rightCalendarFrame->parentWidget() : this;
    QWidget *page = new QWidget(profileParent);
    page->setStyleSheet("background:#F5F7FB;");
    page->setObjectName("userProfilePage");

    QVBoxLayout *mainLay = new QVBoxLayout(page);
    mainLay->setContentsMargins(s(20), s(15), s(20), s(15));
    mainLay->setSpacing(s(12));

    QWidget *header = new QWidget(page);
    QHBoxLayout *hdrLay = new QHBoxLayout(header);
    hdrLay->setContentsMargins(0, 0, 0, 0);
    hdrLay->setSpacing(s(10));

    QPushButton *backBtn = new QPushButton("   Назад", header);
    backBtn->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    backBtn->setIconSize(QSize(s(24), s(24)));
    backBtn->setFixedSize(s(150), s(50));
    backBtn->setStyleSheet(QString(
        "QPushButton{background-color:#E6E6E6;border-radius:%1px;border:1px solid #C8C8C8;"
        "font-family:Inter;font-size:%2px;font-weight:800;color:black;text-align:left;padding-left:%3px;}"
        "QPushButton:hover{background-color:#D5D5D5;}"
    ).arg(s(10)).arg(s(16)).arg(s(10)));

    connect(backBtn, &QPushButton::clicked, this, [this, page](){
        page->setVisible(false);
        page->deleteLater();
        showUsersPage();
    });

    hdrLay->addWidget(backBtn, 0, Qt::AlignLeft);
    hdrLay->addStretch();

    QLabel *titleLbl = new QLabel(
        info.fullName.isEmpty() ? info.username : info.fullName, header);
    titleLbl->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#0F172A;background:transparent;"
    ).arg(s(24)));
    hdrLay->addWidget(titleLbl, 0, Qt::AlignCenter);
    hdrLay->addStretch();

    mainLay->addWidget(header);

    QScrollArea *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea{background:transparent;}");

    QWidget *content = new QWidget();
    content->setStyleSheet("background:transparent;");
    QVBoxLayout *contentLay = new QVBoxLayout(content);
    contentLay->setContentsMargins(s(10), 0, s(10), 0);
    contentLay->setSpacing(s(14));

    QString roleText;
    if (info.role == "admin") roleText = "Администратор";
    else if (info.role == "tech") roleText = "Разработчик";
    else roleText = "Пользователь";

    const QString editorUsername = AppSession::currentUsername();
    const QString editorRole = getUserRole(editorUsername);
    const bool isSelf = (username == editorUsername);
    const bool isAdminEditor = (editorRole == QStringLiteral("admin"));
    const bool isTechEditor = (editorRole == QStringLiteral("tech"));
    const bool canEditRole = !isSelf && (isAdminEditor || isTechEditor);
    bool roleEditable = false;
    if (canEditRole) {
        if (isTechEditor)
            roleEditable = true;
        else if (isAdminEditor
                 && (info.role == QStringLiteral("viewer") || info.role == QStringLiteral("admin")))
            roleEditable = true;
    }

    auto addCopyableRow = [&](const QString &label, const QString &value) {
        if (value.trimmed().isEmpty()) return;
        QWidget *row = new QWidget(content);
        row->setStyleSheet("background:transparent;");
        QHBoxLayout *h = new QHBoxLayout(row);
        h->setContentsMargins(0, s(4), 0, s(4));
        h->setSpacing(s(10));

        QLabel *lbl = new QLabel(label + ":", row);
        lbl->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:700;color:#334155;background:transparent;"
        ).arg(s(15)));
        lbl->setMinimumWidth(s(140));

        QLineEdit *valEdit = new QLineEdit(row);
        valEdit->setReadOnly(true);
        valEdit->setText(value);
        valEdit->setCursor(Qt::IBeamCursor);
        valEdit->setStyleSheet(QString(
            "QLineEdit{background:#F1F5F9;border:1px solid #E2E8F0;border-radius:8px;"
            "padding:8px 12px;font-family:Inter;font-size:%1px;color:#0F172A;}"
            "QLineEdit:focus{border:1px solid #3B82F6;}"
        ).arg(s(14)));
        valEdit->setMinimumWidth(s(200));

        QPushButton *copyBtn = new QPushButton("Копировать", row);
        copyBtn->setFixedHeight(s(36));
        copyBtn->setStyleSheet(QString(
            "QPushButton{background:#0EA5E9;color:white;font-family:Inter;font-size:%1px;"
            "font-weight:700;border:none;border-radius:6px;padding:0 12px;}"
            "QPushButton:hover{background:#0284C7;}"
        ).arg(s(12)));
        connect(copyBtn, &QPushButton::clicked, row, [valEdit, copyBtn](){
            QApplication::clipboard()->setText(valEdit->text());
            copyBtn->setText("Скопировано");
            QTimer::singleShot(1500, copyBtn, [copyBtn](){
                copyBtn->setText("Копировать");
            });
        });

        h->addWidget(lbl);
        h->addWidget(valEdit, 1);
        h->addWidget(copyBtn);
        contentLay->addWidget(row);
    };

    // Сетка: слева ФИО, Табельный, Должность; справа Логин, Роль, Подразделение
    QGridLayout *grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(s(8));
    auto addGridCell = [&](int row, int col, const QString &label, const QString &value) {
        QString v = value.trimmed().isEmpty() ? "—" : value;
        QLabel *l = new QLabel(QString("<b>%1:</b> %2").arg(label, v), content);
        l->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:#1A1A1A;background:transparent;").arg(s(15)));
        l->setWordWrap(true);
        grid->addWidget(l, row, col);
    };
    addGridCell(0, 0, "ФИО", info.fullName);
    addGridCell(0, 1, "Логин", info.username);
    addGridCell(1, 0, "Табельный номер", info.employeeId);
    if (!roleEditable) {
        addGridCell(1, 1, "Роль", roleText);
    } else {
        QWidget *roleCell = new QWidget(content);
        roleCell->setStyleSheet("background:transparent;");
        QVBoxLayout *roleCellLay = new QVBoxLayout(roleCell);
        roleCellLay->setContentsMargins(0, 0, 0, 0);
        roleCellLay->setSpacing(s(6));

        QLabel *roleLbl = new QLabel(QStringLiteral("<b>Роль:</b>"), roleCell);
        roleLbl->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;color:#1A1A1A;background:transparent;"
        ).arg(s(15)));

        QComboBox *roleCombo = new QComboBox(roleCell);
        roleCombo->setStyleSheet(QString(
            "QComboBox{background:#FFFFFF;border:1px solid #CBD5E1;border-radius:8px;"
            "padding:6px 10px;font-family:Inter;font-size:%1px;color:#0F172A;}"
            "QComboBox::drop-down{border:none;width:24px;}"
        ).arg(s(14)));
        if (isTechEditor) {
            roleCombo->addItem(QStringLiteral("Пользователь"), QStringLiteral("viewer"));
            roleCombo->addItem(QStringLiteral("Администратор"), QStringLiteral("admin"));
            roleCombo->addItem(QStringLiteral("Разработчик"), QStringLiteral("tech"));
        } else {
            roleCombo->addItem(QStringLiteral("Пользователь"), QStringLiteral("viewer"));
            roleCombo->addItem(QStringLiteral("Администратор"), QStringLiteral("admin"));
        }
        for (int i = 0; i < roleCombo->count(); ++i) {
            if (roleCombo->itemData(i).toString() == info.role) {
                roleCombo->setCurrentIndex(i);
                break;
            }
        }

        QPushButton *saveRoleBtn = new QPushButton(QStringLiteral("Сохранить роль"), roleCell);
        saveRoleBtn->setFixedHeight(s(36));
        saveRoleBtn->setStyleSheet(QString(
            "QPushButton{background:#2563EB;color:white;font-family:Inter;font-size:%1px;"
            "font-weight:700;border:none;border-radius:6px;padding:0 14px;}"
            "QPushButton:hover{background:#1D4ED8;}"
        ).arg(s(13)));

        connect(saveRoleBtn, &QPushButton::clicked, this,
                [this, username, roleCombo, saveRoleBtn]() {
            const QString newRole = roleCombo->currentData().toString();
            const QString currentRole = getUserRole(username);
            if (newRole == currentRole) {
                QMessageBox::information(this,
                                         QStringLiteral("Роль"),
                                         QStringLiteral("Роль не изменилась."));
                return;
            }

            QString err;
            if (!setUserRole(username, newRole, err)) {
                QMessageBox::warning(this, QStringLiteral("Ошибка"), err);
                return;
            }

            if (usersPage)
                usersPage->loadUsers();

            saveRoleBtn->setText(QStringLiteral("Сохранено"));
            QTimer::singleShot(1500, saveRoleBtn, [saveRoleBtn]() {
                saveRoleBtn->setText(QStringLiteral("Сохранить роль"));
            });
            QMessageBox::information(this,
                                     QStringLiteral("Роль"),
                                     QStringLiteral("Роль пользователя обновлена."));
        });

        roleCellLay->addWidget(roleLbl);
        roleCellLay->addWidget(roleCombo);
        roleCellLay->addWidget(saveRoleBtn);
        grid->addWidget(roleCell, 1, 1);
    }
    addGridCell(2, 0, "Должность", info.position);
    addGridCell(2, 1, "Подразделение", info.department);
    contentLay->addLayout(grid);
    addCopyableRow("Телефон", info.mobile);
    addCopyableRow("Внутренний номер", info.extPhone);
    addCopyableRow("Email", info.email);
    addCopyableRow("Telegram", info.telegram);
    addCopyableRow("Recovery key", info.permanentRecoveryKey);

    // AGV закреплены за пользователем
    QStringList agvList = getAgvIdsAssignedToUser(username);
    if (!agvList.isEmpty()) {
        QLabel *agvTitle = new QLabel("Закреплённые AGV:", content);
        agvTitle->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:700;color:#334155;background:transparent;"
        ).arg(s(15)));
        agvTitle->setMinimumWidth(s(140));
        contentLay->addWidget(agvTitle);
        QLabel *agvVal = new QLabel(agvList.join(", "), content);
        agvVal->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;color:#0F172A;background:transparent;"
        ).arg(s(14)));
        agvVal->setWordWrap(true);
        contentLay->addWidget(agvVal);
    }

    // Activity history section (без рамки)
    QWidget *histFrame = new QWidget(content);
    histFrame->setStyleSheet("background:transparent;");
    QVBoxLayout *histLay = new QVBoxLayout(histFrame);
    histLay->setContentsMargins(0, s(10), 0, 0);
    histLay->setSpacing(s(6));

    QFrame *histLine = new QFrame(histFrame);
    histLine->setFixedHeight(1);
    histLine->setStyleSheet("background:#E2E8F0;");
    histLay->addWidget(histLine);

    QHBoxLayout *histTitleRow = new QHBoxLayout();
    histTitleRow->addStretch();
    QLabel *histTitle = new QLabel("История действий", histFrame);
    histTitle->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#0F172A;background:transparent;"
    ).arg(s(18)));
    histTitleRow->addWidget(histTitle, 0, Qt::AlignCenter);
    histTitleRow->addStretch();
    histLay->addLayout(histTitleRow);

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (db.isOpen()) {
        QSqlQuery histQ(db);
        histQ.prepare(R"(
            SELECT task_name, agv_id, completed_ts
            FROM agv_task_history
            WHERE performed_by = :u
            ORDER BY completed_ts DESC
            LIMIT 50
        )");
        histQ.bindValue(":u", username);
        if (histQ.exec()) {
            bool hasHistory = false;
            while (histQ.next()) {
                hasHistory = true;
                QString taskName = histQ.value(0).toString();
                QString agvId = histQ.value(1).toString();
                QDateTime completedTs = histQ.value(2).toDateTime();
                QString dateStr = completedTs.isValid() ? completedTs.toString("dd.MM.yyyy") : "—";

                QLabel *histLbl = new QLabel(
                    QString("• %1 — %2 (выполнено %3)").arg(taskName, agvId, dateStr),
                    histFrame);
                histLbl->setStyleSheet(QString(
                    "font-family:Inter;font-size:%1px;color:#475569;background:transparent;"
                    "padding:4px 0;"
                ).arg(s(14)));
                histLay->addWidget(histLbl);
            }
            if (!hasHistory) {
                QLabel *noHist = new QLabel("Нет записей", histFrame);
                noHist->setStyleSheet(QString(
                    "font-family:Inter;font-size:%1px;color:#94A3B8;background:transparent;"
                ).arg(s(14)));
                histLay->addWidget(noHist);
            }
        }
    }
    contentLay->addWidget(histFrame);

    contentLay->addStretch();
    scroll->setWidget(content);
    mainLay->addWidget(scroll, 1);

    if (rightCalendarFrame) {
        QWidget *rightBodyFrame = rightCalendarFrame->parentWidget();
        if (rightBodyFrame) {
            if (QVBoxLayout *rbl = qobject_cast<QVBoxLayout*>(rightBodyFrame->layout())) {
                rbl->addWidget(page, 3);
            }
        }
    }

    page->setVisible(true);
    stressSuiteLogPageEntered(QStringLiteral("user_profile"));
}

void leftMenu::openNotificationTarget(const Notification &n)
{
    const QString currentUser = AppSession::currentUsername();
    if (n.id > 0) {
        markNotificationReadById(n.id);
        updateNotifBadge();
    }

    const int chatId = notificationChatId(n.message);
    if (chatId > 0) {
        QString agvMark;
        static const QRegularExpression ra(QStringLiteral("AGV\\s+([A-Za-z0-9_\\-]+)"));
        const QRegularExpressionMatch mx = ra.match(n.message);
        if (mx.hasMatch())
            agvMark = mx.captured(1).trimmed();
        if (!agvMark.isEmpty())
            TaskChatDialog::markNextMessageSpecial(chatId, QStringLiteral("Чат по AGV %1").arg(agvMark));
        activeChatThreadId_ = chatId;
        activeChatPeer_.clear();
        showChatsPage();
        return;
    }

    const QString peerU = notificationPeerUsername(n.message);
    const bool taskNotif =
        (n.title == QStringLiteral("Задача выполнена") ||
         n.title == QStringLiteral("Задача назначена") ||
         n.title == QStringLiteral("Новая задача") ||
         n.title == QStringLiteral("Задача делегирована") ||
         n.title == QStringLiteral("AGV закреплена за вами"));
    if (!peerU.isEmpty() && taskNotif) {
        const QString agvIdTask = notificationAgvId(n.message);
        QString tname;
        static const QRegularExpression reTn(QStringLiteral("Задача\\s+\"([^\"]+)\""));
        static const QRegularExpression reAvail(QStringLiteral("Доступна задача «([^»]+)»"));
        static const QRegularExpression reDel(QStringLiteral("делегирована задача\\s+\"([^\"]+)\""),
                                             QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch m = reTn.match(n.message);
        if (m.hasMatch()) tname = m.captured(1).trimmed();
        if (tname.isEmpty()) {
            m = reAvail.match(n.message);
            if (m.hasMatch()) tname = m.captured(1).trimmed();
        }
        if (tname.isEmpty()) {
            m = reDel.match(n.message);
            if (m.hasMatch()) tname = m.captured(1).trimmed();
        }
        QString ctx;
        if (!tname.isEmpty() && !agvIdTask.isEmpty())
            ctx = QStringLiteral("По задаче «%1», AGV %2").arg(tname, agvIdTask);
        else if (!agvIdTask.isEmpty())
            ctx = QStringLiteral("Чат по AGV %1").arg(agvIdTask);
        else if (!tname.isEmpty())
            ctx = QStringLiteral("По задаче «%1»").arg(tname);
        else
            ctx = QStringLiteral("По уведомлению");

        QString err;
        const int tid = TaskChatDialog::ensureThreadWithUser(currentUser, peerU, agvIdTask, &err);
        if (tid <= 0) {
            QMessageBox::warning(this, QStringLiteral("Чат"),
                                 err.isEmpty() ? QStringLiteral("Не удалось открыть чат") : err);
            return;
        }
        TaskChatDialog::markNextMessageSpecial(tid, ctx);
        activeChatThreadId_ = tid;
        activeChatPeer_ = peerU;
        showChatsPage();
        return;
    }

    const QString agvId = notificationAgvId(n.message);
    if (!agvId.isEmpty()) {
        showAgvDetailInfo(agvId);
        QString tname;
        static const QRegularExpression reTn(QStringLiteral("Задача\\s+\"([^\"]+)\""));
        static const QRegularExpression reAvail(QStringLiteral("«([^»]+)»"));
        QRegularExpressionMatch m = reTn.match(n.message);
        if (m.hasMatch()) tname = m.captured(1).trimmed();
        if (tname.isEmpty()) {
            m = reAvail.match(n.message);
            if (m.hasMatch()) tname = m.captured(1).trimmed();
        }
        if (!tname.isEmpty() && agvSettingsPage) {
            QTimer::singleShot(0, this, [this, tname]() {
                if (agvSettingsPage)
                    agvSettingsPage->highlightTask(tname);
            });
        }
        return;
    }

    if (n.title.contains(QStringLiteral("чат"), Qt::CaseInsensitive) ||
        n.title == QStringLiteral("Новое сообщение по задаче") ||
        n.title == QStringLiteral("Ответ в чате по задаче")) {
        showChatsPage();
        return;
    }

    showNotificationsPanel();
}

void leftMenu::showChatsPage()
{
    activePage_ = ActivePage::Chats;
    hideAllPages();
    clearSearch();

    if (!chatsPage) {
        QWidget *parent = rightCalendarFrame ? rightCalendarFrame->parentWidget() : this;
        chatsPage = new QWidget(parent);
        chatsPage->setStyleSheet("background:#F5F7FB;");

        QVBoxLayout *main = new QVBoxLayout(chatsPage);
        main->setContentsMargins(0, 0, 0, 0);
        main->setSpacing(0);

        chatsStack_ = new QStackedWidget(chatsPage);
        main->addWidget(chatsStack_);

        // Страница 0: список чатов
        QWidget *listPage = new QWidget(chatsStack_);
        listPage->setStyleSheet("background:#F5F7FB;");
        QVBoxLayout *listMain = new QVBoxLayout(listPage);
        listMain->setContentsMargins(s(12), s(10), s(12), s(10));
        listMain->setSpacing(s(10));

        QWidget *header = new QWidget(listPage);
        QHBoxLayout *hdr = new QHBoxLayout(header);
        hdr->setContentsMargins(0, 0, 0, 0);
        hdr->setSpacing(s(10));

        QPushButton *backBtn = new QPushButton("   Назад", header);
        backBtn->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
        backBtn->setIconSize(QSize(s(24), s(24)));
        backBtn->setFixedSize(s(150), s(50));
        backBtn->setStyleSheet(QString(
            "QPushButton{background-color:#E6E6E6;border-radius:%1px;border:1px solid #C8C8C8;"
            "font-family:Inter;font-size:%2px;font-weight:800;color:black;text-align:left;padding-left:%3px;}"
            "QPushButton:hover{background-color:#D5D5D5;}"
        ).arg(s(10)).arg(s(16)).arg(s(10)));
        connect(backBtn, &QPushButton::clicked, this, [this]() { showCalendar(); });

        QLabel *title = new QLabel("Чаты", header);
        title->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#0F172A;"
        ).arg(s(22)));

        QWidget *rightPad = new QWidget(header);
        rightPad->setFixedSize(s(150), s(50));

        hdr->addWidget(backBtn, 0, Qt::AlignLeft);
        hdr->addStretch();
        hdr->addWidget(title, 0, Qt::AlignVCenter);
        hdr->addStretch();
        hdr->addWidget(rightPad, 0, Qt::AlignRight);
        listMain->addWidget(header);

        // Панель поиска / сортировки / фильтра
        QWidget *chatToolbar = new QWidget(listPage);
        chatToolbar->setStyleSheet("background:transparent;");
        QHBoxLayout *tbLay = new QHBoxLayout(chatToolbar);
        tbLay->setContentsMargins(0, 0, 0, 0);
        tbLay->setSpacing(s(8));

        chatSearchEdit_ = new QLineEdit(chatToolbar);
        chatSearchEdit_->setPlaceholderText(QStringLiteral("Поиск по чатам..."));
        chatSearchEdit_->setClearButtonEnabled(true);
        chatSearchEdit_->setStyleSheet(QString(
            "QLineEdit{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:%1px;"
            "padding:%2px %3px;font-family:Inter;font-size:%4px;font-weight:600;color:#0F172A;}"
            "QLineEdit:focus{border:1px solid #5076FB;}"
        ).arg(s(10)).arg(s(8)).arg(s(12)).arg(s(14)));
        chatSearchTimer_ = new QTimer(this);
        chatSearchTimer_->setSingleShot(true);
        chatSearchTimer_->setInterval(350);
        connect(chatSearchTimer_, &QTimer::timeout, this, [this]() {
            chatSearchQuery_ = chatSearchEdit_ ? chatSearchEdit_->text().trimmed().toLower() : QString();
            if (!chatSearchQuery_.isEmpty())
                pushChatSearchHistory(chatSearchEdit_->text().trimmed());
            lastChatsListSignature_.clear();
            reloadChatsPageList();
        });
        connect(chatSearchEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
            if (chatSearchTimer_) chatSearchTimer_->start();
        });
        connect(chatSearchEdit_, &QLineEdit::returnPressed, this, [this]() {
            if (chatSearchTimer_) chatSearchTimer_->stop();
            chatSearchQuery_ = chatSearchEdit_ ? chatSearchEdit_->text().trimmed().toLower() : QString();
            if (!chatSearchQuery_.isEmpty())
                pushChatSearchHistory(chatSearchEdit_->text().trimmed());
            lastChatsListSignature_.clear();
            reloadChatsPageList();
        });

        // Кнопка истории поиска (dropdown с последними запросами)
        QToolButton *searchHistoryBtn = new QToolButton(chatToolbar);
        searchHistoryBtn->setText(QStringLiteral("▾"));
        searchHistoryBtn->setToolTip(QStringLiteral("Последние запросы"));
        searchHistoryBtn->setFixedSize(s(34), s(34));
        searchHistoryBtn->setStyleSheet(QString(
            "QToolButton{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:%1px;"
            "font-family:Inter;font-size:%2px;font-weight:900;color:#475569;}"
            "QToolButton:hover{background:#F1F5F9;}"
        ).arg(s(10)).arg(s(14)));
        connect(searchHistoryBtn, &QToolButton::clicked, this, [this, searchHistoryBtn]() {
            const QStringList hist = loadChatSearchHistory();
            QMenu menu(searchHistoryBtn);
            menu.setStyleSheet(QStringLiteral(
                "QMenu{background:#FFFFFF;border:1px solid #D5DCE8;padding:4px;}"
                "QMenu::item{padding:6px 14px;font-family:Inter;font-size:13px;font-weight:600;color:#0F172A;}"
                "QMenu::item:selected{background:#E6E6E6;}"
                "QMenu::separator{height:1px;background:#E2E8F0;margin:4px 0;}"));
            if (hist.isEmpty()) {
                QAction *empty = menu.addAction(QStringLiteral("История пуста"));
                empty->setEnabled(false);
            } else {
                for (const QString &q : hist)
                    menu.addAction(q);
                menu.addSeparator();
                QAction *clear = menu.addAction(QStringLiteral("Очистить историю"));
                connect(clear, &QAction::triggered, this, [this]() {
                    saveChatSearchHistory(QStringList());
                });
            }
            QAction *chosen = menu.exec(searchHistoryBtn->mapToGlobal(
                QPoint(0, searchHistoryBtn->height() + s(2))));
            if (chosen && chatSearchEdit_ && !chosen->text().isEmpty()
                && chosen->text() != QStringLiteral("Очистить историю")) {
                chatSearchEdit_->setText(chosen->text());
                chatSearchEdit_->setFocus();
            }
        });

        chatFilterCombo_ = new QComboBox(chatToolbar);
        chatFilterCombo_->addItem(QStringLiteral("Все"));
        chatFilterCombo_->addItem(QStringLiteral("Открытые"));
        chatFilterCombo_->addItem(QStringLiteral("Закрытые"));
        chatFilterCombo_->setStyleSheet(QString(
            "QComboBox{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:%1px;"
            "padding:%2px %3px;font-family:Inter;font-size:%4px;font-weight:700;color:#0F172A;}"
            "QComboBox::drop-down{border:none;width:%5px;}"
            "QComboBox QAbstractItemView{background:#FFFFFF;border:1px solid #D5DCE8;selection-background-color:#E6E6E6;}"
        ).arg(s(10)).arg(s(6)).arg(s(10)).arg(s(13)).arg(s(20)));
        connect(chatFilterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
            chatFilterMode_ = idx;
            lastChatsListSignature_.clear();
            reloadChatsPageList();
        });

        // Кнопка расширенного поиска по истории сообщений (вглубь) с фильтрами
        QToolButton *advancedSearchBtn = new QToolButton(chatToolbar);
        advancedSearchBtn->setText(QStringLiteral("⌕"));
        advancedSearchBtn->setToolTip(QStringLiteral("Расширенный поиск по сообщениям (по дате / AGV / отправителю)"));
        advancedSearchBtn->setFixedSize(s(34), s(34));
        advancedSearchBtn->setStyleSheet(QString(
            "QToolButton{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:%1px;"
            "font-family:Inter;font-size:%2px;font-weight:900;color:#0F00DB;}"
            "QToolButton:hover{background:#F1F5F9;}"
        ).arg(s(10)).arg(s(16)));
        connect(advancedSearchBtn, &QToolButton::clicked, this, [this]() {
            showChatAdvancedSearchDialog();
        });

        chatSortCombo_ = new QComboBox(chatToolbar);
        chatSortCombo_->addItem(QStringLiteral("Сначала непрочитанные"));
        chatSortCombo_->addItem(QStringLiteral("По последнему сообщению"));
        chatSortCombo_->addItem(QStringLiteral("По имени"));
        chatSortCombo_->setStyleSheet(chatFilterCombo_->styleSheet());
        connect(chatSortCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
            chatSortMode_ = idx;
            lastChatsListSignature_.clear();
            reloadChatsPageList();
        });

        tbLay->addWidget(chatSearchEdit_, 1);
        tbLay->addWidget(searchHistoryBtn, 0);
        tbLay->addWidget(advancedSearchBtn, 0);
        tbLay->addWidget(chatFilterCombo_, 0);
        tbLay->addWidget(chatSortCombo_, 0);
        listMain->addWidget(chatToolbar);

        QScrollArea *scroll = new QScrollArea(listPage);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet("QScrollArea{background:transparent;border:none;}");
        QWidget *host = new QWidget(scroll);
        host->setStyleSheet("background:transparent;");
        chatsListLayout_ = new QVBoxLayout(host);
        chatsListLayout_->setContentsMargins(0, 0, 0, 0);
        chatsListLayout_->setSpacing(s(12));
        scroll->setWidget(host);
        listMain->addWidget(scroll, 1);

        QHBoxLayout *fabRow = new QHBoxLayout();
        fabRow->setContentsMargins(0, 0, 0, 0);
        fabRow->addStretch();
        QToolButton *addChatFab = new QToolButton(listPage);
        addChatFab->setFixedSize(s(56), s(56));
        addChatFab->setIcon(QIcon(":/new/mainWindowIcons/noback/edit.png"));
        addChatFab->setIconSize(QSize(s(24), s(24)));
        addChatFab->setStyleSheet(QString(
            "QToolButton{background:#55BFFF;border:none;border-radius:%1px;color:white;}"
            "QToolButton:hover{background:#43AEEA;}"
        ).arg(s(28)));
        addChatFab->setToolTip("Начать чат");
        connect(addChatFab, &QToolButton::clicked, this, [this]() {
            const QString currentUser = AppSession::currentUsername();
            QVector<UserInfo> allUsers = getAllUsers(false);
            QVector<UserInfo> candidates;
            for (const UserInfo &u : allUsers) {
                if (u.username != currentUser) candidates.push_back(u);
            }
            if (candidates.isEmpty()) {
                QMessageBox::information(this, "Начать чат", "Нет пользователей для чата.");
                return;
            }
            QDialog pick(this);
            pick.setWindowTitle("Начать чат");
            pick.setMinimumSize(s(460), s(560));
            pick.setStyleSheet(
                "QDialog{background:#F8FAFF;}"
                "QLabel{font-family:Inter;font-size:14px;font-weight:700;color:#334155;}"
                "QListWidget{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:10px;padding:6px;}"
                "QListWidget::item{padding:10px 8px;border-radius:10px;}"
                "QListWidget::item:hover{background:rgba(80,118,251,36);}"
                "QListWidget::item:selected{background:rgba(80,118,251,52);}"
                "QPushButton{background:#E2E8F0;color:#334155;font-weight:800;border:none;border-radius:10px;padding:8px 14px;}"
                "QPushButton:hover{background:#CBD5E1;}"
            );
            QVBoxLayout *root = new QVBoxLayout(&pick);
            QLabel *hint = new QLabel("Выберите пользователя", &pick);
            root->addWidget(hint);
            QListWidget *list = new QListWidget(&pick);
            list->setIconSize(QSize(s(40), s(40)));
            const QPixmap defaultUserPm = QPixmap(QStringLiteral(":/new/mainWindowIcons/noback/user.png"));
            QSet<QString> peerNames;
            for (const UserInfo &u : candidates)
                peerNames.insert(u.username);
            const QHash<QString, ChatPeerMeta> peerMeta = loadChatPeerMeta(peerNames);
            for (const UserInfo &u : candidates) {
                const ChatPeerMeta meta = peerMeta.value(u.username);
                const QString display = meta.displayName.isEmpty()
                    ? (u.fullName.isEmpty() ? u.username : (u.fullName + " (" + u.username + ")"))
                    : (meta.displayName + " (" + u.username + ")");
                QPixmap avatar = meta.avatar;
                if (avatar.isNull())
                    avatar = defaultUserPm;
                QListWidgetItem *it = new QListWidgetItem(QIcon(makeRoundPixmap(avatar, s(40))), display, list);
                it->setSizeHint(QSize(0, s(58)));
                it->setData(Qt::UserRole, u.username);
            }
            root->addWidget(list, 1);
            QPushButton *cancel = new QPushButton("Отмена", &pick);
            connect(cancel, &QPushButton::clicked, &pick, &QDialog::reject);
            root->addWidget(cancel);
            connect(list, &QListWidget::itemClicked, &pick, [&pick, this, currentUser](QListWidgetItem *it) {
                if (!it) return;
                const QString other = it->data(Qt::UserRole).toString().trimmed();
                if (other.isEmpty()) return;
                QString err;
                const int tid = TaskChatDialog::ensureThreadWithUser(currentUser, other, QString(), &err);
                if (tid <= 0) {
                    QMessageBox::warning(this, "Чат", err.isEmpty() ? QStringLiteral("Не удалось открыть чат") : err);
                    return;
                }
                pick.accept();
                activeChatThreadId_ = tid;
                activeChatPeer_ = other;
                showChatsPage();
                QTimer::singleShot(0, this, [this]() { reloadChatsPageList(); });
            });
            pick.exec();
        });
        fabRow->addWidget(addChatFab, 0, Qt::AlignRight | Qt::AlignBottom);
        listMain->addLayout(fabRow);

        chatsStack_->addWidget(listPage);

        // Страница 1: встроенный чат с человеком (замещает список)
        const QString curUser = AppSession::currentUsername();
        const QString role = getUserRole(curUser);
        const bool isAdmin = (role == "admin" || role == "tech");
        embeddedChatWidget_ = new TaskChatWidget(0, curUser, isAdmin, [this](int v) { return s(v); }, chatsStack_);
        connect(embeddedChatWidget_, &TaskChatWidget::showProfileRequested, this, [this](const QString &u) {
            showUserProfilePage(u);
        });
connect(embeddedChatWidget_, &TaskChatWidget::backRequested, this, [this]() {
    activeChatThreadId_ = 0;
    activeChatPeer_.clear();
    activeChatDraft_.clear();
    activeChatDraftFocused_ = false;
    if (chatsStack_) chatsStack_->setCurrentIndex(0);
    reloadChatsPageList();
});
chatsStack_->addWidget(embeddedChatWidget_);

        if (rightCalendarFrame) {
            QWidget *rightBodyFrame = rightCalendarFrame->parentWidget();
            if (rightBodyFrame) {
                if (QVBoxLayout *rightBodyLayout = qobject_cast<QVBoxLayout*>(rightBodyFrame->layout()))
                    rightBodyLayout->addWidget(chatsPage, 3);
            }
        }
    }

    if (chatsStack_) {
        if (activeChatThreadId_ > 0 && embeddedChatWidget_) {
            embeddedChatWidget_->setThreadId(activeChatThreadId_, activeChatPeer_);
            if (!activeChatDraft_.isEmpty() || activeChatDraftFocused_) {
                embeddedChatWidget_->setDraft(activeChatDraft_, activeChatDraftFocused_);
                activeChatDraft_.clear();
                activeChatDraftFocused_ = false;
            }
            chatsStack_->setCurrentIndex(1);
        } else {
            chatsStack_->setCurrentIndex(0);
        }
    }
    chatsPage->setVisible(true);
    reloadChatsPageList();
    stressSuiteLogPageEntered(QStringLiteral("chats"));
}

void leftMenu::openEmbeddedDelegatorChatForAgv(const QString &agvId)
{
    const QString aid = agvId.trimmed();
    if (aid.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Перейти в диалог"), QStringLiteral("AGV не выбран."));
        return;
    }
    const QString currentUser = AppSession::currentUsername();
    const QVector<QString> order = TaskChatDialog::delegatorUsernamesForAgv(aid, currentUser);
    if (order.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Перейти в диалог"),
            QStringLiteral("По этому AGV вам никто не назначал задачи и не закреплял его за вами. Используйте «Начать чат» в разделе Чаты."));
        return;
    }
    const QString specialContext = QStringLiteral("AGV %1").arg(aid);

    const auto openWithPeer = [this, currentUser, specialContext](const QString &otherRaw) {
        const QString otherUser = otherRaw.trimmed();
        if (otherUser.isEmpty())
            return;
        QString err;
        const int tid = TaskChatDialog::ensureThreadWithUser(currentUser, otherUser, QString(), &err);
        if (tid <= 0) {
            QMessageBox::warning(this, QStringLiteral("Перейти в диалог"),
                err.isEmpty() ? QStringLiteral("Не удалось открыть чат") : err);
            return;
        }
        TaskChatDialog::markNextMessageSpecial(tid, specialContext);
        activeChatThreadId_ = tid;
        activeChatPeer_ = otherUser;
        showChatsPage();
        reloadChatsPageList();
    };

    if (order.size() == 1) {
        openWithPeer(order[0]);
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Перейти в диалог"));
    dlg.setMinimumSize(s(360), s(300));
    dlg.setStyleSheet(
        "QDialog{background:#F4F6FA;} QListWidget{background:#FFFFFF;border:1px solid #E5EAF2;border-radius:12px;padding:4px;} "
        "QListWidget::item{min-height:48px;padding:8px 12px;border-radius:8px;} QListWidget::item:hover{background:#E8EEFF;} "
        "QLabel{font-weight:600;color:#475569;font-size:13px;}"
    );
    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 16, 16, 16);
    QLabel *hint = new QLabel(QStringLiteral("С кем открыть диалог?"), &dlg);
    root->addWidget(hint);
    QListWidget *list = new QListWidget(&dlg);
    const QVector<UserInfo> allUsers = getAllUsers(false);
    for (const QString &username : order) {
        QString display = username;
        for (const UserInfo &u : allUsers) {
            if (u.username.compare(username, Qt::CaseInsensitive) == 0) {
                display = u.fullName.trimmed().isEmpty() ? u.username : (u.fullName + QStringLiteral(" (") + u.username + QLatin1Char(')'));
                break;
            }
        }
        list->addItem(display);
        list->item(list->count() - 1)->setData(Qt::UserRole, username);
    }
    root->addWidget(list, 1);
    connect(list, &QListWidget::itemClicked, &dlg, [&dlg, openWithPeer](QListWidgetItem *item) {
        if (!item)
            return;
        const QString otherUser = item->data(Qt::UserRole).toString().trimmed();
        if (otherUser.isEmpty())
            return;
        dlg.accept();
        openWithPeer(otherUser);
    });
    QPushButton *cancelBtn = new QPushButton(QStringLiteral("Отмена"), &dlg);
    cancelBtn->setStyleSheet(
        "QPushButton{background:#E2E8F0;color:#334155;font-weight:700;padding:10px 20px;border-radius:10px;} "
        "QPushButton:hover{background:#CBD5E1;}");
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    root->addWidget(cancelBtn);
    dlg.exec();
}

void leftMenu::reloadChatsPageList()
{
    if (!chatsPage || !chatsListLayout_) return;

    QWidget *listHost = qobject_cast<QWidget*>(chatsListLayout_->parent());
    if (!listHost) listHost = chatsPage;

    if (chatsPage) chatsPage->setUpdatesEnabled(false);
    if (listHost) listHost->setUpdatesEnabled(false);

    const QString currentUser = AppSession::currentUsername();
    const QString role = getUserRole(currentUser);
    const bool isAdmin = (role == "admin" || role == "tech");
    QVector<TaskChatThread> threads = isAdmin ? getThreadsForAdmin(currentUser) : getThreadsForUser(currentUser);
    const QHash<int, int> unreadMap = unreadChatCountsByThread(currentUser);
    for (TaskChatThread &t : threads)
        t.unreadCount = unreadMap.value(t.id, 0);

    // Фильтр: все / открытые / закрытые
    if (chatFilterMode_ == 1) {
        QVector<TaskChatThread> filtered;
        filtered.reserve(threads.size());
        for (const TaskChatThread &t : threads)
            if (!t.isClosed()) filtered.append(t);
        threads = filtered;
    } else if (chatFilterMode_ == 2) {
        QVector<TaskChatThread> filtered;
        filtered.reserve(threads.size());
        for (const TaskChatThread &t : threads)
            if (t.isClosed()) filtered.append(t);
        threads = filtered;
    }

    // Поиск по имени задачи / AGV / собеседнику / превью
    if (!chatSearchQuery_.isEmpty()) {
        QVector<TaskChatThread> matched;
        matched.reserve(threads.size());
        for (const TaskChatThread &t : threads) {
            const QString otherUser = (t.createdBy == currentUser) ? t.recipientUser : t.createdBy;
            const QString hay = (otherUser + QStringLiteral(" ")
                                 + t.agvId + QStringLiteral(" ")
                                 + t.taskName + QStringLiteral(" ")
                                 + t.lastMessagePreview).toLower();
            if (hay.contains(chatSearchQuery_))
                matched.append(t);
        }
        threads = matched;
    }

    // Сортировка
    if (chatSortMode_ == 0) {
        // Сначала непрочитанные (по убыванию), затем по последнему сообщению
        std::sort(threads.begin(), threads.end(), [](const TaskChatThread &a, const TaskChatThread &b) {
            if (a.unreadCount != b.unreadCount)
                return a.unreadCount > b.unreadCount;
            return a.lastMessageAt > b.lastMessageAt;
        });
    } else if (chatSortMode_ == 1) {
        std::sort(threads.begin(), threads.end(), [](const TaskChatThread &a, const TaskChatThread &b) {
            return a.lastMessageAt > b.lastMessageAt;
        });
    } else if (chatSortMode_ == 2) {
        std::sort(threads.begin(), threads.end(), [currentUser](const TaskChatThread &a, const TaskChatThread &b) {
            const QString ua = (a.createdBy == currentUser) ? a.recipientUser : a.createdBy;
            const QString ub = (b.createdBy == currentUser) ? b.recipientUser : b.createdBy;
            return ua.toLower() < ub.toLower();
        });
    }

    // Комплексный тест: не грузим ФИО/аватар по каждому ряду (N запросов к БД → подвисание UI).
    const bool complexTestFast = stressSuiteRunning_;
    if (threads.size() > 100)
        threads.resize(100);

    const QString newSignature = makeChatListSignature(threads);
    if (!complexTestFast && newSignature == lastChatsListSignature_ && chatsListLayout_->count() > 0) {
        if (chatsPage) chatsPage->setUpdatesEnabled(true);
        if (listHost) listHost->setUpdatesEnabled(true);
        return;
    }

    while (QLayoutItem *it = chatsListLayout_->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }

    if (threads.isEmpty()) {
        lastChatsListSignature_ = newSignature;
        QLabel *empty = new QLabel("Чатов пока нет", listHost);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:700;color:#94A3B8;"
        ).arg(s(16)));
        chatsListLayout_->addStretch();
        chatsListLayout_->addWidget(empty);
        chatsListLayout_->addStretch();
        if (chatsPage) chatsPage->setUpdatesEnabled(true);
        if (listHost) listHost->setUpdatesEnabled(true);
        return;
    }

    QSet<QString> peerUsers;
    for (const TaskChatThread &t : threads) {
        const QString otherUser = (t.createdBy == currentUser) ? t.recipientUser : t.createdBy;
        if (!otherUser.isEmpty())
            peerUsers.insert(otherUser);
    }
    const QHash<QString, ChatPeerMeta> peerMetaMap = loadChatPeerMeta(peerUsers);
    const QPixmap defaultUserPm = QPixmap(QStringLiteral(":/new/mainWindowIcons/noback/user.png"));

    for (const TaskChatThread &t : threads) {
        QString otherUser = (t.createdBy == currentUser) ? t.recipientUser : t.createdBy;
        QString otherDisplay = otherUser.isEmpty() ? QString("Без адресата") : otherUser;
        const ChatPeerMeta meta = peerMetaMap.value(otherUser);
        if (!meta.displayName.isEmpty())
            otherDisplay = meta.displayName;
        const QString lastSeenText = formatLastSeenText(meta.isActive, meta.lastLogin);

        const QString agvText = t.agvId.trimmed();
        QString secondaryText;
        if (!agvText.isEmpty() && agvText != QStringLiteral("—")) {
            secondaryText = agvText;
            if (!t.taskName.isEmpty())
                secondaryText += QString(" • %1").arg(t.taskName);
        } else if (!t.taskName.isEmpty()) {
            secondaryText = t.taskName;
        }

        QPushButton *btn = new QPushButton(listHost);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(s(t.lastMessagePreview.isEmpty() ? 94 : 108));
        QPixmap peerAvatar;
        if (!otherUser.isEmpty()) {
            const QString cacheKey = otherUser.trimmed();
            if (avatarCache_.contains(cacheKey)) {
                peerAvatar = avatarCache_.value(cacheKey);
            } else if (!meta.avatar.isNull()) {
                peerAvatar = meta.avatar;
                avatarCache_.insert(cacheKey, peerAvatar);
            }
        }
        if (peerAvatar.isNull())
            peerAvatar = defaultUserPm;
        btn->setStyleSheet(QString(
            "QPushButton{background:white;border:1px solid transparent;border-radius:%1px;padding:%2px %3px;"
            "text-align:left;%4}"
            "QPushButton:hover{background:rgba(80,118,251,36);border:1px solid rgb(80,118,251);}"
        ).arg(s(14)).arg(s(13)).arg(s(14))
         .arg(t.isClosed() ? "opacity:0.9;" : ""));

        QHBoxLayout *btnLay = new QHBoxLayout(btn);
        btnLay->setContentsMargins(s(14), s(12), s(14), s(12));
        btnLay->setSpacing(s(12));

        QWidget *avatarWrap = new QWidget(btn);
        avatarWrap->setFixedSize(s(42), s(42));
        avatarWrap->setAttribute(Qt::WA_TransparentForMouseEvents);
        QLabel *avatarLbl = new QLabel(avatarWrap);
        avatarLbl->setGeometry(0, 0, s(42), s(42));
        avatarLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        if (!peerAvatar.isNull()) {
            avatarLbl->setPixmap(makeRoundPixmap(peerAvatar, s(42)));
            avatarLbl->setAlignment(Qt::AlignCenter);
        }
        const int dotPx = s(10);
        const int dotRadius = qMax(1, dotPx / 2);
        QLabel *statusDot = new QLabel(avatarWrap);
        statusDot->setFixedSize(dotPx, dotPx);
        statusDot->move(s(42) - dotPx - s(2), s(42) - dotPx - s(2));
        statusDot->setAttribute(Qt::WA_TransparentForMouseEvents);
        const bool peerOnline = !otherUser.isEmpty() && chatListPeerShowsOnline(meta.isActive, meta.lastLogin);
        statusDot->setStyleSheet(
            peerOnline
                ? QStringLiteral("background:#22C55E;border:%1px solid #FFFFFF;border-radius:%2px;")
                      .arg(s(2))
                      .arg(dotRadius)
                : QStringLiteral("background:#CBD5E1;border:%1px solid #FFFFFF;border-radius:%2px;")
                      .arg(s(2))
                      .arg(dotRadius));
        statusDot->raise();

        QWidget *textHost = new QWidget(btn);
        textHost->setAttribute(Qt::WA_TransparentForMouseEvents);
        QVBoxLayout *textLay = new QVBoxLayout(textHost);
        textLay->setContentsMargins(0, 0, 0, 0);
        textLay->setSpacing(s(2));

        QWidget *nameRow = new QWidget(textHost);
        nameRow->setAttribute(Qt::WA_TransparentForMouseEvents);
        QHBoxLayout *nameRowLay = new QHBoxLayout(nameRow);
        nameRowLay->setContentsMargins(0, 0, 0, 0);
        nameRowLay->setSpacing(s(6));

        QLabel *nameLbl = new QLabel(nameRow);
        nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        nameLbl->setTextFormat(Qt::RichText);
        nameLbl->setText(highlightSearchMatch(otherDisplay, chatSearchQuery_));
        nameLbl->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:800;color:%2;background:transparent;%3"
        ).arg(s(15))
         .arg(t.isClosed() ? "#94A3B8" : "#0F172A")
         .arg(t.isClosed() ? "text-decoration: line-through;" : ""));
        nameRowLay->addWidget(nameLbl, 1);

        if (t.unreadCount > 0) {
            QLabel *unreadBadge = new QLabel(nameRow);
            unreadBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
            unreadBadge->setText(t.unreadCount > 99 ? QStringLiteral("99+") : QString::number(t.unreadCount));
            unreadBadge->setAlignment(Qt::AlignCenter);
            unreadBadge->setFixedSize(s(26), s(22));
            unreadBadge->setStyleSheet(QString(
                "background:#FF3B30;color:white;font-family:Inter;font-size:%1px;font-weight:900;"
                "border-radius:%2px;"
            ).arg(s(10)).arg(s(11)));
            nameRowLay->addWidget(unreadBadge, 0, Qt::AlignRight);
        }
        textLay->addWidget(nameRow);

        if (!t.lastMessagePreview.isEmpty()) {
            QLabel *previewLbl = new QLabel(textHost);
            previewLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            previewLbl->setWordWrap(false);
            previewLbl->setTextFormat(Qt::RichText);
            const QString elided = QFontMetrics(previewLbl->font())
                                       .elidedText(t.lastMessagePreview, Qt::ElideRight, s(360));
            previewLbl->setText(highlightSearchMatch(elided, chatSearchQuery_));
            previewLbl->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;font-weight:600;color:%2;background:transparent;"
            ).arg(s(12)).arg(t.unreadCount > 0 ? "#1D4ED8" : "#64748B"));
            textLay->addWidget(previewLbl);
        }

        if (!secondaryText.trimmed().isEmpty()) {
            QLabel *secondaryLbl = new QLabel(textHost);
            secondaryLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            secondaryLbl->setWordWrap(true);
            secondaryLbl->setTextFormat(Qt::RichText);
            secondaryLbl->setText(highlightSearchMatch(secondaryText, chatSearchQuery_));
            secondaryLbl->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;font-weight:700;color:%2;background:transparent;%3"
            ).arg(s(13))
             .arg(t.isClosed() ? "#94A3B8" : "#475569")
             .arg(t.isClosed() ? "text-decoration: line-through;" : ""));
            textLay->addWidget(secondaryLbl);
        }

        QLabel *lastSeenLbl = new QLabel(lastSeenText, textHost);
        lastSeenLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        lastSeenLbl->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:600;color:#94A3B8;background:transparent;"
        ).arg(s(12)));
        textLay->addWidget(lastSeenLbl);

        btnLay->addWidget(avatarWrap, 0, Qt::AlignTop);
        btnLay->addWidget(textHost, 1);

        connect(btn, &QPushButton::clicked, this, [this, t]() {
            const QString currentUser = AppSession::currentUsername();
            const QString peer = (t.createdBy == currentUser) ? t.recipientUser : t.createdBy;
            activeChatThreadId_ = t.id;
            activeChatPeer_ = peer;
            if (embeddedChatWidget_) {
                embeddedChatWidget_->setThreadId(t.id, peer);
                if (chatsStack_) chatsStack_->setCurrentIndex(1);
            }
        });

        // Контекстное меню строки чата: отметить прочитанным / архивировать / восстановить
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QPushButton::customContextMenuRequested, this, [this, t, btn](const QPoint &pos) {
            QMenu menu(btn);
            menu.setStyleSheet(QStringLiteral(
                "QMenu{background:#FFFFFF;border:1px solid #D5DCE8;padding:4px;}"
                "QMenu::item{padding:8px 18px;font-family:Inter;font-size:13px;font-weight:700;color:#0F172A;}"
                "QMenu::item:selected{background:#E6E6E6;}"
                "QMenu::separator{height:1px;background:#E2E8F0;margin:4px 0;}"));
            const QString currentUser = AppSession::currentUsername();

            if (t.unreadCount > 0) {
                QAction *markRead = menu.addAction(QStringLiteral("Отметить прочитанным"));
                connect(markRead, &QAction::triggered, this, [this, t, currentUser]() {
                    clearChatNotificationsForThread(currentUser, t.id);
                    DataBus::instance().triggerNotificationsChanged();
                    lastChatsListSignature_.clear();
                    reloadChatsPageList();
                });
            }

            if (!t.isClosed()) {
                QAction *archive = menu.addAction(QStringLiteral("Архивировать"));
                connect(archive, &QAction::triggered, this, [this, t, currentUser]() {
                    QString err;
                    if (!closeThread(t.id, currentUser, err)) {
                        QMessageBox::warning(this, QStringLiteral("Ошибка"), err);
                        return;
                    }
                    lastChatsListSignature_.clear();
                    reloadChatsPageList();
                });
            } else {
                QAction *restore = menu.addAction(QStringLiteral("Восстановить из архива"));
                connect(restore, &QAction::triggered, this, [this, t]() {
                    QString err;
                    if (!reopenThread(t.id, err)) {
                        QMessageBox::warning(this, QStringLiteral("Ошибка"), err);
                        return;
                    }
                    lastChatsListSignature_.clear();
                    reloadChatsPageList();
                });
            }

            menu.addSeparator();
            QAction *openChat = menu.addAction(QStringLiteral("Открыть чат"));
            connect(openChat, &QAction::triggered, this, [this, t]() {
                const QString cu = AppSession::currentUsername();
                const QString peer = (t.createdBy == cu) ? t.recipientUser : t.createdBy;
                activeChatThreadId_ = t.id;
                activeChatPeer_ = peer;
                if (embeddedChatWidget_) {
                    embeddedChatWidget_->setThreadId(t.id, peer);
                    if (chatsStack_) chatsStack_->setCurrentIndex(1);
                }
            });

            menu.exec(btn->mapToGlobal(pos));
        });
        chatsListLayout_->addWidget(btn);
    }
    chatsListLayout_->addStretch();
    lastChatsListSignature_ = newSignature;

    if (chatsPage) chatsPage->setUpdatesEnabled(true);
    if (listHost) listHost->setUpdatesEnabled(true);
    if (chatsPage) chatsPage->update();
}

void leftMenu::showChatAdvancedSearchDialog()
{
    const QString currentUser = AppSession::currentUsername();
    const QString role = getUserRole(currentUser);
    const bool isAdmin = (role == "admin" || role == "tech");

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Расширенный поиск по сообщениям"));
    dlg.setMinimumSize(QSize(s(620), s(520)));
    dlg.setStyleSheet(QStringLiteral(
        "QDialog{background:#F0F4FA;}"
        "QLabel{color:#0F172A;font-family:Inter;font-size:13px;font-weight:700;background:transparent;}"
        "QLineEdit,QComboBox,QDateEdit{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:8px;"
        "padding:6px 10px;font-family:Inter;font-size:13px;font-weight:600;color:#0F172A;}"
        "QPushButton{font-family:Inter;font-size:13px;font-weight:800;border-radius:8px;padding:8px 16px;border:none;}"
        "QPushButton#searchBtn{background-color:#0F00DB;color:white;}"
        "QPushButton#searchBtn:hover{background-color:#1A4ACD;}"
        "QPushButton#closeBtn{background-color:#E6E6E6;color:#1A1A1A;border:1px solid #C8C8C8;}"
        "QListWidget{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:10px;"
        "font-family:Inter;font-size:13px;font-weight:600;color:#0F172A;}"
        "QListWidget::item{padding:10px 12px;border-bottom:1px solid #EEF2F7;}"
        "QListWidget::item:selected{background:#E6E6E6;}"
        "QCheckBox{color:#0F172A;font-family:Inter;font-size:13px;font-weight:700;}"));

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(s(18), s(16), s(18), s(16));
    root->setSpacing(s(10));

    // Панель фильтров
    QHBoxLayout *filterRow = new QHBoxLayout();
    filterRow->setSpacing(s(8));

    QLineEdit *queryEdit = new QLineEdit(&dlg);
    queryEdit->setPlaceholderText(QStringLiteral("Текст сообщения..."));
    filterRow->addWidget(queryEdit, 3);

    QComboBox *agvCombo = new QComboBox(&dlg);
    agvCombo->addItem(QStringLiteral("Любой AGV"));
    filterRow->addWidget(agvCombo, 2);

    QComboBox *senderCombo = new QComboBox(&dlg);
    senderCombo->addItem(QStringLiteral("Любой отправитель"));
    filterRow->addWidget(senderCombo, 2);

    root->addLayout(filterRow);

    // Дата
    QHBoxLayout *dateRow = new QHBoxLayout();
    dateRow->setSpacing(s(8));
    QCheckBox *dateCheck = new QCheckBox(QStringLiteral("Ограничить по дате"), &dlg);
    dateCheck->setChecked(false);
    QDateEdit *fromEdit = new QDateEdit(QDate::currentDate().addMonths(-1), &dlg);
    fromEdit->setCalendarPopup(true);
    fromEdit->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    fromEdit->setEnabled(false);
    QDateEdit *toEdit = new QDateEdit(QDate::currentDate(), &dlg);
    toEdit->setCalendarPopup(true);
    toEdit->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    toEdit->setEnabled(false);
    QObject::connect(dateCheck, &QCheckBox::toggled, &dlg, [fromEdit, toEdit](bool on) {
        fromEdit->setEnabled(on);
        toEdit->setEnabled(on);
    });
    dateRow->addWidget(dateCheck);
    dateRow->addWidget(new QLabel(QStringLiteral("с:"), &dlg));
    dateRow->addWidget(fromEdit);
    dateRow->addWidget(new QLabel(QStringLiteral("по:"), &dlg));
    dateRow->addWidget(toEdit);
    dateRow->addStretch();
    root->addLayout(dateRow);

    // Кнопки
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(s(8));
    QPushButton *searchBtn = new QPushButton(QStringLiteral("Искать"), &dlg);
    searchBtn->setObjectName(QStringLiteral("searchBtn"));
    QPushButton *closeBtn = new QPushButton(QStringLiteral("Закрыть"), &dlg);
    closeBtn->setObjectName(QStringLiteral("closeBtn"));
    btnRow->addStretch();
    btnRow->addWidget(searchBtn);
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    QLabel *statusLbl = new QLabel(QStringLiteral("Введите запрос и нажмите «Искать»"), &dlg);
    root->addWidget(statusLbl);

    QListWidget *resultsList = new QListWidget(&dlg);
    resultsList->setSelectionMode(QAbstractItemView::SingleSelection);
    root->addWidget(resultsList, 1);

    // Предзаполнение фильтров AGV / отправители из доступных тредов
    QVector<TaskChatThread> preThreads = isAdmin ? getThreadsForAdmin(currentUser) : getThreadsForUser(currentUser);
    QSet<QString> agvSet, senderSet;
    for (const TaskChatThread &t : preThreads) {
        const QString a = t.agvId.trimmed();
        if (!a.isEmpty() && a != QStringLiteral("—"))
            agvSet.insert(a);
        if (!t.createdBy.isEmpty()) senderSet.insert(t.createdBy);
        if (!t.recipientUser.isEmpty()) senderSet.insert(t.recipientUser);
    }
    QStringList agvList = agvSet.values();
    agvList.sort();
    agvCombo->addItems(agvList);
    QStringList senderList = senderSet.values();
    senderList.sort();
    senderCombo->addItems(senderList);

    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    QObject::connect(searchBtn, &QPushButton::clicked, &dlg, [&, currentUser, isAdmin]() {
        const QString query = queryEdit->text().trimmed();
        const QString queryLower = query.toLower();
        const bool hasText = !queryLower.isEmpty();
        const bool useDate = dateCheck->isChecked();
        const QDate fromDate = fromEdit->date();
        const QDate toDate = toEdit->date();
        const QString agvFilter = agvCombo->currentIndex() > 0 ? agvCombo->currentText() : QString();
        const QString senderFilter = senderCombo->currentIndex() > 0 ? senderCombo->currentText() : QString();

        if (!hasText && !useDate && agvFilter.isEmpty() && senderFilter.isEmpty()) {
            statusLbl->setText(QStringLiteral("Укажите хотя бы один критерий поиска"));
            return;
        }

        resultsList->clear();
        statusLbl->setText(QStringLiteral("Поиск..."));
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QApplication::processEvents();

        QVector<TaskChatThread> threads = isAdmin ? getThreadsForAdmin(currentUser) : getThreadsForUser(currentUser);

        // Предфильтр по AGV
        if (!agvFilter.isEmpty()) {
            QVector<TaskChatThread> f;
            f.reserve(threads.size());
            for (const TaskChatThread &t : threads)
                if (t.agvId.trimmed() == agvFilter) f.append(t);
            threads = f;
        }

        // Предфильтр по отправителю: оставляем треды, где отправитель — участник
        if (!senderFilter.isEmpty()) {
            QVector<TaskChatThread> f;
            f.reserve(threads.size());
            for (const TaskChatThread &t : threads)
                if (t.createdBy == senderFilter || t.recipientUser == senderFilter) f.append(t);
            threads = f;
        }

        // Ограничиваем количество просматриваемых тредов ради отзывчивости
        if (threads.size() > 120)
            threads.resize(120);

        struct SearchResult {
            int threadId;
            int messageId;
            QString display;
        };
        QVector<SearchResult> results;
        const int perThreadLimit = 800;
        const int maxResults = 300;
        int scannedThreads = 0;

        for (const TaskChatThread &t : threads) {
            if (results.size() >= maxResults) break;
            const QVector<TaskChatMessage> msgs = getMessagesForThreadLastN(t.id, currentUser, perThreadLimit, true);
            scannedThreads++;
            const QString peer = (t.createdBy == currentUser) ? t.recipientUser : t.createdBy;
            const QString peerDisplay = peer.isEmpty() ? QStringLiteral("Без адресата") : peer;
            const QString agvText = t.agvId.trimmed();
            for (const TaskChatMessage &m : msgs) {
                if (results.size() >= maxResults) break;
                if (!senderFilter.isEmpty() && m.fromUser != senderFilter)
                    continue;
                if (useDate) {
                    const QDate d = m.createdAt.date();
                    if (d < fromDate || d > toDate) continue;
                }
                // Тело сообщения хранит (возможно) преикс ответа "↩ N\n",
                // forwarded-обёртку и хвост вложения "[[ATTACHMENT]]...". Чистим
                // только для поисковой выдачи — ищем по всему тексту.
                QString body = m.message;
                int attIdx = body.indexOf(QStringLiteral("[[ATTACHMENT]]"));
                if (attIdx >= 0)
                    body = body.left(attIdx);
                // Убираем префикс ответа «↩ N»
                static const QRegularExpression replyRe(QStringLiteral("^↩\\s*\\d+\\s*\\n"));
                body.remove(replyRe);
                body = body.trimmed();
                if (body.isEmpty()) continue;
                if (hasText && !body.toLower().contains(queryLower))
                    continue;
                // Найдено совпадение
                QString snippet = body;
                if (snippet.length() > 160)
                    snippet = snippet.left(157) + QStringLiteral("...");
                const QString dateStr = m.createdAt.toString(QStringLiteral("dd.MM.yyyy HH:mm"));
                const QString fromDisp = m.fromUser.isEmpty() ? QStringLiteral("?") : m.fromUser;
                QString line = QStringLiteral("[%1] %2").arg(dateStr, fromDisp);
                if (!agvText.isEmpty() && agvText != QStringLiteral("—"))
                    line += QStringLiteral(" • AGV %1").arg(agvText);
                line += QStringLiteral(" • %1").arg(peerDisplay);
                line += QStringLiteral(" → %1").arg(snippet);
                SearchResult r;
                r.threadId = t.id;
                r.messageId = m.id;
                r.display = line;
                results.append(r);
            }
        }

        QApplication::restoreOverrideCursor();

        if (results.isEmpty()) {
            statusLbl->setText(QStringLiteral("Ничего не найдено (просмотрено тредов: %1)").arg(scannedThreads));
            return;
        }

        for (const SearchResult &r : results) {
            QListWidgetItem *item = new QListWidgetItem(r.display, resultsList);
            item->setData(Qt::UserRole, r.threadId);
            item->setData(Qt::UserRole + 1, r.messageId);
            resultsList->addItem(item);
        }
        // Сохраняем peer-метку тредов для перехода
        statusLbl->setText(QStringLiteral("Найдено: %1 (просмотрено тредов: %2)")
                               .arg(results.size())
                               .arg(scannedThreads));
    });

    QObject::connect(resultsList, &QListWidget::itemDoubleClicked, &dlg, [this, currentUser, &dlg](QListWidgetItem *item) {
        if (!item || !embeddedChatWidget_) return;
        const int threadId = item->data(Qt::UserRole).toInt();
        if (threadId <= 0) return;
        TaskChatThread t = getThreadById(threadId);
        const QString peer = (t.createdBy == currentUser) ? t.recipientUser : t.createdBy;
        embeddedChatWidget_->setThreadId(threadId, peer);
        if (chatsStack_) chatsStack_->setCurrentIndex(1);
        dlg.accept();
    });

    dlg.exec();
}
