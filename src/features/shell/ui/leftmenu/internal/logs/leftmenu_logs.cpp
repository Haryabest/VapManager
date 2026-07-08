#include "leftmenu.h"

#include "app_session.h"
#include "db.h"
#include "db_bench.h"
#include "db_users.h"
#include "diag_logger.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSqlDatabase>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
void leftMenu::showLogs()
{
    // Только для пропуска тяжёлого reloadLogs; hideAllPages() нужен ВСЕГДА — иначе после
    // стресс-теста календаря (когда показывали только календарь) при restore на «Логи»
    // activePage_ уже Logs, и мы бы пропустили hideAllPages: календарь+ТО остались бы видимыми.
    const bool skipHeavyReload = (activePage_ == ActivePage::Logs)
        && logsTable && logsTable->rowCount() > 0
        && !logsStale_;

    activePage_ = ActivePage::Logs;
    hideAllPages();
    clearSearch();
    if (logsPage)
        logsPage->setVisible(true);

    // Повторный клик по «Логи», когда страница уже открыта: не гоняем reloadLogs
    // (чтение большого app.log + тысячи setItem в GUI — выглядит как зависание).
    if (skipHeavyReload) {
        stressSuiteLogPageEntered(QStringLiteral("logs"));
        return;
    }

    // При быстрой навигации/автотесте не перечитываем лог снова и снова без паузы.
    const bool lightLogNav = stressSuiteRunning_
                          || (qApp && qApp->property("autotest_running").toBool());
    const int logDebounceMs = lightLogNav ? 4500 : 1200;
    const bool recentReload = lastLogsReloadTimer_.isValid()
                           && lastLogsReloadTimer_.elapsed() < logDebounceMs;
    const bool hasRenderedRows = logsTable && logsTable->rowCount() > 0;
    if (!logsStale_ && recentReload && hasRenderedRows) {
        stressSuiteLogPageEntered(QStringLiteral("logs"));
        return;
    }

    // После тяжёлых сценариев reloadLogs мог не дойти до конца — снимаем залипание.
    reloadingLogs_ = false;
    logsStale_ = false;
    reloadLogs(lastLogsMaxRows_);
    stressSuiteLogPageEntered(QStringLiteral("logs"));
}


static const QStringList AGV_ACTIONS = {
    "agv_created", "agv_deleted", "agv_task_completed", "agv_tasks_copied",
    "model_created", "model_deleted", "agv_restore_batch", "agv_deleted_batch"
};

