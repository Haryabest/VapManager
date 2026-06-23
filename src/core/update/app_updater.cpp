#include "app_updater.h"
#include "app_version.h"
#include "db.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

namespace {

constexpr int kPeriodicCheckHours = 5;
constexpr int kSnoozeHours = 1;

struct RemoteRelease
{
    QString version;
    int build = 0;
    QString notes;
    QString baseUrl;
    QString setupUrl;
    struct FileEntry {
        QString path;
        qint64 size = -1;
    };
    QVector<FileEntry> files;
};

RemoteRelease g_pendingRelease;
bool g_hasPendingRelease = false;

QString updateCheckUrl()
{
    const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini");
    QSettings cfg(path, QSettings::IniFormat);
    const QString fromCfg = cfg.value(QStringLiteral("update_check_url")).toString().trimmed();
    if (!fromCfg.isEmpty())
        return fromCfg;

    const int port = cfg.value(QStringLiteral("update_server_port"), 8765).toInt();
    const QString host = getDbHost();
    if (!host.isEmpty())
        return QStringLiteral("http://%1:%2/version.json").arg(host).arg(port > 0 ? port : 8765);

    return QStringLiteral("https://raw.githubusercontent.com/Haryabest/VapManager/main/releases/version.json");
}

QDateTime snoozeUntil()
{
    QSettings s(QStringLiteral("VapManager"), QStringLiteral("VapManager"));
    return s.value(QStringLiteral("updates/snooze_until")).toDateTime();
}

void setSnoozeUntil(const QDateTime &when)
{
    QSettings s(QStringLiteral("VapManager"), QStringLiteral("VapManager"));
    s.setValue(QStringLiteral("updates/snooze_until"), when);
    s.sync();
}

QDateTime lastUpdateDateTime()
{
    QSettings s(QStringLiteral("VapManager"), QStringLiteral("VapManager"));
    return s.value(QStringLiteral("updates/last_update_at")).toDateTime();
}

void setLastUpdateDateTime(const QDateTime &when)
{
    QSettings s(QStringLiteral("VapManager"), QStringLiteral("VapManager"));
    s.setValue(QStringLiteral("updates/last_update_at"), when);
    s.sync();
}

void showUpToDateStatus(QLabel *statusLabel)
{
    if (!statusLabel)
        return;

    const QString dateText = AppUpdater::formattedLastUpdateDate();
    statusLabel->setWordWrap(true);
    statusLabel->setStyleSheet(
        QStringLiteral("background:transparent;color:#16A34A;font-family:Inter;font-size:12px;font-weight:700;"));
    statusLabel->setText(QStringLiteral("У вас установлена актуальная версия программы\n"
                                         "Последнее обновление: %1")
                             .arg(dateText));
    statusLabel->show();
}

bool parseRelease(const QByteArray &jsonBytes, RemoteRelease *out, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (!doc.isObject()) {
        if (error)
            *error = QStringLiteral("Некорректный JSON: %1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = doc.object();
    out->version = root.value(QStringLiteral("version")).toString().trimmed();
    out->build = root.value(QStringLiteral("build")).toInt();
    out->notes = root.value(QStringLiteral("notes")).toString().trimmed();
    out->baseUrl = root.value(QStringLiteral("baseUrl")).toString().trimmed();
    out->setupUrl = root.value(QStringLiteral("setupUrl")).toString().trimmed();

    const QJsonArray files = root.value(QStringLiteral("files")).toArray();
    for (const QJsonValue &v : files) {
        if (!v.isObject())
            continue;
        const QJsonObject o = v.toObject();
        RemoteRelease::FileEntry entry;
        entry.path = o.value(QStringLiteral("path")).toString().trimmed();
        entry.size = o.value(QStringLiteral("size")).toVariant().toLongLong();
        if (!entry.path.isEmpty())
            out->files.append(entry);
    }
    return out->build > 0;
}

bool fetchBytesSync(QNetworkAccessManager *nam,
                    const QUrl &url,
                    QByteArray *out,
                    QString *error,
                    QProgressDialog *progress)
{
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("VapManager/") + AppVersion::string());
#endif

    QNetworkReply *reply = nam->get(req);
    if (progress) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, progress,
                         [progress](qint64 received, qint64 total) {
                             if (total > 0) {
                                 progress->setMaximum(static_cast<int>(total));
                                 progress->setValue(static_cast<int>(received));
                             } else {
                                 progress->setMaximum(0);
                             }
                         });
    }

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const bool ok = reply->error() == QNetworkReply::NoError;
    if (ok)
        *out = reply->readAll();
    else if (error)
        *error = reply->errorString();

