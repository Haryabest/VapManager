#pragma once

#include <QLabel>

class QWidget;
class QTimer;
class QNetworkAccessManager;
class QNetworkReply;
class QSystemTrayIcon;

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
    void showUpdateTrayNotification();
    void snoozeForOneHour();
    bool isSnoozed() const;
    void scheduleSnoozeTimerIfNeeded();

    QWidget *mainWindow_ = nullptr;
    QTimer *periodicTimer_ = nullptr;
    QTimer *snoozeTimer_ = nullptr;
    QNetworkAccessManager *nam_ = nullptr;
    QNetworkReply *activeReply_ = nullptr;
    QSystemTrayIcon *trayIcon_ = nullptr;
};

namespace AppUpdater {

void checkAndUpdate(QWidget *parent, QLabel *statusLabel = nullptr);
QString formattedLastUpdateDate();
QString updateCheckUrl();
QString updateCheckUrlForHost(const QString &host);
void ensureLastUpdateDateFromInstall();
void reconcilePendingUpdate();
void startBackgroundChecks(QWidget *mainWindow);

struct UpdateHistoryRecord {
    int build = 0;
    QString version;
    QString notes;
    QString date;
};
QVector<UpdateHistoryRecord> updateHistory();
void recordUpdateSeen(int build, const QString &version, const QString &notes);
void recordUpdateApplied(int build, const QString &version, const QString &notes);
void showChangelogDialog(QWidget *parent);
void reinstallCurrentVersion(QWidget *parent);

} // namespace AppUpdater
