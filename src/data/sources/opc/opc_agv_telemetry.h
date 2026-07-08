#pragma once

#include "opc_config.h"
#include "opc_types.h"

#include <QHash>
#include <QString>

class OpcAgvTelemetryProvider
{
public:
    virtual ~OpcAgvTelemetryProvider() = default;

    virtual bool connectServer(QString *outError = nullptr) = 0;
    virtual void disconnectServer() = 0;
    virtual bool fetchTelemetry(QHash<QString, OpcAgvTelemetry> *outTelemetry,
                                QString *outError = nullptr) = 0;
};

class OpcStubTelemetryProvider : public OpcAgvTelemetryProvider
{
public:
    explicit OpcStubTelemetryProvider(const OpcConfig &config);

    bool connectServer(QString *outError = nullptr) override;
    void disconnectServer() override;
    bool fetchTelemetry(QHash<QString, OpcAgvTelemetry> *outTelemetry,
                        QString *outError = nullptr) override;

private:
    OpcConfig config_;
    bool connected_ = false;
};
