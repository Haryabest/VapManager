#include "leftmenu.h"

#include "app_session.h"
#include "db_users.h"
#include "leftmenu/internal/calendar/leftmenu_calendar_utils.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

void leftMenu::showUsersPage()
{
    activePage_ = ActivePage::Users;
    hideAllPages();
    clearSearch();
    if (usersPage) {
        usersPage->setVisible(true);
        if (!usersPage->property("loaded_once").toBool()) {
            usersPage->loadUsers();
            usersPage->setProperty("loaded_once", true);
        }
    }
    stressSuiteLogPageEntered(QStringLiteral("users"));
}

void leftMenu::clearSearch()
{
    if (searchEdit_) {
        searchEdit_->blockSignals(true);
        searchEdit_->clear();
        searchEdit_->blockSignals(false);
        onSearchTextChanged(QString());
    }
}

void leftMenu::onSearchTextChanged(const QString &text)
{
    auto normalize = [](QString s) -> QString {
        s = s.toLower().trimmed();
        // Убираем пробелы и большую часть "мусора", чтобы порядок символов учитывался строго.
        s.remove(QRegularExpression("[\\s\\-_/]+"));
        return s;
    };
    const QString term = normalize(text);

    // Calendar + upcoming maintenance visible = main page
    if (rightCalendarFrame && rightCalendarFrame->isVisible()) {
        // Filter upcoming maintenance items
        if (rightUpcomingMaintenanceFrame) {
            QScrollArea *scroll = rightUpcomingMaintenanceFrame->findChild<QScrollArea*>();
            if (scroll && scroll->widget()) {
                QList<QFrame*> items = scroll->widget()->findChildren<QFrame*>(QString(), Qt::FindDirectChildrenOnly);
                for (QFrame *frame : items) {
                    if (term.isEmpty()) {
                        frame->setVisible(true);
                    } else {
                        bool match = false;
                        const QList<QLabel*> labels = frame->findChildren<QLabel*>();
                        for (QLabel *lbl : labels) {
                            if (!lbl) continue;
                            if (normalize(lbl->text()).contains(term)) { match = true; break; }
                        }
                        frame->setVisible(match);
                    }
                }
            }
        }

        // Filter calendar previews (applyCalendarEventsToVisibleCells читает searchEdit_)
        if (calendarTablePtr)
            applyCalendarEventsToVisibleCells(calendarLoadGeneration_);
        return;
    }

    // AGV list visible
    if (listAgvInfo && listAgvInfo->isVisible()) {
        if (term.isEmpty()) {
            QVector<AgvInfo> agvs = listAgvInfo->loadAgvList();
            listAgvInfo->rebuildList(agvs);
        } else {
            QVector<AgvInfo> agvs = listAgvInfo->loadAgvList();
            agvs.erase(std::remove_if(agvs.begin(), agvs.end(),
                [&](const AgvInfo &a){
                    return !normalize(a.id).contains(term)
                        && !normalize(a.model).contains(term)
                        && !normalize(a.serial).contains(term);
                }), agvs.end());
            listAgvInfo->rebuildList(agvs);
        }
        return;
    }

    // Users page visible
    if (usersPage && usersPage->isVisible()) {
        QList<QWidget*> items = usersPage->findChildren<QWidget*>("userItem");
        for (QWidget *w : items) {
            if (term.isEmpty()) {
                w->setVisible(true);
            } else {
                bool match = false;
                QList<QLabel*> labels = w->findChildren<QLabel*>();
                for (QLabel *lbl : labels) {
                    if (lbl->text().toLower().contains(term)) {
                        match = true;
                        break;
                    }
                }
                w->setVisible(match);
            }
        }
        return;
    }

    // Model list page visible
    if (modelListPage && modelListPage->isVisible()) {
        QScrollArea *scroll = modelListPage->findChild<QScrollArea*>();
        if (scroll && scroll->widget()) {
            QList<QFrame*> cards = scroll->widget()->findChildren<QFrame*>(QString(), Qt::FindDirectChildrenOnly);
            for (QFrame *card : cards) {
                if (term.isEmpty()) {
                    card->setVisible(true);
                } else {
                    bool match = false;
                    QList<QLabel*> labels = card->findChildren<QLabel*>();
                    for (QLabel *lbl : labels) {
                        if (lbl->text().toLower().contains(term)) {
                            match = true;
                            break;
                        }
                    }
                    card->setVisible(match);
                }
            }
        }
        return;
    }
}
void leftMenu::hideAllPages()
{
    if (topRow_)                         topRow_->setVisible(true);
    if (bottomRow_)                      bottomRow_->setVisible(true);

    if (rightCalendarFrame)              rightCalendarFrame->setVisible(false);
    if (rightUpcomingMaintenanceFrame)   rightUpcomingMaintenanceFrame->setVisible(false);
    if (listAgvInfo)                     listAgvInfo->setVisible(false);
    if (agvSettingsPage)                 agvSettingsPage->setVisible(false);
    if (modelListPage)                   modelListPage->setVisible(false);
    if (logsPage)                        logsPage->setVisible(false);
    if (profilePage)                     profilePage->setVisible(false);
    if (chatsPage)                       chatsPage->setVisible(false);
    if (usersPage)                       usersPage->setVisible(false);

    QList<QWidget*> profilePages = findChildren<QWidget*>("userProfilePage");
    for (QWidget *w : profilePages) {
        w->setVisible(false);
        w->deleteLater();
    }
}
