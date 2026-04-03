#ifndef DB_H
#define DB_H

#include <QString>

// Функция для подключения к базе данных (outError — текст ошибки при неудаче)
bool connectToDB(QString *outError = nullptr);

// Переподключение с новым хостом (сохраняет в QSettings)
bool reconnectWithHost(const QString &host, QString *outError = nullptr);

// Получить текущий хост из настроек
QString getDbHost();

// Тестовая функция для проверки подключения
void testConnection();

#endif // DB_H
