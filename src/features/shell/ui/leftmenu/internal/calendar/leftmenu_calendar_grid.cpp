#include "leftmenu.h"
#include "leftmenu_calendar_utils.h"
#include "db_users.h"
#include "app_session.h"
#include <QScrollArea>
#include <QTimer>
#include <algorithm>

void leftMenu::updateCalendarNavButtons()
{
    if (!prevMonthBtn_ || !nextMonthBtn_)
        return;
    const bool atMin = (selectedYear_ == LeftMenuCalendar::minYear() && selectedMonth_ == 1);
    const bool atMax = (selectedYear_ == LeftMenuCalendar::maxYear() && selectedMonth_ == 12);
    prevMonthBtn_->setEnabled(!atMin);
    nextMonthBtn_->setEnabled(!atMax);
}

void leftMenu::refreshCalendarMonthLight()
{
    if (monthLabel)
        monthLabel->setText(monthYearLabelText(selectedMonth_, selectedYear_));
    updateCalendarNavButtons();
    if (calendarDayOverlay_) {
        destroyCalendarDayOverlay();
    }
    if (calendarTablePtr && rightCalendarLayout_) {
        rightCalendarLayout_->removeWidget(calendarTablePtr);
        delete calendarTablePtr;
        calendarTablePtr = nullptr;
    }
    buildCalendarTable();
}

