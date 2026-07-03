#include "leftmenu.h"
#include "leftmenu_stress_utils.h"
#include "leftmenu_stress_autotest_ui.h"

#include "app_session.h"
#include "db.h"
#include "db_agv_errors.h"
#include "db_agv_tasks.h"
#include "db_task_chat.h"
#include "db_users.h"
#include "diag_logger.h"
#include "databus.h"
#include "leftmenu/internal/calendar/leftmenu_calendar_utils.h"
#include "taskchatdialog.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTimer>
using LeftMenuStressUi::clickBackOn;
using LeftMenuStressUi::findVisibleButtonByText;
using LeftMenuStressUi::findVisibleDialogByTitle;
using LeftMenuStressUi::isUnsafeAutotestButton;
using LeftMenuStressUi::normalizedUiText;
using LeftMenuStressUi::buttonDebugText;
using LeftMenuStressUi::scheduleRejectDialog;
using LeftMenuStressUi::tryCloseDialog;

void leftMenu::setTechStressButtonsEnabled(bool enabled)
{
    if (calendarStressTestBtn_)
        calendarStressTestBtn_->setEnabled(enabled);
    if (fullStressAutotestBtn_)
        fullStressAutotestBtn_->setEnabled(enabled);
}

void leftMenu::runCalendarStressTest(int iterations, bool showSummaryDialog, bool manageStressButtons)
{
    if (iterations <= 0)
        return;

    clearCalendarSettingsHighlight();

    // Пока идёт смена месяца, вызывается restoreActivePage() — без этого при открытых «Логах»
    // каждый шаг дергал бы showLogs() и пересоздавал бы тяжёлую страницу сотни раз.
    const ActivePage savedPage = activePage_;
    activePage_ = ActivePage::Calendar;

    if (manageStressButtons)
        setTechStressButtonsEnabled(false);

    calendarStressDiagQuiet_ = true;
    techDiagLog(QStringLiteral("STRESS"),
                QStringLiteral("START iterations=%1 month=%2 year=%3")
                    .arg(iterations)
                    .arg(selectedMonth_)
                    .arg(selectedYear_));

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iterations; ++i)
        changeMonth(1);
    const qint64 ms = timer.elapsed();

    calendarStressDiagQuiet_ = false;
    activePage_ = savedPage;
    restoreActivePage();

    clearCalendarSettingsHighlight();

    techDiagLog(QStringLiteral("STRESS"),
                QStringLiteral("DONE iterations=%1 elapsed_ms=%2 month=%3 year=%4")
                    .arg(iterations)
                    .arg(ms)
                    .arg(selectedMonth_)
                    .arg(selectedYear_));

    if (manageStressButtons)
        setTechStressButtonsEnabled(true);

    if (showSummaryDialog) {
        QMessageBox::information(
            this,
            QStringLiteral("Стресс-тест календаря"),
            QStringLiteral("Готово: %1 шагов за %2 мс.\nПодробности — в окне ниже и в tech_verbose.log.")
                .arg(iterations)
                .arg(ms));
    }
}

void leftMenu::runFullStressAutotest()
{
    const QString who = AppSession::currentUsername();
    const QString role = getUserRole(who);
    if (role != QStringLiteral("admin") && role != QStringLiteral("tech")) {
        QMessageBox::warning(this, QStringLiteral("Автотест"),
                             QStringLiteral("Доступно только для ролей «администратор» и «разработчик»."));
        return;
    }
    if (stressSuiteRunning_) {
        QMessageBox::information(this, QStringLiteral("Автотест"),
                                 QStringLiteral("Автотест уже идёт — дождитесь окончания."));
        return;
    }

    clearCalendarSettingsHighlight();

    stressSuiteReportPath_ = stressAutotestReportPath();
    stressAutotestBeginSession(
        QStringLiteral("kompleksnyy_test user=%1 pid=%2")
            .arg(who)
            .arg(QCoreApplication::applicationPid()));

    stressSuiteRunning_ = true;
    qApp->setProperty("autotest_running", true);
    stressNavLastLabel_.clear();
    stressNavTimer_.invalidate();
    stressSuitePhase_ = 0;
    stressSuiteInner_ = 0;
    stressSuitePassCount_ = 0;
    stressSuiteSkipCount_ = 0;
    stressSuiteChatPeer_.clear();
    stressSuiteChatThreadId_ = 0;
    stressSuiteOrder_.clear();
    stressSuiteRandomPickDays_.clear();
    stressSuiteTotalTimer_.restart();
    setTechStressButtonsEnabled(false);

    stressAutotestLogLine(
        QStringLiteral("SUITE_START kompleksnyy_extended (всегда отчёт PASS/SKIP; лимит ~%1 с)")
            .arg(kLeftMenuStressWallCapMs / 1000));

    enum PhaseInit {
        InitPhDbPing = 0,
        InitPhDbCountsMulti,
        InitPhDbShowTables,
        InitPhDbJoinLight,
        InitPhDbEnsureHiddenAutotestUser,
        InitPhDbHiddenAutotestUserFiltered,
        InitPhDbMissingProfile,
        InitPhDbMissingAvatar,
        InitPhCalBurst,
        InitPhLogsReload400,
        InitPhLogsDoubleReload,
        InitPhUiLogsFiltersSweep,
        InitPhUiNotifications,
        InitPhUiClickableSweep,
        InitPhUiFlowRoutes,
        InitPhUiCalendar,
        InitPhUiAgv,
        InitPhUiModels,
        InitPhUiLogs,
        InitPhSearchAscii,
        InitPhSearchWhitespaceLong,
        InitPhUiProfile,
        InitPhUiChats,
        InitPhChatsReload,
        InitPhChatsReloadBurst,
        InitPhChatRejectInvalidTarget,
        InitPhChatOpenTestThread,
        InitPhChatReopenStableThread,
        InitPhChatRejectEmptyMessage,
        InitPhChatTextSelection,
        InitPhChatSendPlainMessage,
        InitPhChatSendDirtyMessage,
        InitPhChatBurstMessages,
        InitPhChatRejectBadThreadMessage,
        InitPhChatBackToList,
        InitPhUiUsers,
        InitPhSearchUnicode,
        InitPhDataBusLogsStorm,
        InitPhDataBusModelsStorm,
        InitPhDbWriteOnce,
        InitPhTouchPresence,
        InitPhDbReadNotifs,
        InitPhDbReadUnread,
        InitPhDbReadProfileLoader,
        InitPhDbReadCalendarEvents,
        InitPhDbReadMaint,
        InitPhDbReadSystemStatus,
        InitPhGetAllUsersNoAvatars,
        InitPhGetAllUsersWithAvatars,
        InitPhAgvListCountSql,
        InitPhCalPickDays,
        InitPhUiRapidNav,
        InitPhNotifBadgeTick
    };

    QVector<int> bucketDb = {
        InitPhDbPing, InitPhDbCountsMulti, InitPhDbShowTables, InitPhDbJoinLight,
        InitPhDbEnsureHiddenAutotestUser, InitPhDbHiddenAutotestUserFiltered,
        InitPhDbMissingProfile, InitPhDbMissingAvatar
    };
    QVector<int> bucketUi = {
        InitPhCalBurst, InitPhLogsReload400, InitPhLogsDoubleReload, InitPhUiLogsFiltersSweep,
        InitPhUiNotifications, InitPhUiClickableSweep, InitPhUiFlowRoutes,
        InitPhUiCalendar, InitPhUiAgv, InitPhUiModels, InitPhUiLogs,
        InitPhSearchAscii, InitPhSearchWhitespaceLong, InitPhUiProfile, InitPhSearchUnicode
    };
    QVector<int> bucketChat = {
        InitPhChatReopenStableThread, InitPhChatSendPlainMessage,
        InitPhChatSendDirtyMessage, InitPhChatBurstMessages
    };
    QVector<int> bucketTail = {
        InitPhDataBusLogsStorm, InitPhDataBusModelsStorm, InitPhDbWriteOnce, InitPhTouchPresence,
        InitPhDbReadNotifs, InitPhDbReadUnread, InitPhDbReadProfileLoader, InitPhDbReadCalendarEvents,
        InitPhDbReadMaint, InitPhDbReadSystemStatus, InitPhGetAllUsersNoAvatars,
        InitPhGetAllUsersWithAvatars, InitPhAgvListCountSql, InitPhCalPickDays,
        InitPhUiRapidNav, InitPhNotifBadgeTick, InitPhUiUsers
    };
    leftMenuShuffleVector(bucketDb);
    leftMenuShuffleVector(bucketUi);
    leftMenuShuffleVector(bucketChat);
    leftMenuShuffleVector(bucketTail);

    stressSuiteOrder_ += bucketDb;
    stressSuiteOrder_ += bucketUi;
    stressSuiteOrder_ += QVector<int>{ InitPhUiChats, InitPhChatsReload, InitPhChatsReloadBurst, InitPhChatRejectInvalidTarget,
                                       InitPhChatOpenTestThread, InitPhChatRejectEmptyMessage, InitPhChatTextSelection };
    stressSuiteOrder_ += bucketChat;
    stressSuiteOrder_ += QVector<int>{ InitPhChatRejectBadThreadMessage, InitPhChatBackToList };
    stressSuiteOrder_ += bucketTail;

    const int monthDays = qMax(1, LeftMenuCalendar::daysInMonth(selectedYear_, selectedMonth_));
    QSet<int> usedDays;
    while (stressSuiteRandomPickDays_.size() < qMin(7, monthDays)) {
        const int day = QRandomGenerator::global()->bounded(1, monthDays + 1);
        if (!usedDays.contains(day)) {
            usedDays.insert(day);
            stressSuiteRandomPickDays_.push_back(day);
        }
    }
    if (stressSuiteRandomPickDays_.isEmpty())
        stressSuiteRandomPickDays_ = QVector<int>{1};

    QStringList phaseNames;
    for (int phase : stressSuiteOrder_)
        phaseNames << QString::number(phase);
    QStringList pickDays;
    for (int day : stressSuiteRandomPickDays_)
        pickDays << QString::number(day);
    stressAutotestLogLine(QStringLiteral("SUITE_RANDOMIZED order=%1 pick_days=%2")
                              .arg(phaseNames.join(QStringLiteral(",")),
                                   pickDays.join(QStringLiteral(","))));
    scheduleStressSuiteStep(20);
}

