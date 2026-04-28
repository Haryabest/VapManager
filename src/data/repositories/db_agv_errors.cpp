#include "db_agv_errors.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

bool initAgvErrorLogsTable()
{
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);
    if (!q.exec(R"(
        CREATE TABLE IF NOT EXISTS agv_error_logs (
            id INT AUTO_INCREMENT PRIMARY KEY,
            agv_id VARCHAR(64) NOT NULL,
            error_date DATE NOT NULL,
            error_type VARCHAR(64) NOT NULL,
            title VARCHAR(256) NOT NULL,
            time_from TIME NULL,
            time_to TIME NULL,
            duration_minutes INT NOT NULL DEFAULT 0,
            created_by VARCHAR(64) NOT NULL,
            created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
            INDEX idx_agv_date (agv_id, error_date),
            INDEX idx_date (error_date)
        )
        CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci
    )"))
        return false;

    // If the table already existed with a legacy charset, upgrade it.
    // This fixes "Incorrect string value" for Cyrillic/emoji/special chars.
    if (!q.exec(QStringLiteral(
            "ALTER TABLE agv_error_logs CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci"))) {
        const QString e = q.lastError().text();
        // Ignore if not supported / already converted; only log unexpected.
        if (!e.contains("collation", Qt::CaseInsensitive) &&
            !e.contains("charset", Qt::CaseInsensitive) &&
            !e.contains("1064") && !e.contains("syntax", Qt::CaseInsensitive)) {
            qDebug() << "agv_error_logs charset upgrade warning:" << e;
        }
    }
    return true;
}

static QString normalizeText(QString v, int maxLen)
{
    v = v.trimmed();
    if (maxLen > 0 && v.size() > maxLen)
        v = v.left(maxLen);
    return v;
}

bool addAgvErrorLog(const QString &agvId,
                    const QDate &date,
                    const QString &type,
                    const QString &title,
                    const QTime &from,
                    const QTime &to,
                    int durationMinutes,
                    const QString &createdBy,
                    QString *outError)
{
    if (agvId.trimmed().isEmpty()) {
        if (outError) *outError = QStringLiteral("AGV не указан");
        return false;
    }
    if (!date.isValid()) {
        if (outError) *outError = QStringLiteral("Дата не указана");
        return false;
    }
    const QString t = normalizeText(type, 64);
    const QString name = normalizeText(title, 256);
    const QString by = normalizeText(createdBy, 64);
    if (t.isEmpty() || name.isEmpty() || by.isEmpty()) {
        if (outError) *outError = QStringLiteral("Тип/Название/Пользователь не должны быть пустыми");
        return false;
    }
    durationMinutes = qMax(0, durationMinutes);

    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen()) {
        if (outError) *outError = QStringLiteral("Нет подключения к БД");
        return false;
    }
    if (!initAgvErrorLogsTable()) {
        if (outError) *outError = QStringLiteral("Не удалось создать таблицу ошибок");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO agv_error_logs (agv_id, error_date, error_type, title, time_from, time_to, duration_minutes, created_by) "
        "VALUES (:agv, :d, :ty, :ti, :tf, :tt, :mins, :by)"));
    q.bindValue(QStringLiteral(":agv"), agvId.trimmed());
    q.bindValue(QStringLiteral(":d"), date);
    q.bindValue(QStringLiteral(":ty"), t);
    q.bindValue(QStringLiteral(":ti"), name);
    q.bindValue(QStringLiteral(":tf"), from.isValid() ? from : QVariant(QVariant::Time));
    q.bindValue(QStringLiteral(":tt"), to.isValid() ? to : QVariant(QVariant::Time));
    q.bindValue(QStringLiteral(":mins"), durationMinutes);
    q.bindValue(QStringLiteral(":by"), by);
    if (!q.exec()) {
        if (outError) *outError = q.lastError().text();
        qDebug() << "addAgvErrorLog failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QVector<AgvErrorLog> loadAgvErrorLogs(const QString &agvId, const QDate &fromDate, const QDate &toDate, QString *outError)
{
    QVector<AgvErrorLog> list;
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen()) {
        if (outError) *outError = QStringLiteral("Нет подключения к БД");
        return list;
    }
    if (!initAgvErrorLogsTable())
        return list;

    QString sql = QStringLiteral(
        "SELECT id, agv_id, error_date, error_type, title, time_from, time_to, duration_minutes, created_by, created_at "
        "FROM agv_error_logs WHERE 1=1");
    if (!agvId.trimmed().isEmpty())
        sql += QStringLiteral(" AND agv_id = :agv");
    if (fromDate.isValid())
        sql += QStringLiteral(" AND error_date >= :fromD");
    if (toDate.isValid())
        sql += QStringLiteral(" AND error_date <= :toD");
    sql += QStringLiteral(" ORDER BY error_date DESC, created_at DESC LIMIT 500");

    QSqlQuery q(db);
    q.prepare(sql);
    if (!agvId.trimmed().isEmpty())
        q.bindValue(QStringLiteral(":agv"), agvId.trimmed());
    if (fromDate.isValid())
        q.bindValue(QStringLiteral(":fromD"), fromDate);
    if (toDate.isValid())
        q.bindValue(QStringLiteral(":toD"), toDate);

    if (!q.exec()) {
        if (outError) *outError = q.lastError().text();
        return list;
    }
    while (q.next()) {
        AgvErrorLog e;
        e.id = q.value(0).toInt();
        e.agvId = q.value(1).toString();
        e.errorDate = q.value(2).toDate();
        e.type = q.value(3).toString();
        e.title = q.value(4).toString();
        e.timeFrom = q.value(5).toTime();
        e.timeTo = q.value(6).toTime();
        e.durationMinutes = q.value(7).toInt();
        e.createdBy = q.value(8).toString();
        e.createdAt = q.value(9).toDateTime();
        list.push_back(e);
    }
    return list;
}

