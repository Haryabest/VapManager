#pragma once

#include <QDate>
#include <QString>

struct CalendarEvent {
    QString agvId;
    QString taskTitle;
    QDate date;
    QString severity;
};

struct MaintenanceItemData {
    QString agvId;
    QString agvName;
    QString type;
    QDate date;
    QString details;
    QString severity;
    QString assignedInfo;
    QString assignedUser;
    bool isDelegatedToMe = false;
};

struct SystemStatus {
    int active = 0;
    int maintenance = 0;
    int error = 0;
    int disabled = 0;
};
