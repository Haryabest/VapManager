#include "listagvinfo.h"

#include "app_session.h"
#include "db_users.h"
#include "internal/listagvinfo_ui_modules.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <algorithm>

using ListAgvInfoUi::CollapsibleSection;

namespace {

struct AgvDispSlot {
    AgvInfo info;
    enum Kind {
        MineOver, MineSoon, MineDone,
        ComOver, ComSoon, ComDone,
        DelOver, DelSoon, DelDone
    } kind;
    QString delegUser;

    AgvDispSlot(const AgvInfo &i, Kind k, const QString &du = QString())
        : info(i), kind(k), delegUser(du) {}
};

} // namespace

void ListAgvInfo::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    if (addBtn_) {
        int x = (width() - addBtn_->width()) / 2;
        int y = height() - addBtn_->height() - s(20);
        addBtn_->move(x, y);
        addBtn_->raise();
    }
    if (undoToast_) {
        int x = width() - undoToast_->width() - s(20);
        int y = height() - undoToast_->height() - s(20);
        if (x < s(10)) x = s(10);
        if (y < s(10)) y = s(10);
        undoToast_->move(x, y);
        undoToast_->raise();
    }
}

void ListAgvInfo::applyFilter(const FilterSettings &fs)
{
    QVector<AgvInfo> list = loadAgvList();

    if (!fs.nameFilter.isEmpty()) {
        list.erase(
            std::remove_if(list.begin(), list.end(),
                [&](const AgvInfo &a){
                    return !a.id.contains(fs.nameFilter, Qt::CaseInsensitive)
                        && !a.model.contains(fs.nameFilter, Qt::CaseInsensitive);
                }),
            list.end()
        );
    }

    if (fs.serv == FilterSettings::Asc) {
        std::sort(list.begin(), list.end(), [](const AgvInfo &a, const AgvInfo &b){ return a.kilometers < b.kilometers; });
    } else if (fs.serv == FilterSettings::Desc) {
        std::sort(list.begin(), list.end(), [](const AgvInfo &a, const AgvInfo &b){ return a.kilometers > b.kilometers; });
    } else if (fs.up == FilterSettings::UpAsc) {
        std::sort(list.begin(), list.end(), [](const AgvInfo &a, const AgvInfo &b){ return a.lastActive < b.lastActive; });
    } else if (fs.up == FilterSettings::UpDesc) {
        std::sort(list.begin(), list.end(), [](const AgvInfo &a, const AgvInfo &b){ return a.lastActive > b.lastActive; });
    } else if (fs.over == FilterSettings::OverOld) {
        std::sort(list.begin(), list.end(), [](const AgvInfo &a, const AgvInfo &b){ return a.lastActive < b.lastActive; });
    } else if (fs.over == FilterSettings::OverNew) {
        std::sort(list.begin(), list.end(), [](const AgvInfo &a, const AgvInfo &b){ return a.lastActive > b.lastActive; });
    } else if (fs.modelSort == FilterSettings::ModelAZ) {
        std::sort(list.begin(), list.end(), [](const AgvInfo &a, const AgvInfo &b){ return a.model < b.model; });
    } else if (fs.modelSort == FilterSettings::ModelZA) {
        std::sort(list.begin(), list.end(), [](const AgvInfo &a, const AgvInfo &b){ return a.model > b.model; });
    } else if (fs.km == FilterSettings::KmAsc) {
        std::sort(list.begin(), list.end(), [](const AgvInfo &a, const AgvInfo &b){ return a.kilometers < b.kilometers; });
    } else if (fs.km == FilterSettings::KmDesc) {
        std::sort(list.begin(), list.end(), [](const AgvInfo &a, const AgvInfo &b){ return a.kilometers > b.kilometers; });
    }

    rebuildList(list);
}

void ListAgvInfo::rebuildList(const QVector<AgvInfo> &list)
{
    currentDisplayList_ = list;
    shownCount_ = batchSize_;
    appearRetryLeft_ = 2;
    hasRenderedState_ = false;
    rebuildShownChunk();
}

