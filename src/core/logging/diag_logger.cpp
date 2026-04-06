#include "diag_logger.h"
#include "db_users.h"
#include "app_session.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QPointer>
#include <QStandardPaths>
#include <QTextEdit>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QTextStream>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QMutex g_mutex;
QVector<QString> g_techRing;
const int g_techRingMax = 8000;

QPointer<QTextEdit> g_techSink;

QMutex g_stressMutex;

QString sanitizeUserForFile(const QString &username)
{
    QString s = username.trimmed();
    if (s.isEmpty())
        s = QStringLiteral("unknown");
    s.replace(QRegularExpression(QStringLiteral("[\\\\/:\\*\\?\"<>\\|]")), QStringLiteral("_"));
    if (s.size() > 64)
        s = s.left(64);
    return s;
}

void markPathHiddenWin(const QString &nativePath)
{
#ifdef Q_OS_WIN
    const std::wstring w = nativePath.toStdWString();
    DWORD attr = GetFileAttributesW(w.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
        SetFileAttributesW(w.c_str(), attr | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
    }
#else
    Q_UNUSED(nativePath);
#endif
}

void ensureHiddenDir(const QString &dirPath)
{
    QDir().mkpath(dirPath);
    markPathHiddenWin(QDir::toNativeSeparators(dirPath));
}

} // namespace

QString viewerSecureLogDirPath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString p = base + QStringLiteral("/AgvNewUi/_private_audit");
    ensureHiddenDir(p);
    return p;
}

QString viewerSecureLogFilePath(const QString &username)
{
    return viewerSecureLogDirPath() + QStringLiteral("/") + sanitizeUserForFile(username)
           + QStringLiteral("_extended.log");
}

QString techDiagLogFilePath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString dir = base + QStringLiteral("/AgvNewUi/_tech_diag");
    ensureHiddenDir(dir);
    return dir + QStringLiteral("/tech_verbose.log");
}

QString stressAutotestReportPath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString dir = base + QStringLiteral("/AgvNewUi/_tech_diag");
    ensureHiddenDir(dir);
    return dir + QStringLiteral("/stress_autotest_last.log");
}

void stressAutotestBeginSession(const QString &headline)
{
    {
        QMutexLocker lock(&g_stressMutex);
        QFile f(stressAutotestReportPath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream ts(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            ts.setCodec("UTF-8");
#endif
            ts << QStringLiteral("=== AgvNewUi: комплексный тест ===\n");
            ts << headline << QLatin1Char('\n');
            ts << QStringLiteral("Qt compile: ") << QStringLiteral(QT_VERSION_STR) << QLatin1Char('\n');
            ts << QStringLiteral("Qt runtime: ") << QString::fromUtf8(qVersion()) << QLatin1Char('\n');
            ts << QStringLiteral("---\n");
        }
    }
    techDiagLog(QStringLiteral("AUTOTEST"), QStringLiteral("session_begin | %1").arg(headline));
}

void stressAutotestLogLine(const QString &message)
{
    const QString line = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"))
        + QLatin1Char(' ') + message;
    {
        QMutexLocker lock(&g_stressMutex);
        QFile f(stressAutotestReportPath());
        if (f.open(QIODevice::Append | QIODevice::Text))
            f.write((line + QLatin1Char('\n')).toUtf8());
    }
    techDiagLog(QStringLiteral("AUTOTEST"), message);
}

void setTechDiagLogSink(QTextEdit *w)
{
    QMutexLocker lock(&g_mutex);
    g_techSink = w;
}

void viewerSecureExtendedLog(const QString &username,
                             const QString &action,
                             const QString &details)
{
    const QString role = getUserRole(username);
    if (role != QStringLiteral("viewer"))
        return;

    const QString path = viewerSecureLogFilePath(username);
    QFile f(path);
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return;
    const QString line = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"))
        + QStringLiteral(" [viewer_secure] [")
        + username + QStringLiteral("] ")
        + action + QStringLiteral(" | ")
        + details + QLatin1Char('\n');
    f.write(line.toUtf8());
    f.close();
#ifdef Q_OS_WIN
    markPathHiddenWin(QDir::toNativeSeparators(path));
#endif
}

void techDiagLog(const QString &tag, const QString &message)
{
    const QString user = AppSession::currentUsername();
    const QString role = getUserRole(user);
    if (role != QStringLiteral("tech") && role != QStringLiteral("admin"))
        return;

    const QString line = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"))
        + QStringLiteral(" [") + tag + QStringLiteral("] ")
        + message;

    {
        QMutexLocker lock(&g_mutex);
        g_techRing.push_back(line);
        while (g_techRing.size() > g_techRingMax)
            g_techRing.removeFirst();
    }

    QFile f(techDiagLogFilePath());
    if (f.open(QIODevice::Append | QIODevice::Text))
        f.write((line + QLatin1Char('\n')).toUtf8());

    qDebug().noquote() << QStringLiteral("TECH_DIAG:") << line;

    QTextEdit *w = g_techSink.data();
    if (w) {
        QMetaObject::invokeMethod(w, [w, line]() {
            if (w)
                w->append(line);
        }, Qt::QueuedConnection);
    }
}

QVector<QString> techDiagRecentLines(int maxLines)
{
    QMutexLocker lock(&g_mutex);
    if (maxLines <= 0 || g_techRing.isEmpty())
        return {};
    const int n = qMin(maxLines, g_techRing.size());
    return g_techRing.mid(g_techRing.size() - n);
}

void clearTechDiagRecentLines()
{
    QMutexLocker lock(&g_mutex);
    g_techRing.clear();
}
