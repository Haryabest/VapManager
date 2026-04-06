#include "leftmenu_calendar_utils.h"

#include <QDate>

namespace LeftMenuCalendar {

int minYear()
{
    return QDate::currentDate().year() - 10;
}

int maxYear()
{
    return QDate::currentDate().year() + 10;
}

int daysInMonth(int year, int month)
{
    static const int kRegular[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12 || year <= 0)
        return 31;
    if (month == 2)
        return QDate::isLeapYear(year) ? 29 : 28;
    return kRegular[month - 1];
}

} // namespace LeftMenuCalendar
