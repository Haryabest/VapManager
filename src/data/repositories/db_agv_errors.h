#pragma once
#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QVector>
#include <QString>

struct AgvErrorLog
{
    int id = 0;
    QString agvId;
    QDate errorDate;
    QString type;
    QString title;
    QTime timeFrom;
    QTime timeTo;
    int durationMinutes = 0;
    QString createdBy;
    QDateTime createdAt;
};

bool initAgvErrorLogsTable();

bool addAgvErrorLog(const QString &agvId,
                    const QDate &date,
                    const QString &type,
                    const QString &title,
                    const QTime &from,
                    const QTime &to,
                    int durationMinutes,
                    const QString &createdBy,
                    QString *outError = nullptr);

QVector<AgvErrorLog> loadAgvErrorLogs(const QString &agvId /* empty = all */,
                                     const QDate &fromDate /* invalid = no bound */,
                                     const QDate &toDate /* invalid = no bound */,
                                     QString *outError = nullptr);

