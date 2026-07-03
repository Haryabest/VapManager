#include "db_users.h"
#include "diag_logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

namespace {

const int MAX_LOG_LINES = 100000;

void trimLogFileIfNeeded(const QString &path)
{
    QFile f(path);
    if (!f.exists())
        return;

    qint64 lineCount = 0;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        in.setCodec("UTF-8");
        while (!in.atEnd()) {
            in.readLine();
            ++lineCount;
        }
        f.close();
    }

    if (lineCount <= MAX_LOG_LINES)
        return;

    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&f);
    in.setCodec("UTF-8");
    QStringList allLines;
    while (!in.atEnd())
        allLines.append(in.readLine());
    f.close();

    const int linesToKeep = MAX_LOG_LINES / 2;
    const QStringList trimmed = allLines.mid(allLines.size() - linesToKeep);

    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&f);
        out.setCodec("UTF-8");
        for (const QString &line : trimmed)
            out << line << "\n";
        f.close();
    }
}

} // namespace

QString localLogsDirPath()
{
    QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (docs.trimmed().isEmpty())
        docs = QCoreApplication::applicationDirPath();
    return docs + "/VapManagerLogs";
}

QString localLogFilePath()
{
    return localLogsDirPath() + "/app.log";
}

void logAction(const QString &username,
               const QString &action,
               const QString &details)
{
    const QString logsDir = localLogsDirPath();
    QDir().mkpath(logsDir);

    const QString path = localLogFilePath();
    trimLogFileIfNeeded(path);

    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out.setCodec("UTF-8");
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
            << " [" << username << "] "
            << action << " - " << details << "\n";
    }

    if (getUserRole(username) == QStringLiteral("viewer"))
        viewerSecureExtendedLog(username, action, details);
}
