#ifndef DB_BENCH_H
#define DB_BENCH_H

#include <QString>

// Нагрузочный тест типичных запросов VapManager через открытое подключение Qt SQL.
// Возвращает текст отчёта; failedScenarios > 0 при ошибках запросов.
QString runDatabaseBenchReport(int iterations,
                               int *failedScenarios,
                               const QString &connectionName = QStringLiteral("main_connection"));

#endif // DB_BENCH_H
