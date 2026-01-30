#ifndef DB_H
#define DB_H

#include <QSqlDatabase>

// Функция для подключения к базе данных
bool connectToDB();

// Тестовая функция для проверки подключения
void testConnection();

#endif // DB_H