    reply->deleteLater();
    return ok;
}

QVector<RemoteRelease::FileEntry> filesToDownload(const RemoteRelease &release)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const bool newerBuild = release.build > AppVersion::build();
    QVector<RemoteRelease::FileEntry> todo;
    RemoteRelease::FileEntry exeEntry;
    bool hasExeEntry = false;

    for (const RemoteRelease::FileEntry &entry : release.files) {
        if (entry.path.compare(QStringLiteral("config.ini"), Qt::CaseInsensitive) == 0)
            continue;
        if (entry.path.compare(QStringLiteral("VapManager.exe"), Qt::CaseInsensitive) == 0) {
            exeEntry = entry;
            hasExeEntry = true;
        }

        const QString localPath = QDir(appDir).filePath(entry.path);
        const QFileInfo info(localPath);
        if (!info.exists() || (entry.size >= 0 && info.size() != entry.size))
            todo.append(entry);
    }

    // Новый build может совпадать по размеру exe с предыдущим — всё равно качаем exe.
    if (newerBuild && hasExeEntry) {
        bool exeInTodo = false;
        for (const RemoteRelease::FileEntry &entry : todo) {
            if (entry.path.compare(QStringLiteral("VapManager.exe"), Qt::CaseInsensitive) == 0) {
                exeInTodo = true;
                break;
            }
        }
        if (!exeInTodo)
            todo.append(exeEntry);
    }

    return todo;
}

QString stagingPathFor(const QString &stagingRoot, const QString &relativePath)
{
    return QDir(stagingRoot).filePath(relativePath);
}

bool downloadReleaseFiles(QNetworkAccessManager *nam,
                          const RemoteRelease &release,
                          const QVector<RemoteRelease::FileEntry> &files,
                          const QString &stagingRoot,
                          QProgressDialog *progress,
                          QString *error)
{
    if (release.baseUrl.isEmpty()) {
        if (error)
            *error = QStringLiteral("В version.json не указан baseUrl");
        return false;
    }

    const QUrl baseUrl(release.baseUrl);
    int index = 0;
    for (const RemoteRelease::FileEntry &entry : files) {
        ++index;
        progress->setLabelText(QStringLiteral("Загрузка %1 (%2/%3)...")
                                   .arg(entry.path)
                                   .arg(index)
                                   .arg(files.size()));
        progress->setValue(0);
        progress->setMaximum(0);

        const QString dest = stagingPathFor(stagingRoot, entry.path);
        QDir().mkpath(QFileInfo(dest).absolutePath());

        const QUrl fileUrl = baseUrl.resolved(QUrl(entry.path));
        QByteArray bytes;
        if (!fetchBytesSync(nam, fileUrl, &bytes, error, progress))
            return false;

        QFile out(dest);
        if (!out.open(QIODevice::WriteOnly)) {
            if (error)
                *error = QStringLiteral("Не удалось записать %1").arg(dest);
            return false;
        }
        out.write(bytes);
        out.close();
    }
    return true;
}

bool downloadSetupInstaller(QNetworkAccessManager *nam,
                            const RemoteRelease &release,
                            const QString &destPath,
                            QProgressDialog *progress,
                            QString *error)
{
    if (release.setupUrl.isEmpty()) {
        if (error)
            *error = QStringLiteral("URL установщика не указан");
        return false;
    }

    progress->setLabelText(QStringLiteral("Загрузка установщика..."));
    progress->setValue(0);
    progress->setMaximum(0);

    QByteArray bytes;
    if (!fetchBytesSync(nam, QUrl(release.setupUrl), &bytes, error, progress))
        return false;

    QFile out(destPath);
    if (!out.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Не удалось сохранить установщик");
        return false;
    }
    out.write(bytes);
    out.close();
    return true;
}

