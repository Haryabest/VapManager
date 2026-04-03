    #pragma once
    #include <QString>
    #include <QVector>
    #include <QDate>

    struct AgvTask
    {
        QString id;
        QString agvId;
        QString taskName;
        QString taskDescription;
        int intervalDays = 0;
        int durationMinutes = 0;
        bool isDefault = false;
        QDate nextDate;
        QDate lastService;
        QString assignedTo;
    QString delegatedBy;
    };


bool copyModelTasksToAgv(const QString &agvId, const QString &modelName);
QVector<AgvTask> loadAgvTasks(const QString &agvId);
bool ensureAssignedToColumn();
bool ensureAgvListAssignedUserColumn();
QStringList getAgvIdsAssignedToUser(const QString &username);