void ListAgvInfo::rebuildShownChunk()
{
    if (content)
        content->setUpdatesEnabled(false);

    QLayoutItem *child;
    while ((child = layout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            if (child->widget() == loadMoreBtn_) {
                loadMoreBtn_->setParent(this);
            } else {
                child->widget()->deleteLater();
            }
        }
        delete child;
    }

    if (currentDisplayList_.isEmpty()) {
        displayQueueTotal_ = 0;
        if (loadMoreBtn_)
            loadMoreBtn_->hide();
        QLabel *emptyLabel = new QLabel("Здесь ничего нет", content);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet(QString(
            "font-family:Inter;"
            "font-size:%1px;"
            "font-weight:900;"
            "color:#555;"
        ).arg(s(28)));

        layout->addStretch();
        layout->addWidget(emptyLabel, 0, Qt::AlignCenter);
        layout->addStretch();
        hasRenderedState_ = true;
        if (content) {
            content->setUpdatesEnabled(true);
            content->update();
        }
        return;
    }

    QString currentUser = AppSession::currentUsername();
    const QString curRole = getUserRole(currentUser);
    const bool isStaff = (curRole == QStringLiteral("admin") || curRole == QStringLiteral("tech"));

    QVector<AgvInfo> mO, mS, mD, cO, cS, cD;
    for (const AgvInfo &a : currentDisplayList_) {
        const QString assignee = a.assignedUser.trimmed();
        const bool isMine = (assignee == currentUser);
        const bool isUnassigned = assignee.isEmpty();
        const bool overdue = a.hasOverdueMaintenance;
        const bool soon = a.hasSoonMaintenance && !overdue;
        auto push = [&](QVector<AgvInfo> &ov, QVector<AgvInfo> &sn, QVector<AgvInfo> &dn) {
            if (overdue) ov.append(a);
            else if (soon) sn.append(a);
            else dn.append(a);
        };
        if (isMine)
            push(mO, mS, mD);
        if (isStaff)
            push(cO, cS, cD);
        else if (isUnassigned)
            push(cO, cS, cD);
    }

    QHash<QString, AgvInfo> agvFull;
    agvFull.reserve(currentDisplayList_.size());
    for (const AgvInfo &a : currentDisplayList_)
        agvFull.insert(a.id, a);

    QVector<AgvDispSlot> queue;
    auto appendVec = [&queue](const QVector<AgvInfo> &v, AgvDispSlot::Kind k) {
        for (const AgvInfo &x : v)
            queue.push_back(AgvDispSlot(x, k));
    };

    appendVec(mO, AgvDispSlot::MineOver);
    appendVec(mS, AgvDispSlot::MineSoon);
    appendVec(mD, AgvDispSlot::MineDone);
    appendVec(cO, AgvDispSlot::ComOver);
    appendVec(cS, AgvDispSlot::ComSoon);
    appendVec(cD, AgvDispSlot::ComDone);

    QMap<QString, QVector<AgvInfo>> delFull;
    if (curRole == QStringLiteral("admin") || curRole == QStringLiteral("tech")) {
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "SELECT DISTINCT assigned_to, agv_id "
                "FROM agv_tasks "
                "WHERE assigned_to != '' AND assigned_to != :me "
                "ORDER BY assigned_to, agv_id"));
            q.bindValue(QStringLiteral(":me"), currentUser);
            if (q.exec()) {
                QHash<QString, QSet<QString>> seen;
                while (q.next()) {
                    const QString delegatedUser = q.value(0).toString().trimmed();
                    const QString agvId = q.value(1).toString().trimmed();
                    if (delegatedUser.isEmpty() || agvId.isEmpty())
                        continue;
                    if (!agvFull.contains(agvId))
                        continue;
                    if (seen[delegatedUser].contains(agvId))
                        continue;
                    seen[delegatedUser].insert(agvId);
                    delFull[delegatedUser].append(agvFull.value(agvId));
                }
            }
        }

        QHash<QString, QSet<QString>> idsInDel;
        for (auto it = delFull.constBegin(); it != delFull.constEnd(); ++it) {
            for (const AgvInfo &x : it.value())
                idsInDel[it.key()].insert(x.id);
        }
        for (const AgvInfo &a : currentDisplayList_) {
            const QString owner = a.assignedUser.trimmed();
            if (owner.isEmpty() || owner == currentUser)
                continue;
            if (!agvFull.contains(a.id))
                continue;
            if (idsInDel[owner].contains(a.id))
                continue;
            idsInDel[owner].insert(a.id);
            delFull[owner].append(a);
        }
    }

    QStringList delUsers = delFull.keys();
    std::sort(delUsers.begin(), delUsers.end());

    QMap<QString, QVector<AgvInfo>> delOFull, delSFull, delDFull;
    for (const QString &du : delUsers) {
        QVector<AgvInfo> dO, dS, dD;
        for (const AgvInfo &a : delFull.value(du)) {
            if (a.hasOverdueMaintenance)
                dO.append(a);
            else if (a.hasSoonMaintenance)
                dS.append(a);
            else
                dD.append(a);
        }
        delOFull.insert(du, dO);
        delSFull.insert(du, dS);
        delDFull.insert(du, dD);
        for (const AgvInfo &x : dO)
            queue.push_back(AgvDispSlot(x, AgvDispSlot::DelOver, du));
        for (const AgvInfo &x : dS)
            queue.push_back(AgvDispSlot(x, AgvDispSlot::DelSoon, du));
        for (const AgvInfo &x : dD)
            queue.push_back(AgvDispSlot(x, AgvDispSlot::DelDone, du));
    }

    const int totalSlots = queue.size();
    displayQueueTotal_ = totalSlots;
    shownCount_ = qBound(0, shownCount_, totalSlots);
    const QVector<AgvDispSlot> visibleSlots = queue.mid(0, shownCount_);

    QVector<AgvInfo> mineOverdue, mineSoon, mineDone;
    QVector<AgvInfo> commonOverdue, commonSoon, commonDone;
    QMap<QString, QVector<AgvInfo>> vDelO, vDelS, vDelD;
    for (const AgvDispSlot &sl : visibleSlots) {
        switch (sl.kind) {
        case AgvDispSlot::MineOver: mineOverdue.append(sl.info); break;
        case AgvDispSlot::MineSoon: mineSoon.append(sl.info); break;
        case AgvDispSlot::MineDone: mineDone.append(sl.info); break;
        case AgvDispSlot::ComOver: commonOverdue.append(sl.info); break;
        case AgvDispSlot::ComSoon: commonSoon.append(sl.info); break;
        case AgvDispSlot::ComDone: commonDone.append(sl.info); break;
        case AgvDispSlot::DelOver: vDelO[sl.delegUser].append(sl.info); break;
        case AgvDispSlot::DelSoon: vDelS[sl.delegUser].append(sl.info); break;
        case AgvDispSlot::DelDone: vDelD[sl.delegUser].append(sl.info); break;
        }
    }

    const bool hasMineFull = !mO.isEmpty() || !mS.isEmpty() || !mD.isEmpty();
    const bool hasCommonFull = !cO.isEmpty() || !cS.isEmpty() || !cD.isEmpty();

    auto addSubSectionStyled = [&](QVBoxLayout *parentLayout, const QString &title,
                                   const QVector<AgvInfo> &items, int fullCount, bool defaultExpanded,
                                   CollapsibleSection::SectionStyle subStyle) {
        if (fullCount <= 0)
            return;
        CollapsibleSection *sec = new CollapsibleSection(
            QStringLiteral("%1 (%2)").arg(title).arg(fullCount),
            defaultExpanded, s, content, subStyle);
        for (const AgvInfo &info : items) {
            AgvItem *item = new AgvItem(info, s, sec);
            connect(item, &AgvItem::openDetailsRequested, this, [this](const QString &id) {
                emit openAgvDetails(id);
            });
            sec->contentLayout()->addWidget(item);
        }
        parentLayout->addWidget(sec);
    };

    if (hasMineFull) {
        const int mineFullCount = mO.size() + mS.size() + mD.size();
        CollapsibleSection *mineParent = new CollapsibleSection(
            QStringLiteral("Ваши (%1)").arg(mineFullCount), true, s, content, CollapsibleSection::StyleMine);
        addSubSectionStyled(mineParent->contentLayout(), QStringLiteral("Просроченные"),
                            mineOverdue, mO.size(), true, CollapsibleSection::StyleOverdue);
        addSubSectionStyled(mineParent->contentLayout(), QStringLiteral("Скоро обслуживание"),
                            mineSoon, mS.size(), true, CollapsibleSection::StyleSoon);
        addSubSectionStyled(mineParent->contentLayout(), QStringLiteral("Обслужены"),
                            mineDone, mD.size(), true, CollapsibleSection::StyleDone);
        layout->addWidget(mineParent);
    }
    if (hasCommonFull) {
        const int commonFullCount = cO.size() + cS.size() + cD.size();
        const bool expandCommon = !hasMineFull;
        CollapsibleSection *commonParent = new CollapsibleSection(
            QStringLiteral("Общие (%1)").arg(commonFullCount), expandCommon, s, content, CollapsibleSection::StyleCommon);
        addSubSectionStyled(commonParent->contentLayout(), QStringLiteral("Просроченные"),
                            commonOverdue, cO.size(), expandCommon, CollapsibleSection::StyleOverdue);
        addSubSectionStyled(commonParent->contentLayout(), QStringLiteral("Скоро обслуживание"),
                            commonSoon, cS.size(), expandCommon, CollapsibleSection::StyleSoon);
        addSubSectionStyled(commonParent->contentLayout(), QStringLiteral("Обслужены"),
                            commonDone, cD.size(), expandCommon, CollapsibleSection::StyleDone);
        layout->addWidget(commonParent);
    }

    if (curRole == QStringLiteral("admin") || curRole == QStringLiteral("tech")) {
        for (const QString &delegatedUser : delUsers) {
            const QVector<AgvInfo> &fullList = delFull.value(delegatedUser);
            if (fullList.isEmpty())
                continue;
            CollapsibleSection *delegatedParent = new CollapsibleSection(
                QStringLiteral("%1 (%2)").arg(delegatedUser).arg(fullList.size()),
                false, s, content, CollapsibleSection::StyleDelegated);
            addSubSectionStyled(delegatedParent->contentLayout(), QStringLiteral("Просроченные"),
                                vDelO.value(delegatedUser), delOFull.value(delegatedUser).size(), false,
                                CollapsibleSection::StyleOverdue);
            addSubSectionStyled(delegatedParent->contentLayout(), QStringLiteral("Скоро обслуживание"),
                                vDelS.value(delegatedUser), delSFull.value(delegatedUser).size(), false,
                                CollapsibleSection::StyleSoon);
            addSubSectionStyled(delegatedParent->contentLayout(), QStringLiteral("Обслужены"),
                                vDelD.value(delegatedUser), delDFull.value(delegatedUser).size(), false,
                                CollapsibleSection::StyleDone);
            layout->addWidget(delegatedParent);
        }
    }

    if (shownCount_ < totalSlots) {
        const int rem = totalSlots - shownCount_;
        const int nextN = qMin(batchSize_, rem);
        loadMoreBtn_->setText(QStringLiteral("Показать ещё %1").arg(nextN));
        loadMoreBtn_->show();
        layout->addWidget(loadMoreBtn_, 0, Qt::AlignHCenter);
    } else {
        loadMoreBtn_->hide();
    }

    layout->addStretch();
    hasRenderedState_ = true;
    if (content) {
        content->setUpdatesEnabled(true);
        content->update();
    }
    runListAppearSmokeTest(1);
}

bool ListAgvInfo::hasRenderedState() const
{
    return hasRenderedState_;
}

void ListAgvInfo::runListAppearSmokeTest(int expectedVisible)
{
    if (expectedVisible <= 0 || appearRetryLeft_ <= 0)
        return;

    QTimer::singleShot(80, this, [this, expectedVisible]() {
        if (!content)
            return;
        const int rendered = content->findChildren<AgvItem*>().size();
        if (rendered > 0)
            return;

        --appearRetryLeft_;
        qDebug() << "ListAgvInfo smoke-test: список пуст после рендера, повторная отрисовка. retry="
                 << appearRetryLeft_ << "expectedVisible=" << expectedVisible;
        rebuildShownChunk();
    });
}
