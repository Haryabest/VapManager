#include "db_bench.h"

#include <QDate>
#include <QDateTime>
#include <QElapsedTimer>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <functional>

namespace {

struct BenchResult
{
    QString name;
    int samples = 0;
    qint64 minMicros = 0;
    qint64 maxMicros = 0;
    double sumMicros = 0.0;
    qint64 p50Micros = 0;
    qint64 p95Micros = 0;
    qint64 p99Micros = 0;
    bool ok = true;
    QString error;
};

qint64 percentileMicros(QVector<qint64> values, double p)
{
    if (values.isEmpty())
        return 0;
    std::sort(values.begin(), values.end());
    const double rank = (p / 100.0) * (values.size() - 1);
    const int lo = int(std::floor(rank));
    const int hi = int(std::ceil(rank));
    if (lo == hi)
        return values.at(lo);
    const double frac = rank - lo;
    return qint64(values.at(lo) * (1.0 - frac) + values.at(hi) * frac);
}

BenchResult summarize(const QString &name, const QVector<qint64> &micros, bool ok, const QString &error = {})
{
    BenchResult r;
    r.name = name;
    r.ok = ok;
    r.error = error;
    if (!ok || micros.isEmpty())
        return r;

    r.samples = micros.size();
    r.minMicros = *std::min_element(micros.begin(), micros.end());
    r.maxMicros = *std::max_element(micros.begin(), micros.end());
    for (qint64 v : micros)
        r.sumMicros += v;
    r.p50Micros = percentileMicros(micros, 50.0);
    r.p95Micros = percentileMicros(micros, 95.0);
    r.p99Micros = percentileMicros(micros, 99.0);
    return r;
}

BenchResult runTimedQuery(QSqlDatabase db, const QString &name, int iterations,
                          const std::function<bool(QSqlQuery &)> &execOnce)
{
    QVector<qint64> micros;
    micros.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        QSqlQuery q(db);
        QElapsedTimer t;
        t.start();
        const bool ok = execOnce(q);
        micros.push_back(qMax<qint64>(1, t.nsecsElapsed() / 1000));
        if (!ok)
            return summarize(name, micros, false, q.lastError().text());
    }
    return summarize(name, micros, true);
}

QString formatLine(const BenchResult &r)
{
    if (!r.ok)
        return QStringLiteral("  FAIL  %1 — %2\n").arg(r.name, r.error);

    const auto toMs = [](qint64 micros) { return micros / 1000.0; };
    return QStringLiteral("  OK    %1  n=%2  min=%3 ms  avg=%4 ms  p50=%5 ms  p95=%6 ms  p99=%7 ms  max=%8 ms\n")
        .arg(r.name, -28)
        .arg(r.samples, 3)
        .arg(toMs(r.minMicros), 0, 'f', 1)
        .arg(r.sumMicros / r.samples / 1000.0, 0, 'f', 1)
        .arg(toMs(r.p50Micros), 0, 'f', 1)
        .arg(toMs(r.p95Micros), 0, 'f', 1)
        .arg(toMs(r.p99Micros), 0, 'f', 1)
        .arg(toMs(r.maxMicros), 0, 'f', 1);
}

QString ratingText(double worstP95Ms)
{
    if (worstP95Ms <= 80)
        return QStringLiteral("Оценка: отлично (удалённая БД отзывается быстро)");
    if (worstP95Ms <= 250)
        return QStringLiteral("Оценка: нормально (есть задержка сети, но терпимо)");
    if (worstP95Ms <= 800)
        return QStringLiteral("Оценка: медленно (пользователи могут замечать подвисания)");
    return QStringLiteral("Оценка: плохо (проверьте сеть, VPN, нагрузку на сервер PostgreSQL)");
}

} // namespace