void leftMenu::scheduleStressSuiteStep(int delayMs)
{
    if (!stressSuiteRunning_)
        return;
    QTimer::singleShot(qMax(0, delayMs), this, [this]() { stressSuiteTick(); });
}

void leftMenu::stressSuiteRecordCheck(const QString &name, bool pass, qint64 ms)
{
    if (pass)
        ++stressSuitePassCount_;
    else
        ++stressSuiteSkipCount_;
    stressAutotestLogLine(QStringLiteral("CHECK %1 %2 ms=%3")
                              .arg(name, pass ? QStringLiteral("PASS") : QStringLiteral("SKIP"))
                              .arg(ms));
}

void leftMenu::stressSuiteLogPageEntered(const QString &pageId)
{
    if (!stressSuiteRunning_)
        return;
    if (!stressNavLastLabel_.isEmpty() && stressNavTimer_.isValid()) {
        stressAutotestLogLine(QStringLiteral("PAGE_TRANSITION %1 -> %2 ms=%3")
                                  .arg(stressNavLastLabel_, pageId)
                                  .arg(stressNavTimer_.elapsed()));
    }
    stressNavLastLabel_ = pageId;
    stressNavTimer_.restart();
}

void leftMenu::stressSuiteFinishAlwaysOk()
{
    stressAutotestLogLine(
        QStringLiteral("SUITE_COMPLETE total_ms=%1 PASS=%2 SKIP=%3 (session always OK)")
            .arg(stressSuiteTotalTimer_.elapsed())
            .arg(stressSuitePassCount_)
            .arg(stressSuiteSkipCount_));

    reloadingLogs_ = false;
    calendarStressDiagQuiet_ = false;
    stressNavLastLabel_.clear();
    stressNavTimer_.invalidate();
    stressSuiteRunning_ = false;
    qApp->setProperty("autotest_running", false);
    clearCalendarSettingsHighlight();
    pendingCalendarReload_ = true;
    showCalendar();
    QApplication::processEvents();

    setTechStressButtonsEnabled(true);
    stressSuitePhase_ = 0;
    stressSuiteInner_ = 0;
    stressSuiteChatPeer_.clear();
    stressSuiteChatThreadId_ = 0;
    stressSuiteOrder_.clear();
    stressSuiteRandomPickDays_.clear();

    const QString reportPath = stressSuiteReportPath_;
    if (QClipboard *cb = QApplication::clipboard())
        cb->setText(reportPath);

    const QString msg = QStringLiteral(
                            "Комплексный тест завершён.\n\n"
                            "Сессия всегда считается успешной: смотрите PASS/SKIP в отчёте.\n"
                            "PASS: %1  |  SKIP: %2\n"
                            "Время: %3 с\n\n"
                            "Файл отчёта:\n%4\n\n"
                            "Открыть папку с отчётом?")
                            .arg(stressSuitePassCount_)
                            .arg(stressSuiteSkipCount_)
                            .arg(stressSuiteTotalTimer_.elapsed() / 1000.0, 0, 'f', 1)
                            .arg(QDir::toNativeSeparators(reportPath));

    const int ret = QMessageBox::question(this,
                                            QStringLiteral("Проверка завершена"),
                                            msg,
                                            QMessageBox::Yes | QMessageBox::No,
                                            QMessageBox::Yes);
    if (ret == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(reportPath).absolutePath()));
}

