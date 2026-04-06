#include "listagvinfo.h"

#include "addagvdialog.h"
#include "app_session.h"
#include "databus.h"
#include "db_agv_tasks.h"
#include "db_users.h"

#include <QCheckBox>
#include <QDate>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QVBoxLayout>

ListAgvInfo::ListAgvInfo(std::function<int(int)> scale, QWidget *parent)
    : QFrame(parent), s(scale)
{
    currentFilter = FilterSettings();

    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background-color:#F1F2F4;border-radius:12px;");

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(s(10), s(10), s(10), s(10));
    root->setSpacing(s(12));

    QWidget *header = new QWidget(this);
    QHBoxLayout *hdr = new QHBoxLayout(header);
    hdr->setContentsMargins(0,0,0,0);
    hdr->setSpacing(s(10));

    QPushButton *back = new QPushButton("   Назад", header);
    back->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    back->setIconSize(QSize(s(24), s(24)));
    back->setFixedSize(s(150), s(50));
    back->setStyleSheet(QString(
        "QPushButton {"
        "   background-color:#E6E6E6;"
        "   border-radius:%1px;"
        "   border:1px solid #C8C8C8;"
        "   font-family:Inter;"
        "   font-size:%2px;"
        "   font-weight:800;"
        "   color:black;"
        "   text-align:left;"
        "   padding-left:%3px;"
        "}"
        "QPushButton:hover { background-color:#D5D5D5; }"
    ).arg(s(10)).arg(s(16)).arg(s(10)));

    connect(back, &QPushButton::clicked, this, [this](){ emit backRequested(); });
    hdr->addWidget(back, 0, Qt::AlignLeft);
    hdr->addStretch();

    QLabel *title = new QLabel("Список AGV", header);
    title->setStyleSheet(QString("font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;").arg(s(26)));
    title->setAlignment(Qt::AlignCenter);
    hdr->addWidget(title, 0, Qt::AlignCenter);

    hdr->addStretch();

    QPushButton *deleteBtn = new QPushButton("Удалить", header);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setFixedSize(s(165), s(50));
    deleteBtn->setStyleSheet(QString(
        "QPushButton{"
        "   background-color:#FF3B30;"
        "   border:1px solid #C72B22;"
        "   border-radius:%1px;"
        "   font-family:Inter;"
        "   font-size:%2px;"
        "   font-weight:800;"
        "   color:white;"
        "   text-align:center;"
        "   padding:0 14px;"
        "}"
        "QPushButton:hover{background-color:#E4372D;}"
        "QPushButton:pressed{background-color:#C92D24;}"
    ).arg(s(10)).arg(s(16)));

    {
        QString role = getUserRole(AppSession::currentUsername());
        if (role == "viewer")
            deleteBtn->hide();
    }

    hdr->addWidget(deleteBtn, 0, Qt::AlignRight);
    hdr->setAlignment(deleteBtn, Qt::AlignRight);
    deleteBtn->setContentsMargins(0, 0, s(5), 0);
    root->addWidget(header);

    QWidget *legend = new QWidget(this);
    QHBoxLayout *lg = new QHBoxLayout(legend);
    lg->setContentsMargins(0, s(7), 0, 0);
    lg->setSpacing(s(12));

    auto makeDot = [&](QString color){
        QLabel *dot = new QLabel(legend);
        dot->setFixedSize(s(18), s(18));
        dot->setStyleSheet(QString("background:%1;border-radius:%2px;").arg(color).arg(s(9)));
        return dot;
    };

    auto makeLabel = [&](QString text){
        QLabel *l = new QLabel(text, legend);
        l->setStyleSheet(QString("font-family:Inter;font-size:%1px;font-weight:800;color:#222;").arg(s(18)));
        return l;
    };

    lg->addWidget(makeDot("#18CF00"));
    lg->addWidget(makeLabel("Обслужено"));
    lg->addSpacing(s(18));
    lg->addWidget(makeDot("#FF8800"));
    lg->addWidget(makeLabel("Скоро обслуживание"));
    lg->addSpacing(s(18));
    lg->addWidget(makeDot("#FF0000"));
    lg->addWidget(makeLabel("Просрочено"));
    lg->addSpacing(s(18));
    lg->addWidget(makeDot("#00C8FF"));
    lg->addWidget(makeLabel("В работе"));
    lg->addStretch();
    root->addWidget(legend);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");

    content = new QWidget();
    content->setStyleSheet("background:transparent;");
    layout = new QVBoxLayout(content);
    layout->setSpacing(s(8));
    layout->setContentsMargins(0,0,0,0);

    scroll->setWidget(content);
    root->addWidget(scroll);

    addBtn_ = new QPushButton("+ Добавить AGV", this);
    addBtn_->setFixedSize(s(320), s(50));
    addBtn_->setStyleSheet(QString(
        "QPushButton { background-color:#0F00DB; border-radius:%1px; font-family:Inter; font-size:%2px; font-weight:800; color:white; }"
        "QPushButton:hover { background-color:#1A4ACD; }"
    ).arg(s(10)).arg(s(16)));
    addBtn_->raise();
    {
        QString role = getUserRole(AppSession::currentUsername());
        if (role == "viewer")
            addBtn_->hide();
    }
    root->addWidget(addBtn_, 0, Qt::AlignHCenter);

    undoToast_ = new QFrame(this);
    undoToast_->setStyleSheet(
        "QFrame{background:#111827;border-radius:12px;}"
        "QLabel{color:white;font-family:Inter;font-size:14px;font-weight:700;background:transparent;}"
        "QPushButton{background:#2563EB;color:white;border:none;border-radius:8px;padding:6px 14px;"
        "font-family:Inter;font-size:13px;font-weight:800;}"
        "QPushButton:hover{background:#1D4ED8;}"
    );
    undoToast_->setFixedHeight(s(50));
    undoToast_->hide();
    QHBoxLayout *undoLay = new QHBoxLayout(undoToast_);
    undoLay->setContentsMargins(s(12), s(8), s(12), s(8));
    undoLay->setSpacing(s(10));
    QLabel *undoText = new QLabel("AGV удалены", undoToast_);
    undoBtn_ = new QPushButton("Вернуть", undoToast_);
    undoLay->addWidget(undoText);
    undoLay->addStretch();
    undoLay->addWidget(undoBtn_);

    undoTimer_ = new QTimer(this);
    undoTimer_->setSingleShot(true);
    connect(undoTimer_, &QTimer::timeout, this, [this](){
        undoToast_->hide();
        clearUndoSnapshot();
    });
    connect(undoBtn_, &QPushButton::clicked, this, [this](){
        restoreDeletedAgvs();
    });

    loadMoreBtn_ = new QPushButton(QStringLiteral("Показать ещё 50"), this);
    loadMoreBtn_->setFixedSize(s(260), s(44));
    loadMoreBtn_->setStyleSheet(QString(
        "QPushButton { background-color:#E6E6E6; border:1px solid #C8C8C8; border-radius:%1px;"
        "font-family:Inter; font-size:%2px; font-weight:800; color:#1A1A1A; }"
        "QPushButton:hover { background-color:#D5D5D5; }"
    ).arg(s(10)).arg(s(15)));
    loadMoreBtn_->hide();
    connect(loadMoreBtn_, &QPushButton::clicked, this, [this](){
        shownCount_ = qMin(shownCount_ + batchSize_, displayQueueTotal_);
        rebuildShownChunk();
    });

    connect(addBtn_, &QPushButton::clicked, this, [this](){
        AddAgvDialog dlg(s, this);
        if (dlg.exec() == QDialog::Accepted)
        {
            AgvInfo info;

            QString baseName = dlg.result.name.trimmed();
            QString digits;
            QRegularExpression re("\\d+");
            auto it = re.globalMatch(dlg.result.serial);
            while (it.hasNext())
                digits += it.next().captured();

            QString last4 = digits.right(4);
            if (last4.isEmpty())
                last4 = "0000";

            QString modelUpper = dlg.result.model.toUpper();
            QString finalId = QString("%1_%2_%3").arg(baseName).arg(last4).arg(modelUpper);

            info.id = finalId;
            info.model = modelUpper;
            info.serial = dlg.result.serial;
            info.status = dlg.result.status;
            info.task = dlg.result.alias.trimmed();
            info.kilometers = 0;
            info.blueprintPath = ":/new/mainWindowIcons/noback/blueprint.png";
            info.lastActive = QDate::currentDate();

            if (!insertAgvToDb(info)) {
                qDebug() << "insertAgvToDb: не удалось записать AGV";
                return;
            }

            if (!copyModelTasksToAgv(info.id, info.model)) {
                qDebug() << "copyModelTasksToAgv: ошибка копирования задач для" << info.id;
            }

            QVector<AgvInfo> agvs = loadAgvList();
            rebuildList(agvs);
            emit agvListChanged();
        }
    });

    connect(deleteBtn, &QPushButton::clicked, this, [this](){
        QDialog dlg(this);
        dlg.setWindowTitle("Удалить AGV");
        dlg.setFixedSize(s(460), s(520));
        dlg.setStyleSheet(
            "QDialog { background: #FFFFFF; border: 1px solid #E7E9ED; border-radius: 12px; }"
            "QLabel { background: transparent; font-family: Inter; color: #1A1A1A; }"
            "QLabel#title { font-size: 20px; font-weight: 900; }"
            "QLabel#subtitle { font-size: 14px; font-weight: 600; color: #687083; }"
            "QFrame#listBox { background: #F7F8FA; border: 1px solid #E1E4EA; border-radius: 10px; }"
            "QCheckBox { background: transparent; font-family: Inter; font-size: 15px; color: #1A1A1A; spacing: 8px; }"
            "QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #B7BECC; border-radius: 5px; background: #FFFFFF; }"
            "QCheckBox::indicator:checked { background: #0F00DB; border: 1px solid #0F00DB; }"
            "QPushButton { font-family: Inter; font-size: 15px; font-weight: 800; border-radius: 8px; padding: 7px 14px; border: 1px solid transparent; }"
            "QPushButton#ok { background: #FF3B30; color: white; }"
            "QPushButton#ok:hover { background: #E13228; }"
            "QPushButton#ok:pressed { background: #C92D24; }"
            "QPushButton#cancel { background: #EFF1F5; border-color: #D3D9E4; color: #1A1A1A; }"
            "QPushButton#cancel:hover { background: #E2E6EE; }"
            "QPushButton#selectAll { background: #0F00DB; color: white; border-color: #0B00A6; }"
            "QPushButton#selectAll:hover { background: #1A4ACD; }"
        );

        QVBoxLayout *v = new QVBoxLayout(&dlg);
        v->setContentsMargins(s(18), s(16), s(18), s(16));
        v->setSpacing(s(10));

        QLabel *titleLbl = new QLabel("Удаление AGV", &dlg);
        titleLbl->setObjectName("title");
        v->addWidget(titleLbl);

        QLabel *selectedLbl = new QLabel("Выбрано: 0", &dlg);
        selectedLbl->setObjectName("subtitle");
        v->addWidget(selectedLbl);

        QFrame *listBox = new QFrame(&dlg);
        listBox->setObjectName("listBox");
        QVBoxLayout *listBoxLay = new QVBoxLayout(listBox);
        listBoxLay->setContentsMargins(s(10), s(10), s(10), s(10));
        listBoxLay->setSpacing(s(8));

        QScrollArea *listScroll = new QScrollArea(listBox);
        listScroll->setWidgetResizable(true);
        listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        listScroll->setStyleSheet(
            "QScrollArea { border: none; background: transparent; }"
            "QScrollBar:vertical { width: 8px; background: transparent; margin: 2px; }"
            "QScrollBar::handle:vertical { background: #C5CAD5; border-radius: 4px; min-height: 26px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        );

        QWidget *listHost = new QWidget(listScroll);
        listHost->setObjectName("listHost");
        listHost->setStyleSheet("background: transparent;");
        QVBoxLayout *listHostLay = new QVBoxLayout(listHost);
        listHostLay->setContentsMargins(0, 0, 0, 0);
        listHostLay->setSpacing(s(6));

        QVector<QCheckBox*> boxes;
        QSqlQuery idsQ(QSqlDatabase::database("main_connection"));
        idsQ.prepare("SELECT agv_id FROM agv_list ORDER BY created_at DESC");
        if (idsQ.exec()) {
            while (idsQ.next()) {
                const QString agvId = idsQ.value(0).toString();
                QCheckBox *cb = new QCheckBox(agvId, &dlg);
                cb->setProperty("db_id", agvId);
                boxes.push_back(cb);
                listHostLay->addWidget(cb);
            }
        } else {
            qDebug() << "delete dialog: failed to load AGV ids:" << idsQ.lastError().text();
        }

        if (boxes.isEmpty()) {
            QLabel *emptyLbl = new QLabel("Список AGV пуст.", listHost);
            emptyLbl->setObjectName("subtitle");
            listHostLay->addWidget(emptyLbl);
        }

        listHostLay->addStretch();
        listScroll->setWidget(listHost);
        listBoxLay->addWidget(listScroll);
        v->addWidget(listBox, 1);

        QPushButton *selectAllBtn = new QPushButton("Выбрать все", &dlg);
        selectAllBtn->setObjectName("selectAll");
        selectAllBtn->setCursor(Qt::PointingHandCursor);
        v->addWidget(selectAllBtn);

        auto updateSelectionUi = [selectedLbl, selectAllBtn, boxes]() {
            int selectedCount = 0;
            for (int i = 0; i < boxes.size(); ++i) {
                if (boxes[i]->isChecked())
                    ++selectedCount;
            }

            selectedLbl->setText(QString("Выбрано: %1").arg(selectedCount));
            const bool allChecked = (!boxes.isEmpty() && selectedCount == boxes.size());
            selectAllBtn->setText(allChecked ? "Снять выбор" : "Выбрать все");
        };

        connect(selectAllBtn, &QPushButton::clicked, &dlg, [boxes, updateSelectionUi]() {
            bool allChecked = true;
            for (int i = 0; i < boxes.size(); ++i) {
                if (!boxes[i]->isChecked()) {
                    allChecked = false;
                    break;
                }
            }

            const bool targetState = !allChecked;
            for (int i = 0; i < boxes.size(); ++i)
                boxes[i]->setChecked(targetState);

            updateSelectionUi();
        });

        for (int i = 0; i < boxes.size(); ++i) {
            connect(boxes[i], &QCheckBox::toggled, &dlg, [updateSelectionUi](bool checked){
                Q_UNUSED(checked)
                updateSelectionUi();
            });
        }

        updateSelectionUi();

        v->addStretch();

        QHBoxLayout *btns = new QHBoxLayout();
        QPushButton *cancel = new QPushButton("Отмена", &dlg);
        cancel->setObjectName("cancel");
        QPushButton *ok = new QPushButton("Удалить", &dlg);
        ok->setObjectName("ok");

        btns->addWidget(cancel);
        btns->addStretch();
        btns->addWidget(ok);
        v->addLayout(btns);

        connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
        connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);

        if (dlg.exec() == QDialog::Accepted) {
            QStringList selectedIds;
            for (QCheckBox *cb : boxes) {
                if (cb->isChecked()) {
                    const QString id = cb->property("db_id").toString().trimmed();
                    if (!id.isEmpty())
                        selectedIds << id;
                }
            }

            if (selectedIds.isEmpty()) {
                QMessageBox::information(this, "Удаление AGV", "Выберите хотя бы один AGV.");
                return;
            }

            QSqlDatabase db = QSqlDatabase::database("main_connection");
            if (!db.isOpen()) {
                QMessageBox::warning(this, "Удаление AGV", "База данных не открыта.");
                return;
            }

            lastDeletedAgvs_.clear();
            lastDeletedTasks_.clear();
            lastDeletedHistory_.clear();

            auto loadAgvSnapshot = [&](const QStringList &ids) {
                const int chunkSize = 300;
                for (int offset = 0; offset < ids.size(); offset += chunkSize) {
                    const int count = qMin(chunkSize, ids.size() - offset);
                    QStringList placeholders;
                    placeholders.reserve(count);
                    for (int i = 0; i < count; ++i)
                        placeholders << QString(":id%1").arg(i);

                    QSqlQuery q(db);
                    q.prepare(QString(
                        "SELECT agv_id, model, serial, status, alias, kilometers, blueprintPath, lastActive "
                        "FROM agv_list WHERE agv_id IN (%1)").arg(placeholders.join(",")));
                    for (int i = 0; i < count; ++i)
                        q.bindValue(QString(":id%1").arg(i), ids[offset + i]);

                    if (q.exec()) {
                        while (q.next()) {
                            AgvInfo info;
                            info.id = q.value(0).toString();
                            info.model = q.value(1).toString();
                            info.serial = q.value(2).toString();
                            info.status = q.value(3).toString();
                            info.task = q.value(4).toString().trimmed();
                            if (info.task == "—")
                                info.task.clear();
                            info.kilometers = q.value(5).toInt();
                            info.blueprintPath = q.value(6).toString();
                            info.lastActive = q.value(7).toDate();
                            lastDeletedAgvs_.push_back(info);
                        }
                    }
                }
            };

            auto loadTaskSnapshot = [&](const QStringList &ids) {
                const int chunkSize = 300;
                for (int offset = 0; offset < ids.size(); offset += chunkSize) {
                    const int count = qMin(chunkSize, ids.size() - offset);
                    QStringList placeholders;
                    placeholders.reserve(count);
                    for (int i = 0; i < count; ++i)
                        placeholders << QString(":id%1").arg(i);

                    QSqlQuery q(db);
                    q.prepare(QString(
                        "SELECT agv_id, task_name, task_description, interval_days, duration_minutes, is_default, next_date "
                        "FROM agv_tasks WHERE agv_id IN (%1)").arg(placeholders.join(",")));
                    for (int i = 0; i < count; ++i)
                        q.bindValue(QString(":id%1").arg(i), ids[offset + i]);

                    if (q.exec()) {
                        while (q.next()) {
                            AgvTask t;
                            t.agvId = q.value(0).toString();
                            t.taskName = q.value(1).toString();
                            t.taskDescription = q.value(2).toString();
                            t.intervalDays = q.value(3).toInt();
                            t.durationMinutes = q.value(4).toInt();
                            t.isDefault = q.value(5).toBool();
                            t.nextDate = q.value(6).toDate();
                            lastDeletedTasks_.push_back(t);
                        }
                    }
                }
            };

            auto loadHistorySnapshot = [&](const QStringList &ids) {
                const int chunkSize = 300;
                for (int offset = 0; offset < ids.size(); offset += chunkSize) {
                    const int count = qMin(chunkSize, ids.size() - offset);
                    QStringList placeholders;
                    placeholders.reserve(count);
                    for (int i = 0; i < count; ++i)
                        placeholders << QString(":id%1").arg(i);

                    QSqlQuery q(db);
                    q.prepare(QString(
                        "SELECT agv_id, task_id, task_name, interval_days, completed_at, next_date_after, performed_by "
                        "FROM agv_task_history WHERE agv_id IN (%1)").arg(placeholders.join(",")));
                    for (int i = 0; i < count; ++i)
                        q.bindValue(QString(":id%1").arg(i), ids[offset + i]);

                    if (q.exec()) {
                        while (q.next()) {
                            DeletedHistoryRow h;
                            h.agvId = q.value(0).toString();
                            h.taskId = q.value(1).toInt();
                            h.taskName = q.value(2).toString();
                            h.intervalDays = q.value(3).toInt();
                            h.completedAt = q.value(4).toDate();
                            h.nextDateAfter = q.value(5).toDate();
                            h.performedBy = q.value(6).toString();
                            lastDeletedHistory_.push_back(h);
                        }
                    }
                }
            };

            loadAgvSnapshot(selectedIds);
            loadTaskSnapshot(selectedIds);
            loadHistorySnapshot(selectedIds);

            auto deleteByIds = [&](const QString &table, const QString &column) -> bool {
                const int chunkSize = 300;
                for (int offset = 0; offset < selectedIds.size(); offset += chunkSize) {
                    const int count = qMin(chunkSize, selectedIds.size() - offset);
                    QStringList placeholders;
                    placeholders.reserve(count);

                    QSqlQuery q(db);
                    for (int i = 0; i < count; ++i)
                        placeholders << QString(":id%1").arg(i);

                    q.prepare(QString("DELETE FROM %1 WHERE %2 IN (%3)")
                              .arg(table, column, placeholders.join(",")));
                    for (int i = 0; i < count; ++i)
                        q.bindValue(QString(":id%1").arg(i), selectedIds[offset + i]);

                    if (!q.exec()) {
                        qDebug() << "Batch delete failed for" << table << ":" << q.lastError().text();
                        return false;
                    }
                }
                return true;
            };

            const bool txSupported = db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions);
            bool txStarted = false;
            if (txSupported)
                txStarted = db.transaction();

            bool okDelete = deleteByIds("agv_task_history", "agv_id")
                         && deleteByIds("agv_tasks", "agv_id")
                         && deleteByIds("agv_list", "agv_id");

            if (!okDelete) {
                if (txStarted)
                    db.rollback();
                QMessageBox::warning(this, "Удаление AGV", "Не удалось удалить выбранные AGV.");
                return;
            }

            if (txStarted && !db.commit()) {
                db.rollback();
                QMessageBox::warning(this, "Удаление AGV", "Ошибка сохранения удаления.");
                return;
            }

            logAction(AppSession::currentUsername(),
                      "agv_deleted_batch",
                      QString("Пакетное удаление AGV: %1 шт.").arg(selectedIds.size()));

            QVector<AgvInfo> agvs = loadAgvList();
            rebuildList(agvs);
            emit agvListChanged();
            emit DataBus::instance().agvListChanged();
            emit DataBus::instance().calendarChanged();
            showUndoToast();
        }
    });
}
