#pragma once

class QWidget;

namespace LeftMenuDialogs {

struct CalendarDialogSelection
{
    int year = 0;
    int month = 0;
    int week = 0;
    int day = 0;
};

bool showCalendarSettingsDialog(QWidget *parent, CalendarDialogSelection &selection);
void showAppSettingsDialog(QWidget *parent);

} // namespace LeftMenuDialogs
