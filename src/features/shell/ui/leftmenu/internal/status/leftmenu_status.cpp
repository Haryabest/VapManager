#include "leftmenu.h"

#include "app_session.h"
#include "databus.h"
#include "db_users.h"
#include "notifications_logs.h"

#include <QDebug>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVBoxLayout>
#include <algorithm>

namespace {

constexpr int kUpcomingMaintenanceUiMax = 40;
constexpr int kMaintenanceNotifyMaxPerRun = 25;

void checkAndSendMaintenanceNotifications(const QVector<MaintenanceItemData> &upcoming)
{
    QSet<QString> sentThisRun;
    int processed = 0;
    for (const auto &item : upcoming) {
        if (++processed > kMaintenanceNotifyMaxPerRun)
            break;
        if (item.assignedUser.isEmpty()) continue;
        if (sentThisRun.contains(item.agvId)) continue;
        if (wasMaintenanceNotificationSentToday(item.agvId)) continue;

        QString title = (item.severity == "red") ? "Просрочено" : "Скоро обслуживание";
        QString msg = (item.severity == "red")
            ? QString("AGV %1: просрочено обслуживание (%2 задач(и))").arg(item.agvName).arg(item.details)
            : QString("AGV %1: скоро обслуживание (%2 задач(и))").arg(item.agvName).arg(item.details);

        addNotificationForUser(item.assignedUser, title, msg);
        markMaintenanceNotificationSentToday(item.agvId);
        sentThisRun.insert(item.agvId);
    }
}

} // namespace

