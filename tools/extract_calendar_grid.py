from pathlib import Path

path = Path(__file__).resolve().parent.parent / "src/features/shell/ui/leftmenu.cpp"
lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
start = end = None
for i, l in enumerate(lines):
    if start is None and l.strip().startswith("QDate firstDay(selectedYear_"):
        start = i
    if start is not None and "rightCalendarLayout->addWidget(calendarTable)" in l:
        end = i
        break
if start is None or end is None:
    raise SystemExit(f"markers not found: start={start} end={end}")

body = "".join(lines[start : end + 1])
body = body.replace("rightCalendarLayout->", "rightCalendarLayout_->")
body = body.replace("QFrame *dayOverlay = new", "calendarDayOverlay_ = new QFrame")
body = body.replace("dayOverlay->", "calendarDayOverlay_->")
body = body.replace("dayOverlay,", "calendarDayOverlay_,")
body = body.replace("actionsLabel->", "calendarActionsLabel_->")
body = body.replace("[this, ev, dayOverlay]", "[this, ev]")

header = """#include "leftmenu.h"
#include "db_users.h"
#include "app_session.h"
#include <QScrollArea>
#include <algorithm>

void leftMenu::updateCalendarNavButtons()
{
    if (!prevMonthBtn_ || !nextMonthBtn_)
        return;
    const bool atMin = (selectedYear_ == minCalendarYear() && selectedMonth_ == 1);
    const bool atMax = (selectedYear_ == maxCalendarYear() && selectedMonth_ == 12);
    prevMonthBtn_->setEnabled(!atMin);
    nextMonthBtn_->setEnabled(!atMax);
}

void leftMenu::refreshCalendarMonthLight()
{
    if (monthLabel)
        monthLabel->setText(monthYearLabelText(selectedMonth_, selectedYear_));
    updateCalendarNavButtons();
    if (calendarDayOverlay_) {
        calendarDayOverlay_->hide();
        delete calendarDayOverlay_;
        calendarDayOverlay_ = nullptr;
    }
    if (calendarTablePtr && rightCalendarLayout_) {
        rightCalendarLayout_->removeWidget(calendarTablePtr);
        delete calendarTablePtr;
        calendarTablePtr = nullptr;
    }
    buildCalendarTable();
    updateUpcomingMaintenance();
}

void leftMenu::buildCalendarTable()
{
    if (!rightCalendarFrame || !rightCalendarLayout_)
        return;

"""

footer = "\n    rightCalendarLayout_->addWidget(calendarTable);\n}\n"
body = body.replace("    rightCalendarLayout_->addWidget(calendarTable);\n", "")

out = path.parent / "internal/leftmenu_calendar_grid.cpp"
out.write_text(header + body + footer, encoding="utf-8")

new_lines = lines[:start] + ["    buildCalendarTable();\n", "\n"] + lines[end + 1 :]
path.write_text("".join(new_lines), encoding="utf-8")
print(f"OK: {end - start + 1} lines -> {out.name}")