void leftMenu::reloadLogs(int maxRows)
{
    if (!logsTable) {
        reloadingLogs_ = false;
        return;
    }
    if (reloadingLogs_) {
        logsReloadPending_ = true;
        lastLogsMaxRows_ = maxRows;
        return;
    }
    reloadingLogs_ = true;
    logsReloadPending_ = false;
    lastLogsMaxRows_ = maxRows;

    struct LogsReloadGuard {
        leftMenu *menu;
        explicit LogsReloadGuard(leftMenu *m) : menu(m) {}
        ~LogsReloadGuard()
        {
            leftMenu *const target = menu;
            target->reloadingLogs_ = false;
            if (target->logsReloadPending_) {
                target->logsReloadPending_ = false;
                QTimer::singleShot(0, target, [target]() {
                    target->reloadLogs(target->lastLogsMaxRows_);
                });
            }
        }
    } guard(this);

    struct Row { QString time, source, user, category, details; };
    QVector<Row> rows;

    QSet<QString> uniqueUsers, uniqueSources, uniqueCategories;

    QString filterUser = logFilterUser_ ? logFilterUser_->currentData().toString() : "";
    if (filterUser.isEmpty()) {
        const QString me = AppSession::currentUsername().trimmed();
        if (!me.isEmpty())
            filterUser = me;
    }
    QString filterSource = logFilterSource_ ? logFilterSource_->currentData().toString() : "";
    QString filterCategory = logFilterCategory_ ? logFilterCategory_->currentData().toString() : "";
    QString filterTime = logFilterTime_ ? logFilterTime_->currentData().toString() : "";

    const bool autotestLight = qApp && qApp->property("autotest_running").toBool();
    int maxRowsEff = (maxRows <= 0) ? 2000 : maxRows;
    if (autotestLight)
        maxRowsEff = qMin(maxRowsEff, 400);
    const int MAX_ROWS = maxRowsEff;
    const qint64 tailCapBytes = autotestLight ? (512LL * 1024) : (2LL * 1024 * 1024);

    QString logPath = localLogFilePath();
    if (!QFile::exists(logPath)) {
        const QString oldLogPath = QCoreApplication::applicationDirPath() + "/logs/app.log";
        if (QFile::exists(oldLogPath))
            logPath = oldLogPath;
    }
    uniqueSources.insert("Локально");

    QStringList lines;
    {
        QFile f(logPath);
        // Раньше читали файл целиком даже для «последних 2000 строк» — при большом app.log UI зависал на минуты.
        if (f.open(QIODevice::ReadOnly)) {
            const qint64 sz = f.size();
            if (sz <= 0) {
                f.close();
            } else if (MAX_ROWS <= 0) {
                // Режим «все строки»: всё равно ограничиваем хвостом, иначе можно убить UI.
                if (sz > tailCapBytes && f.seek(sz - tailCapBytes)) {
                    QByteArray raw = f.readAll();
                    const int cut = raw.indexOf('\n');
                    if (cut >= 0)
                        raw = raw.mid(cut + 1);
                    lines = QString::fromUtf8(raw).split(QLatin1Char('\n'));
                } else {
                    f.seek(0);
        QTextStream in(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                    in.setCodec("UTF-8");
#endif
                    while (!in.atEnd())
                        lines.append(in.readLine());
                }
                f.close();
            } else if (sz > tailCapBytes && f.seek(sz - tailCapBytes)) {
                QByteArray raw = f.readAll();
                const int cut = raw.indexOf('\n');
                if (cut >= 0)
                    raw = raw.mid(cut + 1);
                lines = QString::fromUtf8(raw).split(QLatin1Char('\n'));
                f.close();
            } else {
                f.seek(0);
                QTextStream in(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                in.setCodec("UTF-8");
#endif
        while (!in.atEnd()) {
                    lines.append(in.readLine());
                    if (lines.size() > MAX_ROWS * 2)
                        lines.removeFirst();
                }
                f.close();
            }
        }
    }

    // Страховка от гигантского split (редкие длинные строки / мусор)
    int lineCap = qMax(MAX_ROWS * 2, 2000);
    if (autotestLight)
        lineCap = qMin(lineCap, 4000);
    if (lines.size() > lineCap)
        lines = lines.mid(lines.size() - lineCap);

    {
            int start = (MAX_ROWS > 0) ? 0 : qMax(0, lines.size() - MAX_ROWS);
            const int maxLinesToProcess = qMin(lines.size(), MAX_ROWS * 2);
            int processedCount = 0;
            for (int i = lines.size() - 1; i >= start && rows.size() < MAX_ROWS && processedCount < maxLinesToProcess; --i) {
                ++processedCount;
                const QString line = lines[i].trimmed();
                if (line.isEmpty()) continue;

            int brOpen = line.indexOf('[');
            int brClose = line.indexOf(']');
            int dash = line.indexOf(" - ");

            Row r;
                r.source = "Локально";
            r.time = (line.size() >= 19) ? line.left(19) : "";
                if (brOpen >= 0 && brClose > brOpen)
                r.user = line.mid(brOpen + 1, brClose - brOpen - 1);
            if (brClose >= 0 && dash > brClose) {
                r.category = line.mid(brClose + 2, dash - (brClose + 2)).trimmed();
                r.details = line.mid(dash + 3).trimmed();
            } else {
                r.details = line;
            }
                if (!filterUser.isEmpty() && r.user != filterUser) continue;
                if (!filterSource.isEmpty() && r.source != filterSource) continue;
                if (!filterCategory.isEmpty()) {
                    if (filterCategory == "agv_actions" && !AGV_ACTIONS.contains(r.category)) continue;
                    else if (filterCategory != "agv_actions" && r.category != filterCategory) continue;
                }
                if (!filterTime.isEmpty()) {
                    QDateTime dt = QDateTime::fromString(r.time, "yyyy-MM-dd hh:mm:ss");
                    QDate today = QDate::currentDate();
                    if (filterTime == "today" && dt.date() != today) continue;
                    if (filterTime == "week" && dt.date() < today.addDays(-7)) continue;
                    if (filterTime == "month" && dt.date() < today.addDays(-30)) continue;
            }
            rows.push_back(r);
                uniqueUsers.insert(r.user);
                uniqueSources.insert(r.source);
                uniqueCategories.insert(r.category);
            }
    }

    auto populateCombo = [](QComboBox *cb, const QSet<QString> &values) {
        if (!cb) return;
        QString cur = cb->currentData().toString();
        cb->blockSignals(true);
        while (cb->count() > 1) cb->removeItem(1);
        QStringList sorted = values.values();
        sorted.sort();
        for (const QString &v : sorted) {
            if (!v.isEmpty())
                cb->addItem(v, v);
        }
        int idx = cb->findData(cur);
        if (idx >= 0) cb->setCurrentIndex(idx);
        cb->blockSignals(false);
    };
    populateCombo(logFilterUser_, uniqueUsers);
    populateCombo(logFilterSource_, uniqueSources);

    if (logFilterCategory_) {
        QString cur = logFilterCategory_->currentData().toString();
        logFilterCategory_->blockSignals(true);
        while (logFilterCategory_->count() > 1) logFilterCategory_->removeItem(1);
        logFilterCategory_->addItem("Действия с AGV/моделями", "agv_actions");
        QStringList sorted = uniqueCategories.values();
        sorted.sort();
        for (const QString &v : sorted) {
            if (!v.isEmpty() && v != "agv_actions")
                logFilterCategory_->addItem(v, v);
        }
        int idx = logFilterCategory_->findData(cur);
        if (idx >= 0) logFilterCategory_->setCurrentIndex(idx);
        logFilterCategory_->blockSignals(false);
    }

    if (logFilterUser_) {
        int defIdx = logFilterUser_->findData(AppSession::currentUsername());
        if (defIdx >= 0 && logFilterUser_->currentData().toString().isEmpty()) {
            logFilterUser_->blockSignals(true);
            logFilterUser_->setCurrentIndex(defIdx);
            logFilterUser_->blockSignals(false);
        }
    }

    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b){
        return a.time > b.time;
    });

    logsTable->setUpdatesEnabled(false);
    logsTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QString values[5] = {
            rows[i].time,
            rows[i].source,
            rows[i].user,
            rows[i].category,
            rows[i].details
        };
        for (int col = 0; col < 5; ++col) {
            QTableWidgetItem *item = logsTable->item(i, col);
            if (!item) {
                item = new QTableWidgetItem();
                logsTable->setItem(i, col, item);
            }
            item->setText(values[col]);
        }
    }
    logsTable->setUpdatesEnabled(true);
    logsTable->viewport()->update();
    lastLogsReloadTimer_.restart();
}

