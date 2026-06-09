#ifndef DB_TABLES_H
#define DB_TABLES_H

#include <QSqlDatabase>
#include <QString>

/// Returns true if table exists in schema public (works without CREATE privilege).
bool dbTableExists(const QSqlDatabase &db, const QString &tableName);

/// If table exists, returns true. Otherwise runs createSql (needs CREATE on schema).
bool ensureDbTable(const QSqlDatabase &db, const QString &tableName, const QString &createSql,
                   QString *outError = nullptr);

#endif // DB_TABLES_H
