#include "app_updater.h"
#include "app_version.h"
#include "db.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QProgressBar>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantList>
#include <QVariantMap>

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

QString normalizeReleaseBaseUrl(const QString &baseUrl)
{
    const QString trimmed = baseUrl.trimmed();
    if (trimmed.isEmpty())
        return trimmed;

    const QUrl checkUrl(updateCheckUrl());
    if (!checkUrl.isValid() || checkUrl.host().isEmpty())
        return trimmed;

    QUrl resolved(trimmed);
    if (!resolved.isValid())
        return trimmed;

    resolved.setHost(checkUrl.host());
    if (checkUrl.port() > 0)
        resolved.setPort(checkUrl.port());
    return resolved.toString(QUrl::FullyEncoded);
}

void setPendingUpdateBuild(int build)
{
    QSettings s(QStringLiteral("VapManager"), QStringLiteral("VapManager"));
    s.setValue(QStringLiteral("updates/pending_build"), build);
    s.sync();
}

int pendingUpdateBuild()
{
    QSettings s(QStringLiteral("VapManager"), QStringLiteral("VapManager"));
    return s.value(QStringLiteral("updates/pending_build"), 0).toInt();
}

void clearPendingUpdateBuild()
{
    QSettings s(QStringLiteral("VapManager"), QStringLiteral("VapManager"));
    s.remove(QStringLiteral("updates/pending_build"));
    s.sync();
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

QVariantList loadUpdateHistoryRaw()
{
    QSettings s(QStringLiteral("VapManager"), QStringLiteral("VapManager"));
    return s.value(QStringLiteral("updates/history")).toList();
}

void saveUpdateHistoryRaw(const QVariantList &list)
{
    QSettings s(QStringLiteral("VapManager"), QStringLiteral("VapManager"));
    s.setValue(QStringLiteral("updates/history"), list);
    s.sync();
}

void appendUpdateHistory(int build, const QString &version, const QString &notes, const QString &status)
{
    QVariantList list = loadUpdateHistoryRaw();
    // Dedup by build+status, keep latest
    for (int i = list.size() - 1; i >= 0; --i) {
        const QVariantMap m = list.at(i).toMap();
        if (m.value(QStringLiteral("build")).toInt() == build
            && m.value(QStringLiteral("status")).toString() == status) {
            list.removeAt(i);
        }
    }
    QVariantMap rec;
    rec.insert(QStringLiteral("build"), build);
    rec.insert(QStringLiteral("version"), version);
    rec.insert(QStringLiteral("notes"), notes);
    rec.insert(QStringLiteral("status"), status);
    rec.insert(QStringLiteral("date"), QDateTime::currentDateTime().toString(Qt::ISODate));
    list.prepend(rec);
    while (list.size() > 20)
        list.removeLast();
    saveUpdateHistoryRaw(list);
}

QString formatChangelogFromNotes(const QString &notes)
{
    QString pretty = notes;
    pretty.replace(QStringLiteral(", "), QStringLiteral("\n• "));
    if (pretty.contains(QStringLiteral(": "))) {
        const int idx = pretty.indexOf(QStringLiteral(": "));
        pretty = pretty.mid(idx + 2);
    }
    return pretty;
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
                                 progress->setMaximum(static_cast<int>(qMin<qint64>(total, INT_MAX)));
                                 progress->setValue(static_cast<int>(qMin<qint64>(received, INT_MAX)));
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

// Вариант с прогресс-баром внутри диалога обновления.
bool fetchBytesSyncBar(QNetworkAccessManager *nam,
                       const QUrl &url,
                       QByteArray *out,
                       QString *error,
                       QProgressBar *bar,
                       QLabel *label)
{
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("VapManager/") + AppVersion::string());
#endif

    QNetworkReply *reply = nam->get(req);
    if (bar || label) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, qApp,
                         [bar, label](qint64 received, qint64 total) {
                             if (bar) {
                                 if (total > 0) {
                                     bar->setMaximum(static_cast<int>(qMin<qint64>(total, INT_MAX)));
                                     bar->setValue(static_cast<int>(qMin<qint64>(received, INT_MAX)));
                                 } else {
                                     bar->setMaximum(0);
                                 }
                             }
            Q_UNUSED(label);
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

QVector<RemoteRelease::FileEntry> filesToDownload(const RemoteRelease &release, bool forceAll)
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
        if (forceAll) {
            todo.append(entry);
            continue;
        }
        const QString localPath = QDir(appDir).filePath(entry.path);
        const QFileInfo info(localPath);
        if (!info.exists() || (entry.size >= 0 && info.size() != entry.size))
            todo.append(entry);
    }

    if ((newerBuild || forceAll) && hasExeEntry) {
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

QVector<RemoteRelease::FileEntry> filesToDownload(const RemoteRelease &release)
{
    return filesToDownload(release, false);
}

QVector<RemoteRelease::FileEntry> filesToDownloadForced(const RemoteRelease &release)
{
    return filesToDownload(release, true);
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

bool downloadReleaseFilesBar(QNetworkAccessManager *nam,
                             const RemoteRelease &release,
                             const QVector<RemoteRelease::FileEntry> &files,
                             const QString &stagingRoot,
                             QProgressBar *bar,
                             QLabel *statusLabel,
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
        if (statusLabel)
            statusLabel->setText(QStringLiteral("Загрузка %1 (%2/%3)...")
                                     .arg(entry.path)
                                     .arg(index)
                                     .arg(files.size()));
        if (bar) {
            bar->setValue(0);
            bar->setMaximum(0);
        }

        const QString dest = stagingPathFor(stagingRoot, entry.path);
        QDir().mkpath(QFileInfo(dest).absolutePath());

        const QUrl fileUrl = baseUrl.resolved(QUrl(entry.path));
        QByteArray bytes;
        if (!fetchBytesSyncBar(nam, fileUrl, &bytes, error, bar, statusLabel))
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
    const QString logPath = QDir::toNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/vapmanager_apply_update.log"));
    const QString scriptPath = QDir::toNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/vapmanager_apply_update.cmd"));

    QString script;
    script += QStringLiteral("@echo off\r\n");
    script += QStringLiteral("setlocal EnableDelayedExpansion\r\n");
    script += QStringLiteral("echo [%date% %time%] apply update start > \"%1\"\r\n").arg(logPath);
    script += QStringLiteral("set \"APP_DIR=%1\"\r\n").arg(appDir);
    script += QStringLiteral("set \"EXE_PATH=%1\"\r\n").arg(exePath);
    script += QStringLiteral("timeout /t 2 /nobreak >nul\r\n");
    script += QStringLiteral("taskkill /IM VapManager.exe /F >nul 2>&1\r\n");
    script += QStringLiteral("timeout /t 1 /nobreak >nul\r\n");

    for (const RemoteRelease::FileEntry &entry : files) {
        const QString from = QDir::toNativeSeparators(stagingPathFor(stagingRoot, entry.path));
        const QString to = QDir::toNativeSeparators(QDir(appDir).filePath(entry.path));
        const bool isExe = entry.path.compare(QStringLiteral("VapManager.exe"), Qt::CaseInsensitive) == 0;
        script += QStringLiteral("call :copy_file \"%1\" \"%2\" %3\r\n")
                      .arg(from, to, isExe ? QStringLiteral("1") : QStringLiteral("0"));
    }

    script += QStringLiteral("start \"\" \"%1\"\r\n").arg(exePath);
    script += QStringLiteral("echo [%date% %time%] apply update done >> \"%1\"\r\n").arg(logPath);
    script += QStringLiteral("del \"%~f0\"\r\n");
    script += QStringLiteral("exit /b 0\r\n");
    script += QStringLiteral(":copy_file\r\n");
    script += QStringLiteral("set \"FROM=%~1\"\r\n");
    script += QStringLiteral("set \"TO=%~2\"\r\n");
    script += QStringLiteral("set \"IS_EXE=%~3\"\r\n");
    script += QStringLiteral("mkdir \"%~dp2\" 2>nul\r\n");
    script += QStringLiteral("set /a TRY=0\r\n");
    script += QStringLiteral(":retry_copy\r\n");
    script += QStringLiteral("set /a TRY+=1\r\n");
    script += QStringLiteral("if \"%IS_EXE%\"==\"1\" taskkill /IM VapManager.exe /F >nul 2>&1\r\n");
    script += QStringLiteral("if \"%IS_EXE%\"==\"1\" copy /Y \"%FROM%\" \"%TO%.new\" >nul 2>&1\r\n");
    script += QStringLiteral("if \"%IS_EXE%\"==\"1\" if exist \"%TO%.new\" (move /Y \"%TO%.new\" \"%TO%\" >nul 2>&1)\r\n");
    script += QStringLiteral("if not \"%IS_EXE%\"==\"1\" copy /Y \"%FROM%\" \"%TO%\" >nul 2>&1\r\n");
    script += QStringLiteral("set \"FROM_SIZE=\"\r\n");
    script += QStringLiteral("set \"TO_SIZE=\"\r\n");
    script += QStringLiteral("for %%A in (\"%FROM%\") do set \"FROM_SIZE=%%~zA\"\r\n");
    script += QStringLiteral("for %%A in (\"%TO%\") do set \"TO_SIZE=%%~zA\"\r\n");
    script += QStringLiteral("if not \"%FROM_SIZE%\"==\"\" if \"%FROM_SIZE%\"==\"%TO_SIZE%\" goto copy_ok\r\n");
    script += QStringLiteral("if %TRY% GEQ 60 goto copy_fail\r\n");
    script += QStringLiteral("timeout /t 1 /nobreak >nul\r\n");
    script += QStringLiteral("goto retry_copy\r\n");
    script += QStringLiteral(":copy_fail\r\n");
    script += QStringLiteral("echo copy failed %FROM% -^> %TO% >> \"%1\"\r\n").arg(logPath);
    script += QStringLiteral("exit /b 1\r\n");
    script += QStringLiteral(":copy_ok\r\n");
    script += QStringLiteral("echo copied %TO% >> \"%1\"\r\n").arg(logPath);
    script += QStringLiteral("exit /b 0\r\n");

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
                          "%5")
        .arg(AppVersion::string())
        .arg(AppVersion::build())
        .arg(release.version.isEmpty() ? QStringLiteral("—") : release.version)
        .arg(release.build)
        .arg(notes);
}

[[maybe_unused]] QString (*const _keep_updatePromptText)(const RemoteRelease &) = &updatePromptText;

bool runUpdateInDialog(QDialog & /*dlg*/,
                      QWidget *hideLayer,
                      QProgressBar *bar,
                      QLabel *statusLabel,
                      const RemoteRelease &release,
                      bool force)
{
    RemoteRelease effective = release;
    effective.baseUrl = normalizeReleaseBaseUrl(release.baseUrl);

    hideLayer->setVisible(false);
    bar->setVisible(true);
    statusLabel->setVisible(true);
    statusLabel->setText(QStringLiteral("Подготовка загрузки..."));
    bar->setValue(0);
    bar->setMaximum(0);
    qApp->processEvents();

    QNetworkAccessManager nam;
    QString error;
    const QVector<RemoteRelease::FileEntry> todo =
        force ? filesToDownloadForced(effective) : filesToDownload(effective);

    if (todo.isEmpty()) {
        statusLabel->setText(QStringLiteral("Файлы уже актуальны — переустановка не требуется."));
        bar->setMaximum(100);
        bar->setValue(100);
        qApp->processEvents();
        return false;
    }

    const QString stagingRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                                + QStringLiteral("/VapManagerUpdate/build_")
                                + QString::number(effective.build);
    QDir().mkpath(stagingRoot);

    if (!downloadReleaseFilesBar(&nam, effective, todo, stagingRoot, bar, statusLabel, &error)) {
        statusLabel->setText(QStringLiteral("Ошибка: %1").arg(error));
        hideLayer->setVisible(true);
        bar->setVisible(false);
        return false;
    }

    statusLabel->setText(QStringLiteral("Применение обновления..."));
    bar->setMaximum(100);
    bar->setValue(100);
    qApp->processEvents();

    setSnoozeUntil(QDateTime());
    setPendingUpdateBuild(effective.build);
    appendUpdateHistory(effective.build, effective.version, effective.notes, QStringLiteral("applied"));
    if (!launchFileUpdaterAndQuit(stagingRoot, todo)) {
        clearPendingUpdateBuild();
        statusLabel->setText(QStringLiteral("Не удалось запустить установку обновления."));
        hideLayer->setVisible(true);
        bar->setVisible(false);
        return false;
    }
    return true; // app will quit
}

bool showUpdatePrompt(QWidget *parent, const RemoteRelease &release)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("Доступно обновление"));
    dlg.setModal(true);
    dlg.setMinimumWidth(500);
    dlg.setStyleSheet(QStringLiteral(
        "QDialog{background:#F0F4FA;}"
        "QLabel#title{color:#0F172A;font-family:Inter;font-size:18px;font-weight:900;background:transparent;}"
        "QLabel#verLine{color:#334155;font-family:Inter;font-size:13px;font-weight:700;background:transparent;}"
        "QLabel#changelogHeader{color:#0F172A;font-family:Inter;font-size:14px;font-weight:900;background:transparent;}"
        "QLabel#statusLabel{color:#334155;font-family:Inter;font-size:12px;font-weight:700;background:transparent;}"
        "QTextEdit#changelog{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:10px;"
        "font-family:Inter;font-size:13px;font-weight:600;color:#1A1A1A;padding:10px;}"
        "QProgressBar#progress{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:8px;"
        "text-align:center;font-family:Inter;font-size:12px;font-weight:800;color:#0F172A;}"
        "QProgressBar#progress::chunk{background:#0F00DB;border-radius:7px;}"
        "QPushButton{font-family:Inter;font-size:14px;font-weight:700;border-radius:8px;padding:9px 18px;border:none;}"
        "QPushButton#updateBtn{background-color:#0F00DB;color:white;min-width:120px;}"
        "QPushButton#updateBtn:hover{background-color:#1A4ACD;}"
        "QPushButton#reinstallBtn{background-color:#FFFFFF;color:#0F172A;border:1px solid #C8C8C8;}"
        "QPushButton#reinstallBtn:hover{background-color:#F1F5F9;}"
        "QPushButton#detailsBtn{background-color:transparent;color:#2563EB;border:none;}"
        "QPushButton#detailsBtn:hover{color:#1D4ED8;}"
        "QPushButton#laterBtn{background-color:#E6E6E6;color:#1A1A1A;border:1px solid #C8C8C8;min-width:110px;}"
        "QPushButton#laterBtn:hover{background-color:#D5D5D5;}"));

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    QLabel *title = new QLabel(QStringLiteral("Доступно обновление"), &dlg);
    title->setObjectName(QStringLiteral("title"));
    root->addWidget(title);

    QLabel *verLine = new QLabel(
        QStringLiteral("Текущая: %1 (build %2)   →   Новая: %3 (build %4)")
            .arg(AppVersion::string())
            .arg(AppVersion::build())
            .arg(release.version.isEmpty() ? QStringLiteral("—") : release.version)
            .arg(release.build),
        &dlg);
    verLine->setObjectName(QStringLiteral("verLine"));
    root->addWidget(verLine);

    // Слой с информацией/кнопками (скрывается на время загрузки)
    QWidget *infoLayer = new QWidget(&dlg);
    QVBoxLayout *infoLay = new QVBoxLayout(infoLayer);
    infoLay->setContentsMargins(0, 0, 0, 0);
    infoLay->setSpacing(10);

    QLabel *changelogHeader = new QLabel(QStringLiteral("Что нового"), infoLayer);
    changelogHeader->setObjectName(QStringLiteral("changelogHeader"));
    infoLay->addWidget(changelogHeader);

    QTextEdit *changelog = new QTextEdit(infoLayer);
    changelog->setObjectName(QStringLiteral("changelog"));
    changelog->setReadOnly(true);
    const QString notesText = release.notes.isEmpty()
        ? QStringLiteral("Описание обновления недоступно.")
        : release.notes;
    const QString pretty = formatChangelogFromNotes(notesText);
    const QString html = QStringLiteral("<b>• </b>") + pretty.toHtmlEscaped()
                             .replace(QStringLiteral("\n"), QStringLiteral("<br><b>• </b>"));
    changelog->setHtml(html);
    changelog->setMinimumHeight(120);
    infoLay->addWidget(changelog, 1);

    // Размер скачиваемого обновления (до старта загрузки)
    const QVector<RemoteRelease::FileEntry> todo = filesToDownload(release);
    qint64 totalBytes = 0;
    for (const RemoteRelease::FileEntry &f : todo)
        totalBytes += f.size;
    const double mb = totalBytes / (1024.0 * 1024.0);
    QString sizeText;
    if (totalBytes <= 0) {
        sizeText = QStringLiteral("Размер загрузки: обновление не требуется");
    } else if (mb < 1.0) {
        sizeText = QStringLiteral("Размер загрузки: %1 КБ (%2 файл(ов))")
                       .arg(totalBytes / 1024.0, 0, 'f', 1)
                       .arg(todo.size());
    } else {
        sizeText = QStringLiteral("Размер загрузки: ~%1 МБ (%2 файл(ов))")
                       .arg(mb, 0, 'f', 1)
                       .arg(todo.size());
    }
    QLabel *sizeLbl = new QLabel(sizeText, infoLayer);
    sizeLbl->setStyleSheet(QStringLiteral(
        "color:#0F00DB;font-family:Inter;font-size:13px;font-weight:800;background:transparent;"));
    infoLay->addWidget(sizeLbl);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(10);
    QPushButton *detailsBtn = new QPushButton(QStringLiteral("Подробнее..."), infoLayer);
    detailsBtn->setObjectName(QStringLiteral("detailsBtn"));
    btnRow->addWidget(detailsBtn);
    btnRow->addStretch();
    QPushButton *reinstallBtn = new QPushButton(QStringLiteral("Переустановить"), infoLayer);
    reinstallBtn->setObjectName(QStringLiteral("reinstallBtn"));
    QPushButton *laterBtn = new QPushButton(QStringLiteral("Позже"), infoLayer);
    laterBtn->setObjectName(QStringLiteral("laterBtn"));
    QPushButton *updateBtn = new QPushButton(QStringLiteral("Обновить"), infoLayer);
    updateBtn->setObjectName(QStringLiteral("updateBtn"));
    btnRow->addWidget(reinstallBtn);
    btnRow->addWidget(laterBtn);
    btnRow->addWidget(updateBtn);
    infoLay->addLayout(btnRow);

    root->addWidget(infoLayer, 1);

    // Слой загрузки (скрыт изначально)
    QLabel *statusLabel = new QLabel(QStringLiteral("..."), &dlg);
    statusLabel->setObjectName(QStringLiteral("statusLabel"));
    statusLabel->setVisible(false);
    root->addWidget(statusLabel);

    QProgressBar *bar = new QProgressBar(&dlg);
    bar->setObjectName(QStringLiteral("progress"));
    bar->setVisible(false);
    bar->setMinimumHeight(28);
    root->addWidget(bar);

    updateBtn->setDefault(true);
    bool launched = false;

    QObject::connect(updateBtn, &QPushButton::clicked, &dlg, [&]() {
        launched = runUpdateInDialog(dlg, infoLayer, bar, statusLabel, release, false);
        if (launched)
            dlg.done(QDialog::Accepted);
    });
    QObject::connect(reinstallBtn, &QPushButton::clicked, &dlg, [&]() {
        launched = runUpdateInDialog(dlg, infoLayer, bar, statusLabel, release, true);
        if (launched)
            dlg.done(QDialog::Accepted);
    });
    QObject::connect(laterBtn, &QPushButton::clicked, &dlg, [&dlg]() { dlg.done(QDialog::Rejected); });
    QObject::connect(detailsBtn, &QPushButton::clicked, &dlg, [parent]() {
        AppUpdater::showChangelogDialog(parent);
    });

    appendUpdateHistory(release.build, release.version, release.notes, QStringLiteral("seen"));
    dlg.exec();
    return launched || dlg.result() == QDialog::Accepted;
}

[[maybe_unused]] bool performUpdateFlow(QWidget *parent, const RemoteRelease &release)
{
    RemoteRelease effectiveRelease = release;
    effectiveRelease.baseUrl = normalizeReleaseBaseUrl(release.baseUrl);

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

    const QVector<RemoteRelease::FileEntry> todo = filesToDownload(effectiveRelease);
    if (!todo.isEmpty()) {
        const QString stagingRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                                    + QStringLiteral("/VapManagerUpdate/build_")
                                    + QString::number(effectiveRelease.build);
        QDir().mkpath(stagingRoot);

        if (!downloadReleaseFiles(&nam, effectiveRelease, todo, stagingRoot, &progress, &error)) {
            progress.close();
            QMessageBox::warning(parent,
                                 QStringLiteral("Обновление"),
                                 QStringLiteral("Ошибка загрузки файлов:\n%1").arg(error));
            return false;
        }

        progress.close();
        setSnoozeUntil(QDateTime());
        setPendingUpdateBuild(effectiveRelease.build);
        if (!launchFileUpdaterAndQuit(stagingRoot, todo)) {
            clearPendingUpdateBuild();
            QMessageBox::warning(parent,
                                 QStringLiteral("Обновление"),
                                 QStringLiteral("Не удалось запустить установку обновления."));
            return false;
        }
        return true;
    }

    if (!effectiveRelease.setupUrl.isEmpty()) {
        const QString setupPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                      .filePath(QStringLiteral("VapManager-setup.exe"));
        if (!downloadSetupInstaller(&nam, effectiveRelease, setupPath, &progress, &error)) {
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

    if (showUpdatePrompt(parent, release))
        return; // диалог сам выполнил загрузку и запустил установку (app завершается)
    // Пользователь нажал «Позже» — оставляем статус без изменений
}

void AppUpdateScheduler::showBackgroundPrompt()
{
    if (!mainWindow_ || !g_hasPendingRelease)
        return;

    // Если окно свёрнуто или скрыто — модальный диалог мешает работе.
    // Показываем всплывающее уведомление в системном трее; по клику на него
    // восстановим окно и покажем обычный диалог обновления.
    if (mainWindow_->isMinimized() || !mainWindow_->isVisible()) {
        showUpdateTrayNotification();
        return; // не сбрасываем g_hasPendingRelease — покажем диалог по клику на трей
    }

    const RemoteRelease release = g_pendingRelease;
    g_hasPendingRelease = false;

    if (showUpdatePrompt(mainWindow_, release)) {
        return; // обновление запущено из диалога
    } else {
        snoozeForOneHour();
    }
}

void AppUpdateScheduler::showUpdateTrayNotification()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    if (!trayIcon_) {
        trayIcon_ = new QSystemTrayIcon(qApp);
        trayIcon_->setIcon(QIcon(QStringLiteral(":/new/mainWindowIcons/noback/agvIcon.png")));
        trayIcon_->setToolTip(QStringLiteral("VapManager"));
        connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick)
                return;
            if (mainWindow_) {
                if (mainWindow_->isMinimized())
                    mainWindow_->showNormal();
                mainWindow_->show();
                mainWindow_->raise();
                mainWindow_->activateWindow();
            }
            // Сбрасываем трей-уведомление и показываем диалог
            if (g_hasPendingRelease)
                showBackgroundPrompt();
        });
        trayIcon_->show();
    }

    const RemoteRelease release = g_pendingRelease;
    const QString title = QStringLiteral("Доступно обновление VapManager");
    const QString msg = QStringLiteral("Новая версия %1 (build %2). Нажмите, чтобы обновить.")
                            .arg(release.version.isEmpty() ? QStringLiteral("—") : release.version)
                            .arg(release.build);
    trayIcon_->showMessage(title, msg, QSystemTrayIcon::Information, 10000);
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

