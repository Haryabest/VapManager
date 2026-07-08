#pragma once

#include <QString>
#include <QStringList>

struct OpcConfig
{
    bool enabled = false;
    QString endpoint;
    int pollIntervalMs = 3000;
    bool stubMode = true;
    bool stubForceDisconnected = false;
    QStringList offlineAgvIds;
    QStringList extraAgvIds;
};

OpcConfig loadOpcConfig();