void leftMenu::buildCalendarTable()
{
    if (!rightCalendarFrame || !rightCalendarLayout_)
        return;

    ++calendarLoadGeneration_;

    QDate firstDay(selectedYear_, selectedMonth_, 1);
    int daysInMonth = firstDay.daysInMonth();
    // dayOfWeek(): Monday=1 ... Sunday=7; у нас колонки тоже Пн..Вс
    int startCol = firstDay.dayOfWeek() - 1;

    // Жесткое ограничение: максимум 35 видимых дней (5 строк по 7 дней).
    // Чтобы месяц всегда помещался, уменьшаем количество "предыдущих" дней при необходимости.
    const int maxVisibleDays = 35;
    const int maxLeadingAllowed = qMax(0, maxVisibleDays - daysInMonth);
    const int leadingCells = qMin(startCol, maxLeadingAllowed);
    const int usedWithoutTail = leadingCells + daysInMonth;
    int tail = qMax(0, maxVisibleDays - usedWithoutTail);

    int calendarRows = 5;
    int totalRows = calendarRows + 1;

    QTableWidget *calendarTable = new QTableWidget(totalRows, 7, rightCalendarFrame);
    calendarTablePtr = calendarTable;

    // таблица не растягивается сверх нужного
    calendarTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

    calendarTable->setStyleSheet(
        "QTableWidget { border:none; }"
        "QTableWidget::item { border:none; padding:3px; }"
    );

    calendarTable->horizontalHeader()->setVisible(false);
    calendarTable->verticalHeader()->setVisible(false);
    calendarTable->setShowGrid(false);
    calendarTable->setSelectionMode(QAbstractItemView::NoSelection);
    calendarTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    calendarTable->setFocusPolicy(Qt::NoFocus);
    calendarTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    calendarTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    calendarTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // ===== ВСЕ ЯЧЕЙКИ ОДИНАКОВОГО РАЗМЕРА =====
    int cellH = s(71);
    int headerH = s(52);

    calendarTable->verticalHeader()->setDefaultSectionSize(cellH);
    calendarTable->verticalHeader()->setMinimumSectionSize(cellH);

    calendarTable->setRowHeight(0, headerH);

    for (int r = 1; r < totalRows; r++)
        calendarTable->setRowHeight(r, cellH);

    //
    // ===== ДНИ НЕДЕЛИ (фон 555555, текст 222222) =====
    //
    QStringList weekdaysList = {
        "Понедельник",
        "Вторник",
        "Среда",
        "Четверг",
        "Пятница",
        "Суббота",
        "Воскресенье"
    };

    for (int c = 0; c < 7; c++) {
        QTableWidgetItem *item = new QTableWidgetItem(weekdaysList[c]);
        item->setTextAlignment(Qt::AlignCenter);
        item->setFont(QFont("Inter", s(17), QFont::Bold));

        item->setForeground(QBrush(QColor("#222222")));
        item->setBackground(QBrush(QColor("#555555")));

        calendarTable->setItem(0, c, item);
    }

    //
    // ===== ЗАПОЛНЕНИЕ КАЛЕНДАРЯ =====
    //

    // строка 1 — первая строка с числами
    int row = 1;
    int col = 0;

    // ===== ХВОСТ ПРЕДЫДУЩЕГО МЕСЯЦА =====
    QDate prevMonth = firstDay.addMonths(-1);
    int prevDays = prevMonth.daysInMonth();
    QDate nextMonth = firstDay.addMonths(1);

    int prevCount = leadingCells;
    int tailStart = prevDays - prevCount + 1;

    // старые дни — на 10% темнее: #A5A5A5
    QColor oldMonthColor("#A5A5A5");

    QMap<QDate, QTableWidgetItem*> visibleDateItems;

    for (int i = 0; i < prevCount; ++i) {
        const int d = tailStart + i;
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(d));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        item->setFont(QFont("Inter", s(14), QFont::Bold));
        item->setForeground(QBrush(oldMonthColor));
        const QDate itemDate(prevMonth.year(), prevMonth.month(), d);
        item->setData(Qt::UserRole, itemDate);
        item->setData(Qt::UserRole + 1, QStringList());
        item->setData(Qt::UserRole + 2, QStringList());
        item->setData(Qt::UserRole + 5, false);
        calendarTable->setItem(row, col, item);
        visibleDateItems[itemDate] = item;
        col++;
        if (col > 6) { col = 0; row++; }
    }

    // ===== ТЕКУЩИЙ МЕСЯЦ =====
    row = 1;
    col = startCol;

    QMap<int, QTableWidgetItem*> currentMonthItems;
    for (int d = 1; d <= daysInMonth; d++) {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(d));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        item->setFont(QFont("Inter", s(14), QFont::Bold));
        item->setForeground(QBrush(QColor("#000000")));
        item->setData(Qt::UserRole, QDate(selectedYear_, selectedMonth_, d));
        item->setData(Qt::UserRole + 1, QStringList());
        item->setData(Qt::UserRole + 2, QStringList());
        item->setData(Qt::UserRole + 5, false);
        calendarTable->setItem(row, col, item);
        currentMonthItems[d] = item;
        visibleDateItems[QDate(selectedYear_, selectedMonth_, d)] = item;

        col++;
        if (col > 6) { col = 0; row++; }
    }

    // ===== ХВОСТ СЛЕДУЮЩЕГО МЕСЯЦА =====
    int nextDay = 1;

    // новые дни — тоже на 10% темнее: #A5A5A5
    QColor nextMonthColor("#A5A5A5");

    for (int i = 0; i < tail; i++) {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(nextDay));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        item->setFont(QFont("Inter", s(14), QFont::Bold));
        item->setForeground(QBrush(nextMonthColor));
        const QDate itemDate(nextMonth.year(), nextMonth.month(), nextDay);
        item->setData(Qt::UserRole, itemDate);
        item->setData(Qt::UserRole + 1, QStringList());
        item->setData(Qt::UserRole + 2, QStringList());
        item->setData(Qt::UserRole + 5, false);
        calendarTable->setItem(row, col, item);
        visibleDateItems[itemDate] = item;

        col++;
        if (col > 6) { col = 0; row++; }
        nextDay++;
    }

    calendarVisibleCells_ = visibleDateItems;
    QDate visibleFrom;
    QDate visibleTo;
    for (auto it = visibleDateItems.constBegin(); it != visibleDateItems.constEnd(); ++it) {
        const QDate d = it.key();
        if (!d.isValid())
            continue;
        if (!visibleFrom.isValid() || d < visibleFrom)
            visibleFrom = d;
        if (!visibleTo.isValid() || d > visibleTo)
            visibleTo = d;
    }
    if (!visibleFrom.isValid() || !visibleTo.isValid()) {
        visibleFrom = firstDay;
        visibleTo = firstDay.addDays(daysInMonth - 1);
    }

    calendarEventsByDate_.clear();
    for (auto it = visibleDateItems.constBegin(); it != visibleDateItems.constEnd(); ++it) {
        const QDate date = it.key();
        QTableWidgetItem *item = it.value();
        if (!item || !date.isValid())
            continue;

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
        item->setData(Qt::UserRole + 1, QStringList());
        item->setData(Qt::UserRole + 2, QStringList());
        item->setData(Qt::UserRole + 10, QStringList());
        item->setData(Qt::UserRole + 11, QStringList());
        item->setData(Qt::UserRole + 5, isHighlighted);
    }

    //
    // ===== ДЕЛЕГАТ (ТОЛЬКО ВНУТРЕННИЕ ЛИНИИ) =====
    //
    class CalendarDelegate : public QStyledItemDelegate {
    public:
        void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
            QStyledItemDelegate::paint(p, opt, idx);

            const QDate d = idx.data(Qt::UserRole).toDate();
            if (d.isValid()) {
                const bool isHighlighted = idx.data(Qt::UserRole + 5).toBool();
                if (isHighlighted) {
                    p->save();
                    p->setRenderHint(QPainter::Antialiasing, true);
                    p->setRenderHint(QPainter::TextAntialiasing, true);

                    const QString dayText = idx.data(Qt::DisplayRole).toString();
                    QFont badgeFont("Inter", 10, QFont::Black);
                    p->setFont(badgeFont);
                    QFontMetrics badgeFm(badgeFont);
                    const int badgeW = qMax(30, badgeFm.horizontalAdvance(dayText) + 16);
                    const QRect badgeRect(opt.rect.left() + 4, opt.rect.top() + 2, badgeW, 24);

                    p->setBrush(Qt::NoBrush);
                    p->setPen(QPen(QColor("#1976FF"), 2));
                    p->drawRoundedRect(badgeRect, 9, 9);
                    p->restore();
                }

                const QStringList previewLines = idx.data(Qt::UserRole + 1).toStringList();
                const QStringList previewSeverities = idx.data(Qt::UserRole + 2).toStringList();
                if (!previewLines.isEmpty()) {
                    p->save();
                    p->setRenderHint(QPainter::Antialiasing, true);
                    p->setRenderHint(QPainter::TextAntialiasing, true);
                    p->setPen(QColor("#4B5563"));
                    QFont small("Inter", 9, QFont::DemiBold);
                    p->setFont(small);
                    QFontMetrics fm(small);
                    QRect r = opt.rect;
                    const int availW = qMax(10, r.width() - 20);
                    int y = r.top() + 22;
                    for (int i = 0; i < previewLines.size() && i < 2; ++i) {
                        QString line = previewLines[i];
                        if (line != "...")
                            line = fm.elidedText(line, Qt::ElideRight, availW);

                        QString sev = (i < previewSeverities.size()) ? previewSeverities[i] : QString();
                        QColor dotColor("#9CA3AF");
                        if (sev == "overdue") dotColor = QColor("#FF0000");
                        else if (sev == "soon") dotColor = QColor("#FF8800");
                        else if (sev == "planned") dotColor = QColor("#18CF00");
                        else if (sev == "completed") dotColor = QColor("#00E5FF");

                        if (line != "...") {
                            p->setBrush(dotColor);
                            p->setPen(Qt::NoPen);
                            p->drawEllipse(QPoint(r.left() + 8, y + fm.height() / 2), 4, 4);
                        }

                        p->setPen(QColor("#4B5563"));
                        p->drawText(QRect(r.left() + 16, y, availW, fm.height()),
                                    Qt::AlignLeft | Qt::AlignTop, line);
                        y += fm.height() + 1;
                    }
                    p->restore();
                }
            }

            if (idx.row() == 0) return;

            p->setPen(QColor("#D3D3D3"));
            QRect r = opt.rect;

            if (idx.column() < idx.model()->columnCount() - 1)
                p->drawLine(r.right(), r.top(), r.right(), r.bottom());

            if (idx.row() < idx.model()->rowCount() - 1)
                p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
        }
    };

    calendarTable->setItemDelegate(new CalendarDelegate());
    rightCalendarLayout_->addWidget(calendarTable);

    // ===== ПОПАП СПИСКА ЗАДАЧ НА ВЫБРАННЫЙ ДЕНЬ =====
    calendarDayOverlay_ = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    calendarDayOverlay_->hide();
    calendarDayOverlay_->setStyleSheet(
        "QFrame{background:#CDCDCD;border:none;border-radius:6px;}"
        "QLabel{font-family:Inter;color:#111827;background:#CDCDCD;}"
        "QWidget{background:#CDCDCD;}"
    );
    calendarDayOverlay_->raise();

    QVBoxLayout *dayOverlayLayout = new QVBoxLayout(calendarDayOverlay_);
    dayOverlayLayout->setContentsMargins(s(5), s(4), s(5), s(5));
    dayOverlayLayout->setSpacing(s(6));

    QHBoxLayout *dayOverlayHeader = new QHBoxLayout();
    dayOverlayHeader->setContentsMargins(0,0,0,0);
    QLabel *dayOverlayTitle = new QLabel("1", calendarDayOverlay_);
    dayOverlayTitle->setStyleSheet("font-family:Inter;font-size:13px;font-weight:800;color:#111111;");
    dayOverlayHeader->addWidget(dayOverlayTitle);
    dayOverlayHeader->addStretch();
    dayOverlayLayout->addLayout(dayOverlayHeader);

    QScrollArea *dayOverlayScroll = new QScrollArea(calendarDayOverlay_);
    dayOverlayScroll->setWidgetResizable(true);
    dayOverlayScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    dayOverlayScroll->setStyleSheet(
        "QScrollArea{border:none;background:#CDCDCD;}"
        "QScrollArea > QWidget > QWidget{background:#CDCDCD;}"
        "QScrollBar:vertical { width:6px; background:transparent; margin:2px; }"
        "QScrollBar::handle:vertical { background:#C0C0C0; border-radius:3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0px; }"
    );
    QWidget *dayOverlayHost = new QWidget(dayOverlayScroll);
    dayOverlayHost->setStyleSheet("background:#CDCDCD;");
    QVBoxLayout *dayOverlayTasksLayout = new QVBoxLayout(dayOverlayHost);
    dayOverlayTasksLayout->setContentsMargins(0, 0, 0, 0);
    dayOverlayTasksLayout->setSpacing(s(6));
    dayOverlayScroll->setWidget(dayOverlayHost);
    dayOverlayLayout->addWidget(dayOverlayScroll, 1);

    QDate overlayDate;
    connect(calendarTable, &QTableWidget::cellClicked, this, [=, &overlayDate](int r, int c){
        if (r == 0)
            return;
        QTableWidgetItem *item = calendarTable->item(r, c);
        if (!item)
            return;

        const QDate date = item->data(Qt::UserRole).toDate();
        if (!date.isValid())
            return;

        const QVector<CalendarEvent> dayEvents = calendarEventsByDate_.value(date);
        if (calendarDayOverlay_ && calendarDayOverlay_->isVisible() && overlayDate == date) {
            calendarDayOverlay_->hide();
            overlayDate = QDate();
            return;
        }

        if (!calendarDayOverlay_)
            return;

        QRect cellRect = calendarTable->visualRect(calendarTable->model()->index(r, c));
        QPoint globalTopLeft = calendarTable->viewport()->mapToGlobal(cellRect.topLeft());

        const QPoint frameTopLeft = rightCalendarFrame->mapToGlobal(QPoint(0, 0));
        const QPoint frameBottomRight = rightCalendarFrame->mapToGlobal(QPoint(rightCalendarFrame->width(), rightCalendarFrame->height()));

        int x = globalTopLeft.x() + s(2);
        int y = globalTopLeft.y() + s(2);
        QFont rowFont("Inter", s(12));
        QFontMetrics rowFm(rowFont);
        int maxTextW = 0;
        for (const CalendarEvent &ev : dayEvents) {
            const QString fullText = QString("%1 - %2").arg(ev.agvId, ev.taskTitle);
            maxTextW = qMax(maxTextW, rowFm.horizontalAdvance(fullText));
        }
        int desiredW = qMax(cellRect.width() - s(2), maxTextW + s(42));
        int availableW = frameBottomRight.x() - x - s(8);
        int width = qBound(s(120), desiredW, qMax(s(120), availableW));
        int maxHeight = frameBottomRight.y() - y - s(8);
        int targetHeight = cellRect.height() * 3;
        int height = qMax(s(120), qMin(maxHeight, targetHeight));

        if (x < frameTopLeft.x() + s(8))
            x = frameTopLeft.x() + s(8);
        if (x + width > frameBottomRight.x() - s(8))
            x = frameBottomRight.x() - width - s(8);

        if (height < s(80))
            height = s(80);

        calendarDayOverlay_->setGeometry(x, y, width, height);

        while (dayOverlayTasksLayout->count() > 0) {
            QLayoutItem *itemToRemove = dayOverlayTasksLayout->takeAt(0);
            if (itemToRemove->widget())
                itemToRemove->widget()->deleteLater();
            delete itemToRemove;
        }

        dayOverlayTitle->setText(QString::number(date.day()));

        if (dayEvents.isEmpty()) {
            QLabel *empty = new QLabel("На этот день задач нет.", calendarDayOverlay_);
            empty->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:#6B7280;").arg(s(11)));
            dayOverlayTasksLayout->addWidget(empty);
        }

        for (const CalendarEvent &ev : dayEvents) {
            const QString fullText = QString("%1 - %2").arg(ev.agvId, ev.taskTitle);
            QWidget *rowHost = new QWidget(calendarDayOverlay_);
            rowHost->setStyleSheet("background:#CDCDCD;");
            QHBoxLayout *rowLayout = new QHBoxLayout(rowHost);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(s(4));

            QColor dotColor("#9CA3AF");
            if (ev.severity == "overdue") dotColor = QColor("#FF0000");
            else if (ev.severity == "soon") dotColor = QColor("#FF8800");
            else if (ev.severity == "planned") dotColor = QColor("#18CF00");
            else if (ev.severity == "completed") dotColor = QColor("#00E5FF");

            QLabel *dot = new QLabel(rowHost);
            dot->setFixedSize(s(16), s(16));
            dot->setStyleSheet(QString("background:%1;border:none;border-radius:%2px;")
                               .arg(dotColor.name()).arg(s(8)));
            rowLayout->addWidget(dot, 0, Qt::AlignVCenter);

            QPushButton *taskBtn = new QPushButton(fullText, rowHost);
            taskBtn->setStyleSheet(QString(
                "QPushButton{text-align:left;background:#CDCDCD;border:none;border-radius:0px;"
                "padding:%1px %2px;font-family:Inter;font-size:%3px;color:#1F2937;}"
                "QPushButton:hover{background:#BDBDBD;}"
            ).arg(s(1)).arg(s(2)).arg(s(12)));
            taskBtn->setMinimumHeight(s(24));
            taskBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            taskBtn->setToolTip(fullText);
            connect(taskBtn, &QPushButton::clicked, this, [this, ev](){
                openAgvTaskFromCalendar(ev.agvId, ev.taskTitle);
            });
            rowLayout->addWidget(taskBtn, 1, Qt::AlignVCenter);
            dayOverlayTasksLayout->addWidget(rowHost);
        }

        dayOverlayTasksLayout->addStretch();
        calendarDayOverlay_->show();
        calendarDayOverlay_->raise();
        overlayDate = date;

        calendarActionsLabel_->setText("Выберите день");
        calendarActionsFrame->setVisible(false);
    });

    const int loadGen = calendarLoadGeneration_;
    const QDate loadFrom = visibleFrom;
    const QDate loadTo = visibleTo;
    QTimer::singleShot(0, this, [this, loadGen, loadFrom, loadTo]() {
        if (loadGen != calendarLoadGeneration_ || !calendarTablePtr)
            return;
        const QVector<CalendarEvent> events = loadCalendarEventsRange(loadFrom, loadTo);
        calendarEventsByDate_.clear();
        for (const CalendarEvent &e : events)
            calendarEventsByDate_[e.date].push_back(e);
        applyCalendarEventsToVisibleCells(loadGen);
    });
}

