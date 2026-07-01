#include "leftmenu.h"
#include "leftmenu/internal/calendar/leftmenu_calendar_utils.h"
#include "db_users.h"
#include "app_session.h"
#include "databus.h"
#include "diag_logger.h"

#include <QDebug>
#include <QDate>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTimer>

void leftMenu::destroyCalendarDayOverlay()
{
    if (!calendarDayOverlay_)
        return;
    calendarDayOverlay_->hide();
    delete calendarDayOverlay_;
    calendarDayOverlay_ = nullptr;
}

void leftMenu::hideCalendarDayOverlay()
{
    if (calendarDayOverlay_)
        calendarDayOverlay_->hide();
}
void leftMenu::changeMonth(int delta)
{
    int month = selectedMonth_ + delta;
    int year = selectedYear_;
    if (month < 1) { month = 12; year--; }
    if (month > 12) { month = 1; year++; }

    if (year < LeftMenuCalendar::minYear() || year > LeftMenuCalendar::maxYear())
        return;

    setSelectedMonthYear(month, year);
}

void leftMenu::refreshCalendarSelectionVisuals()
{
    if (!calendarTablePtr)
        return;

    for (int r = 1; r < calendarTablePtr->rowCount(); ++r) {
        for (int c = 0; c < calendarTablePtr->columnCount(); ++c) {
            QTableWidgetItem *item = calendarTablePtr->item(r, c);
            if (!item)
                continue;

            const QDate date = item->data(Qt::UserRole).toDate();
            if (!date.isValid()) {
                item->setData(Qt::UserRole + 5, false);
                continue;
            }

            bool isHighlighted = false;
            if (calendarHighlightActive_ && date.month() == selectedMonth_ && date.year() == selectedYear_) {
                if (highlightWeek_ && selectedWeek_ > 0) {
                    const int monthDays = QDate(selectedYear_, selectedMonth_, 1).daysInMonth();
                    const int startDay = 1 + (selectedWeek_ - 1) * 7;
                    const int endDay = (selectedWeek_ == 4) ? monthDays : qMin(startDay + 6, monthDays);
                    isHighlighted = (date.day() >= startDay && date.day() <= endDay);
                } else if (selectedDay_.isValid()) {
                    isHighlighted = (date == selectedDay_);
                }
            }
            item->setData(Qt::UserRole + 5, isHighlighted);
        }
    }

    calendarTablePtr->viewport()->update();
}

void leftMenu::clearCalendarSettingsHighlight()
{
    if (calendarHighlightTimer)
        calendarHighlightTimer->stop();
    calendarHighlightActive_ = false;
    highlightWeek_ = false;
    selectedWeek_ = 0;
    if (selectedYear_ >= LeftMenuCalendar::minYear() && selectedYear_ <= LeftMenuCalendar::maxYear()
        && selectedMonth_ >= 1 && selectedMonth_ <= 12)
        selectedDay_ = QDate(selectedYear_, selectedMonth_, 1);
    if (calendarTablePtr)
        refreshCalendarSelectionVisuals();
}