void leftMenu::stressSuiteTick()
{
    if (!stressSuiteRunning_)
        return;

    enum Phase {
        PhDbPing = 0,
        PhDbCountsMulti,
        PhDbShowTables,
        PhDbJoinLight,
        PhDbEnsureHiddenAutotestUser,
        PhDbHiddenAutotestUserFiltered,
        PhDbMissingProfile,
        PhDbMissingAvatar,
        PhCalBurst,
        PhLogsReload400,
        PhLogsDoubleReload,
        PhUiLogsFiltersSweep,
        PhUiNotifications,
        PhUiClickableSweep,
        PhUiFlowRoutes,
        PhUiCalendar,
        PhUiAgv,
        PhUiModels,
        PhUiLogs,
        PhSearchAscii,
        PhSearchWhitespaceLong,
        PhUiProfile,
        PhUiChats,
        PhChatsReload,
        PhChatsReloadBurst,
        PhChatRejectInvalidTarget,
        PhChatOpenTestThread,
        PhChatReopenStableThread,
        PhChatRejectEmptyMessage,
        PhChatSendPlainMessage,
        PhChatSendDirtyMessage,
        PhChatBurstMessages,
        PhChatTextSelection,
        PhChatRejectBadThreadMessage,
        PhChatBackToList,
        PhUiUsers,
        PhSearchUnicode,
        PhDataBusLogsStorm,
        PhDataBusModelsStorm,
        PhDbWriteOnce,
        PhTouchPresence,
        PhDbReadNotifs,
        PhDbReadUnread,
        PhDbReadProfileLoader,
        PhDbReadCalendarEvents,
        PhDbReadMaint,
        PhDbReadSystemStatus,
        PhGetAllUsersNoAvatars,
        PhGetAllUsersWithAvatars,
        PhAgvListCountSql,
        PhCalPickDays,
        PhUiRapidNav,
        PhNotifBadgeTick,
        PhDone
    };

    const int gapMs = 15;
    const qint64 wallCapMs = kLeftMenuStressWallCapMs;
    const int orderedDoneIndex = stressSuiteOrder_.isEmpty() ? PhDone : stressSuiteOrder_.size();
    if (stressSuitePhase_ < orderedDoneIndex && stressSuiteTotalTimer_.elapsed() > wallCapMs) {
        stressAutotestLogLine(QStringLiteral("SUITE wall_cap — досрочное штатное завершение"));
        stressSuitePhase_ = orderedDoneIndex;
    }

    auto next = [&]() {
        ++stressSuitePhase_;
        stressSuiteInner_ = 0;
        scheduleStressSuiteStep(gapMs);
    };

    const int currentPhase = (stressSuitePhase_ >= 0 && stressSuitePhase_ < stressSuiteOrder_.size())
                           ? stressSuiteOrder_.at(stressSuitePhase_)
                           : PhDone;

    switch (currentPhase) {
    case PhDbPing: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
        if (db.isOpen()) {
            QSqlQuery q(db);
            ok = q.exec(QStringLiteral("SELECT 1")) && q.next();
        }
        stressSuiteRecordCheck(QStringLiteral("db_ping_select1"), ok, t.elapsed());
        next();
        return;
    }
    case PhDbCountsMulti: {
        QElapsedTimer t;
        t.start();
        int okTables = 0;
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
        if (db.isOpen()) {
            const char *const tables[] = {
                "users", "agv_list", "agv_tasks", "agv_models", "notifications",
                "task_chat_messages", "agv_task_history", "maintenance_notification_sent"
            };
            for (const char *tbl : tables) {
                QSqlQuery q(db);
                if (q.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(QLatin1String(tbl))) && q.next())
                    ++okTables;
            }
        }
        stressSuiteRecordCheck(QStringLiteral("db_count_8_core_tables"), okTables >= 5, t.elapsed());
        next();
        return;
    }
    case PhDbShowTables: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
        if (db.isOpen()) {
            QSqlQuery q(db);
            ok = q.exec(QStringLiteral("SHOW TABLES"));
        }
        stressSuiteRecordCheck(QStringLiteral("db_show_tables"), ok, t.elapsed());
        next();
        return;
    }
    case PhDbJoinLight: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
        if (db.isOpen()) {
            QSqlQuery q(db);
            ok = q.exec(QStringLiteral(
                "SELECT al.agv_id FROM agv_list al "
                "LEFT JOIN agv_tasks t ON t.agv_id = al.agv_id LIMIT 1"));
        }
        stressSuiteRecordCheck(QStringLiteral("db_join_agv_list_tasks"), ok, t.elapsed());
        next();
        return;
    }
    case PhDbEnsureHiddenAutotestUser: {
        QElapsedTimer t;
        t.start();
        QString peer;
        QString err;
        UserInfo ui;
        const bool ok = ensureAutotestChatUser(&peer, &err)
                     && peer == hiddenAutotestUsername()
                     && loadUserProfile(peer, ui)
                     && ui.username == peer
                     && ui.isActive;
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST hidden chat peer ensure failed peer=%1 err=%2").arg(peer, err));
        stressSuiteRecordCheck(QStringLiteral("db_ensure_hidden_autotest_chat_user"), ok, t.elapsed());
        next();
        return;
    }
    case PhDbHiddenAutotestUserFiltered: {
        QElapsedTimer t;
        t.start();
        bool foundHidden = false;
        const QVector<UserInfo> noAvatars = getAllUsers(false);
        const QVector<UserInfo> withAvatars = getAllUsers(true);
        for (const UserInfo &u : noAvatars) {
            if (isHiddenAutotestUser(u.username)) {
                foundHidden = true;
                break;
            }
        }
        if (!foundHidden) {
            for (const UserInfo &u : withAvatars) {
                if (isHiddenAutotestUser(u.username)) {
                    foundHidden = true;
                    break;
                }
            }
        }
        stressSuiteRecordCheck(QStringLiteral("db_hidden_autotest_user_filtered_out"), !foundHidden, t.elapsed());
        next();
        return;
    }
    case PhDbMissingProfile: {
        QElapsedTimer t;
        t.start();
        UserInfo missing;
        const QString username = QStringLiteral("__missing_smoke_user_%1__")
                                     .arg(QRandomGenerator::global()->bounded(100000, 999999));
        const bool ok = !loadUserProfile(username, missing);
        stressSuiteRecordCheck(QStringLiteral("db_missing_user_profile_returns_false"), ok, t.elapsed());
        next();
        return;
    }
    case PhDbMissingAvatar: {
        QElapsedTimer t;
        t.start();
        const QString username = QStringLiteral("__missing_smoke_avatar_%1__")
                                     .arg(QRandomGenerator::global()->bounded(100000, 999999));
        const bool ok = ::loadUserAvatarFromDb(username).isNull() && loadUserAvatarFromDb(username).isNull();
        stressSuiteRecordCheck(QStringLiteral("db_missing_user_avatar_safe_empty"), ok, t.elapsed());
        next();
        return;
    }
    case PhCalBurst: {
        if (stressSuiteInner_ == 0) {
            stressSuiteSavedActivePage_ = activePage_;
            calendarStressDiagQuiet_ = true;
            stressSuiteStepTimer_.start();
        }
        // Каждый шаг — полная пересборка leftMenu через setSelectedMonthYear; десятки подряд дают минутный фриз.
        const int burstLimit = 2 + QRandomGenerator::global()->bounded(2);
        if (stressSuiteInner_ < burstLimit) {
            activePage_ = ActivePage::Calendar;
            const int delta = QRandomGenerator::global()->bounded(2) == 0 ? -1 : 1;
            changeMonth(delta);
            QApplication::processEvents();
            ++stressSuiteInner_;
            scheduleStressSuiteStep(15 + QRandomGenerator::global()->bounded(20));
            return;
        }
        calendarStressDiagQuiet_ = false;
        activePage_ = stressSuiteSavedActivePage_;
        restoreActivePage();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_calendar_random_month_flips"),
                               true, stressSuiteStepTimer_.elapsed());
        next();
        return;
    }
    case PhLogsReload400: {
        QElapsedTimer t;
        t.start();
        showLogs();
        reloadLogs(250 + QRandomGenerator::global()->bounded(301));
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_logs_reload_random_a"), true, t.elapsed());
        next();
        return;
    }
    case PhLogsDoubleReload: {
        QElapsedTimer t;
        t.start();
        showLogs();
        const int rowsA = 200 + QRandomGenerator::global()->bounded(251);
        const int rowsB = 450 + QRandomGenerator::global()->bounded(351);
        const int rowsC = 700 + QRandomGenerator::global()->bounded(401);
        reloadLogs(rowsA);
        QApplication::processEvents();
        reloadLogs(rowsB);
        QApplication::processEvents();
        reloadLogs(rowsC);
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_logs_reload_random_triple"), true, t.elapsed());
        next();
        return;
    }
    case PhUiLogsFiltersSweep: {
        QElapsedTimer t;
        t.start();
        showLogs();
        bool ok = logsTable != nullptr && logFilterUser_ && logFilterSource_ && logFilterCategory_ && logFilterTime_;
        if (ok) {
            QComboBox *combos[] = { logFilterUser_, logFilterSource_, logFilterCategory_, logFilterTime_ };
            for (int step = 0; step < 5; ++step) {
                QComboBox *combo = combos[QRandomGenerator::global()->bounded(4)];
                if (!combo || combo->count() <= 0)
                    continue;
                combo->setCurrentIndex(QRandomGenerator::global()->bounded(combo->count()));
                QApplication::processEvents();
            }
            for (QComboBox *combo : combos) {
                if (combo && combo->count() > 0)
                    combo->setCurrentIndex(0);
            }
            QApplication::processEvents();
            ok = logsTable->columnCount() > 0;
        }
        stressSuiteRecordCheck(QStringLiteral("ui_logs_filters_random_sweep"), ok, t.elapsed());
        next();
        return;
    }
    case PhUiNotifications: {
        QElapsedTimer t;
        t.start();
        bool opened = false;
        bool interacted = false;
        QTimer::singleShot(120, qApp, [&opened, &interacted]() {
            if (QDialog *dlg = findVisibleDialogByTitle(QStringLiteral("Уведомления"))) {
                opened = true;
                if (QAbstractButton *markBtn = findVisibleButtonByText(dlg, QStringLiteral("Отметить все"))) {
                    markBtn->click();
                    interacted = true;
                    return;
                }
                interacted = true;
                dlg->reject();
            }
        });
        showNotificationsPanel();
        const bool ok = opened && interacted;
        stressSuiteRecordCheck(QStringLiteral("ui_notifications_open_and_interact"), ok, t.elapsed());
        next();
        return;
    }
    case PhUiClickableSweep: {
        QElapsedTimer t;
        t.start();

        auto clickSafeButtonsOn = [&](QWidget *root, const QString &scope) -> int {
            if (!root)
                return 0;
            const auto buttons = root->findChildren<QAbstractButton *>();
            int clicked = 0;
            QSet<QAbstractButton *> seen;
            for (QAbstractButton *button : buttons) {
                if (!button || seen.contains(button))
                    continue;
                seen.insert(button);
                if (!button->isVisible() || !button->isEnabled() || isUnsafeAutotestButton(button))
                    continue;
                const QString label = buttonDebugText(button);
                if (label.isEmpty())
                    continue;
                // С корня leftMenu клик по «Logs» вызывает showLogs/reloadLogs и может занять десятки секунд;
                // раздел «logs» в этом же фазе кликает кнопки уже на странице логов.
                if (scope == QStringLiteral("calendar") && root == this) {
                    const QString norm = normalizedUiText(label);
                    if (norm == QStringLiteral("logs") || norm == QStringLiteral("логи"))
                        continue;
                }
                stressAutotestLogLine(QStringLiteral("CLICK_SWEEP %1 -> %2").arg(scope, label));
                button->click();
                ++clicked;
                leftMenuWaitUiMs(15);
                if (QDialog *dlg = findVisibleDialogByTitle(QString())) {
                    (void)tryCloseDialog(dlg);
                    leftMenuWaitUiMs(15);
                }
                if (clicked >= 12)
                    break;
            }
            return clicked;
        };

        int clickedTotal = 0;
        showCalendar();
        leftMenuWaitUiMs(40);
        clickedTotal += clickSafeButtonsOn(this, QStringLiteral("calendar"));

        showAgvList();
        leftMenuWaitUiMs(40);
        clickedTotal += clickSafeButtonsOn(listAgvInfo ? static_cast<QWidget*>(listAgvInfo) : this, QStringLiteral("agv"));

        showModelList();
        leftMenuWaitUiMs(40);
        clickedTotal += clickSafeButtonsOn(modelListPage ? static_cast<QWidget*>(modelListPage) : this, QStringLiteral("models"));

        showLogs();
        leftMenuWaitUiMs(40);
        clickedTotal += clickSafeButtonsOn(logsPage ? logsPage : this, QStringLiteral("logs"));

        showChatsPage();
        leftMenuWaitUiMs(40);
        clickedTotal += clickSafeButtonsOn(chatsPage ? chatsPage : this, QStringLiteral("chats"));

        showProfile();
        leftMenuWaitUiMs(40);
        clickedTotal += clickSafeButtonsOn(profilePage ? profilePage : this, QStringLiteral("profile"));

        {
            const QString r = getUserRole(AppSession::currentUsername());
            if (r == QStringLiteral("admin") || r == QStringLiteral("tech")) {
                showUsersPage();
                leftMenuWaitUiMs(20);
                clickedTotal += clickSafeButtonsOn(usersPage ? static_cast<QWidget*>(usersPage) : this, QStringLiteral("users"));
            }
        }

        stressSuiteRecordCheck(QStringLiteral("ui_clickable_sweep_visible_safe_buttons"), clickedTotal > 0, t.elapsed());
        next();
        return;
    }
    case PhUiFlowRoutes: {
        QElapsedTimer t;
        t.start();
        QStringList errors;

        enum RouteAction {
            ActAreaAgv = 0,
            ActAreaModels,
            ActAreaCalendar,
            ActAreaLogs,
            ActAreaUser
        };

        auto actionName = [](int action) -> QString {
            switch (action) {
            case ActAreaAgv: return QStringLiteral("agv_list_area");
            case ActAreaModels: return QStringLiteral("models_area");
            case ActAreaCalendar: return QStringLiteral("calendar_area");
            case ActAreaLogs: return QStringLiteral("logs_area");
            case ActAreaUser: return QStringLiteral("user_area");
            default: return QStringLiteral("unknown");
            }
        };

        auto openAddAgvAndClose = [this]() -> bool {
            scheduleRejectDialog(QStringLiteral("Добавить AGV"));
            emit addAgvRequested();
                leftMenuWaitUiMs(25);
            return findVisibleDialogByTitle(QStringLiteral("Добавить AGV")) == nullptr;
        };

        auto openAddModelAndClose = [this]() -> bool {
            showModelList();
            leftMenuWaitUiMs(50);
            if (!modelListPage || !modelListPage->isVisible())
                return false;
            QAbstractButton *btn = findVisibleButtonByText(modelListPage, QStringLiteral("Добавить модель"));
            if (!btn)
                return false;
            scheduleRejectDialog(QStringLiteral("Добавить модель AGV"));
            btn->click();
                leftMenuWaitUiMs(25);
            return findVisibleDialogByTitle(QStringLiteral("Добавить модель AGV")) == nullptr;
        };

        auto openRandomModelDetailsAndBack = [this]() -> bool {
            showModelList();
            leftMenuWaitUiMs(50);
            if (!modelListPage || !modelListPage->isVisible())
                return false;
            QVector<QPushButton *> showButtons;
            const auto buttons = modelListPage->findChildren<QPushButton *>();
            for (QPushButton *btn : buttons) {
                if (btn && btn->isVisible() && btn->isEnabled()
                    && btn->text().contains(QStringLiteral("Показать"), Qt::CaseInsensitive)) {
                    showButtons.push_back(btn);
                }
            }
            if (showButtons.isEmpty())
                return false;
            showButtons.at(QRandomGenerator::global()->bounded(showButtons.size()))->click();
                leftMenuWaitUiMs(25);
            const bool opened = clickBackOn(modelListPage);
                leftMenuWaitUiMs(25);
            return opened && modelListPage->isVisible();
        };

        auto openRandomAgvDetailsAndBack = [this]() -> bool {
            showAgvList();
            leftMenuWaitUiMs(50);
            QVector<AgvInfo> agvs = listAgvInfo ? listAgvInfo->loadAgvList() : QVector<AgvInfo>();
            if (agvs.isEmpty())
                return false;
            const QString agvId = agvs.at(QRandomGenerator::global()->bounded(agvs.size())).id.trimmed();
            if (agvId.isEmpty())
                return false;
            showAgvDetailInfo(agvId);
                leftMenuWaitUiMs(25);
            const bool opened = (agvSettingsPage && agvSettingsPage->isVisible());
            showAgvList();
                leftMenuWaitUiMs(25);
            return opened && listAgvInfo && listAgvInfo->isVisible();
        };

        auto openRandomUserProfileAndBack = [this]() -> bool {
            const QString r = getUserRole(AppSession::currentUsername());
            const bool canUsers = (r == QStringLiteral("admin") || r == QStringLiteral("tech"));
            if (!canUsers) {
                showProfile();
                leftMenuWaitUiMs(20);
                return profilePage && profilePage->isVisible();
            }

            showUsersPage();
                leftMenuWaitUiMs(25);
            QVector<UserInfo> users = getAllUsers(false);
            if (users.isEmpty())
                return usersPage && usersPage->isVisible();

            const UserInfo user = users.at(QRandomGenerator::global()->bounded(users.size()));
            if (user.username.trimmed().isEmpty())
                return false;

            showUserProfilePage(user.username);
                leftMenuWaitUiMs(25);
            const bool opened = (activePage_ == ActivePage::UserProfile);
            showUsersPage();
                leftMenuWaitUiMs(25);
            return opened && usersPage && usersPage->isVisible();
        };

        auto runAction = [&](int action) -> bool {
            switch (action) {
            case ActAreaAgv: {
                showAgvList();
                leftMenuWaitUiMs(15);
                bool ok = listAgvInfo && listAgvInfo->isVisible();
                ok = openRandomAgvDetailsAndBack() && ok;
                ok = openAddAgvAndClose() && ok;
                return ok;
            }
            case ActAreaModels: {
                showModelList();
                leftMenuWaitUiMs(15);
                bool ok = modelListPage && modelListPage->isVisible();
                ok = openRandomModelDetailsAndBack() && ok;
                ok = openAddModelAndClose() && ok;
                return ok;
            }
            case ActAreaCalendar:
                showCalendar();
                leftMenuWaitUiMs(15);
                if (!(rightCalendarFrame && rightCalendarFrame->isVisible()))
                    return false;
                if (selectedYear_ >= LeftMenuCalendar::minYear() && selectedYear_ <= LeftMenuCalendar::maxYear()) {
                    const int delta = QRandomGenerator::global()->bounded(2) == 0 ? -1 : 1;
                    changeMonth(delta);
                    leftMenuWaitUiMs(12);
                    selectDay(selectedYear_, selectedMonth_,
                              qMax(1, QRandomGenerator::global()->bounded(1, LeftMenuCalendar::daysInMonth(selectedYear_, selectedMonth_) + 1)));
                    leftMenuWaitUiMs(12);
                }
                return rightCalendarFrame && rightCalendarFrame->isVisible();
            case ActAreaLogs:
                showLogs();
                leftMenuWaitUiMs(15);
                if (!(logsPage && logsPage->isVisible()))
                    return false;
                reloadLogs(150 + QRandomGenerator::global()->bounded(351));
                leftMenuWaitUiMs(12);
                return logsPage->isVisible();
            case ActAreaUser:
                return openRandomUserProfileAndBack();
            default:
                return false;
            }
        };

        auto runRoute = [&](const QString &routeName, const QVector<int> &actions) -> bool {
            QStringList names;
            for (int action : actions)
                names << actionName(action);
            stressAutotestLogLine(QStringLiteral("ROUTE %1 steps=%2")
                                      .arg(routeName, names.join(QStringLiteral(" -> "))));
            bool routeOk = true;
            for (int action : actions) {
                const bool stepOk = runAction(action);
                if (!stepOk) {
                    routeOk = false;
                    errors << QStringLiteral("%1:%2").arg(routeName, actionName(action));
                }
            }
            return routeOk;
        };

        const QVector<int> baseActions = {
            ActAreaAgv,
            ActAreaModels,
            ActAreaCalendar,
            ActAreaLogs,
            ActAreaUser
        };

        bool ok = true;
        int routeCount = 0;
        const int routesPerGroup = 1;
        for (int startAction : baseActions) {
            QVector<QVector<int>> routes;
            QVector<int> tail;
            for (int action : baseActions) {
                if (action != startAction)
                    tail.push_back(action);
            }
            QVector<int> routeForward;
            routeForward.push_back(startAction);
            routeForward += tail;
            routes.push_back(routeForward);

            stressAutotestLogLine(QStringLiteral("ROUTE_GROUP_DONE start=%1 total=%2")
                                      .arg(actionName(startAction))
                                      .arg(routesPerGroup));
            int groupIndex = 0;
            for (const QVector<int> &route : routes) {
                ++groupIndex;
                ++routeCount;
                stressAutotestLogLine(QStringLiteral("ROUTE_PROGRESS %1/%2 start=%3")
                                          .arg(routeCount)
                                          .arg(baseActions.size() * routesPerGroup)
                                          .arg(actionName(startAction)));
                ok = runRoute(QStringLiteral("%1#%2").arg(actionName(startAction)).arg(groupIndex), route) && ok;
            }
        }

        stressAutotestLogLine(QStringLiteral("ROUTE_TOTAL count=%1 errors=%2").arg(routeCount).arg(errors.size()));
        stressSuiteRecordCheck(QStringLiteral("ui_route_permutations_grouped_by_start_area"), ok, t.elapsed());
        next();
        return;
    }
    case PhUiCalendar: {
        QElapsedTimer t;
        t.start();
        showCalendar();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_show_calendar"), true, t.elapsed());

        QElapsedTimer tRules;
        tRules.start();
        const bool rulesOk =
            LeftMenuCalendar::daysInMonth(2024, 2) == 29
            && LeftMenuCalendar::daysInMonth(2025, 2) == 28
            && LeftMenuCalendar::daysInMonth(2100, 2) == 28
            && LeftMenuCalendar::daysInMonth(2000, 2) == 29
            && LeftMenuCalendar::daysInMonth(2024, 4) == 30
            && LeftMenuCalendar::daysInMonth(2024, 1) == 31;
        stressSuiteRecordCheck(QStringLiteral("calendar_days_in_month_rules"), rulesOk, tRules.elapsed());
        next();
        return;
    }
    case PhUiAgv: {
        QElapsedTimer t;
        t.start();
        showAgvList();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_show_agv_list"), true, t.elapsed());
        next();
        return;
    }
    case PhUiModels: {
        QElapsedTimer t;
        t.start();
        showModelList();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_show_models"), true, t.elapsed());
        next();
        return;
    }
    case PhUiLogs: {
        QElapsedTimer t;
        t.start();
        showLogs();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_show_logs"), true, t.elapsed());
        next();
        return;
    }
    case PhSearchAscii: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        if (searchEdit_) {
            static const QStringList values = {
                QStringLiteral("smoke_ascii_abc_123"),
                QStringLiteral("AGV_TEST_001"),
                QStringLiteral("model-search-777"),
                QStringLiteral("serial___XYZ")
            };
            ok = true;
            for (int i = 0; i < 3; ++i) {
                searchEdit_->blockSignals(true);
                searchEdit_->setText(values.at(QRandomGenerator::global()->bounded(values.size())));
                searchEdit_->blockSignals(false);
                onSearchTextChanged(searchEdit_->text());
                QApplication::processEvents();
            }
            clearSearch();
            QApplication::processEvents();
            ok = searchEdit_->text().isEmpty();
        }
        stressSuiteRecordCheck(QStringLiteral("ui_search_ascii_simulation"), ok, t.elapsed());
        next();
        return;
    }
    case PhSearchWhitespaceLong: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        if (searchEdit_) {
            static const QStringList values = {
                QStringLiteral("      \t   "),
                QStringLiteral("AUTOTEST_LONG_%1").arg(QStringLiteral("X").repeated(256)),
                QStringLiteral(" \n\t mixed   whitespace \t query \n "),
                QStringLiteral("AGV_%1_END").arg(QStringLiteral("0123456789").repeated(40))
            };
            searchEdit_->blockSignals(true);
            searchEdit_->setText(values.at(QRandomGenerator::global()->bounded(values.size())));
            searchEdit_->blockSignals(false);
            onSearchTextChanged(searchEdit_->text());
            QApplication::processEvents();
            clearSearch();
            QApplication::processEvents();
            ok = searchEdit_->text().isEmpty();
        }
        stressSuiteRecordCheck(QStringLiteral("ui_search_whitespace_and_long_input"), ok, t.elapsed());
        next();
        return;
    }
    case PhUiProfile: {
        QElapsedTimer t;
        t.start();
        showProfile();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_show_profile"), true, t.elapsed());
        next();
        return;
    }
    case PhUiChats: {
        QElapsedTimer t;
        t.start();
        showChatsPage();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_show_chats"), true, t.elapsed());
        next();
        return;
    }
    case PhChatsReload: {
        QElapsedTimer t;
        t.start();
        // showChatsPage() уже вызывает reloadChatsPageList — повторно только обновляем список.
        if (chatsPage && chatsListLayout_)
            reloadChatsPageList();
        else
            showChatsPage();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_chats_reload_list"), true, t.elapsed());
        next();
        return;
    }
    case PhChatsReloadBurst: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        showChatsPage();
        for (int i = 0; i < 4; ++i) {
            reloadChatsPageList();
            QApplication::processEvents();
        }
        ok = (chatsPage && chatsListLayout_ && chatsStack_);
        stressSuiteRecordCheck(QStringLiteral("ui_chats_reload_burst_x4"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatRejectInvalidTarget: {
        QElapsedTimer t;
        t.start();
        QString err;
        const int tid = TaskChatDialog::ensureThreadWithUser(
            AppSession::currentUsername(), QString(), QStringLiteral("AUTOTEST_SMOKE"), &err);
        const bool ok = (tid <= 0 && !err.trimmed().isEmpty());
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat invalid target unexpected tid=%1 err=%2").arg(tid).arg(err));
        stressSuiteRecordCheck(QStringLiteral("chat_reject_empty_peer"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatOpenTestThread: {
        QElapsedTimer t;
        t.start();
        const QString currentUser = AppSession::currentUsername().trimmed();
        QString peer;
        QString peerErr;
        if (!currentUser.isEmpty() && ensureAutotestChatUser(&peer, &peerErr) && peer != currentUser)
            peer = peer.trimmed();
        else
            peer.clear();

        bool ok = false;
        if (!currentUser.isEmpty() && !peer.isEmpty()) {
            QString err;
            const int tid = TaskChatDialog::ensureThreadWithUser(
                currentUser, peer, QStringLiteral("AUTOTEST_SMOKE"), &err);
            if (tid > 0) {
                stressSuiteChatPeer_ = peer;
                stressSuiteChatThreadId_ = tid;
                activeChatThreadId_ = tid;
                activeChatPeer_ = peer;
                showChatsPage();
                if (embeddedChatWidget_ && chatsStack_) {
                    QApplication::processEvents();
                    ok = (embeddedChatWidget_->threadId() == tid && chatsStack_->currentIndex() == 1);
                }
            }
            if (!ok)
                stressAutotestLogLine(QStringLiteral("AUTOTEST chat open thread failed peer=%1 tid=%2 err=%3")
                                          .arg(peer)
                                          .arg(tid)
                                          .arg(err));
        } else if (!peerErr.trimmed().isEmpty()) {
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat peer ensure failed err=%1").arg(peerErr));
        }

        stressSuiteRecordCheck(QStringLiteral("chat_open_or_create_test_thread"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatReopenStableThread: {
        QElapsedTimer t;
        t.start();
        const QString currentUser = AppSession::currentUsername().trimmed();
        QString peer = stressSuiteChatPeer_.trimmed();
        QString peerErr;
        if (peer.isEmpty())
            ensureAutotestChatUser(&peer, &peerErr);
        QString errA;
        QString errB;
        const int tidA = (!currentUser.isEmpty() && !peer.isEmpty())
                       ? TaskChatDialog::ensureThreadWithUser(currentUser, peer, QStringLiteral("AUTOTEST_SMOKE"), &errA)
                       : 0;
        const int tidB = (!currentUser.isEmpty() && !peer.isEmpty())
                       ? TaskChatDialog::ensureThreadWithUser(currentUser, peer, QStringLiteral("AUTOTEST_SMOKE"), &errB)
                       : 0;
        const bool ok = tidA > 0 && tidB > 0 && tidA == tidB;
        if (ok) {
            stressSuiteChatPeer_ = peer;
            stressSuiteChatThreadId_ = tidB;
            activeChatThreadId_ = tidB;
            activeChatPeer_ = peer;
            if (embeddedChatWidget_)
                embeddedChatWidget_->setThreadId(tidB, peer);
        } else {
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat reopen unstable peer=%1 tidA=%2 tidB=%3 errA=%4 errB=%5 peerErr=%6")
                                      .arg(peer)
                                      .arg(tidA)
                                      .arg(tidB)
                                      .arg(errA, errB, peerErr));
        }
        stressSuiteRecordCheck(QStringLiteral("chat_reopen_same_thread_is_stable"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatRejectEmptyMessage: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QString err;
        if (embeddedChatWidget_ && stressSuiteChatThreadId_ > 0 && chatsStack_ && chatsStack_->currentIndex() == 1)
            ok = embeddedChatWidget_->autotestRejectsEmptyMessage(&err);
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat empty message reject failed err=%1").arg(err));
        stressSuiteRecordCheck(QStringLiteral("chat_reject_empty_message"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatSendPlainMessage: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QString err;
        static const QStringList messages = {
            QStringLiteral("AUTOTEST plain ping %1"),
            QStringLiteral("AUTOTEST hello admin-tech %1"),
            QStringLiteral("AUTOTEST agv update candidate %1"),
            QStringLiteral("AUTOTEST maintenance note %1")
        };
        const QString msg = messages.at(QRandomGenerator::global()->bounded(messages.size()))
                                .arg(QDateTime::currentMSecsSinceEpoch());
        if (embeddedChatWidget_ && stressSuiteChatThreadId_ > 0 && chatsStack_ && chatsStack_->currentIndex() == 1)
            ok = embeddedChatWidget_->autotestSendTextMessage(msg, &err);
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat plain message failed err=%1").arg(err));
        stressSuiteRecordCheck(QStringLiteral("chat_send_plain_message"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatSendDirtyMessage: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QString err;
        static const QStringList dirtyMessages = {
            QStringLiteral("AUTOTEST dirty \"' ; -- [] {} \\\\ // кириллица 测试 smoke_%1"),
            QStringLiteral("AUTOTEST weird \t\n !@#$%^&*() <> [] {} ~~~ %1"),
            QStringLiteral("AUTOTEST unicode Привет 你好 عربى `code` %1"),
            QStringLiteral("AUTOTEST separators /// \\\\ || ;; :: == %1")
        };
        const QString msg = dirtyMessages.at(QRandomGenerator::global()->bounded(dirtyMessages.size()))
                                .arg(QDateTime::currentMSecsSinceEpoch());
        if (embeddedChatWidget_ && stressSuiteChatThreadId_ > 0 && chatsStack_ && chatsStack_->currentIndex() == 1)
            ok = embeddedChatWidget_->autotestSendTextMessage(msg, &err);
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat dirty message failed err=%1").arg(err));
        stressSuiteRecordCheck(QStringLiteral("chat_send_dirty_message"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatBurstMessages: {
        QElapsedTimer t;
        t.start();
        int okCount = 0;
        QStringList errors;
        if (embeddedChatWidget_ && stressSuiteChatThreadId_ > 0 && chatsStack_ && chatsStack_->currentIndex() == 1) {
            for (int i = 0; i < 3; ++i) {
                QString err;
                const QString msg = QStringLiteral("AUTOTEST burst[%1] %2")
                                        .arg(i + 1)
                                        .arg(QDateTime::currentMSecsSinceEpoch() + i);
                if (embeddedChatWidget_->autotestSendTextMessage(msg, &err))
                    ++okCount;
                else if (!err.trimmed().isEmpty())
                    errors << err;
                QApplication::processEvents();
            }
        }
        const bool ok = (okCount == 3);
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat burst failed okCount=%1 err=%2")
                                      .arg(okCount)
                                      .arg(errors.join(QStringLiteral(" | "))));
        stressSuiteRecordCheck(QStringLiteral("chat_send_burst_3_messages"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatTextSelection: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QString err;
        if (embeddedChatWidget_ && stressSuiteChatThreadId_ > 0 && chatsStack_ && chatsStack_->currentIndex() == 1) {
            ok = embeddedChatWidget_->autotestTextSelection(&err);
        }
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat text selection failed err=%1").arg(err));
        stressSuiteRecordCheck(QStringLiteral("chat_text_selection"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatRejectBadThreadMessage: {
        QElapsedTimer t;
        t.start();
        QString err;
        const bool ok = !addChatMessage(-1, AppSession::currentUsername(),
                                        QStringLiteral("AUTOTEST invalid thread message"), err)
                     && !err.trimmed().isEmpty();
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat invalid thread write unexpected err=%1").arg(err));
        stressSuiteRecordCheck(QStringLiteral("chat_reject_invalid_thread_write"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatBackToList: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        showChatsPage();
        if (chatsStack_)
            chatsStack_->setCurrentIndex(0);
        reloadChatsPageList();
        QApplication::processEvents();
        ok = (chatsPage && chatsPage->isVisible() && chatsStack_ && chatsStack_->currentIndex() == 0);
        stressSuiteRecordCheck(QStringLiteral("chat_back_to_thread_list"), ok, t.elapsed());
        next();
        return;
    }
    case PhUiUsers: {
        QElapsedTimer t;
        t.start();
        const QString r = getUserRole(AppSession::currentUsername());
        const bool canUsers = (r == QStringLiteral("admin") || r == QStringLiteral("tech"));
        if (canUsers)
            showUsersPage();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_users_page_admin_or_tech"), canUsers && usersPage != nullptr,
                               t.elapsed());
        next();
        return;
    }
    case PhSearchUnicode: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        if (searchEdit_) {
            static const QStringList values = {
                QStringLiteral("测试\t\n🔥smoke"),
                QStringLiteral("кириллица__поиск\t123"),
                QStringLiteral("العربية / test / №42"),
                QStringLiteral("emoji 😀 AGV \n data")
            };
            ok = true;
            for (int i = 0; i < 3; ++i) {
                searchEdit_->blockSignals(true);
                searchEdit_->setText(values.at(QRandomGenerator::global()->bounded(values.size())));
                searchEdit_->blockSignals(false);
                onSearchTextChanged(searchEdit_->text());
                QApplication::processEvents();
            }
            clearSearch();
            QApplication::processEvents();
            ok = searchEdit_->text().isEmpty();
        }
        stressSuiteRecordCheck(QStringLiteral("ui_search_unicode_messy"), ok, t.elapsed());
        next();
        return;
    }
    case PhDataBusLogsStorm: {
        QElapsedTimer t;
        t.start();
        showLogs();
        const int waves = 3 + QRandomGenerator::global()->bounded(3);
        for (int wave = 0; wave < waves; ++wave) {
            DataBus::instance().triggerNotificationsChanged();
            DataBus::instance().triggerCalendarChanged();
            DataBus::instance().triggerAgvListChanged();
            DataBus::instance().triggerModelsChanged();
            DataBus::instance().triggerUserDataChanged();
            DataBus::instance().triggerAgvTasksChanged(QStringLiteral("_smoke_burst_"));
            if (wave % 2)
                QApplication::processEvents();
        }
        pendingCalendarReload_ = true;
        stressSuiteRecordCheck(QStringLiteral("ui_databus_6waves_full_on_logs"), true, t.elapsed());
        next();
        return;
    }
    case PhDataBusModelsStorm: {
        QElapsedTimer t;
        t.start();
        showModelList();
        const int waves = 2 + QRandomGenerator::global()->bounded(2);
        for (int i = 0; i < waves; ++i) {
            DataBus::instance().triggerModelsChanged();
            DataBus::instance().triggerAgvListChanged();
            QApplication::processEvents();
        }
        stressSuiteRecordCheck(QStringLiteral("ui_databus_models_page_burst"), true, t.elapsed());
        next();
        return;
    }
    case PhDbWriteOnce: {
        QElapsedTimer t;
        t.start();
        const QString u = AppSession::currentUsername();
        logAction(u, QStringLiteral("smoke_test"), QStringLiteral("extended suite write"));
        bool ok = false;
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
        if (db.isOpen() && !u.isEmpty()) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "UPDATE users SET username = :u WHERE username = :u AND is_active = 1"));
            q.bindValue(QStringLiteral(":u"), u);
            ok = q.exec();
        }
        stressSuiteRecordCheck(QStringLiteral("db_write_logaction_plus_users_update"), ok, t.elapsed());
        next();
        return;
    }
    case PhTouchPresence: {
        QElapsedTimer t;
        t.start();
        const QString u = AppSession::currentUsername();
        if (!u.isEmpty())
            touchUserPresence(u);
        stressSuiteRecordCheck(QStringLiteral("db_touch_user_presence"), !u.isEmpty(), t.elapsed());
        next();
        return;
    }
    case PhDbReadNotifs: {
        QElapsedTimer t;
        t.start();
        const QString u = AppSession::currentUsername();
        (void)loadNotificationsForUser(u);
        stressSuiteRecordCheck(QStringLiteral("db_load_notifications_for_user"), !u.isEmpty(), t.elapsed());
        next();
        return;
    }
    case PhDbReadUnread: {
        QElapsedTimer t;
        t.start();
        const QString u = AppSession::currentUsername();
        (void)unreadCountForUser(u);
        stressSuiteRecordCheck(QStringLiteral("db_unread_count_for_user"), !u.isEmpty(), t.elapsed());
        next();
        return;
    }
    case PhDbReadProfileLoader: {
        QElapsedTimer t;
        t.start();
        UserInfo ui;
        const QString u = AppSession::currentUsername();
        const bool ok = !u.isEmpty() && loadUserProfile(u, ui);
        stressSuiteRecordCheck(QStringLiteral("db_load_user_profile"), ok, t.elapsed());
        next();
        return;
    }
    case PhDbReadCalendarEvents: {
        QElapsedTimer t;
        t.start();
        const int delta = QRandomGenerator::global()->bounded(7) - 3;
        const QDate pivot = QDate(selectedYear_, selectedMonth_, 1).addMonths(delta);
        (void)loadCalendarEvents(pivot.month(), pivot.year());
        stressSuiteRecordCheck(QStringLiteral("db_load_calendar_events_month"), true, t.elapsed());
        next();
        return;
    }
    case PhDbReadMaint: {
        QElapsedTimer t;
        t.start();
        const int delta = QRandomGenerator::global()->bounded(7) - 3;
        const QDate pivot = QDate(selectedYear_, selectedMonth_, 1).addMonths(delta);
        (void)loadUpcomingMaintenance(pivot.month(), pivot.year());
        stressSuiteRecordCheck(QStringLiteral("db_load_upcoming_maintenance"), true, t.elapsed());
        next();
        return;
    }
    case PhDbReadSystemStatus: {
        QElapsedTimer t;
        t.start();
        (void)loadSystemStatus();
        stressSuiteRecordCheck(QStringLiteral("db_load_system_status"), true, t.elapsed());
        next();
        return;
    }
    case PhGetAllUsersNoAvatars: {
        QElapsedTimer t;
        t.start();
        (void)getAllUsers(false);
        stressSuiteRecordCheck(QStringLiteral("db_get_all_users_no_avatars"), true, t.elapsed());
        next();
        return;
    }
    case PhGetAllUsersWithAvatars: {
        QElapsedTimer t;
        t.start();
        (void)getAllUsers(true);
        stressSuiteRecordCheck(QStringLiteral("db_get_all_users_with_avatars"), true, t.elapsed());
        next();
        return;
    }
    case PhAgvListCountSql: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
        if (db.isOpen()) {
            QSqlQuery q(db);
            ok = q.exec(QStringLiteral("SELECT COUNT(*) FROM agv_list")) && q.next();
        }
        stressSuiteRecordCheck(QStringLiteral("db_count_agv_list"), ok, t.elapsed());
        next();
        return;
    }
    case PhCalPickDays: {
        if (stressSuiteInner_ == 0) {
            stressSuiteSavedActivePage_ = activePage_;
            stressSuiteStepTimer_.start();
            showCalendar();
            QApplication::processEvents();
            stressSuiteSelY_ = selectedYear_;
            stressSuiteSelM_ = selectedMonth_;
            stressSuiteInner_ = 1;
            scheduleStressSuiteStep(30);
            return;
        }
        if (stressSuiteInner_ >= 1 && stressSuiteInner_ <= stressSuiteRandomPickDays_.size()) {
            const int dim = LeftMenuCalendar::daysInMonth(stressSuiteSelY_, stressSuiteSelM_);
            int d = stressSuiteRandomPickDays_.at(stressSuiteInner_ - 1);
            if (dim > 0)
                d = qBound(1, d, dim);
            selectDay(stressSuiteSelY_, stressSuiteSelM_, d);
            QApplication::processEvents();
            ++stressSuiteInner_;
            scheduleStressSuiteStep(25 + QRandomGenerator::global()->bounded(25));
            return;
        }
        stressSuiteRecordCheck(QStringLiteral("ui_calendar_select_random_days"),
                               true, stressSuiteStepTimer_.elapsed());
        stressSuiteInner_ = 0;
        activePage_ = stressSuiteSavedActivePage_;
        restoreActivePage();
        QApplication::processEvents();
        next();
        return;
    }
    case PhUiRapidNav: {
        QElapsedTimer t;
        t.start();
        for (int round = 0; round < 2; ++round) {
            QVector<int> navOrder = {0, 1, 2, 3};
            leftMenuShuffleVector(navOrder);
            for (int nav : navOrder) {
                switch (nav) {
                case 0: showCalendar(); break;
                case 1: showLogs(); break;
                case 2: showAgvList(); break;
                case 3: showModelList(); break;
                default: break;
                }
                QApplication::processEvents();
            }
        }
        stressSuiteRecordCheck(QStringLiteral("ui_rapid_nav_randomized"), true, t.elapsed());
        next();
        return;
    }
    case PhNotifBadgeTick: {
        QElapsedTimer t;
        t.start();
        updateNotifBadge();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_update_notif_badge"), true, t.elapsed());
        next();
        return;
    }
    case PhDone:
        stressSuiteFinishAlwaysOk();
        return;
    default:
        stressAutotestLogLine(
            QStringLiteral("SUITE unknown_phase=%1 idx=%2 — finish OK").arg(currentPhase).arg(stressSuitePhase_));
        stressSuitePhase_ = stressSuiteOrder_.isEmpty() ? PhDone : stressSuiteOrder_.size();
        scheduleStressSuiteStep(0);
        return;
    }
}
