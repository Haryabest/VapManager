#include "opc_snapshot_store.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QJsonObject telemetryToJson(const OpcAgvTelemetry &t)
{
    QJsonObject o;
    o.insert(QStringLiteral("agvId"), t.agvId);
    o.insert(QStringLiteral("status"), t.status);
    o.insert(QStringLiteral("kilometers"), t.kilometers);
    o.insert(QStringLiteral("lastActive"), t.lastActive.isValid() ? t.lastActive.toString(Qt::ISODate) : QString());
    o.insert(QStringLiteral("linkOk"), t.linkOk);
    o.insert(QStringLiteral("hasOverdueMaintenance"), t.hasOverdueMaintenance);
    o.insert(QStringLiteral("hasSoonMaintenance"), t.hasSoonMaintenance);
    o.insert(QStringLiteral("hasMissedMaintenance"), t.hasMissedMaintenance);
    o.insert(QStringLiteral("model"), t.model);
    o.insert(QStringLiteral("serial"), t.serial);
    o.insert(QStringLiteral("updatedAt"), t.updatedAt.isValid() ? t.updatedAt.toString(Qt::ISODate) : QString());
    return o;
}

OpcAgvTelemetry telemetryFromJson(const QJsonObject &o)
{
    OpcAgvTelemetry t;
    t.agvId = o.value(QStringLiteral("agvId")).toString().trimmed();
    t.status = o.value(QStringLiteral("status")).toString().trimmed();
    t.kilometers = o.value(QStringLiteral("kilometers")).toInt();
    t.lastActive = QDate::fromString(o.value(QStringLiteral("lastActive")).toString(), Qt::ISODate);
    t.linkOk = o.value(QStringLiteral("linkOk")).toBool(true);
    t.hasOverdueMaintenance = o.value(QStringLiteral("hasOverdueMaintenance")).toBool(false);
    t.hasSoonMaintenance = o.value(QStringLiteral("hasSoonMaintenance")).toBool(false);
    t.hasMissedMaintenance = o.value(QStringLiteral("hasMissedMaintenance")).toBool(false);
    t.model = o.value(QStringLiteral("model")).toString();
    t.serial = o.value(QStringLiteral("serial")).toString();
    t.updatedAt = QDateTime::fromString(o.value(QStringLiteral("updatedAt")).toString(), Qt::ISODate);
    return t;
}

} // namespace

OpcSnapshotStore::OpcSnapshotStore(const QString &cacheFilePath)
    : cachePath_(cacheFilePath)
{
}

void OpcSnapshotStore::ensureLoaded() const
{
    if (loaded_)
        return;
    loaded_ = true;
    cache_.clear();

    QFile f(cachePath_);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;

    const QJsonArray arr = doc.object().value(QStringLiteral("agv")).toArray();
    for (const QJsonValue &v : arr) {
        if (!v.isObject())
            continue;
        const OpcAgvTelemetry t = telemetryFromJson(v.toObject());
        if (!t.agvId.isEmpty())
            cache_.insert(t.agvId, t);
    }
}

QHash<QString, OpcAgvTelemetry> OpcSnapshotStore::loadAll() const
{
    ensureLoaded();
    return cache_;
}

OpcAgvTelemetry OpcSnapshotStore::value(const QString &agvId) const
{
    ensureLoaded();
    return cache_.value(agvId);
}

bool OpcSnapshotStore::contains(const QString &agvId) const
{
    ensureLoaded();
    return cache_.contains(agvId);
}

void OpcSnapshotStore::saveAll(const QHash<QString, OpcAgvTelemetry> &snapshots)
{
    cache_ = snapshots;
    loaded_ = true;

    QJsonArray arr;
    for (const OpcAgvTelemetry &t : snapshots) {
        if (!t.agvId.isEmpty())
            arr.append(telemetryToJson(t));
    }

    QJsonObject root;
    root.insert(QStringLiteral("agv"), arr);
    root.insert(QStringLiteral("savedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QSaveFile f(cachePath_);
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    f.commit();
}