QString runDatabaseBenchReport(int iterations, int *failedScenarios, const QString &connectionName)
{
    if (failedScenarios)
        *failedScenarios = 0;

    iterations = qMax(1, iterations);

    if (!QSqlDatabase::contains(connectionName)) {
        if (failedScenarios)
            *failedScenarios = 1;
        return QString();
    }

    QSqlDatabase db = QSqlDatabase::database(connectionName);
    if (!db.isOpen()) {
        if (failedScenarios)
            *failedScenarios = 1;
        return QString();
    }

    const QDate today = QDate::currentDate();
    const QDate monthStart(today.year(), today.month(), 1);
    const QDate monthEnd = monthStart.addMonths(1).addDays(-1);

    QVector<BenchResult> results;
    results.reserve(9);

    results.push_back(runTimedQuery(db, QStringLiteral("ping SELECT 1"), iterations, [](QSqlQuery &q) {
        return q.exec(QStringLiteral("SELECT 1"));
    }));

    results.push_back(runTimedQuery(db, QStringLiteral("agv_list COUNT"), iterations, [](QSqlQuery &q) {
        return q.exec(QStringLiteral("SELECT COUNT(*) FROM agv_list"));
    }));

    results.push_back(runTimedQuery(db, QStringLiteral("agv_list full scan"), iterations, [](QSqlQuery &q) {
        return q.exec(QStringLiteral(
            "SELECT agv_id, model, status, kilometers FROM agv_list ORDER BY agv_id ASC"));
    }));

    results.push_back(runTimedQuery(db, QStringLiteral("calendar tasks month"), iterations,
                                    [&](QSqlQuery &q) {
        q.prepare(QStringLiteral(
            "SELECT t.agv_id, t.task_name, t.next_date "
            "FROM agv_tasks t JOIN agv_list a ON a.agv_id = t.agv_id "
            "WHERE t.next_date BETWEEN :from AND :to "
            "ORDER BY t.next_date ASC LIMIT 500"));
        q.bindValue(QStringLiteral(":from"), monthStart);
        q.bindValue(QStringLiteral(":to"), monthEnd);
        return q.exec();
    }));

    results.push_back(runTimedQuery(db, QStringLiteral("users list"), iterations, [](QSqlQuery &q) {
        return q.exec(QStringLiteral(
            "SELECT username, full_name, role, is_active, last_login FROM users ORDER BY full_name"));
    }));

    results.push_back(runTimedQuery(db, QStringLiteral("notifications unread"), iterations, [](QSqlQuery &q) {
        return q.exec(QStringLiteral(
            "SELECT id, target_user, message, created_at FROM notifications "
            "WHERE is_read = FALSE ORDER BY created_at DESC LIMIT 100"));
    }));

    results.push_back(runTimedQuery(db, QStringLiteral("chat threads"), iterations, [](QSqlQuery &q) {
        return q.exec(QStringLiteral(
            "SELECT id, created_by, recipient_user, created_at FROM task_chat_threads "
            "ORDER BY created_at DESC LIMIT 100"));
    }));

    results.push_back(runTimedQuery(db, QStringLiteral("task history month"), iterations,
                                    [&](QSqlQuery &q) {
        q.prepare(QStringLiteral(
            "SELECT agv_id, task_name, completed_at FROM agv_task_history "
            "WHERE completed_at BETWEEN :from AND :to ORDER BY completed_at DESC LIMIT 200"));
        q.bindValue(QStringLiteral(":from"), monthStart);
        q.bindValue(QStringLiteral(":to"), monthEnd);
        return q.exec();
    }));

    results.push_back(runTimedQuery(db, QStringLiteral("mixed burst x5"), qMax(1, iterations / 3),
                                    [](QSqlQuery &q) {
        if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM agv_list")))
            return false;
        if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM users")))
            return false;
        if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM agv_tasks")))
            return false;
        if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM notifications")))
            return false;
        return q.exec(QStringLiteral("SELECT COUNT(*) FROM task_chat_threads"));
    }));

    QString report;
    report += QStringLiteral("--- Результаты ---\n");

    int fails = 0;
    qint64 worstP95 = 0;
    QString worstName;
    for (const BenchResult &r : results) {
        report += formatLine(r);
        if (!r.ok) {
            ++fails;
        } else if (r.p95Micros > worstP95) {
            worstP95 = r.p95Micros;
            worstName = r.name;
        }
    }

    report += QStringLiteral("\n--- Итог ---\n");
    if (fails > 0) {
        report += QStringLiteral("Есть ошибки запросов: %1\n").arg(fails);
    } else {
        const double worstP95Ms = worstP95 / 1000.0;
        report += QStringLiteral("Все сценарии OK.\n");
        report += QStringLiteral("Худший p95: %1 — %2 ms\n")
                      .arg(worstName, QString::number(worstP95Ms, 'f', 1));
        report += ratingText(worstP95Ms) + QLatin1Char('\n');
    }

    if (failedScenarios)
        *failedScenarios = fails;

    return report;
}
