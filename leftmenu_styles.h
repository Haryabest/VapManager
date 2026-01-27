#ifndef LEFTMENU_STYLES_H
#define LEFTMENU_STYLES_H

#include <QString>

class leftMenu;

class LeftMenuStyles
{
public:
    explicit LeftMenuStyles(const leftMenu *owner);

    QString styleCalendarTitle() const;
    QString styleCalendarSubtitle() const;
    QString styleLegendText() const;
    QString styleLegendDot(const QString &color) const;
    QString styleNavButton() const;
    QString styleAddAgvButton() const;
    QString styleSearchEdit() const;
    QString styleLogsButton() const;
    QString styleStatusLabel() const;
    QString styleMaintenanceTitle() const;
    QString styleMaintenanceSubtitle() const;
    QString styleCalendarCell(bool faded) const;
    QString styleCalendarWeekday() const;

private:
    const leftMenu *owner_;
    int s(int v) const;
};

#endif // LEFTMENU_STYLES_H