void AppUpdater::reconcilePendingUpdate()
{
    const int pending = pendingUpdateBuild();
    if (pending <= 0)
        return;

    if (AppVersion::build() >= pending) {
        setLastUpdateDateTime(QDateTime::currentDateTime());
        clearPendingUpdateBuild();
        return;
    }

    clearPendingUpdateBuild();
}

void AppUpdater::startBackgroundChecks(QWidget *mainWindow)
{
    AppUpdateScheduler::instance()->start(mainWindow);
}

QString AppUpdater::updateCheckUrl()
{
    return ::updateCheckUrl();
}

QString AppUpdater::updateCheckUrlForHost(const QString &host)
{
    const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini");
    QSettings cfg(path, QSettings::IniFormat);
    const QString fromCfg = cfg.value(QStringLiteral("update_check_url")).toString().trimmed();
    if (!fromCfg.isEmpty())
        return fromCfg;

    const int port = cfg.value(QStringLiteral("update_server_port"), 8765).toInt();
    const QString h = host.trimmed().isEmpty() ? getDbHost() : host.trimmed();
    if (!h.isEmpty())
        return QStringLiteral("http://%1:%2/version.json").arg(h).arg(port > 0 ? port : 8765);

    return QStringLiteral("https://raw.githubusercontent.com/Haryabest/VapManager/main/releases/version.json");
}