QVector<MaintenanceItemData> leftMenu::loadUpcomingMaintenance(int month, int year)
{
    Q_UNUSED(month)
    Q_UNUSED(year)

    QVector<MaintenanceItemData> list;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        qDebug() << "loadUpcomingMaintenance: DB NOT OPEN";
        return list;
    }

    QSqlQuery q(db);
    const QDate today = QDate::currentDate();
    QSet<QString> completedToday;
    QSqlQuery qDoneToday(db);
    qDoneToday.prepare(R"(
        SELECT DISTINCT h.agv_id, h.task_name
        FROM agv_task_history h
        WHERE h.completed_at = :today
    )");
    qDoneToday.bindValue(":today", today);
    if (qDoneToday.exec()) {
        while (qDoneToday.next()) {
            const QString key = qDoneToday.value(0).toString().trimmed()
                                + "||" +
                                qDoneToday.value(1).toString().trimmed();
            completedToday.insert(key);
        }
    }

    q.prepare(R"(
        SELECT
            t.agv_id,
            a.agv_id,
            t.task_name,
            t.next_date,
            a.assigned_user,
            t.assigned_to
        FROM agv_tasks t
        JOIN agv_list a ON a.agv_id = t.agv_id
        WHERE t.next_date IS NOT NULL
          AND a.status IN ('online', 'working')
          AND t.next_date <= CURRENT_DATE + 6
        ORDER BY t.next_date ASC, t.task_name ASC
    )");

    if (!q.exec()) {
        qDebug() << "loadUpcomingMaintenance SQL error:" << q.lastError().text();
        return list;
    }

    struct AgvAgg {
        QString agvId;
        QString agvName;
        QString assignedUser;
        QString delegatedTo;  // assigned_to из лучшей задачи
        int overdueCount = 0;
        int soonCount = 0;
        QDate bestOverdueDate;
        QString bestOverdueTaskName;
        QString bestOverdueDelegatedTo;
        QDate bestSoonDate;
        QString bestSoonTaskName;
        QString bestSoonDelegatedTo;
    };

    QMap<QString, AgvAgg> agg;
    const QString currentUser = AppSession::currentUsername();
    const QString curRole = getUserRole(currentUser);

    while (q.next()) {
        QString agvId    = q.value(0).toString();
        QString agvName  = q.value(1).toString();
        QString taskName = q.value(2).toString();
        QDate   nextDate = q.value(3).toDate();
        QString assignedUser = q.value(4).toString().trimmed();
        QString assignedTo = q.value(5).toString().trimmed();

        if (!nextDate.isValid())
            continue;

        const QString key = agvId.trimmed() + "||" + taskName.trimmed();
        if (completedToday.contains(key))
            continue;

        if (curRole == "viewer") {
            const bool mineByTask = !assignedTo.isEmpty() && assignedTo == currentUser;
            const bool mineByAgv = !assignedUser.isEmpty() && assignedUser == currentUser;
            const bool isCommon = assignedTo.isEmpty() && assignedUser.isEmpty();
            if (!(mineByTask || mineByAgv || isCommon))
                continue;
        }

        int daysLeft = QDate::currentDate().daysTo(nextDate);

        AgvAgg &a = agg[agvId];
        if (a.agvId.isEmpty()) {
            a.agvId = agvId;
            a.agvName = agvName;
            a.assignedUser = assignedUser;
        }

        if (daysLeft < 0) {
            a.overdueCount++;

            if (!a.bestOverdueDate.isValid() ||
                nextDate < a.bestOverdueDate ||
                (nextDate == a.bestOverdueDate && taskName < a.bestOverdueTaskName))
            {
                a.bestOverdueDate = nextDate;
                a.bestOverdueTaskName = taskName;
                a.bestOverdueDelegatedTo = assignedTo;
            }
        } else if (daysLeft < 7) {
            a.soonCount++;

            if (!a.bestSoonDate.isValid() ||
                nextDate < a.bestSoonDate ||
                (nextDate == a.bestSoonDate && taskName < a.bestSoonTaskName))
            {
                a.bestSoonDate = nextDate;
                a.bestSoonTaskName = taskName;
                a.bestSoonDelegatedTo = assignedTo;
            }
        }
    }

    for (const AgvAgg &a : agg) {
        auto canViewerSee = [&](const QString &delegatedTo) {
            if (curRole != "viewer") return true;
            const bool mineByTask = !delegatedTo.isEmpty() && delegatedTo == currentUser;
            const bool mineByAgv = !a.assignedUser.isEmpty() && a.assignedUser == currentUser;
            const bool isCommon = delegatedTo.isEmpty() && a.assignedUser.isEmpty();
            return mineByTask || mineByAgv || isCommon;
        };
        auto assignInfo = [&](const QString &delegatedTo) {
            if (!a.assignedUser.isEmpty()) return QString("за %1").arg(a.assignedUser);
            if (!delegatedTo.isEmpty()) return QString("кому делегирована: %1").arg(delegatedTo);
            return QString("общая");
        };
        auto isDelegatedToMe = [&](const QString &delegatedTo) {
            return !delegatedTo.isEmpty() && delegatedTo == currentUser && a.assignedUser != currentUser;
        };
        if (a.overdueCount > 0 && canViewerSee(a.bestOverdueDelegatedTo)) {
            MaintenanceItemData item;
            item.agvId = a.agvId;
            item.agvName = a.agvName;
            item.type = a.bestOverdueTaskName;
            item.date = a.bestOverdueDate;
            item.details = QString::number(a.overdueCount);
            item.severity = "red";
            item.assignedInfo = assignInfo(a.bestOverdueDelegatedTo);
            item.assignedUser = a.assignedUser.isEmpty() ? a.bestOverdueDelegatedTo : a.assignedUser;
            item.isDelegatedToMe = isDelegatedToMe(a.bestOverdueDelegatedTo);
            list.append(item);
        }

        if (a.soonCount > 0 && canViewerSee(a.bestSoonDelegatedTo)) {
            MaintenanceItemData item;
            item.agvId = a.agvId;
            item.agvName = a.agvName;
            item.type = a.bestSoonTaskName;
            item.date = a.bestSoonDate;
            item.details = QString::number(a.soonCount);
            item.severity = "orange";
            item.assignedInfo = assignInfo(a.bestSoonDelegatedTo);
            item.assignedUser = a.assignedUser.isEmpty() ? a.bestSoonDelegatedTo : a.assignedUser;
            item.isDelegatedToMe = isDelegatedToMe(a.bestSoonDelegatedTo);
            list.append(item);
        }
    }

    std::sort(list.begin(), list.end(),
        [](const MaintenanceItemData &a, const MaintenanceItemData &b){
            return a.date < b.date;
        }
    );

    return list;
}