bool launchSetupAndQuit(const QString &setupPath)
{
    const QString native = QDir::toNativeSeparators(setupPath);
    if (!QProcess::startDetached(native,
                                 QStringList{QStringLiteral("/SILENT"),
                                             QStringLiteral("/CLOSEAPPLICATIONS")}))
        return false;
    QApplication::quit();
    return true;
}

bool launchFileUpdaterAndQuit(const QString &stagingRoot, const QVector<RemoteRelease::FileEntry> &files)
{
    const QString appDir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString scriptPath = QDir::toNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/vapmanager_apply_update.cmd"));

    QString script;
    script += QStringLiteral("@echo off\r\n");
    script += QStringLiteral("timeout /t 2 /nobreak >nul\r\n");
    for (const RemoteRelease::FileEntry &entry : files) {
        const QString from = QDir::toNativeSeparators(stagingPathFor(stagingRoot, entry.path));
        const QString to = QDir::toNativeSeparators(QDir(appDir).filePath(entry.path));
        script += QStringLiteral("mkdir \"%1\" 2>nul\r\n")
                      .arg(QDir::toNativeSeparators(QFileInfo(to).absolutePath()));
        script += QStringLiteral("copy /Y \"%1\" \"%2\" >nul\r\n").arg(from, to);
    }
    script += QStringLiteral("start \"\" \"%1\"\r\n").arg(exePath);
    script += QStringLiteral("del \"%~f0\"\r\n");

    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    scriptFile.write(script.toLocal8Bit());
    scriptFile.close();

    if (!QProcess::startDetached(QStringLiteral("cmd.exe"),
                                 QStringList{QStringLiteral("/C"), scriptPath}))
        return false;
    QApplication::quit();
    return true;
}

QString updatePromptText(const RemoteRelease &release)
{
    const QString notes = release.notes.isEmpty()
        ? QStringLiteral("Доступна новая версия.")
        : release.notes;
    return QStringLiteral("Текущая версия: %1 (build %2)\n"
                          "Новая версия: %3 (build %4)\n\n"
                          "%5\n\n"
                          "Обновить сейчас?")
        .arg(AppVersion::string())
        .arg(AppVersion::build())
        .arg(release.version.isEmpty() ? QStringLiteral("—") : release.version)
        .arg(release.build)
        .arg(notes);
}

