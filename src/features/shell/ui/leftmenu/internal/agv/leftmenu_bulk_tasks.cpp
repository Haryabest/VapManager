#include "leftmenu.h"

#include "app_session.h"
#include "databus.h"
#include "db_users.h"
#include "listagvinfo.h"
#include "ui_action_logger.h"

#include <QCheckBox>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QVBoxLayout>

namespace {

bool ensureTaskHistoryTable(QSqlDatabase db)
{
    QSqlQuery q(db);
    return q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS agv_task_history ("
        "  id SERIAL PRIMARY KEY,"
        "  agv_id VARCHAR(64) NOT NULL,"
        "  task_id INT NULL,"
        "  task_name VARCHAR(255) NOT NULL,"
        "  interval_days INT NOT NULL DEFAULT 0,"
        "  completed_at DATE NOT NULL,"
        "  completed_ts TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "  next_date_after DATE NULL,"
        "  performed_by VARCHAR(128) NULL"
        ")"));
}

QDate computeNextDate(const QDate &lastService, int intervalDays)
{
    if (intervalDays <= 0)
        return lastService.isValid() ? lastService : QDate::currentDate();
    return lastService.addDays(intervalDays);
}

int completeAllTasksForAgv(const QString &agvId, QString *errorOut)
{
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen()) {
        if (errorOut) *errorOut = QStringLiteral("БД не открыта");
        return -1;
    }
    if (!ensureTaskHistoryTable(db)) {
        if (errorOut) *errorOut = QStringLiteral("Не удалось подготовить историю задач");
        return -1;
    }

    QString agvStatus;
    QString assignedUser;
    QString assignedBy;
    {
        QSqlQuery agvQ(db);
        agvQ.prepare(QStringLiteral(
            "SELECT status, COALESCE(assigned_user, ''), COALESCE(assigned_by, '') FROM agv_list WHERE agv_id = :id"));
        agvQ.bindValue(QStringLiteral(":id"), agvId);
        if (!agvQ.exec() || !agvQ.next()) {
            if (errorOut) *errorOut = QStringLiteral("AGV не найден: %1").arg(agvId);
            return -1;
        }
        agvStatus = agvQ.value(0).toString();
        assignedUser = agvQ.value(1).toString();
        assignedBy = agvQ.value(2).toString();
    }
    if (agvStatus.trimmed().toLower() == QStringLiteral("offline"))
        return 0;

    const QString currentUser = AppSession::currentUsername();
    const QDate performedDate = QDate::currentDate();

    QSqlQuery tasksQ(db);
    tasksQ.prepare(QStringLiteral(
        "SELECT id, task_name, interval_days, COALESCE(assigned_to, ''), COALESCE(delegated_by, '') "
        "FROM agv_tasks WHERE agv_id = :id ORDER BY id"));
    tasksQ.bindValue(QStringLiteral(":id"), agvId);
    if (!tasksQ.exec()) {
        if (errorOut) *errorOut = tasksQ.lastError().text();
        return -1;
    }

    int done = 0;
    const bool useTransaction = db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions);
    while (tasksQ.next()) {
        const int taskId = tasksQ.value(0).toInt();
        const QString taskName = tasksQ.value(1).toString();
        const int intervalDays = tasksQ.value(2).toInt();
        const QString taskAssignee = tasksQ.value(3).toString();
        const QString taskDelegatedBy = tasksQ.value(4).toString();
        const QString effectiveAssignee = !taskAssignee.isEmpty() ? taskAssignee : assignedUser;
        if (!effectiveAssignee.isEmpty() && effectiveAssignee != currentUser)
            continue;

        const QDate nextDate = computeNextDate(performedDate, intervalDays);
        if (useTransaction && !db.transaction())
            continue;

        QSqlQuery upd(db);
        if (currentUser == assignedUser) {
            upd.prepare(QStringLiteral("UPDATE agv_tasks SET next_date = :next WHERE id = :id"));
        } else {
            upd.prepare(QStringLiteral(
                "UPDATE agv_tasks SET next_date = :next, assigned_to = :assign, delegated_by = :delegated WHERE id = :id"));
            upd.bindValue(QStringLiteral(":assign"), assignedUser);
            upd.bindValue(QStringLiteral(":delegated"), assignedBy);
        }
        upd.bindValue(QStringLiteral(":next"), nextDate.toString(QStringLiteral("yyyy-MM-dd")));
        upd.bindValue(QStringLiteral(":id"), taskId);
        if (!upd.exec()) {
            if (useTransaction) db.rollback();
            continue;
        }

        QSqlQuery ins(db);
        ins.prepare(QStringLiteral(
            "INSERT INTO agv_task_history "
            "(agv_id, task_id, task_name, interval_days, completed_at, next_date_after, performed_by) "
            "VALUES (:agv, :tid, :name, :intv, :done, :next, :by)"));
        ins.bindValue(QStringLiteral(":agv"), agvId);
        ins.bindValue(QStringLiteral(":tid"), taskId);
        ins.bindValue(QStringLiteral(":name"), taskName);
        ins.bindValue(QStringLiteral(":intv"), intervalDays);
        ins.bindValue(QStringLiteral(":done"), performedDate.toString(QStringLiteral("yyyy-MM-dd")));
        ins.bindValue(QStringLiteral(":next"), nextDate.toString(QStringLiteral("yyyy-MM-dd")));
        ins.bindValue(QStringLiteral(":by"), currentUser);
        if (!ins.exec()) {
            if (useTransaction) db.rollback();
            continue;
        }

        if (useTransaction && !db.commit()) {
            db.rollback();
            continue;
        }

        logAction(currentUser,
                  QStringLiteral("agv_task_completed"),
                  QStringLiteral("AGV=%1, task=%2, next=%3 (bulk)")
                      .arg(agvId, taskName, nextDate.toString(QStringLiteral("dd.MM.yyyy"))));
        Q_UNUSED(taskDelegatedBy);
        ++done;
    }
    return done;
}

} // namespace

