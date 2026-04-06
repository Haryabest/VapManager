#ifndef UI_ACTION_LOGGER_H
#define UI_ACTION_LOGGER_H

#include <QObject>
#include <QDateTime>
#include <QSet>

class QAction;
class QWidget;

class UiActionLogger : public QObject
{
    Q_OBJECT
public:
    explicit UiActionLogger(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void attachExistingActions();
    void attachAction(QAction *action);
    void writeUiLog(const QString &action, const QString &details);
    QString compactText(const QString &text) const;
    QString resolveWindowName(QWidget *widget) const;

private:
    QSet<QAction*> attachedActions_;
    QString lastEventKey_;
    QDateTime lastEventAt_;
    QDateTime lastAnyEventAt_;
};

#endif // UI_ACTION_LOGGER_H