QVector<AppUpdater::UpdateHistoryRecord> AppUpdater::updateHistory()
{
    QVector<UpdateHistoryRecord> out;
    const QVariantList list = loadUpdateHistoryRaw();
    out.reserve(list.size());
    for (const QVariant &v : list) {
        const QVariantMap m = v.toMap();
        UpdateHistoryRecord r;
        r.build = m.value(QStringLiteral("build")).toInt();
        r.version = m.value(QStringLiteral("version")).toString();
        r.notes = m.value(QStringLiteral("notes")).toString();
        const QDateTime dt = QDateTime::fromString(m.value(QStringLiteral("date")).toString(), Qt::ISODate);
        r.date = dt.isValid() ? dt.toString(QStringLiteral("dd.MM.yyyy HH:mm")) : QStringLiteral("—");
        out.append(r);
    }
    return out;
}

void AppUpdater::recordUpdateSeen(int build, const QString &version, const QString &notes)
{
    appendUpdateHistory(build, version, notes, QStringLiteral("seen"));
}

void AppUpdater::recordUpdateApplied(int build, const QString &version, const QString &notes)
{
    appendUpdateHistory(build, version, notes, QStringLiteral("applied"));
    setLastUpdateDateTime(QDateTime::currentDateTime());
}

void AppUpdater::showChangelogDialog(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("История обновлений"));
    dlg.setModal(true);
    dlg.setMinimumSize(520, 460);
    dlg.setStyleSheet(QStringLiteral(
        "QDialog{background:#F0F4FA;}"
        "QLabel#title{color:#0F172A;font-family:Inter;font-size:18px;font-weight:900;background:transparent;}"
        "QTextEdit{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:10px;"
        "font-family:Inter;font-size:13px;font-weight:600;color:#1A1A1A;padding:10px;}"
        "QPushButton{background-color:#E6E6E6;color:#1A1A1A;font-family:Inter;font-size:14px;"
        "font-weight:700;border-radius:8px;padding:8px 20px;border:1px solid #C8C8C8;}"
        "QPushButton:hover{background-color:#D5D5D5;}"));

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    QLabel *title = new QLabel(QStringLiteral("История обновлений"), &dlg);
    title->setObjectName(QStringLiteral("title"));
    root->addWidget(title);

    QTextEdit *view = new QTextEdit(&dlg);
    view->setReadOnly(true);
    const QVector<UpdateHistoryRecord> hist = updateHistory();
    if (hist.isEmpty()) {
        view->setHtml(QStringLiteral("<i>История пуста.</i>"));
    } else {
        QString html;
        for (const UpdateHistoryRecord &r : hist) {
            const QString header = QStringLiteral("<b>Build %1</b> (%2) — %3")
                                       .arg(r.build)
                                       .arg(r.version.isEmpty() ? QStringLiteral("—") : r.version)
                                       .arg(r.date);
            const QString pretty = formatChangelogFromNotes(
                r.notes.isEmpty() ? QStringLiteral("—") : r.notes);
            html += QStringLiteral("<div style=\"margin-bottom:10px;\">")
                 + header
                 + QStringLiteral("<br><b>• </b>")
                 + pretty.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br><b>• </b>"))
                 + QStringLiteral("</div>");
        }
        view->setHtml(html);
    }
    root->addWidget(view, 1);

    QPushButton *closeBtn = new QPushButton(QStringLiteral("Закрыть"), &dlg);
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    root->addWidget(closeBtn, 0, Qt::AlignRight);

    dlg.exec();
}

