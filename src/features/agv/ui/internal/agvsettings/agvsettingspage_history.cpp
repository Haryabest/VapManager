#include "agvsettingspage.h"

#include "db_agv_errors.h"

#include <QAbstractItemView>
#include <QDialog>
#include <QHash>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const char *kHistoryDialogStyle =
    "QDialog{background:#F7F8FA;}"
    "QTableWidget{background:white;border:1px solid #D9DDE3;border-radius:8px;}"
    "QHeaderView::section{background:#ECEFF3;color:#1A1A1A;font-weight:800;padding:6px;border:none;}"
    "QHeaderView::section:hover{background:#DDE3EA;}"
    "QPushButton{background:#0F00DB;color:white;border:none;border-radius:8px;padding:8px 14px;font-weight:800;}"
    "QPushButton:hover{background:#1A4ACD;}";

struct TaskHistoryRow {
    QDate completedAt;
    QString taskName;
    int intervalDays = 0;
    QDate nextDate;
    QString performedBy;
};

struct ErrorHistoryRow {
    QDate errorDate;
    QString type;
    QString title;
    QString timeRange;
    int durationMinutes = 0;
    QString createdBy;
};

QString formatTimeRange(const QTime &from, const QTime &to)
{
    if (!from.isValid() && !to.isValid())
        return QStringLiteral("—");
    if (from.isValid() && to.isValid())
        return QStringLiteral("%1 — %2").arg(from.toString("HH:mm"), to.toString("HH:mm"));
    if (from.isValid())
        return QStringLiteral("с %1").arg(from.toString("HH:mm"));
    return QStringLiteral("до %1").arg(to.toString("HH:mm"));
}

void fillTaskTable(QTableWidget *tbl, const QVector<TaskHistoryRow> &rows)
{
    tbl->clearSpans();
    tbl->setRowCount(0);
    if (rows.isEmpty()) {
        tbl->insertRow(0);
        QTableWidgetItem *empty = new QTableWidgetItem(QStringLiteral("История пока пуста"));
        empty->setFlags(empty->flags() & ~Qt::ItemIsSelectable);
        tbl->setItem(0, 0, empty);
        tbl->setSpan(0, 0, 1, tbl->columnCount());
        return;
    }

    int row = 0;
    for (const TaskHistoryRow &r : rows) {
        tbl->insertRow(row);
        tbl->setItem(row, 0, new QTableWidgetItem(r.completedAt.toString(QStringLiteral("dd.MM.yyyy"))));
        tbl->setItem(row, 1, new QTableWidgetItem(r.taskName));
        tbl->setItem(row, 2, new QTableWidgetItem(QString::number(r.intervalDays) + QStringLiteral(" дней")));
        tbl->setItem(row, 3, new QTableWidgetItem(r.nextDate.isValid()
                                                     ? r.nextDate.toString(QStringLiteral("dd.MM.yyyy"))
                                                     : QStringLiteral("—")));
        tbl->setItem(row, 4, new QTableWidgetItem(r.performedBy));
        ++row;
    }
}

void fillErrorTable(QTableWidget *tbl, const QVector<ErrorHistoryRow> &rows)
{
    tbl->clearSpans();
    tbl->setRowCount(0);
    if (rows.isEmpty()) {
        tbl->insertRow(0);
        QTableWidgetItem *empty = new QTableWidgetItem(QStringLiteral("История пока пуста"));
        empty->setFlags(empty->flags() & ~Qt::ItemIsSelectable);
        tbl->setItem(0, 0, empty);
        tbl->setSpan(0, 0, 1, tbl->columnCount());
        return;
    }

    int row = 0;
    for (const ErrorHistoryRow &r : rows) {
        tbl->insertRow(row);
        tbl->setItem(row, 0, new QTableWidgetItem(r.errorDate.toString(QStringLiteral("dd.MM.yyyy"))));
        tbl->setItem(row, 1, new QTableWidgetItem(r.type));
        tbl->setItem(row, 2, new QTableWidgetItem(r.title));
        tbl->setItem(row, 3, new QTableWidgetItem(r.timeRange));
        tbl->setItem(row, 4, new QTableWidgetItem(QString::number(r.durationMinutes) + QStringLiteral(" мин")));
        tbl->setItem(row, 5, new QTableWidgetItem(r.createdBy));
        ++row;
    }
}

