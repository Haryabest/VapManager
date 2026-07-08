#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>

enum class OpcServerState {
    Disabled,
    Disconnected,
    Connected
};

struct OpcAgvTelemetry
{
    QString agvId;
    QString status;
    int kilometers = 0;
    QDate lastActive;
    bool linkOk = true;
    bool hasOverdueMaintenance = false;
    bool hasSoonMaintenance = false;
    bool hasMissedMaintenance = false;
    QString model;
    QString serial;
    QDateTime updatedAt;
};

inline QString opcServerStateLabel(OpcServerState state)
{
    switch (state) {
    case OpcServerState::Disabled: return QStringLiteral("отключён");
    case OpcServerState::Disconnected: return QStringLiteral("нет связи");
    case OpcServerState::Connected: return QStringLiteral("подключён");
    }
    return QStringLiteral("неизвестно");
}