void leftMenu::applyCalendarEventsToVisibleCells(int loadGeneration)
{
    if (loadGeneration != calendarLoadGeneration_ || !calendarTablePtr)
        return;

    auto severityRank = [](const QString &sev) {
        if (sev == "overdue") return 4;
        if (sev == "soon") return 3;
        if (sev == "planned") return 2;
        if (sev == "completed") return 1;
        return 0;
    };

    auto shortenAgvIdForCell = [](const QString &rawAgvId) -> QString {
        const QString agvId = rawAgvId.trimmed();
        const int lastDash = agvId.lastIndexOf('-');
        if (lastDash <= 0 || lastDash >= agvId.size() - 1)
            return agvId;

        const QString prefix = agvId.left(lastDash);
        const QString suffix = agvId.mid(lastDash + 1);
        if (suffix.size() <= 2)
            return agvId;

        QString shortSuffix;
        const QStringList parts = suffix.split(QRegularExpression("[_\\s]+"), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            for (const QString &part : parts) {
                if (!part.isEmpty())
                    shortSuffix += part.left(1).toUpper();
            }
        } else {
            shortSuffix = suffix.left(1).toUpper() + suffix.right(1).toUpper();
        }

        if (shortSuffix.isEmpty())
            return agvId;
        return prefix + "-" + shortSuffix;
    };

    const QString searchTerm = searchEdit_ ? searchEdit_->text().trimmed().toLower() : QString();
    auto normSearch = [](QString s) {
        s = s.toLower().trimmed();
        s.remove(QRegularExpression("[\\s\\-_/]+"));
        return s;
    };
    const QString searchKey = searchTerm.isEmpty() ? QString() : normSearch(searchTerm);

    for (auto it = calendarVisibleCells_.constBegin(); it != calendarVisibleCells_.constEnd(); ++it) {
        const QDate date = it.key();
        QTableWidgetItem *item = it.value();
        if (!item || !date.isValid())
            continue;

        QVector<CalendarEvent> dayEvents = calendarEventsByDate_.value(date);
        if (!searchKey.isEmpty()) {
            dayEvents.erase(std::remove_if(dayEvents.begin(), dayEvents.end(),
                [&](const CalendarEvent &ev) {
                    return !normSearch(ev.agvId + ev.taskTitle).contains(searchKey);
                }), dayEvents.end());
        }
        if (dayEvents.isEmpty()) {
            item->setData(Qt::UserRole + 1, QStringList());
            item->setData(Qt::UserRole + 2, QStringList());
            item->setData(Qt::UserRole + 10, QStringList());
            item->setData(Qt::UserRole + 11, QStringList());
            continue;
        }

        QMap<QString, int> agvCounts;
        QMap<QString, QString> agvSeverity;
        QVector<QString> agvOrder;
        QStringList allEventKeys;
        QStringList allEventSeverities;
        for (const CalendarEvent &ev : dayEvents) {
            allEventKeys << (ev.agvId.trimmed() + "||" + ev.taskTitle.trimmed());
            allEventSeverities << ev.severity;
            if (!agvCounts.contains(ev.agvId)) {
                agvOrder.push_back(ev.agvId);
                agvCounts[ev.agvId] = 0;
                agvSeverity[ev.agvId] = ev.severity;
            }
            agvCounts[ev.agvId] += 1;
            if (severityRank(ev.severity) > severityRank(agvSeverity.value(ev.agvId)))
                agvSeverity[ev.agvId] = ev.severity;
        }

        QStringList previewLines;
        QStringList previewSeverities;
        for (int i = 0; i < agvOrder.size() && i < 2; ++i) {
            const QString agvId = agvOrder[i];
            previewLines << QString("%1 - %2 задач").arg(shortenAgvIdForCell(agvId)).arg(agvCounts.value(agvId));
            previewSeverities << agvSeverity.value(agvId);
        }
        if (agvOrder.size() > 2) {
            previewLines[1] = "...";
            previewSeverities[1] = "";
        }

        item->setData(Qt::UserRole + 1, previewLines);
        item->setData(Qt::UserRole + 2, previewSeverities);
        item->setData(Qt::UserRole + 10, allEventKeys);
        item->setData(Qt::UserRole + 11, allEventSeverities);
    }

    calendarTablePtr->viewport()->update();
}
