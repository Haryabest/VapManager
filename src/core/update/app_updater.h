#pragma once

#include <QLabel>

class QWidget;
class QTimer;
class QNetworkAccessManager;
class QNetworkReply;

class AppUpdateScheduler : public QObject
{
    Q_OBJECT
public:
    static AppUpdateScheduler *instance();

    void start(QWidget *mainWindow);
    void checkManually(QWidget *parent, QLabel *statusLabel = nullptr);

private slots:
    void onPeriodicCheck();
    void onSnoozeCheck();
    void onFetchFinished();

private:
    explicit AppUpdateScheduler(QObject *parent = nullptr);

    void runBackgroundCheck();
    void fetchReleaseAsync();
    void handleManualResult(QWidget *parent, QLabel *statusLabel, bool ok, const QByteArray &body, const QString &netError);
    void showBackgroundPrompt();
    void snoozeForOneHour();
    bool isSnoozed() const;
    void scheduleSnoozeTimerIfNeeded();

    QWidget *mainWindow_ = nullptr;
    QTimer *periodicTimer_ = nullptr;
    QTimer *snoozeTimer_ = nullptr;
    QNetworkAccessManager *nam_ = nullptr;
    QNetworkReply *activeReply_ = nullptr;
};

namespace AppUpdater {

void checkAndUpdate(QWidget *parent, QLabel *statusLabel = nullptr);
QString formattedLastUpdateDate();
void ensureLastUpdateDateFromInstall();
void startBackgroundChecks(QWidget *mainWindow);

} // namespace AppUpdater