SystemStatus leftMenu::loadSystemStatus()
{
    SystemStatus st;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return st;

    QSqlQuery q(db);
    if (!q.exec(R"(
        WITH task_flags AS (
            SELECT agv_id,
                   MAX(CASE WHEN next_date <= CURRENT_DATE + 3 THEN 1 ELSE 0 END) AS has_overdue,
                   MAX(CASE WHEN next_date > CURRENT_DATE + 3
                             AND next_date <= CURRENT_DATE + 6 THEN 1 ELSE 0 END) AS has_soon
            FROM agv_tasks
            WHERE next_date IS NOT NULL
            GROUP BY agv_id
        )
        SELECT
            SUM(CASE WHEN LOWER(TRIM(a.status)) IN ('offline', 'disabled', 'off') THEN 1 ELSE 0 END),
            SUM(CASE WHEN LOWER(TRIM(a.status)) NOT IN ('offline', 'disabled', 'off')
                      AND COALESCE(tf.has_overdue, 0) = 1 THEN 1 ELSE 0 END),
            SUM(CASE WHEN LOWER(TRIM(a.status)) NOT IN ('offline', 'disabled', 'off')
                      AND COALESCE(tf.has_overdue, 0) = 0 AND COALESCE(tf.has_soon, 0) = 1 THEN 1 ELSE 0 END),
            SUM(CASE WHEN LOWER(TRIM(a.status)) NOT IN ('offline', 'disabled', 'off')
                      AND COALESCE(tf.has_overdue, 0) = 0 AND COALESCE(tf.has_soon, 0) = 0
                      AND LOWER(TRIM(a.status)) IN ('online', 'working') THEN 1 ELSE 0 END)
        FROM agv_list a
        LEFT JOIN task_flags tf ON tf.agv_id = a.agv_id
    )"))
        return st;

    if (q.next()) {
        st.disabled = q.value(0).toInt();
        st.error = q.value(1).toInt();
        st.maintenance = q.value(2).toInt();
        st.active = q.value(3).toInt();
    }
    return st;
}
void leftMenu::updateAgvCounter()
{
    if (!agvCounter)
        return;

    int count = 0;

        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (db.isOpen()) {
            QSqlQuery q(db);
            if (q.exec("SELECT COUNT(*) FROM agv_list") && q.next()) {
                count = q.value(0).toInt();
        }
    }

    agvCounter->setText(QString::number(count));
    agvCounter->setVisible(true);
}
void leftMenu::updateUpcomingMaintenance()
{
    if (!rightUpcomingMaintenanceFrame)
        return;

    // Находим scroll area → contentContainer → contentLayout
    QScrollArea *scroll = rightUpcomingMaintenanceFrame->findChild<QScrollArea *>();
    if (!scroll)
        return;

    QWidget *contentContainer = scroll->widget();
    if (!contentContainer)
        return;

    QVBoxLayout *contentLayout = qobject_cast<QVBoxLayout*>(contentContainer->layout());
    if (!contentLayout)
        return;

    // Удаляем старые элементы
    QLayoutItem *child;
    while ((child = contentLayout->takeAt(0)) != nullptr) {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }

    // Загружаем новые данные
    QVector<MaintenanceItemData> upcoming =
        loadUpcomingMaintenance(selectedMonth_, selectedYear_);

    std::sort(upcoming.begin(), upcoming.end(),
        [](const MaintenanceItemData &a, const MaintenanceItemData &b){
            return a.date < b.date;
        }
    );

    const int upcomingTotal = upcoming.size();
    checkAndSendMaintenanceNotifications(upcoming);
    DataBus::instance().triggerNotificationsChanged();

    // Функция добавления элемента ТО (копируем из initUI)
    auto addMaintenanceItem = [&](const MaintenanceItemData &item){
        QColor bgColor, btnColor;
        QString iconPath;

        if (item.severity == "red") {
            bgColor = QColor(255,0,0,33);
            btnColor = QColor(235,61,61,204);
            iconPath = ":/new/mainWindowIcons/noback/alert.png";
        }
        else if (item.severity == "orange") {
            bgColor = QColor(255,136,0,33);
            btnColor = QColor(255,196,0,204);
            iconPath = ":/new/mainWindowIcons/noback/warning.png";
        }
        else return;

        QFrame *itemFrame = new QFrame(contentContainer);
        itemFrame->setStyleSheet(QString(
            "QFrame{background-color:rgba(%1,%2,%3,%4);border-radius:10px;}"
        ).arg(bgColor.red()).arg(bgColor.green()).arg(bgColor.blue()).arg(bgColor.alpha()));

        QHBoxLayout *itemLayout = new QHBoxLayout(itemFrame);
        itemLayout->setContentsMargins(s(10), s(8), s(10), s(8));
        itemLayout->setSpacing(s(12));

        QLabel *iconLabel = new QLabel(itemFrame);
        iconLabel->setFixedSize(s(32), s(32));
        iconLabel->setPixmap(
            QPixmap(iconPath).scaled(s(32), s(32), Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );
        iconLabel->setStyleSheet("background:transparent;");
        iconLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        itemLayout->addWidget(iconLabel);

        QLabel *textLabel = new QLabel(itemFrame);

        const QString serviceLabel =
            (item.severity == "red") ? "Текущее обслуживание" : "Скоро обслуживание";

        QString topLine = QString(
            "<span style='font-weight:800; color:#000000;'>%1 — %2 — %3 задач(и)</span>"
        ).arg(item.agvName).arg(serviceLabel).arg(item.details);

        QString assignSuffix = item.assignedInfo.isEmpty() ? "общая" : item.assignedInfo;
        QString bottomLine = QString(
            "<span style='color:#777777;'>%1 — %2 — %3</span>"
        ).arg(item.date.toString("dd.MM.yyyy")).arg(item.type).arg(assignSuffix);

        textLabel->setText(topLine +
            "<br style='line-height:200%; font-size:8px;'>" +
            bottomLine);

        textLabel->setStyleSheet(QString(
            "background:transparent;font-family:Inter;font-size:%1px;"
        ).arg(s(14)));
        textLabel->setWordWrap(true);

        itemLayout->addWidget(textLabel, 1);

        QPushButton *showBtn = new QPushButton("Показать", itemFrame);
        showBtn->setStyleSheet(QString(
            "QPushButton{background-color:rgba(%1,%2,%3,%4);color:white;font-family:Inter;font-size:%5px;"
            "font-weight:700;border-radius:8px;padding:%6px %7px;border:none;} "
        )
        .arg(btnColor.red()).arg(btnColor.green()).arg(btnColor.blue()).arg(btnColor.alpha())
        .arg(s(13)).arg(s(4)).arg(s(10)));

        connect(showBtn, &QPushButton::clicked, this, [this, item](){
            showAgvDetailInfo(item.agvId);
            if (agvSettingsPage)
                agvSettingsPage->highlightTask(item.type);
        });

        itemLayout->addWidget(showBtn, 0, Qt::AlignVCenter | Qt::AlignRight);

        contentLayout->addWidget(itemFrame);
    };

    QVector<MaintenanceItemData> delegated, rest;
    for (const auto &item : upcoming) {
        if (item.isDelegatedToMe)
            delegated.append(item);
        else
            rest.append(item);
    }

    int uiSlotsLeft = kUpcomingMaintenanceUiMax;
    auto addCapped = [&](const QVector<MaintenanceItemData> &items) {
        for (const auto &item : items) {
            if (uiSlotsLeft <= 0)
                break;
            addMaintenanceItem(item);
            --uiSlotsLeft;
        }
    };

    if (!delegated.isEmpty()) {
        QLabel *delegatedHeader = new QLabel("Делегировано вам", contentContainer);
        delegatedHeader->setStyleSheet(QString(
            "background:transparent;font-family:Inter;font-size:%1px;font-weight:800;color:#0F00DB;padding:%2px 0;"
        ).arg(s(14)).arg(s(4)));
        contentLayout->addWidget(delegatedHeader);
        addCapped(delegated);
    }
    if (!rest.isEmpty() && !delegated.isEmpty() && uiSlotsLeft > 0) {
        QFrame *sep = new QFrame(contentContainer);
        sep->setFrameShape(QFrame::HLine);
        sep->setFixedHeight(1);
        sep->setStyleSheet("background:#ddd;border:none;");
        contentLayout->addWidget(sep);
    }
    addCapped(rest);

    const int shownCount = kUpcomingMaintenanceUiMax - uiSlotsLeft;
    const int hiddenCount = qMax(0, upcomingTotal - shownCount);
    if (hiddenCount > 0) {
        QLabel *moreLbl = new QLabel(
            QStringLiteral("… и ещё %1 AGV с просроченным/скорым ТО (откройте список AGV)").arg(hiddenCount),
            contentContainer);
        moreLbl->setWordWrap(true);
        moreLbl->setStyleSheet(QString(
            "background:transparent;font-family:Inter;font-size:%1px;font-weight:700;color:#6B7280;padding:%2px 0;"
        ).arg(s(13)).arg(s(6)));
        contentLayout->addWidget(moreLbl);
    }

    contentLayout->addStretch();
}

void leftMenu::updateSystemStatus()
{
    if (!statusWidget_)
        return;

    SystemStatus st = loadSystemStatus();
    int total = st.active + st.maintenance + st.error + st.disabled;

    statusWidget_->setActiveAGVCurrentCount(st.active);
    statusWidget_->setActiveAGVTotalCount(total);
    statusWidget_->setMaintenanceCurrentCount(st.maintenance);
    statusWidget_->setMaintenanceTotalCount(total);
    statusWidget_->setErrorCurrentCount(st.error);
    statusWidget_->setErrorTotalCount(total);
    statusWidget_->setDisabledCurrentCount(st.disabled);
    statusWidget_->setDisabledTotalCount(total);
}
