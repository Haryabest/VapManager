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

        // Filter calendar previews too (same search box)
        if (calendarTablePtr) {
            for (int r = 1; r < calendarTablePtr->rowCount(); ++r) {
                for (int c = 0; c < calendarTablePtr->columnCount(); ++c) {
                    QTableWidgetItem *it = calendarTablePtr->item(r, c);
                    if (!it) continue;
                    const QDate d = it->data(Qt::UserRole).toDate();
                    if (!d.isValid()) continue;

                    const QStringList allKeys = it->data(Qt::UserRole + 10).toStringList();
                    const QStringList allSev = it->data(Qt::UserRole + 11).toStringList();
                    if (allKeys.isEmpty()) {
                        it->setData(Qt::UserRole + 1, QStringList());
                        it->setData(Qt::UserRole + 2, QStringList());
                        continue;
                    }
                    if (term.isEmpty()) {
                        // восстановим дефолтный превью (пересчитываем из allKeys)
                    }

                    // Фильтруем события дня по term.
                    QStringList filteredKeys;
                    QStringList filteredSev;
                    for (int i = 0; i < allKeys.size(); ++i) {
                        const QString key = allKeys[i];
                        const QString sev = (i < allSev.size()) ? allSev[i] : QString();
                        const QString agv = key.section("||", 0, 0);
                        const QString task = key.section("||", 1, 1);
                        const QString hay = normalize(agv + task);
                        if (term.isEmpty() || hay.contains(term)) {
                            filteredKeys << key;
                            filteredSev << sev;
                        }
                    }

                    // Считаем как раньше: AGV -> количество задач + худшая severity.
                    QMap<QString, int> agvCounts;
                    QMap<QString, QString> agvSeverity;
                    QVector<QString> agvOrder;
                    auto severityRank = [](const QString &sev) {
                        if (sev == "overdue") return 4;
                        if (sev == "soon") return 3;
                        if (sev == "planned") return 2;
                        if (sev == "completed") return 1;
                        return 0;
                    };
                    for (int i = 0; i < filteredKeys.size(); ++i) {
                        const QString agvId = filteredKeys[i].section("||", 0, 0);
                        const QString sev = (i < filteredSev.size()) ? filteredSev[i] : QString();
                        if (!agvCounts.contains(agvId)) {
                            agvOrder.push_back(agvId);
                            agvCounts[agvId] = 0;
                            agvSeverity[agvId] = sev;
                        }
                        agvCounts[agvId] += 1;
                        if (severityRank(sev) > severityRank(agvSeverity.value(agvId)))
                            agvSeverity[agvId] = sev;
                    }

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

                    QStringList previewLines;
                    QStringList previewSeverities;
                    for (int i = 0; i < agvOrder.size() && i < 2; ++i) {
                        const QString agvId = agvOrder[i];
                        const int count = agvCounts.value(agvId);
                        previewLines << QString("%1 - %2 задач").arg(shortenAgvIdForCell(agvId)).arg(count);
                        previewSeverities << agvSeverity.value(agvId);
                    }
                    if (agvOrder.size() > 2) {
                        if (previewLines.size() < 2) previewLines << "...";
                        else previewLines[1] = "...";
                        if (previewSeverities.size() < 2) previewSeverities << "";
                        else previewSeverities[1] = "";
                    }

                    it->setData(Qt::UserRole + 1, previewLines);
                    it->setData(Qt::UserRole + 2, previewSeverities);
                }
            }
            calendarTablePtr->viewport()->update();
        }
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
