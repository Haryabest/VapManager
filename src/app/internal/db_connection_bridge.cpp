#include "db_connection_bridge.h"

#include "db.h"

#include <QtConcurrent/QtConcurrentRun>

DbConnectionBridge::DbConnectionBridge(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<QString>::finished, this, [this]() {
        const QString err = m_watcher.result();
        const bool ok = err.trimmed().isEmpty();
        m_busy = false;
        emit connectFinished(ok, m_lastHost, err);
    });
}

void DbConnectionBridge::tryConnect(const QString &rawHost)
{
    if (m_busy)
        return;

    m_lastHost = rawHost.trimmed();
    if (m_lastHost.isEmpty())
        m_lastHost = QStringLiteral("localhost");

    m_busy = true;
    emit connectStarted();

    const QString host = m_lastHost;
    QFuture<QString> future = QtConcurrent::run([host]() -> QString {
        QString err;
        if (reconnectWithHost(host, &err))
            return QString();
        return err;
    });
    m_watcher.setFuture(future);
}
