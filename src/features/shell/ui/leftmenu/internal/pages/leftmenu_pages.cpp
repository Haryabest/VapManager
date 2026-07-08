#include "leftmenu.h"

#include <QTimer>

void leftMenu::restoreActivePage()
{
    switch (activePage_) {
    case ActivePage::AgvList:
        showAgvList();
        break;
    case ActivePage::AgvDetails:
        if (!activeAgvId_.trimmed().isEmpty()) {
            showAgvDetailInfo(activeAgvId_);
        } else {
            showAgvList();
        }
        break;
    case ActivePage::ModelList:
        showModelList();
        break;
    case ActivePage::Logs:
        showLogs();
        break;
    case ActivePage::Profile:
        showProfile();
        break;
    case ActivePage::Chats:
        showChatsPage();
        break;
    case ActivePage::Users:
        showUsersPage();
        break;
    case ActivePage::UserProfile:
        if (!activeUsername_.trimmed().isEmpty()) {
            showUserProfilePage(activeUsername_);
        } else {
            showUsersPage();
        }
        break;
    case ActivePage::Calendar:
    default:
        showCalendar();
        break;
    }
}
void leftMenu::showAgvList()
{
    activePage_ = ActivePage::AgvList;
    logsStale_ = true;
    hideAllPages();
    clearSearch();

    updateAgvCounter();
    if (listAgvInfo && (agvListDirty_ || !listAgvInfo->hasRenderedState())) {
        QVector<AgvInfo> agvs = listAgvInfo->loadAgvList();
        listAgvInfo->rebuildList(agvs);
        agvListDirty_ = false;
    }

    if (listAgvInfo) listAgvInfo->setVisible(true);
    stressSuiteLogPageEntered(QStringLiteral("agv_list"));
}
void leftMenu::showCalendar()
{
    activePage_ = ActivePage::Calendar;
    hideAllPages();

    if (rightCalendarFrame)              rightCalendarFrame->setVisible(true);
    if (rightUpcomingMaintenanceFrame)   rightUpcomingMaintenanceFrame->setVisible(true);

    if (pendingCalendarReload_) {
        pendingCalendarReload_ = false;
        QTimer::singleShot(0, this, [this](){
            setSelectedMonthYear(selectedMonth_, selectedYear_);
        });
    }
    stressSuiteLogPageEntered(QStringLiteral("calendar"));
}
void leftMenu::scheduleDeferredStartupLoads()
{
    QTimer::singleShot(0, this, [this]() {
        updateAgvCounter();
        updateSystemStatus();
        updateOpcStatusIndicator();
        updateUpcomingMaintenance();
    });
}
void leftMenu::showModelList()
{
    activePage_ = ActivePage::ModelList;
    hideAllPages();
    clearSearch();
    if (modelListPage) modelListPage->setVisible(true);
    stressSuiteLogPageEntered(QStringLiteral("models"));
}
