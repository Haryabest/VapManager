#include "agvsettingspage.h"

#include <QAbstractItemView>
#include <QDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

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

    QDialog dlg(this);
    dlg.setWindowTitle("История задач");
    dlg.setModal(true);
    dlg.resize(s(900), s(520));
    dlg.setStyleSheet(
        "QDialog{background:#F7F8FA;}"
        "QTableWidget{background:white;border:1px solid #D9DDE3;border-radius:8px;}"
        "QHeaderView::section{background:#ECEFF3;color:#1A1A1A;font-weight:800;padding:6px;border:none;}"
        "QPushButton{background:#0F00DB;color:white;border:none;border-radius:8px;padding:8px 14px;font-weight:800;}"
        "QPushButton:hover{background:#1A4ACD;}"
    );

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
    tbl->verticalHeader()->setVisible(false);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    tbl->setSelectionMode(QAbstractItemView::SingleSelection);

    QSqlQuery q(db);
    q.prepare("SELECT completed_at, task_name, interval_days, next_date_after, performed_by "
              "FROM agv_task_history WHERE agv_id = :id ORDER BY completed_ts DESC");
    q.bindValue(":id", currentAgvId);
    if (!q.exec()) {
        QMessageBox::warning(this, "История задач", "Ошибка чтения истории: " + q.lastError().text());
        return;
    }

    int row = 0;
    while (q.next()) {
        tbl->insertRow(row);
        tbl->setItem(row, 0, new QTableWidgetItem(q.value(0).toDate().toString("dd.MM.yyyy")));
        tbl->setItem(row, 1, new QTableWidgetItem(q.value(1).toString()));
        tbl->setItem(row, 2, new QTableWidgetItem(QString::number(q.value(2).toInt()) + " дней"));
        tbl->setItem(row, 3, new QTableWidgetItem(q.value(3).toDate().toString("dd.MM.yyyy")));
        tbl->setItem(row, 4, new QTableWidgetItem(q.value(4).toString()));
        ++row;
    }

    if (row == 0) {
        tbl->insertRow(0);
        QTableWidgetItem *empty = new QTableWidgetItem("История пока пуста");
        empty->setFlags(empty->flags() & ~Qt::ItemIsSelectable);
        tbl->setItem(0, 0, empty);
        tbl->setSpan(0, 0, 1, 5);
    }

    QPushButton *closeBtn = new QPushButton("Закрыть", &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    root->addWidget(tbl, 1);
    root->addWidget(closeBtn, 0, Qt::AlignRight);
    dlg.exec();
}
