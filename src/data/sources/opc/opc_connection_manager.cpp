#include "opc_connection_manager.h"

#include "opc_agv_telemetry.h"
#include "db_agv_errors.h"
#include "db_users.h"
#include "databus.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {

bool isOfflineStatus(const QString &status)
{
    const QString s = status.trimmed().toLower();
    return s == QStringLiteral("offline")
           || s == QStringLiteral("disabled")
           || s == QStringLiteral("off");
}

bool isActiveStatus(const QString &status)
{
    const QString s = status.trimmed().toLower();
    return s == QStringLiteral("online") || s == QStringLiteral("working");
}

void applyMaintenanceFlags(AgvInfo &info, bool overdue, bool soon, bool missed)
{
    info.hasOverdueMaintenance = overdue || missed;
    info.hasSoonMaintenance = soon && !info.hasOverdueMaintenance;
    if (info.hasOverdueMaintenance)
        info.maintenanceState = QStringLiteral("red");
    else if (info.hasSoonMaintenance)
        info.maintenanceState = QStringLiteral("orange");
    else
        info.maintenanceState = QStringLiteral("green");
}

} // namespace

OpcConnectionManager &OpcConnectionManager::instance()
{
    static OpcConnectionManager mgr;
    return mgr;
}

OpcConnectionManager::OpcConnectionManager(QObject *parent)
    : QObject(parent)
    , snapshotStore_(QCoreApplication::applicationDirPath() + QStringLiteral("/opc_snapshot_cache.json"))
{
}

OpcConnectionManager::~OpcConnectionManager() = default;

void OpcConnectionManager::start()
{
    reloadConfig();
    if (!config_.enabled) {
        setServerState(OpcServerState::Disabled);
        writeStatusLog(QStringLiteral("opc_status"), QStringLiteral("старт приложения"));
        return;
    }

    provider_.reset(new OpcStubTelemetryProvider(config_));
    poll();
}

void OpcConnectionManager::reloadConfig()
{
    config_ = loadOpcConfig();
}

QString OpcConnectionManager::statusHint() const
{
    if (!config_.enabled)
        return QStringLiteral("OPC отключён в настройках");
    return QStringLiteral("OPC: %1").arg(opcServerStateLabel(serverState_));
}

QString OpcConnectionManager::buildStatusLogDetails(const QString &reason) const
{
    QStringList parts;
    if (!reason.isEmpty())
        parts << reason;
    parts << QStringLiteral("состояние=%1").arg(opcServerStateLabel(serverState_));
    if (!config_.enabled) {
        parts << QStringLiteral("opc_enabled=false");
        return parts.join(QStringLiteral("; "));
    }

    parts << QStringLiteral("endpoint=%1")
                 .arg(config_.endpoint.isEmpty() ? QStringLiteral("—") : config_.endpoint);
    parts << QStringLiteral("stub=%1").arg(config_.stubMode ? QStringLiteral("да") : QStringLiteral("нет"));

    const int cached = snapshotStore_.loadAll().size();
    parts << QStringLiteral("agv_cache=%1").arg(cached);

    if (isLive()) {
        parts << QStringLiteral("agv_live=%1").arg(liveTelemetry_.size());
        int offline = 0;
        for (auto it = liveTelemetry_.cbegin(); it != liveTelemetry_.cend(); ++it) {
            if (!it.value().linkOk)
                ++offline;
        }
        if (offline > 0)
            parts << QStringLiteral("agv_offline=%1").arg(offline);
    } else if (serverState_ == OpcServerState::Disconnected && cached > 0) {
        parts << QStringLiteral("режим=снимок");
    }

    return parts.join(QStringLiteral("; "));
}

void OpcConnectionManager::writeStatusLog(const QString &action, const QString &reason) const
{
    logAction(QStringLiteral("system"), action, buildStatusLogDetails(reason));
}

void OpcConnectionManager::setServerState(OpcServerState state)
{
    if (serverState_ == state)
        return;
    serverState_ = state;
    writeStatusLog(QStringLiteral("opc_status"), QStringLiteral("изменение состояния"));
    emit serverStateChanged(state);
    DataBus::instance().triggerOpcConnectionChanged();
}

void OpcConnectionManager::poll()
{
    if (!config_.enabled) {
        setServerState(OpcServerState::Disabled);
        return;
    }

    if (!provider_)
        provider_.reset(new OpcStubTelemetryProvider(config_));

    QString connectError;
    if (!provider_->connectServer(&connectError)) {
        qDebug() << "OPC poll: connect failed:" << connectError;
        liveTelemetry_.clear();
        setServerState(OpcServerState::Disconnected);
        return;
    }

    QHash<QString, OpcAgvTelemetry> fetched;
    QString fetchError;
    if (!provider_->fetchTelemetry(&fetched, &fetchError)) {
        qDebug() << "OPC poll: fetch failed:" << fetchError;
        provider_->disconnectServer();
        liveTelemetry_.clear();
        setServerState(OpcServerState::Disconnected);
        return;
    }

    const QHash<QString, OpcAgvTelemetry> previous = liveTelemetry_;
    handleLinkTransitions(previous, fetched);

    liveTelemetry_ = fetched;
    snapshotStore_.saveAll(fetched);

    opcOnlyAgvIds_.clear();
    for (auto it = fetched.cbegin(); it != fetched.cend(); ++it)
        opcOnlyAgvIds_.insert(it.key());

    syncTelemetryToDb(fetched);
    setServerState(OpcServerState::Connected);
    emit telemetryUpdated();
    DataBus::instance().triggerAgvListChanged();
}