void leftMenu::setSelectedMonthYear(int month, int year)
{
    if (month < 1) { month = 12; year--; }
    if (month > 12) { month = 1; year++; }

    year = qBound(LeftMenuCalendar::minYear(), year, LeftMenuCalendar::maxYear());

    selectedMonth_ = month;
    selectedYear_  = year;

    if (!selectedDay_.isValid() ||
        selectedDay_.year()  != selectedYear_ ||
        selectedDay_.month() != selectedMonth_)
    {
        selectedDay_ = QDate(selectedYear_, selectedMonth_, 1);
    }

    const bool canLightRefresh =
        rightCalendarFrame && rightCalendarLayout_ && layout() && !stressSuiteRunning_;

    if (canLightRefresh) {
        if (!calendarStressDiagQuiet_) {
            techDiagLog(QStringLiteral("CALENDAR"),
                        QStringLiteral("setSelectedMonthYear %1/%2 — обновление сетки календаря")
                            .arg(selectedMonth_)
                            .arg(selectedYear_));
        }
        setUpdatesEnabled(false);
        refreshCalendarMonthLight();
        setUpdatesEnabled(true);
        update();
        return;
    }

    if (!calendarStressDiagQuiet_) {
        techDiagLog(QStringLiteral("CALENDAR"),
                    QStringLiteral("setSelectedMonthYear %1/%2 — полная пересборка UI")
                        .arg(selectedMonth_)
                        .arg(selectedYear_));
    }

    if (monthLabel)
        monthLabel->setText(monthYearLabelText(selectedMonth_, selectedYear_));

    setUpdatesEnabled(false);
    disconnect(&DataBus::instance(), nullptr, this, nullptr);

    destroyCalendarDayOverlay();

    // Полностью пересоздаём UI под новый месяц (без изменения scale).
    // Важно: setScaleFactor(scaleFactor_) тут не подходит, т.к. внутри стоит ранний return
    // при том же значении scaleFactor_, и сетка календаря не обновляется.
    if (QLayout *old = layout()) {
        QLayoutItem *item;
        while ((item = old->takeAt(0)) != nullptr) {
            if (QWidget *w = item->widget()) {
                w->setParent(nullptr);
                delete w;
            }
            delete item;
        }
        delete old;
    }

    topRow_ = nullptr;
    bottomRow_ = nullptr;
    rightCalendarFrame = nullptr;
    rightUpcomingMaintenanceFrame = nullptr;
    listAgvInfo = nullptr;
    agvSettingsPage = nullptr;
    modelListPage = nullptr;
    logsPage = nullptr;
    logsTable = nullptr;
    logsLoadAllBtn = nullptr;
    logsExportBtn = nullptr;
    calendarStressTestBtn_ = nullptr;
    fullStressAutotestBtn_ = nullptr;
    techDiagLogEdit_ = nullptr;
    setTechDiagLogSink(nullptr);
    profilePage = nullptr;
    chatsPage = nullptr;
    chatsStack_ = nullptr;
    embeddedChatWidget_ = nullptr;
    chatsListLayout_ = nullptr;
    calendarActionsFrame = nullptr;
    statusWidget_ = nullptr;
    calendarTablePtr = nullptr;
    agvCounter = nullptr;
    userButton = nullptr;
    searchEdit_ = nullptr;
    notifBadge_ = nullptr;
    logFilterUser_ = nullptr;
    logFilterSource_ = nullptr;
    logFilterCategory_ = nullptr;
    logFilterTime_ = nullptr;
    if (profileKeyTimer) { profileKeyTimer->stop(); profileKeyTimer->deleteLater(); profileKeyTimer = nullptr; }
    if (agvCounterTimer) { agvCounterTimer->stop(); agvCounterTimer->deleteLater(); agvCounterTimer = nullptr; }
    if (notifPollTimer) { notifPollTimer->stop(); notifPollTimer->deleteLater(); notifPollTimer = nullptr; }
    if (chatsPollTimer) { chatsPollTimer->stop(); chatsPollTimer->deleteLater(); chatsPollTimer = nullptr; }
    backButton = nullptr;
    monthLabel = nullptr;
    usersPage = nullptr;
    agvListDirty_ = true;

    initUI();
    restoreActivePage();
    setUpdatesEnabled(true);
    updateGeometry();
    update();
}


void leftMenu::selectDay(int year, int month, int day)
{
    const int dim = LeftMenuCalendar::daysInMonth(year, month);
    if (dim > 0)
        day = qBound(1, day, dim);
    selectedDay_ = QDate(year, month, day);
    if (!selectedDay_.isValid())
        return;
    if (year == selectedYear_ && month == selectedMonth_ && calendarTablePtr) {
        calendarHighlightActive_ = true;
        highlightWeek_ = false;
        selectedWeek_ = 0;
        refreshCalendarSelectionVisuals();
        return;
    }
    setSelectedMonthYear(month, year);
}

//
// ======================= ТЕКСТ МЕСЯЦА =======================
//

QString leftMenu::monthYearLabelText(int month, int year) const
{
    static QStringList months = {
        "Январь","Февраль","Март","Апрель","Май","Июнь",
        "Июль","Август","Сентябрь","Октябрь","Ноябрь","Декабрь"
    };

    return QString("%1 %2").arg(months[month - 1]).arg(year);
}
QVector<CalendarEvent> leftMenu::loadCalendarEvents(int month, int year)
{
    const QDate monthStart(year, month, 1);
    const QDate monthEnd = monthStart.addMonths(1).addDays(-1);
    return loadCalendarEventsRange(monthStart, monthEnd);
}

void leftMenu::invalidateCalendarEventsCache()
{
    calendarEventsCacheKey_.clear();
    calendarEventsCacheData_.clear();
    calendarEventsCacheFrom_ = QDate();
    calendarEventsCacheTo_ = QDate();
}

