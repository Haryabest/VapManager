#pragma once

#include "opc_types.h"

#include <QHash>
#include <QString>

class OpcSnapshotStore
{
public:
    explicit OpcSnapshotStore(const QString &cacheFilePath);

    QHash<QString, OpcAgvTelemetry> loadAll() const;
    void saveAll(const QHash<QString, OpcAgvTelemetry> &snapshots);

    OpcAgvTelemetry value(const QString &agvId) const;
    bool contains(const QString &agvId) const;

private:
    QString cachePath_;
    mutable QHash<QString, OpcAgvTelemetry> cache_;
    mutable bool loaded_ = false;

    void ensureLoaded() const;
};
