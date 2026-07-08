#include "opc_agv_telemetry.h"

#include <QDate>
#include <QDebug>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

OpcStubTelemetryProvider::OpcStubTelemetryProvider(const OpcConfig &config)
    : config_(config)
{
}

bool OpcStubTelemetryProvider::connectServer(QString *outError)
{
    if (config_.stubForceDisconnected) {
        connected_ = false;
        if (outError)
            *outError = QStringLiteral("OPC (заглушка): принудительное отключение в config.ini");
        return false;
    }
    if (config_.endpoint.isEmpty()) {
        connected_ = false;
        if (outError)
            *outError = QStringLiteral("OPC: не задан opc_endpoint");
        return false;
    }

    connected_ = true;
    if (outError)
        outError->clear();
    return true;
}

void OpcStubTelemetryProvider::disconnectServer()
{
    connected_ = false;
}

bool OpcStubTelemetryProvider::fetchTelemetry(QHash<QString, OpcAgvTelemetry> *outTelemetry,
                                              QString *outError)
{
    if (!outTelemetry) {
        if (outError)
            *outError = QStringLiteral("OPC: нет буфера телеметрии");
        return false;
    }
    outTelemetry->clear();

    if (!connected_) {
        if (outError)
            *outError = QStringLiteral("OPC: сервер не подключён");
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen()) {
        if (outError)
            *outError = QStringLiteral("OPC (заглушка): PostgreSQL недоступна для чтения телеметрии");
        return false;
    }

    const QSet<QString> forcedOffline(config_.offlineAgvIds.cbegin(), config_.offlineAgvIds.cend());
    const QDateTime now = QDateTime::currentDateTimeUtc();

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(R"(
        SELECT a.agv_id, a.model, a.serial, a.status, a.kilometers, a."lastActive",
               COALESCE(tf.has_overdue, 0),
               COALESCE(tf.has_soon, 0),
               COALESCE(tf.has_missed, 0)
        FROM agv_list a
        LEFT JOIN (
            SELECT agv_id,
                   MAX(CASE WHEN next_date <= CURRENT_DATE + 3 THEN 1 ELSE 0 END) AS has_overdue,
                   MAX(CASE WHEN next_date > CURRENT_DATE + 3
                             AND next_date <= CURRENT_DATE + 6 THEN 1 ELSE 0 END) AS has_soon,
                   MAX(CASE WHEN next_date < CURRENT_DATE THEN 1 ELSE 0 END) AS has_missed
            FROM agv_tasks
            WHERE next_date IS NOT NULL
            GROUP BY agv_id
        ) tf ON tf.agv_id = a.agv_id
    )"))) {
        if (outError)
            *outError = QStringLiteral("OPC (заглушка): %1").arg(q.lastError().text());
        return false;
    }

    while (q.next()) {
        OpcAgvTelemetry t;
        t.agvId = q.value(0).toString().trimmed();
        if (t.agvId.isEmpty())
            continue;
        t.model = q.value(1).toString();
        t.serial = q.value(2).toString();
        t.status = q.value(3).toString();
        t.kilometers = q.value(4).toInt();
        t.lastActive = q.value(5).toDate();
        t.hasOverdueMaintenance = q.value(6).toInt() > 0;
        t.hasSoonMaintenance = q.value(7).toInt() > 0;
        t.hasMissedMaintenance = q.value(8).toInt() > 0;
        t.linkOk = !forcedOffline.contains(t.agvId);
        if (!t.linkOk)
            t.status = QStringLiteral("offline");
        t.updatedAt = now;
        outTelemetry->insert(t.agvId, t);
    }

    for (const QString &extraId : config_.extraAgvIds) {
        const QString id = extraId.trimmed();
        if (id.isEmpty() || outTelemetry->contains(id))
            continue;

        OpcAgvTelemetry t;
        t.agvId = id;
        t.model = QStringLiteral("OPC");
        t.serial = QStringLiteral("auto");
        t.status = QStringLiteral("online");
        t.kilometers = 0;
        t.lastActive = QDate::currentDate();
        t.linkOk = !forcedOffline.contains(id);
        if (!t.linkOk)
            t.status = QStringLiteral("offline");
        t.updatedAt = now;
        outTelemetry->insert(id, t);
    }

    if (outError)
        outError->clear();
    return true;
}