bool performUpdateFlow(QWidget *parent, const RemoteRelease &release)
{
    QProgressDialog progress(parent);
    progress.setWindowTitle(QStringLiteral("Обновление VapManager"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setCancelButton(nullptr);
    progress.setLabelText(QStringLiteral("Загрузка обновления..."));
    progress.setValue(0);
    progress.setMaximum(0);
    progress.show();
    QApplication::processEvents();

    QNetworkAccessManager nam;
    QString error;

    const QVector<RemoteRelease::FileEntry> todo = filesToDownload(release);
    if (!todo.isEmpty()) {
        const QString stagingRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                                    + QStringLiteral("/VapManagerUpdate/build_")
                                    + QString::number(release.build);
        QDir().mkpath(stagingRoot);

        if (!downloadReleaseFiles(&nam, release, todo, stagingRoot, &progress, &error)) {
            progress.close();
            QMessageBox::warning(parent,
                                 QStringLiteral("Обновление"),
                                 QStringLiteral("Ошибка загрузки файлов:\n%1").arg(error));
            return false;
        }

        progress.close();
        setSnoozeUntil(QDateTime());
        setLastUpdateDateTime(QDateTime::currentDateTime());
        if (!launchFileUpdaterAndQuit(stagingRoot, todo)) {
            QMessageBox::warning(parent,
                                 QStringLiteral("Обновление"),
                                 QStringLiteral("Не удалось запустить установку обновления."));
            return false;
        }
        return true;
    }

    if (!release.setupUrl.isEmpty()) {
        const QString setupPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                      .filePath(QStringLiteral("VapManager-setup.exe"));
        if (!downloadSetupInstaller(&nam, release, setupPath, &progress, &error)) {
            progress.close();
            QMessageBox::warning(parent,
                                 QStringLiteral("Обновление"),
                                 QStringLiteral("Ошибка загрузки установщика:\n%1").arg(error));
            return false;
        }
        progress.close();
        setSnoozeUntil(QDateTime());
        setLastUpdateDateTime(QDateTime::currentDateTime());
        if (!launchSetupAndQuit(setupPath)) {
            QMessageBox::warning(parent,
                                 QStringLiteral("Обновление"),
                                 QStringLiteral("Не удалось запустить установщик."));
            return false;
        }
        return true;
    }

    progress.close();
    QMessageBox::information(parent,
                             QStringLiteral("Обновление"),
                             QStringLiteral("На сервере есть версия %1, но список файлов пуст.\n"
                                            "Обратитесь к администратору.")
                                 .arg(release.version));
    return false;
}

} // namespace

AppUpdateScheduler *AppUpdateScheduler::instance()
{
    static AppUpdateScheduler s;
    return &s;
}

AppUpdateScheduler::AppUpdateScheduler(QObject *parent)
    : QObject(parent)
{
}

void AppUpdateScheduler::start(QWidget *mainWindow)
{
    mainWindow_ = mainWindow;

    if (!periodicTimer_) {
        periodicTimer_ = new QTimer(this);
        periodicTimer_->setInterval(kPeriodicCheckHours * 3600 * 1000);
        connect(periodicTimer_, &QTimer::timeout, this, &AppUpdateScheduler::onPeriodicCheck);
    }
    if (!snoozeTimer_) {
        snoozeTimer_ = new QTimer(this);
        snoozeTimer_->setSingleShot(true);
        connect(snoozeTimer_, &QTimer::timeout, this, &AppUpdateScheduler::onSnoozeCheck);
    }
    if (!nam_)
        nam_ = new QNetworkAccessManager(this);

    scheduleSnoozeTimerIfNeeded();
    periodicTimer_->start();
}

void AppUpdateScheduler::checkManually(QWidget *parent, QLabel *statusLabel)
{
    if (activeReply_)
        return;

    QWidget *dlgParent = parent ? parent : mainWindow_;
    if (!dlgParent)
        return;

    if (!nam_)
        nam_ = new QNetworkAccessManager(this);

    QProgressDialog progress(dlgParent);
    progress.setWindowTitle(QStringLiteral("Обновление VapManager"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setCancelButton(nullptr);
    progress.setLabelText(QStringLiteral("Проверка обновлений на сервере..."));
    progress.setValue(0);
    progress.setMaximum(0);
    progress.show();
    QApplication::processEvents();

    QString error;
    QByteArray body;
    const bool ok = fetchBytesSync(nam_, QUrl(updateCheckUrl()), &body, &error, &progress);
    progress.close();
    handleManualResult(dlgParent, statusLabel, ok, body, error);
}

void AppUpdateScheduler::onPeriodicCheck()
{
    runBackgroundCheck();
}

void AppUpdateScheduler::onSnoozeCheck()
{
    runBackgroundCheck();
}

bool AppUpdateScheduler::isSnoozed() const
{
    const QDateTime until = snoozeUntil();
    return until.isValid() && until > QDateTime::currentDateTime();
}

void AppUpdateScheduler::scheduleSnoozeTimerIfNeeded()
{
    if (!snoozeTimer_)
        return;
    const QDateTime until = snoozeUntil();
    if (!until.isValid() || until <= QDateTime::currentDateTime()) {
        snoozeTimer_->stop();
        return;
    }
    const qint64 ms = QDateTime::currentDateTime().msecsTo(until);
    if (ms > 0)
        snoozeTimer_->start(static_cast<int>(qMax<qint64>(ms, 1000)));
}

void AppUpdateScheduler::snoozeForOneHour()
{
    setSnoozeUntil(QDateTime::currentDateTime().addSecs(kSnoozeHours * 3600));
    scheduleSnoozeTimerIfNeeded();
}

void AppUpdateScheduler::runBackgroundCheck()
{
    if (activeReply_)
        return;
    fetchReleaseAsync();
}

void AppUpdateScheduler::fetchReleaseAsync()
{
    if (!nam_ || activeReply_)
        return;

    const QUrl checkUrl(updateCheckUrl());
    QNetworkRequest req(checkUrl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("VapManager/") + AppVersion::string());
#endif

    activeReply_ = nam_->get(req);
    connect(activeReply_, &QNetworkReply::finished, this, &AppUpdateScheduler::onFetchFinished);
}

void AppUpdateScheduler::onFetchFinished()
{
    if (!activeReply_)
        return;

    const bool ok = activeReply_->error() == QNetworkReply::NoError;
    const QByteArray body = activeReply_->readAll();
    activeReply_->deleteLater();
    activeReply_ = nullptr;

    if (!ok)
        return;

    RemoteRelease release;
    QString parseError;
    if (!parseRelease(body, &release, &parseError))
        return;
    if (release.build <= AppVersion::build())
        return;
    if (isSnoozed())
        return;

    g_pendingRelease = release;
    g_hasPendingRelease = true;
    showBackgroundPrompt();
}

void AppUpdateScheduler::handleManualResult(QWidget *parent, QLabel *statusLabel, bool ok, const QByteArray &body, const QString &netError)
{
    if (!parent)
        return;

    if (!ok) {
        if (statusLabel) {
            statusLabel->setWordWrap(true);
            statusLabel->setStyleSheet(
                QStringLiteral("background:transparent;color:#DC2626;font-family:Inter;font-size:12px;font-weight:700;"));
            statusLabel->setText(QStringLiteral("Не удалось проверить обновления:\n%1").arg(netError));
            statusLabel->show();
        } else {
            QMessageBox::warning(parent,
                                 QStringLiteral("Обновление"),
                                 QStringLiteral("Не удалось проверить обновления:\n%1").arg(netError));
        }
        return;
    }

    RemoteRelease release;
    QString parseError;
    if (!parseRelease(body, &release, &parseError)) {
        QMessageBox::warning(parent,
                             QStringLiteral("Обновление"),
                             QStringLiteral("Некорректный файл версии на сервере:\n%1").arg(parseError));
        return;
    }

    if (release.build <= AppVersion::build()) {
        showUpToDateStatus(statusLabel);
        return;
    }

    const int answer = QMessageBox::question(
        parent,
        QStringLiteral("Доступно обновление"),
        updatePromptText(release),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (answer == QMessageBox::Yes)
        performUpdateFlow(parent, release);
}

void AppUpdateScheduler::showBackgroundPrompt()
{
    if (!mainWindow_ || !g_hasPendingRelease)
        return;

    const RemoteRelease release = g_pendingRelease;
    g_hasPendingRelease = false;

    QMessageBox box(mainWindow_);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QStringLiteral("Доступно обновление"));
    box.setText(updatePromptText(release));
    QPushButton *updateBtn = box.addButton(QStringLiteral("Обновить"), QMessageBox::AcceptRole);
    QPushButton *laterBtn = box.addButton(QStringLiteral("Позже"), QMessageBox::RejectRole);
    box.setDefaultButton(updateBtn);
    box.exec();

    if (box.clickedButton() == updateBtn) {
        performUpdateFlow(mainWindow_, release);
    } else if (box.clickedButton() == laterBtn) {
        snoozeForOneHour();
    }
}

void AppUpdater::checkAndUpdate(QWidget *parent, QLabel *statusLabel)
{
    AppUpdateScheduler::instance()->checkManually(parent, statusLabel);
}

QString AppUpdater::formattedLastUpdateDate()
{
    QDateTime when = lastUpdateDateTime();
    if (!when.isValid()) {
        const QString exePath = QCoreApplication::applicationDirPath() + QStringLiteral("/VapManager.exe");
        when = QFileInfo(exePath).lastModified();
    }
    if (!when.isValid())
        return QStringLiteral("—");
    return when.toString(QStringLiteral("dd.MM.yyyy"));
}

void AppUpdater::ensureLastUpdateDateFromInstall()
{
    QSettings s(QStringLiteral("VapManager"), QStringLiteral("VapManager"));
    if (s.contains(QStringLiteral("updates/last_update_at")))
        return;

    const QString exePath = QCoreApplication::applicationDirPath() + QStringLiteral("/VapManager.exe");
    const QDateTime installedAt = QFileInfo(exePath).lastModified();
    if (installedAt.isValid())
        setLastUpdateDateTime(installedAt);
}

void AppUpdater::startBackgroundChecks(QWidget *mainWindow)
{
    AppUpdateScheduler::instance()->start(mainWindow);
}