void leftMenu::openBulkCompleteTasksDialog()
{
    const QString currentUser = AppSession::currentUsername();
    const QString role = getUserRole(currentUser);
    if (role != QStringLiteral("admin") && role != QStringLiteral("tech")) {
        QMessageBox::warning(this, QStringLiteral("Задачи"),
                             QStringLiteral("Массовое проведение задач доступно только admin и tech."));
        return;
    }
    if (!listAgvInfo) {
        QMessageBox::warning(this, QStringLiteral("Задачи"), QStringLiteral("Список AGV недоступен."));
        return;
    }

    const QVector<AgvInfo> agvs = listAgvInfo->loadAgvList();
    if (agvs.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Задачи"), QStringLiteral("Нет AGV для обработки."));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Провести задачи — несколько AGV"));
    dlg.setMinimumSize(s(520), s(480));

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(s(16), s(14), s(16), s(14));
    root->setSpacing(s(10));

    QLabel *hint = new QLabel(
        QStringLiteral("Выберите AGV. Для каждого будут проведены все доступные задачи (как «Провести все задачи»)."),
        &dlg);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("font-family:Inter;font-size:%1px;color:#475569;").arg(s(13)));
    root->addWidget(hint);

    QListWidget *list = new QListWidget(&dlg);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    for (const AgvInfo &a : agvs) {
        if (a.status.trimmed().toLower() == QStringLiteral("offline"))
            continue;
        QListWidgetItem *item = new QListWidgetItem(a.id, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, a.id);
    }
    if (list->count() == 0) {
        QMessageBox::information(this, QStringLiteral("Задачи"),
                                 QStringLiteral("Нет AGV в статусе online для проведения задач."));
        return;
    }
    root->addWidget(list, 1);

    QHBoxLayout *selRow = new QHBoxLayout();
    QPushButton *allBtn = new QPushButton(QStringLiteral("Выбрать все"), &dlg);
    QPushButton *noneBtn = new QPushButton(QStringLiteral("Снять все"), &dlg);
    connect(allBtn, &QPushButton::clicked, &dlg, [list]() {
        for (int i = 0; i < list->count(); ++i)
            list->item(i)->setCheckState(Qt::Checked);
    });
    connect(noneBtn, &QPushButton::clicked, &dlg, [list]() {
        for (int i = 0; i < list->count(); ++i)
            list->item(i)->setCheckState(Qt::Unchecked);
    });
    selRow->addWidget(allBtn);
    selRow->addWidget(noneBtn);
    selRow->addStretch();
    root->addLayout(selRow);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Провести"));
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    root->addWidget(box);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QStringList selected;
    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->checkState() == Qt::Checked)
            selected << list->item(i)->data(Qt::UserRole).toString();
    }
    if (selected.isEmpty())
        return;

    const auto reply = QMessageBox::question(
        this,
        QStringLiteral("Подтверждение"),
        QStringLiteral("Провести все задачи для %1 AGV?").arg(selected.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    int totalDone = 0;
    int agvProcessed = 0;
    int agvSkipped = 0;
    for (const QString &agvId : selected) {
        QString err;
        const int done = completeAllTasksForAgv(agvId, &err);
        if (done < 0) {
            ++agvSkipped;
            continue;
        }
        if (done > 0)
            ++agvProcessed;
        totalDone += qMax(0, done);
    }

    emit DataBus::instance().calendarChanged();
    DataBus::instance().triggerAgvTasksChanged(QString());
    if (listAgvInfo)
        listAgvInfo->rebuildList(listAgvInfo->loadAgvList());

    QMessageBox::information(
        this,
        QStringLiteral("Готово"),
        QStringLiteral("AGV обработано: %1\nЗадач проведено: %2\nПропущено AGV: %3")
            .arg(agvProcessed)
            .arg(totalDone)
            .arg(agvSkipped));
}
