#pragma once

#include <QObject>
#include <QFutureWatcher>

class DbConnectionBridge : public QObject
{
    Q_OBJECT
public:
    explicit DbConnectionBridge(QObject *parent = nullptr);

    Q_INVOKABLE void tryConnect(const QString &rawHost);

signals:
    void connectStarted();
    void connectFinished(bool ok, const QString &host, const QString &errorText);

private:
    QFutureWatcher<QString> m_watcher;
    QString m_lastHost;
    bool m_busy = false;
};
