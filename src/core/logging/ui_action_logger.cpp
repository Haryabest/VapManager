#include "ui_action_logger.h"

#include "db_users.h"
#include "app_session.h"

#include <QAction>
#include <QActionEvent>
#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QWidget>

UiActionLogger::UiActionLogger(QObject *parent)
    : QObject(parent)
{
    attachExistingActions();
}

bool UiActionLogger::eventFilter(QObject *obj, QEvent *event)
{
    if (!obj || !event)
        return QObject::eventFilter(obj, event);

    if (event->type() == QEvent::ActionAdded) {
        QActionEvent *ae = static_cast<QActionEvent*>(event);
        attachAction(ae ? ae->action() : nullptr);
    }

    QWidget *w = qobject_cast<QWidget*>(obj);
    if (w) {
        if (event->type() == QEvent::MouseButtonRelease) {
            QAbstractButton *button = qobject_cast<QAbstractButton*>(w);
            if (button && button->isVisible() && button->isEnabled()) {
                QString text = compactText(button->text());
                if (text.isEmpty())
                    text = compactText(button->objectName());
                if (text.isEmpty())
                    text = button->metaObject()->className();

                QString details = QString("button='%1'; class='%2'; window='%3'")
                                      .arg(text,
                                           button->metaObject()->className(),
                                           resolveWindowName(button));
                writeUiLog("ui_button_click", details);
            }
        } else if (event->type() == QEvent::Show) {
            QDialog *dlg = qobject_cast<QDialog*>(w);
            if (dlg) {
                const QString details = QString("dialog='%1'; class='%2'")
                                            .arg(compactText(dlg->windowTitle()),
                                                 dlg->metaObject()->className());
                writeUiLog("ui_dialog_opened", details);
            }
        } else if (event->type() == QEvent::Close) {
            QDialog *dlg = qobject_cast<QDialog*>(w);
            if (dlg) {
                const QString details = QString("dialog='%1'; class='%2'")
                                            .arg(compactText(dlg->windowTitle()),
                                                 dlg->metaObject()->className());
                writeUiLog("ui_dialog_closed", details);
            }
        }
    }

    return QObject::eventFilter(obj, event);
}

void UiActionLogger::attachExistingActions()
{
    const QList<QWidget*> widgets = QApplication::allWidgets();
    for (QWidget *w : widgets) {
        if (!w)
            continue;
        const QList<QAction*> actions = w->actions();
        for (QAction *a : actions)
            attachAction(a);
    }
}

void UiActionLogger::attachAction(QAction *action)
{
    if (!action || attachedActions_.contains(action))
        return;

    attachedActions_.insert(action);
    connect(action, &QAction::triggered, this, [this, action](bool checked){
        QString text = compactText(action->text());
        if (text.isEmpty())
            text = compactText(action->objectName());
        if (text.isEmpty())
            text = "action";

        QString details = QString("action='%1'; checked=%2")
                              .arg(text)
                              .arg(checked ? "true" : "false");
        writeUiLog("ui_action_triggered", details);
    });
}

void UiActionLogger::writeUiLog(const QString &action, const QString &details)
{
    const QString key = action + "|" + details;
    const QDateTime now = QDateTime::currentDateTime();

    // Глобальный ограничитель: не спамим БД на каждый клик
    // (особенно важно при удаленной MySQL).
    if (lastAnyEventAt_.isValid() && lastAnyEventAt_.msecsTo(now) < 1200) {
        return;
    }

    // Защита от очень частых дублей одного и того же события
    if (lastEventKey_ == key && lastEventAt_.isValid() &&
        lastEventAt_.msecsTo(now) < 250) {
        return;
    }

    lastAnyEventAt_ = now;
    lastEventKey_ = key;
    lastEventAt_ = now;

    logAction(AppSession::currentUsername(), action, details);
}

QString UiActionLogger::compactText(const QString &text) const
{
    QString t = text;
    t.replace('\n', ' ');
    t = t.simplified();
    if (t.size() > 200)
        t = t.left(200) + "...";
    return t;
}

QString UiActionLogger::resolveWindowName(QWidget *widget) const
{
    if (!widget)
        return "unknown";

    QWidget *top = widget->window();
    if (!top)
        return "unknown";

    QString name = compactText(top->windowTitle());
    if (!name.isEmpty())
        return name;

    name = compactText(top->objectName());
    if (!name.isEmpty())
        return name;

    return top->metaObject()->className();
}