void OpcConnectionManager::handleLinkTransitions(const QHash<QString, OpcAgvTelemetry> &previous,
                                                 const QHash<QString, OpcAgvTelemetry> &current)
{
    for (auto it = current.cbegin(); it != current.cend(); ++it) {
        const QString &agvId = it.key();
        const bool nowOk = it.value().linkOk;
        const bool wasOk = previous.contains(agvId) ? previous.value(agvId).linkOk : true;

        if (wasOk && !nowOk)
            createCommunicationError(agvId);

        lastLinkOk_.insert(agvId, nowOk);
    }
}

void OpcConnectionManager::createCommunicationError(const QString &agvId)
{
    if (hasAgvErrorLogToday(agvId, QStringLiteral("communication")))
        return;

    writeStatusLog(QStringLiteral("opc_agv_link_lost"),
                   QStringLiteral("AGV %1: потеря связи на уровне OPC").arg(agvId));

    QString err;
    const bool ok = addAgvErrorLog(
        agvId,
        QDate::currentDate(),
        QStringLiteral("communication"),
        QStringLiteral("Ошибка связи с AGV"),
        QTime(),
        QTime(),
        0,
        QStringLiteral("system"),
        &err);
    if (!ok)
        qDebug() << "OPC: failed to log communication error for" << agvId << err;
}

void OpcConnectionManager::syncTelemetryToDb(const QHash<QString, OpcAgvTelemetry> &telemetry)
{
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen())
        return;

    QSqlQuery q(db);
    for (auto it = telemetry.cbegin(); it != telemetry.cend(); ++it) {
        const OpcAgvTelemetry &t = it.value();
        if (!t.linkOk)
            continue;

        q.prepare(QStringLiteral(R"(
            UPDATE agv_list
            SET status = :status,
                kilometers = :km,
                "lastActive" = :last_active
            WHERE agv_id = :agv_id
        )"));
        q.bindValue(QStringLiteral(":status"), t.status);
        q.bindValue(QStringLiteral(":km"), t.kilometers);
        q.bindValue(QStringLiteral(":last_active"), t.lastActive.isValid() ? t.lastActive : QVariant());
        q.bindValue(QStringLiteral(":agv_id"), t.agvId);
        if (!q.exec())
            qDebug() << "OPC sync:" << t.agvId << q.lastError().text();
    }
}

void OpcConnectionManager::applyTelemetryOverlay(const OpcAgvTelemetry &t, AgvInfo &info, bool fromLive)
{
    Q_UNUSED(fromLive);
    if (isLive()) {
        if (!t.linkOk) {
            info.status = QStringLiteral("offline");
        } else {
            info.status = t.status;
            info.kilometers = t.kilometers;
            if (t.lastActive.isValid())
                info.lastActive = t.lastActive;
        }
        applyMaintenanceFlags(info, t.hasOverdueMaintenance, t.hasSoonMaintenance, t.hasMissedMaintenance);
        return;
    }

    const OpcAgvTelemetry snap = snapshotStore_.value(info.id);
    if (!snap.agvId.isEmpty()) {
        info.status = snap.linkOk ? snap.status : QStringLiteral("offline");
        info.kilometers = snap.kilometers;
        if (snap.lastActive.isValid())
            info.lastActive = snap.lastActive;
        applyMaintenanceFlags(info, snap.hasOverdueMaintenance, snap.hasSoonMaintenance, snap.hasMissedMaintenance);
    }
}

AgvInfo OpcConnectionManager::agvInfoFromTelemetry(const OpcAgvTelemetry &t, bool frozen) const
{
    AgvInfo info;
    info.id = t.agvId;
    info.model = t.model.isEmpty() ? QStringLiteral("OPC") : t.model;
    info.serial = t.serial;
    info.status = (frozen && !t.linkOk) || (!isLive() && !t.linkOk)
                      ? QStringLiteral("offline")
                      : t.status;
    info.kilometers = t.kilometers;
    info.lastActive = t.lastActive;
    info.blueprintPath = QStringLiteral(":/new/mainWindowIcons/noback/blueprint.png");
    applyMaintenanceFlags(info, t.hasOverdueMaintenance, t.hasSoonMaintenance, t.hasMissedMaintenance);
    return info;
}

void OpcConnectionManager::applyToAgvList(QVector<AgvInfo> &list)
{
    if (!config_.enabled)
        return;

    const QHash<QString, OpcAgvTelemetry> source = isLive() ? liveTelemetry_ : snapshotStore_.loadAll();
    for (AgvInfo &info : list) {
        const OpcAgvTelemetry t = source.value(info.id);
        if (t.agvId.isEmpty())
            continue;
        applyTelemetryOverlay(t, info, isLive());
    }
}

void OpcConnectionManager::appendOpcOnlyAgvs(QVector<AgvInfo> &list)
{
    if (!config_.enabled)
        return;

    QSet<QString> existing;
    for (const AgvInfo &a : list)
        existing.insert(a.id);

    const QHash<QString, OpcAgvTelemetry> source = isLive() ? liveTelemetry_ : snapshotStore_.loadAll();
    for (auto it = source.cbegin(); it != source.cend(); ++it) {
        if (existing.contains(it.key()))
            continue;
        list.prepend(agvInfoFromTelemetry(it.value(), !isLive()));
    }
}

SystemStatus OpcConnectionManager::computeSystemStatus(const QVector<AgvInfo> &mergedList) const
{
    SystemStatus st;
    for (const AgvInfo &a : mergedList) {
        if (isOfflineStatus(a.status)) {
            ++st.disabled;
            continue;
        }
        if (a.hasOverdueMaintenance) {
            ++st.error;
            continue;
        }
        if (a.hasSoonMaintenance) {
            ++st.maintenance;
            continue;
        }
        if (isActiveStatus(a.status))
            ++st.active;
    }
    return st;
}