void sortTaskRows(QVector<TaskHistoryRow> &rows, int column, int phase)
{
    const bool asc = (phase == 2);
    auto cmpDate = [asc](const QDate &a, const QDate &b) {
        if (!a.isValid() && !b.isValid()) return false;
        if (!a.isValid()) return false;
        if (!b.isValid()) return true;
        return asc ? (a < b) : (a > b);
    };

    std::stable_sort(rows.begin(), rows.end(), [&](const TaskHistoryRow &x, const TaskHistoryRow &y) {
        switch (column) {
        case 0:
            return cmpDate(x.completedAt, y.completedAt);
        case 1: {
            const int byTask = QString::compare(x.taskName, y.taskName, Qt::CaseInsensitive);
            if (byTask != 0)
                return asc ? (byTask < 0) : (byTask > 0);
            return cmpDate(x.completedAt, y.completedAt);
        }
        case 2:
            if (x.intervalDays != y.intervalDays)
                return asc ? (x.intervalDays < y.intervalDays) : (x.intervalDays > y.intervalDays);
            return cmpDate(x.completedAt, y.completedAt);
        case 3:
            if (x.nextDate != y.nextDate)
                return cmpDate(x.nextDate, y.nextDate);
            return cmpDate(x.completedAt, y.completedAt);
        case 4: {
            const int byWho = QString::compare(x.performedBy, y.performedBy, Qt::CaseInsensitive);
            if (byWho != 0)
                return asc ? (byWho < 0) : (byWho > 0);
            return cmpDate(x.completedAt, y.completedAt);
        }
        default:
            return false;
        }
    });
}

void sortErrorRows(QVector<ErrorHistoryRow> &rows, int column, int phase)
{
    const bool asc = (phase == 2);
    auto cmpDate = [asc](const QDate &a, const QDate &b) {
        if (!a.isValid() && !b.isValid()) return false;
        if (!a.isValid()) return false;
        if (!b.isValid()) return true;
        return asc ? (a < b) : (a > b);
    };

    std::stable_sort(rows.begin(), rows.end(), [&](const ErrorHistoryRow &x, const ErrorHistoryRow &y) {
        switch (column) {
        case 0:
            return cmpDate(x.errorDate, y.errorDate);
        case 1: {
            const int byType = QString::compare(x.type, y.type, Qt::CaseInsensitive);
            if (byType != 0)
                return asc ? (byType < 0) : (byType > 0);
            return cmpDate(x.errorDate, y.errorDate);
        }
        case 2: {
            const int byTitle = QString::compare(x.title, y.title, Qt::CaseInsensitive);
            if (byTitle != 0)
                return asc ? (byTitle < 0) : (byTitle > 0);
            return cmpDate(x.errorDate, y.errorDate);
        }
        case 3: {
            const int byTime = QString::compare(x.timeRange, y.timeRange, Qt::CaseInsensitive);
            if (byTime != 0)
                return asc ? (byTime < 0) : (byTime > 0);
            return cmpDate(x.errorDate, y.errorDate);
        }
        case 4:
            if (x.durationMinutes != y.durationMinutes)
                return asc ? (x.durationMinutes < y.durationMinutes) : (x.durationMinutes > y.durationMinutes);
            return cmpDate(x.errorDate, y.errorDate);
        case 5: {
            const int byWho = QString::compare(x.createdBy, y.createdBy, Qt::CaseInsensitive);
            if (byWho != 0)
                return asc ? (byWho < 0) : (byWho > 0);
            return cmpDate(x.errorDate, y.errorDate);
        }
        default:
            return false;
        }
    });
}

QString headerWithHint(const QString &title, int column, int activeColumn, int phase)
{
    if (column != activeColumn || phase == 0)
        return title;
    if (column == 0)
        return title + (phase == 1 ? QStringLiteral(" ▼") : QStringLiteral(" ▲"));
    return title + (phase == 1 ? QStringLiteral(" ▲") : QStringLiteral(" ▼"));
}

void updateTaskHeaders(QTableWidget *tbl, int activeColumn, int phase)
{
    const QStringList base = {
        QStringLiteral("Дата проведения"),
        QStringLiteral("Задача"),
        QStringLiteral("Интервал"),
        QStringLiteral("Следующая дата"),
        QStringLiteral("Кем проведено")
    };
    for (int c = 0; c < base.size() && c < tbl->columnCount(); ++c)
        tbl->horizontalHeaderItem(c)->setText(headerWithHint(base.at(c), c, activeColumn, phase));
}

void updateErrorHeaders(QTableWidget *tbl, int activeColumn, int phase)
{
    const QStringList base = {
        QStringLiteral("Дата"),
        QStringLiteral("Тип"),
        QStringLiteral("Название"),
        QStringLiteral("Время"),
        QStringLiteral("Длительность"),
        QStringLiteral("Кем записано")
    };
    for (int c = 0; c < base.size() && c < tbl->columnCount(); ++c)
        tbl->horizontalHeaderItem(c)->setText(headerWithHint(base.at(c), c, activeColumn, phase));
}

} // namespace

