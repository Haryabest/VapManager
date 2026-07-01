#include "leftmenu.h"

#include <QTimer>

void leftMenu::openAgvTaskFromCalendar(const QString &agvId, const QString &taskTitle)
{
    const QString id = agvId.trimmed();
    if (id.isEmpty() || !agvSettingsPage)
        return;

    hideCalendarDayOverlay();

    showAgvDetailInfo(id);

    QString taskName = taskTitle.trimmed();
    static const QString kCompletedSuffix = QStringLiteral(" (обслужена)");
    if (taskName.endsWith(kCompletedSuffix))
        taskName.chop(kCompletedSuffix.size());

    if (taskName.isEmpty())
        return;

    QTimer::singleShot(0, this, [this, taskName]() {
        if (agvSettingsPage)
            agvSettingsPage->highlightTask(taskName);
    });
}

void leftMenu::showAgvDetailInfo(const QString &agvId)
{
    const QString id = agvId.trimmed();
    if (id.isEmpty() || !agvSettingsPage)
        return;

    hideCalendarDayOverlay();

    activePage_ = ActivePage::AgvDetails;
    logsStale_ = true;
    activeAgvId_ = id;
    hideAllPages();
    clearSearch();

    agvSettingsPage->loadAgv(id);
    agvSettingsPage->setVisible(true);
    stressSuiteLogPageEntered(QStringLiteral("agv_detail"));
}