QVector<CalendarEvent> leftMenu::loadCalendarEventsRange(const QDate &from, const QDate &to)
{
    QVector<CalendarEvent> events;
    if (!from.isValid() || !to.isValid() || from > to)
        return events;

    const QString currentUser = AppSession::currentUsername();
    const QString curRole = getUserRole(currentUser);
    const QString cacheKey = QStringLiteral("%1|%2|%3|%4")
                                 .arg(from.toJulianDay())
                                 .arg(to.toJulianDay())
                                 .arg(currentUser)
                                 .arg(curRole);
    if (cacheKey == calendarEventsCacheKey_ && calendarEventsCacheFrom_ == from
        && calendarEventsCacheTo_ == to && !calendarEventsCacheData_.isEmpty()) {
        return calendarEventsCacheData_;
    }

    const QDate today = QDate::currentDate();

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        qDebug() << "loadCalendarEvents: DB NOT OPEN";
        return events;
    }

    QSqlQuery q(db);
    QSet<QString> completedToday;
    static QDate cachedCompletedDay;
    static QSet<QString> cachedCompletedToday;
    if (cachedCompletedDay != today) {
        cachedCompletedDay = today;
        cachedCompletedToday.clear();
        QSqlQuery qDoneToday(db);
        qDoneToday.prepare(R"(
            SELECT DISTINCT h.agv_id, h.task_name
            FROM agv_task_history h
            WHERE h.completed_at = :today
        )");
        qDoneToday.bindValue(":today", today);
        if (qDoneToday.exec()) {
            while (qDoneToday.next()) {
                const QString key = qDoneToday.value(0).toString().trimmed()
                                    + "||" +
                                    qDoneToday.value(1).toString().trimmed();
                cachedCompletedToday.insert(key);
            }
        }
    }
    completedToday = cachedCompletedToday;

    q.prepare(R"(
        SELECT t.agv_id, t.task_name, t.interval_days, t.next_date,
               a.assigned_user, t.assigned_to
        FROM agv_tasks t
        JOIN agv_list a ON a.agv_id = t.agv_id
        WHERE t.next_date IS NOT NULL
          AND a.status IN ('online', 'working')
          AND t.next_date BETWEEN :from AND :to
        ORDER BY t.next_date ASC
    )");
    q.bindValue(":from", from);
    q.bindValue(":to", to);
    if (!q.exec()) {
        qDebug() << "loadCalendarEvents SQL error:" << q.lastError().text();
        return events;
    }

    while (q.next()) {
        const QString agvId = q.value(0).toString();
        const QString taskName = q.value(1).toString();
        const QDate nextDate = q.value(3).toDate();
        QString assignedUser = q.value(4).toString().trimmed();
        QString assignedTo = q.value(5).toString().trimmed();
        if (!nextDate.isValid())
            continue;

        // Для viewer показываем:
        // - задачи, делегированные ему (assigned_to),
        // - задачи AGV, закрепленной за ним (assigned_user),
        // - общие задачи без назначения.
        if (curRole == "viewer") {
            const bool mineByTask = !assignedTo.isEmpty() && assignedTo == currentUser;
            const bool mineByAgv = !assignedUser.isEmpty() && assignedUser == currentUser;
            const bool isCommon = assignedTo.isEmpty() && assignedUser.isEmpty();
            if (!(mineByTask || mineByAgv || isCommon))
                continue;
        }

        const QString key = agvId.trimmed() + "||" + taskName.trimmed();
        if (completedToday.contains(key))
            continue;

        CalendarEvent ev;
        ev.agvId = agvId;
        ev.taskTitle = taskName;
        ev.date = nextDate;

        const int daysLeft = today.daysTo(nextDate);
        if (daysLeft < 0) ev.severity = "overdue";
        else if (daysLeft < 7) ev.severity = "soon";
        else ev.severity = "planned";
        events.push_back(ev);
    }

    QSqlQuery qHist(db);
    qHist.prepare(R"(
        SELECT h.agv_id, h.task_name, DATE(h.completed_at) AS completed_day, a.assigned_user
        FROM agv_task_history h
        JOIN agv_list a ON a.agv_id = h.agv_id
        WHERE h.completed_at IS NOT NULL
          AND a.status IN ('online', 'working')
          AND h.completed_at BETWEEN :from AND :to
        ORDER BY h.completed_at ASC
        LIMIT 2500
    )");
    qHist.bindValue(":from", from);
    qHist.bindValue(":to", to);
    if (qHist.exec()) {
        while (qHist.next()) {
            QString histAgvId = qHist.value(0).toString();
            QString histAssignedUser = qHist.value(3).toString().trimmed();
            if (curRole == "viewer" && !histAssignedUser.isEmpty() && histAssignedUser != currentUser)
                continue;

            CalendarEvent done;
            done.agvId = histAgvId;
            done.taskTitle = qHist.value(1).toString() + " (обслужена)";
            done.date = qHist.value(2).toDate();
            if (!done.date.isValid())
                continue;
            done.severity = "completed";
            events.push_back(done);
        }
    } else {
        // Таблица может отсутствовать в старых БД до первого проведения задачи.
        qDebug() << "loadCalendarEvents history query skipped:" << qHist.lastError().text();
    }

    calendarEventsCacheKey_ = cacheKey;
    calendarEventsCacheFrom_ = from;
    calendarEventsCacheTo_ = to;
    calendarEventsCacheData_ = events;
    return events;
}
