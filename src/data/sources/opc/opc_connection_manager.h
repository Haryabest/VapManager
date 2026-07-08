#pragma once

#include "leftmenu/types/leftmenu_types.h"
#include "listagvinfo.h"
#include "opc_config.h"
#include "opc_snapshot_store.h"
#include "opc_types.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <memory>

class OpcAgvTelemetryProvider;

class OpcConnectionManager : public QObject
{
    Q_OBJECT
public:
    static OpcConnectionManager &instance();

    ~OpcConnectionManager() override;

    void start();
    void reloadConfig();
    void poll();

    bool isEnabled() const { return config_.enabled; }
    OpcServerState serverState() const { return serverState_; }
    bool isLive() const { return serverState_ == OpcServerState::Connected; }
    QString statusHint() const;

    void applyToAgvList(QVector<AgvInfo> &list);
    void appendOpcOnlyAgvs(QVector<AgvInfo> &list);
    SystemStatus computeSystemStatus(const QVector<AgvInfo> &mergedList) const;

    QHash<QString, OpcAgvTelemetry> liveTelemetry() const { return liveTelemetry_; }
    QHash<QString, OpcAgvTelemetry> cachedSnapshots() const { return snapshotStore_.loadAll(); }

signals:
    void serverStateChanged(OpcServerState state);
    void telemetryUpdated();

private:
    explicit OpcConnectionManager(QObject *parent = nullptr);

    void setServerState(OpcServerState state);
    void applyTelemetryOverlay(const OpcAgvTelemetry &t, AgvInfo &info, bool fromLive);
    AgvInfo agvInfoFromTelemetry(const OpcAgvTelemetry &t, bool frozen) const;
    void handleLinkTransitions(const QHash<QString, OpcAgvTelemetry> &previous,
                               const QHash<QString, OpcAgvTelemetry> &current);
    void syncTelemetryToDb(const QHash<QString, OpcAgvTelemetry> &telemetry);
    void createCommunicationError(const QString &agvId);
    QString buildStatusLogDetails(const QString &reason) const;
    void writeStatusLog(const QString &action, const QString &reason) const;

    OpcConfig config_;
    OpcSnapshotStore snapshotStore_;
    std::unique_ptr<OpcAgvTelemetryProvider> provider_;
    OpcServerState serverState_ = OpcServerState::Disabled;
    QHash<QString, OpcAgvTelemetry> liveTelemetry_;
    QHash<QString, bool> lastLinkOk_;
    QSet<QString> opcOnlyAgvIds_;
};
