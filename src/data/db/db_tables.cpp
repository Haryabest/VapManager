#include "db_tables.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

bool dbTableExists(const QSqlDatabase &db, const QString &tableName)
{
    if (!db.isOpen() || tableName.isEmpty())
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT 1 FROM information_schema.tables "
        "WHERE table_schema = 'public' AND table_name = :t LIMIT 1"));
    q.bindValue(QStringLiteral(":t"), tableName);
    return q.exec() && q.next();
}

bool ensureDbTable(const QSqlDatabase &db, const QString &tableName, const QString &createSql,
                   QString *outError)
{
    if (outError)
        outError->clear();
    if (!db.isOpen()) {
        if (outError)
            *outError = QStringLiteral("Нет подключения к БД");
        return false;
    }
    if (dbTableExists(db, tableName))
        return true;

    QSqlQuery q(db);
    if (!q.exec(createSql)) {
        if (outError)
            *outError = q.lastError().text();
        return false;
    }
    return true;
}
