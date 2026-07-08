#include "opc_config.h"

#include <QCoreApplication>
#include <QSettings>

static QStringList parseCsvIds(const QString &raw)
{
    QStringList out;
    for (const QString &part : raw.split(QChar(','), Qt::SkipEmptyParts)) {
        const QString id = part.trimmed();
        if (!id.isEmpty())
            out.append(id);
    }
    return out;
}

OpcConfig loadOpcConfig()
{
    OpcConfig cfg;
    const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini");
    QSettings ini(path, QSettings::IniFormat);

    ini.beginGroup(QStringLiteral("Opc"));
    cfg.enabled = ini.value(QStringLiteral("opc_enabled"), false).toBool();
    cfg.endpoint = ini.value(QStringLiteral("opc_endpoint")).toString().trimmed();
    cfg.pollIntervalMs = qMax(1000, ini.value(QStringLiteral("opc_poll_interval_ms"), 3000).toInt());
    cfg.stubMode = ini.value(QStringLiteral("opc_stub_mode"), true).toBool();
    cfg.stubForceDisconnected = ini.value(QStringLiteral("opc_stub_force_disconnected"), false).toBool();
    cfg.offlineAgvIds = parseCsvIds(ini.value(QStringLiteral("opc_offline_agv_ids")).toString());
    cfg.extraAgvIds = parseCsvIds(ini.value(QStringLiteral("opc_extra_agv_ids")).toString());
    ini.endGroup();

    if (!ini.childGroups().contains(QStringLiteral("Opc"))) {
        cfg.enabled = ini.value(QStringLiteral("opc_enabled"), cfg.enabled).toBool();
        cfg.endpoint = ini.value(QStringLiteral("opc_endpoint"), cfg.endpoint).toString().trimmed();
    }

    return cfg;
}