void AgvSettingsPage::openTaskHistoryDialog()
{
    if (currentAgvId.trimmed().isEmpty())
        return;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        QMessageBox::warning(this, "История задач", "База данных не открыта.");
        return;
    }
    if (!ensureTaskHistoryTable()) {
        QMessageBox::warning(this, "История задач", "Таблица истории задач недоступна.");
        return;
    }

    QVector<TaskHistoryRow> rows;
    QSqlQuery q(db);
    q.prepare("SELECT completed_at, task_name, interval_days, next_date_after, performed_by "
              "FROM agv_task_history WHERE agv_id = :id ORDER BY completed_ts DESC");
    q.bindValue(":id", currentAgvId);
    if (!q.exec()) {
        QMessageBox::warning(this, "История задач", "Ошибка чтения истории: " + q.lastError().text());
        return;
    }
    while (q.next()) {
        TaskHistoryRow r;
        r.completedAt = q.value(0).toDate();
        r.taskName = q.value(1).toString();
        r.intervalDays = q.value(2).toInt();
        r.nextDate = q.value(3).toDate();
        r.performedBy = q.value(4).toString();
        rows.push_back(r);
    }

    QDialog dlg(this);
    dlg.setWindowTitle("История задач");
    dlg.setModal(true);
    dlg.resize(s(900), s(520));
    dlg.setStyleSheet(kHistoryDialogStyle);

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(s(12), s(12), s(12), s(12));
    root->setSpacing(s(10));

    QTableWidget *tbl = new QTableWidget(&dlg);
    tbl->setColumnCount(5);
    tbl->setHorizontalHeaderLabels(QStringList() << "Дата проведения"
                                                 << "Задача"
                                                 << "Интервал"
                                                 << "Следующая дата"
                                                 << "Кем проведено");
    tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tbl->horizontalHeader()->setSectionsClickable(true);
    tbl->verticalHeader()->setVisible(false);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    tbl->setSelectionMode(QAbstractItemView::SingleSelection);

    QVector<TaskHistoryRow> displayRows = rows;
    QHash<int, int> columnPhase;
    int activeColumn = -1;

    auto applyView = [&]() {
        fillTaskTable(tbl, displayRows);
        if (!rows.isEmpty())
            updateTaskHeaders(tbl, activeColumn, activeColumn >= 0 ? columnPhase.value(activeColumn, 1) : 0);
    };
    applyView();

    QObject::connect(tbl->horizontalHeader(), &QHeaderView::sectionClicked, &dlg, [&](int column) {
        if (rows.isEmpty())
            return;
        int phase = columnPhase.value(column, 0);
        phase = (phase % 2) + 1;
        columnPhase[column] = phase;
        activeColumn = column;
        displayRows = rows;
        sortTaskRows(displayRows, column, phase);
        applyView();
    });

    QPushButton *closeBtn = new QPushButton("Закрыть", &dlg);
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    root->addWidget(tbl, 1);
    root->addWidget(closeBtn, 0, Qt::AlignRight);
    dlg.exec();
}

void AgvSettingsPage::openErrorHistoryDialog()
{
    if (currentAgvId.trimmed().isEmpty())
        return;

    QString loadErr;
    const QVector<AgvErrorLog> logs = loadAgvErrorLogs(currentAgvId, QDate(), QDate(), &loadErr);
    if (!loadErr.isEmpty()) {
        QMessageBox::warning(this, "История ошибок", "Ошибка чтения истории: " + loadErr);
        return;
    }

    QVector<ErrorHistoryRow> rows;
    rows.reserve(logs.size());
    for (const AgvErrorLog &e : logs) {
        ErrorHistoryRow r;
        r.errorDate = e.errorDate;
        r.type = e.type;
        r.title = e.title;
        r.timeRange = formatTimeRange(e.timeFrom, e.timeTo);
        r.durationMinutes = e.durationMinutes;
        r.createdBy = e.createdBy;
        rows.push_back(r);
    }

    QDialog dlg(this);
    dlg.setWindowTitle("История ошибок");
    dlg.setModal(true);
    dlg.resize(s(960), s(520));
    dlg.setStyleSheet(kHistoryDialogStyle);

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(s(12), s(12), s(12), s(12));
    root->setSpacing(s(10));

    QTableWidget *tbl = new QTableWidget(&dlg);
    tbl->setColumnCount(6);
    tbl->setHorizontalHeaderLabels(QStringList() << "Дата"
                                                 << "Тип"
                                                 << "Название"
                                                 << "Время"
                                                 << "Длительность"
                                                 << "Кем записано");
    tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tbl->horizontalHeader()->setSectionsClickable(true);
    tbl->verticalHeader()->setVisible(false);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    tbl->setSelectionMode(QAbstractItemView::SingleSelection);

    QVector<ErrorHistoryRow> displayRows = rows;
    QHash<int, int> columnPhase;
    int activeColumn = -1;

    auto applyView = [&]() {
        fillErrorTable(tbl, displayRows);
        if (!rows.isEmpty())
            updateErrorHeaders(tbl, activeColumn, activeColumn >= 0 ? columnPhase.value(activeColumn, 1) : 0);
    };
    applyView();

    QObject::connect(tbl->horizontalHeader(), &QHeaderView::sectionClicked, &dlg, [&](int column) {
        if (rows.isEmpty())
            return;
        int phase = columnPhase.value(column, 0);
        phase = (phase % 2) + 1;
        columnPhase[column] = phase;
        activeColumn = column;
        displayRows = rows;
        sortErrorRows(displayRows, column, phase);
        applyView();
    });

    QPushButton *closeBtn = new QPushButton("Закрыть", &dlg);
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    root->addWidget(tbl, 1);
    root->addWidget(closeBtn, 0, Qt::AlignRight);
    dlg.exec();
}
