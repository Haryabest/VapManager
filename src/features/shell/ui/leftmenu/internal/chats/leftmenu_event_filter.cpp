#include "leftmenu.h"
#include "app_session.h"
#include "notifications_logs.h"
#include "taskchatdialog.h"

#include <QDialog>
#include <QEvent>
#include <QMessageBox>
#include <QPainter>

bool leftMenu::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        for (QWidget *w = qobject_cast<QWidget*>(obj); w; w = qobject_cast<QWidget*>(w->parentWidget())) {
            const int notificationId = w->property("notificationId").toInt();

            if (w->property("openBroadcastDetails").toBool()) {
                const QString senderLogin = w->property("broadcastFromUser").toString().trimmed();
                const QString subject = w->property("broadcastSubject").toString();
                const QString body = w->property("broadcastBody").toString();
                showBroadcastNotificationDetails(senderLogin, subject, body, notificationId);
                return true;
            }

            const QString peerOpen = w->property("openChatPeerUser").toString().trimmed();
            if (!peerOpen.isEmpty()) {
                const QString currentUser = AppSession::currentUsername();
                const QString agvHint = w->property("openChatAgvHint").toString().trimmed();
                QString ctx = w->property("openChatContextText").toString().trimmed();
                if (ctx.isEmpty())
                    ctx = QStringLiteral("По уведомлению");
                QString err;
                const int tid = TaskChatDialog::ensureThreadWithUser(currentUser, peerOpen, agvHint, &err);
                if (tid <= 0) {
                    QMessageBox::warning(this, QStringLiteral("Чат"),
                                           err.isEmpty() ? QStringLiteral("Не удалось открыть чат") : err);
                    return true;
                }
                TaskChatDialog::markNextMessageSpecial(tid, ctx);
                if (notificationId > 0) {
                    markNotificationReadById(notificationId);
                    updateNotifBadge();
                }
                activeChatThreadId_ = tid;
                activeChatPeer_ = peerOpen;
                showChatsPage();
                if (QDialog *dlg = qobject_cast<QDialog*>(w->window()))
                    dlg->accept();
                return true;
            }

            const int chatId = w->property("openChatThreadId").toInt();
            if (chatId > 0) {
                const QString agvMark = w->property("openChatAgvHintForThread").toString().trimmed();
                if (!agvMark.isEmpty())
                    TaskChatDialog::markNextMessageSpecial(chatId, QStringLiteral("Чат по AGV %1").arg(agvMark));
                activeChatThreadId_ = chatId;
                activeChatPeer_.clear();
                showChatsPage();
                if (notificationId > 0) {
                    markNotificationReadById(notificationId);
                    updateNotifBadge();
                }
                if (QDialog *dlg = qobject_cast<QDialog*>(w->window()))
                    dlg->accept();
                return true;
            }
            if (w->property("openChatsPageOnClick").toBool()) {
                showChatsPage();
                if (notificationId > 0) {
                    markNotificationReadById(notificationId);
                    updateNotifBadge();
                }
                if (QDialog *dlg = qobject_cast<QDialog*>(w->window()))
                    dlg->accept();
                return true;
            }
        }
    }

    if (calendarTablePtr && obj == calendarTablePtr->viewport() && event->type() == QEvent::Paint) {

        QPainter p(calendarTablePtr->viewport());
        p.setPen(QColor("#D3D3D3"));

        for (int r = 1; r < calendarTablePtr->rowCount(); r++) {
            for (int c = 0; c < calendarTablePtr->columnCount(); c++) {
                QRect rect = calendarTablePtr->visualRect(calendarTablePtr->model()->index(r, c));
                p.drawRect(rect.adjusted(0,0,-1,-1));
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}
