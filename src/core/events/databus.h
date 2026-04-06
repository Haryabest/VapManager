#pragma once
#include <QObject>

class DataBus : public QObject
{
    Q_OBJECT
public:
    static DataBus& instance()
    {
        static DataBus bus;
        return bus;
    }

    void triggerNotificationsChanged() { emit notificationsChanged(); }
    void triggerCalendarChanged() { emit calendarChanged(); }
    void triggerAgvListChanged() { emit agvListChanged(); }
    void triggerModelsChanged() { emit modelsChanged(); }
    void triggerUserDataChanged() { emit userDataChanged(); }
    void triggerAgvTasksChanged(const QString &agvId) { emit agvTasksChanged(agvId); }

signals:
    void agvListChanged();                 // список AGV изменился
    void agvTasksChanged(const QString&);  // задачи конкретного AGV изменились
    void modelsChanged();                  // список моделей изменился
    void calendarChanged();                // влияет на календарь / предстоящее ТО
    void userDataChanged();                // данные пользователя изменились
    void notificationsChanged();           // уведомления обновились

private:
    DataBus() = default;
};