void AppUpdater::reinstallCurrentVersion(QWidget *parent)
{
    if (!parent)
        return;

    QNetworkAccessManager nam;
    const QString checkUrl = updateCheckUrl();
    QUrl url(checkUrl);
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("VapManager/") + AppVersion::string());
#endif
    QNetworkReply *reply = nam.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const bool ok = reply->error() == QNetworkReply::NoError;
    const QByteArray body = ok ? reply->readAll() : QByteArray();
    reply->deleteLater();

    if (!ok) {
        QMessageBox::warning(parent, QStringLiteral("Переустановка"),
                             QStringLiteral("Не удалось получить информацию о версии с сервера."));
        return;
    }

    RemoteRelease release;
    QString parseError;
    if (!parseRelease(body, &release, &parseError)) {
        QMessageBox::warning(parent, QStringLiteral("Переустановка"),
                             QStringLiteral("Некорректный файл версии на сервере:\n%1").arg(parseError));
        return;
    }

    if (QMessageBox::question(parent,
            QStringLiteral("Переустановка"),
            QStringLiteral("Переустановить текущую версию (build %1)?\nФайлы будут заново скачаны с сервера.")
                .arg(AppVersion::build()),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // Диалог обновления сам выполнит загрузку (кнопкой «Обновить» или «Переустановить»).
    showUpdatePrompt(parent, release);
}