void leftMenu::runDatabaseBenchTest()
{
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen()) {
        QString err;
        if (!connectToDB(&err)) {
            QMessageBox::warning(this,
                                 QStringLiteral("Тест базы"),
                                 QStringLiteral("Нет подключения к PostgreSQL.\n%1")
                                     .arg(err.isEmpty() ? QStringLiteral("Проверьте config.ini.") : err));
            return;
        }
        db = QSqlDatabase::database(QStringLiteral("main_connection"));
    }

    const int iterations = 30;
    const QDateTime started = QDateTime::currentDateTime();

    techDiagLog(QStringLiteral("DB_BENCH"),
                QStringLiteral("START iterations=%1 host=%2 db=%3")
                    .arg(iterations)
                    .arg(db.hostName())
                    .arg(db.databaseName()));

    QApplication::setOverrideCursor(Qt::WaitCursor);
    int failedScenarios = 0;
    const QString reportBody = runDatabaseBenchReport(iterations, &failedScenarios);
    QApplication::restoreOverrideCursor();

    if (reportBody.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("Тест базы"),
                             QStringLiteral("Не удалось выполнить тест: подключение к БД недоступно."));
        techDiagLog(QStringLiteral("DB_BENCH"), QStringLiteral("FAIL no connection"));
        return;
    }

    QString report = QStringLiteral("=== Тест базы ===\n");
    report += QStringLiteral("time:   %1\n").arg(started.toString(QStringLiteral("dd.MM.yyyy hh:mm:ss")));
    report += QStringLiteral("user:   %1\n").arg(AppSession::currentUsername());
    report += QStringLiteral("host:   %1:%2\n").arg(db.hostName()).arg(db.port());
    report += QStringLiteral("db:     %1\n").arg(db.databaseName());
    report += QStringLiteral("iter:   %1 на сценарий\n\n").arg(iterations);
    report += reportBody;

    QString reportPath;
    const QString logsDir = localLogsDirPath();
    if (QDir().mkpath(logsDir)) {
        reportPath = logsDir + QStringLiteral("/db_bench_%1.txt")
                                 .arg(started.toString(QStringLiteral("yyyyMMdd_HHmmss")));
        QFile outFile(reportPath);
        if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&outFile);
            out.setCodec("UTF-8");
            out << report;
            outFile.close();
        } else {
            reportPath.clear();
        }
    }

    techDiagLog(QStringLiteral("DB_BENCH"),
                QStringLiteral("DONE fails=%1 elapsed_ms=%2 file=%3")
                    .arg(failedScenarios)
                    .arg(started.msecsTo(QDateTime::currentDateTime()))
                    .arg(reportPath.isEmpty() ? QStringLiteral("-") : reportPath));

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Тест базы"));
    dlg.setMinimumSize(s(640), s(520));
    dlg.setStyleSheet(QStringLiteral(
        "QDialog{background:#F6F8FC;border:1px solid #DCE2EE;border-radius:14px;}"));

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(s(16), s(16), s(16), s(16));
    root->setSpacing(s(10));

    if (!reportPath.isEmpty()) {
        QLabel *pathLbl = new QLabel(
            QStringLiteral("Отчёт сохранён:\n%1").arg(QDir::toNativeSeparators(reportPath)), &dlg);
        pathLbl->setWordWrap(true);
        pathLbl->setStyleSheet(QStringLiteral(
            "font-family:Inter;font-size:13px;color:#334155;background:#E0F2FE;"
            "border:1px solid #BAE6FD;border-radius:8px;padding:8px;"));
        root->addWidget(pathLbl);
    } else {
        QLabel *pathLbl = new QLabel(QStringLiteral("Не удалось сохранить файл отчёта."), &dlg);
        pathLbl->setStyleSheet(QStringLiteral(
            "font-family:Inter;font-size:13px;color:#B91C1C;background:#FEE2E2;"
            "border:1px solid #FECACA;border-radius:8px;padding:8px;"));
        root->addWidget(pathLbl);
    }

    QTextEdit *view = new QTextEdit(&dlg);
    view->setReadOnly(true);
    view->setFontFamily(QStringLiteral("Consolas"));
    view->setPlainText(report);
    view->setStyleSheet(QStringLiteral(
        "QTextEdit{background:#FFFFFF;border:1px solid #E2E8F0;border-radius:8px;padding:8px;}"));
    root->addWidget(view, 1);

    QHBoxLayout *btns = new QHBoxLayout();
    btns->addStretch();
    if (!reportPath.isEmpty()) {
        QPushButton *openDirBtn = new QPushButton(QStringLiteral("Открыть папку"), &dlg);
        openDirBtn->setFixedSize(s(150), s(40));
        openDirBtn->setStyleSheet(QString(
            "QPushButton{background:#0891B2;color:white;font-family:Inter;font-size:%1px;"
            "font-weight:800;border-radius:%2px;}"
            "QPushButton:hover{background:#0E7490;}"
        ).arg(s(14)).arg(s(8)));
        connect(openDirBtn, &QPushButton::clicked, &dlg, [reportPath]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(reportPath).absolutePath()));
        });
        btns->addWidget(openDirBtn);
    }
    QPushButton *closeBtn = new QPushButton(QStringLiteral("Закрыть"), &dlg);
    closeBtn->setFixedSize(s(120), s(40));
    closeBtn->setStyleSheet(QString(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;font-size:%1px;"
        "font-weight:800;border-radius:%2px;}"
        "QPushButton:hover{background:#1A4ACD;}"
    ).arg(s(14)).arg(s(8)));
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btns->addWidget(closeBtn);
    root->addLayout(btns);

    dlg.exec();
}
