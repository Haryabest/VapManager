#ifndef DB_H
#define DB_H

#include <QString>

bool connectToDB(QString *outError = nullptr);
bool reconnectWithHost(const QString &host, QString *outError = nullptr);
bool reconnectWithSettings(const QString &host, const QString &password, QString *outError = nullptr);

QString getDbHost();
int getDbPort();
QString getDbName();
QString getDbUser();
QString getDbPassword();

void ensurePortableConfig();

void testConnection();

#endif // DB_H
