#include "main_shell_bridge.h"

#include "app_session.h"
#include "authdialog_qml.h"
#include "db_users.h"
#include "notifications_logs.h"
#include "internal/leftmenu_settings_dialogs.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QProcess>
#include <QSet>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QVBoxLayout>
#include <algorithm>

namespace {

static QString severityFromDaysLeft(int daysLeft)
{
    if (daysLeft <= 3)
        return QStringLiteral("overdue");
    if (daysLeft < 7)
        return QStringLiteral("soon");
    return QStringLiteral("planned");
}

} // namespace

MainShellBridge::MainShellBridge(QObject *parent)
    : QObject(parent)
{
    refreshRole();
}

QString MainShellBridge::currentUsername() const
{
    return AppSession::currentUsername();
}

QString MainShellBridge::currentUserRole() const
{
    return m_role;
}

void MainShellBridge::refreshRole()
{
    m_role = getUserRole(AppSession::currentUsername());
}

QVariantMap MainShellBridge::loadSystemStatus() const
{
    QVariantMap out;
    out.insert(QStringLiteral("active"), 0);
    out.insert(QStringLiteral("maintenance"), 0);
    out.insert(QStringLiteral("error"), 0);
    out.insert(QStringLiteral("disabled"), 0);

    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen())
        return out;

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT agv_id, status FROM agv_list"));
    if (!q.exec())
        return out;

    QStringList allAgvIds;
    QMap<QString, QString> statusMap;
    while (q.next()) {
        const QString agvId = q.value(0).toString();
        const QString status = q.value(1).toString().trimmed().toLower();
        allAgvIds << agvId;
        statusMap.insert(agvId, status);
    }

    QSet<QString> overdueAgvs;
    QSet<QString> soonAgvs;
    QSqlQuery tasksQ(db);
    tasksQ.prepare(QStringLiteral(
        "SELECT agv_id,"
        " MAX(CASE WHEN next_date <= DATE_ADD(CURDATE(), INTERVAL 3 DAY) THEN 1 ELSE 0 END) AS has_overdue,"
        " MAX(CASE WHEN next_date > DATE_ADD(CURDATE(), INTERVAL 3 DAY)"
        "          AND next_date <= DATE_ADD(CURDATE(), INTERVAL 6 DAY)"
        "     THEN 1 ELSE 0 END) AS has_soon"
        " FROM agv_tasks"
        " GROUP BY agv_id"));
    if (tasksQ.exec()) {
        while (tasksQ.next()) {
            const QString agvId = tasksQ.value(0).toString();
            if (tasksQ.value(1).toInt() > 0)
                overdueAgvs.insert(agvId);
            if (tasksQ.value(2).toInt() > 0)
                soonAgvs.insert(agvId);
        }
    }

    int active = 0;
    int maintenance = 0;
    int error = 0;
    int disabled = 0;
    for (const QString &agvId : allAgvIds) {
        const QString status = statusMap.value(agvId);
        if (status == QStringLiteral("offline") || status == QStringLiteral("disabled")
            || status == QStringLiteral("off")) {
            ++disabled;
        } else if (overdueAgvs.contains(agvId)) {
            ++error;
        } else if (soonAgvs.contains(agvId)) {
            ++maintenance;
        } else if (status == QStringLiteral("online") || status == QStringLiteral("working")) {
            ++active;
        } else {
            ++disabled;
        }
    }

    out.insert(QStringLiteral("active"), active);
    out.insert(QStringLiteral("maintenance"), maintenance);
    out.insert(QStringLiteral("error"), error);
    out.insert(QStringLiteral("disabled"), disabled);
    return out;
}

QVariantList MainShellBridge::loadCalendarEvents(int month, int year) const
{
    QVariantList out;
    const QDate from(year, month, 1);
    const QDate to = from.addMonths(1).addDays(-1);
    if (!from.isValid() || !to.isValid() || from > to)
        return out;

    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen())
        return out;

    const QDate today = QDate::currentDate();
    QSet<QString> completedToday;
    QSqlQuery qDoneToday(db);
    qDoneToday.prepare(QStringLiteral(
        "SELECT DISTINCT h.agv_id, h.task_name FROM agv_task_history h WHERE h.completed_at = :today"));
    qDoneToday.bindValue(QStringLiteral(":today"), today);
    if (qDoneToday.exec()) {
        while (qDoneToday.next()) {
            completedToday.insert(qDoneToday.value(0).toString().trimmed()
                                  + QStringLiteral("||")
                                  + qDoneToday.value(1).toString().trimmed());
        }
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT t.agv_id, t.task_name, t.next_date, a.assigned_user, t.assigned_to "
        "FROM agv_tasks t "
        "JOIN agv_list a ON a.agv_id = t.agv_id "
        "WHERE t.next_date IS NOT NULL "
        "  AND LOWER(TRIM(a.status)) <> 'offline' "
        "  AND t.next_date BETWEEN :from AND :to "
        "ORDER BY t.next_date ASC"));
    q.bindValue(QStringLiteral(":from"), from);
    q.bindValue(QStringLiteral(":to"), to);
    if (!q.exec())
        return out;

    const QString currentUser = AppSession::currentUsername();
    const QString role = getUserRole(currentUser);

    while (q.next()) {
        const QString agvId = q.value(0).toString();
        const QString taskName = q.value(1).toString();
        const QDate nextDate = q.value(2).toDate();
        const QString assignedUser = q.value(3).toString().trimmed();
        const QString assignedTo = q.value(4).toString().trimmed();
        if (!nextDate.isValid())
            continue;

        if (role == QStringLiteral("viewer")) {
            const bool mineByTask = !assignedTo.isEmpty() && assignedTo == currentUser;
            const bool mineByAgv = !assignedUser.isEmpty() && assignedUser == currentUser;
            const bool isCommon = assignedTo.isEmpty() && assignedUser.isEmpty();
            if (!(mineByTask || mineByAgv || isCommon))
                continue;
        }

        const QString key = agvId.trimmed() + QStringLiteral("||") + taskName.trimmed();
        if (completedToday.contains(key))
            continue;

        QVariantMap ev;
        ev.insert(QStringLiteral("agvId"), agvId);
        ev.insert(QStringLiteral("taskTitle"), taskName);
        ev.insert(QStringLiteral("date"), nextDate.toString(Qt::ISODate));
        ev.insert(QStringLiteral("severity"), severityFromDaysLeft(today.daysTo(nextDate)));
        out.push_back(ev);
    }

    QSqlQuery qHist(db);
    qHist.prepare(QStringLiteral(
        "SELECT h.agv_id, h.task_name, DATE(h.completed_at) AS completed_day, a.assigned_user "
        "FROM agv_task_history h "
        "JOIN agv_list a ON a.agv_id = h.agv_id "
        "WHERE h.completed_at IS NOT NULL "
        "  AND LOWER(TRIM(a.status)) <> 'offline' "
        "  AND h.completed_at BETWEEN :from AND :to "
        "ORDER BY h.completed_at ASC"));
    qHist.bindValue(QStringLiteral(":from"), from);
    qHist.bindValue(QStringLiteral(":to"), to);
    if (qHist.exec()) {
        while (qHist.next()) {
            const QString histAssigned = qHist.value(3).toString().trimmed();
            if (role == QStringLiteral("viewer")
                && !histAssigned.isEmpty()
                && histAssigned != currentUser) {
                continue;
            }
            const QDate d = qHist.value(2).toDate();
            if (!d.isValid())
                continue;
            QVariantMap ev;
            ev.insert(QStringLiteral("agvId"), qHist.value(0).toString());
            ev.insert(QStringLiteral("taskTitle"), qHist.value(1).toString() + QStringLiteral(" (обслужена)"));
            ev.insert(QStringLiteral("date"), d.toString(Qt::ISODate));
            ev.insert(QStringLiteral("severity"), QStringLiteral("completed"));
            out.push_back(ev);
        }
    }

    return out;
}

QVariantList MainShellBridge::loadUpcomingMaintenance(int month, int year) const
{
    Q_UNUSED(month)
    Q_UNUSED(year)

    QVariantList out;
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen())
        return out;

    const QDate today = QDate::currentDate();
    QSet<QString> completedToday;
    QSqlQuery qDoneToday(db);
    qDoneToday.prepare(QStringLiteral(
        "SELECT DISTINCT h.agv_id, h.task_name FROM agv_task_history h WHERE h.completed_at = :today"));
    qDoneToday.bindValue(QStringLiteral(":today"), today);
    if (qDoneToday.exec()) {
        while (qDoneToday.next()) {
            completedToday.insert(qDoneToday.value(0).toString().trimmed()
                                  + QStringLiteral("||")
                                  + qDoneToday.value(1).toString().trimmed());
        }
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT t.agv_id, a.agv_id, t.task_name, t.next_date, a.assigned_user, t.assigned_to "
        "FROM agv_tasks t "
        "JOIN agv_list a ON a.agv_id = t.agv_id "
        "WHERE t.next_date IS NOT NULL "
        "  AND LOWER(TRIM(a.status)) <> 'offline' "
        "  AND t.next_date <= DATE_ADD(CURDATE(), INTERVAL 6 DAY) "
        "ORDER BY t.next_date ASC, t.task_name ASC"));
    if (!q.exec())
        return out;

    struct AgvAgg {
        QString agvId;
        QString agvName;
        QString assignedUser;
        int overdueCount = 0;
        int soonCount = 0;
        QDate bestOverdueDate;
        QString bestOverdueTask;
        QString bestOverdueDelegatedTo;
        QDate bestSoonDate;
        QString bestSoonTask;
        QString bestSoonDelegatedTo;
    };

    QMap<QString, AgvAgg> agg;
    const QString currentUser = AppSession::currentUsername();
    const QString role = getUserRole(currentUser);

    while (q.next()) {
        const QString agvId = q.value(0).toString();
        const QString agvName = q.value(1).toString();
        const QString taskName = q.value(2).toString();
        const QDate nextDate = q.value(3).toDate();
        const QString assignedUser = q.value(4).toString().trimmed();
        const QString assignedTo = q.value(5).toString().trimmed();
        if (!nextDate.isValid())
            continue;

        const QString key = agvId.trimmed() + QStringLiteral("||") + taskName.trimmed();
        if (completedToday.contains(key))
            continue;

        if (role == QStringLiteral("viewer")) {
            const bool mineByTask = !assignedTo.isEmpty() && assignedTo == currentUser;
            const bool mineByAgv = !assignedUser.isEmpty() && assignedUser == currentUser;
            const bool isCommon = assignedTo.isEmpty() && assignedUser.isEmpty();
            if (!(mineByTask || mineByAgv || isCommon))
                continue;
        }

        const int daysLeft = today.daysTo(nextDate);
        AgvAgg &a = agg[agvId];
        if (a.agvId.isEmpty()) {
            a.agvId = agvId;
            a.agvName = agvName;
            a.assignedUser = assignedUser;
        }

        if (daysLeft <= 3) {
            a.overdueCount++;
            if (!a.bestOverdueDate.isValid()
                || nextDate < a.bestOverdueDate
                || (nextDate == a.bestOverdueDate && taskName < a.bestOverdueTask)) {
                a.bestOverdueDate = nextDate;
                a.bestOverdueTask = taskName;
                a.bestOverdueDelegatedTo = assignedTo;
            }
        } else if (daysLeft < 7) {
            a.soonCount++;
            if (!a.bestSoonDate.isValid()
                || nextDate < a.bestSoonDate
                || (nextDate == a.bestSoonDate && taskName < a.bestSoonTask)) {
                a.bestSoonDate = nextDate;
                a.bestSoonTask = taskName;
                a.bestSoonDelegatedTo = assignedTo;
            }
        }
    }

    auto canViewerSee = [&](const AgvAgg &a, const QString &delegatedTo) {
        if (role != QStringLiteral("viewer"))
            return true;
        const bool mineByTask = !delegatedTo.isEmpty() && delegatedTo == currentUser;
        const bool mineByAgv = !a.assignedUser.isEmpty() && a.assignedUser == currentUser;
        const bool isCommon = delegatedTo.isEmpty() && a.assignedUser.isEmpty();
        return mineByTask || mineByAgv || isCommon;
    };
    auto assignedInfo = [&](const AgvAgg &a, const QString &delegatedTo) {
        if (!a.assignedUser.isEmpty())
            return QStringLiteral("за %1").arg(a.assignedUser);
        if (!delegatedTo.isEmpty())
            return QStringLiteral("кому делегирована: %1").arg(delegatedTo);
        return QStringLiteral("общая");
    };
    auto delegatedToMe = [&](const AgvAgg &a, const QString &delegatedTo) {
        return !delegatedTo.isEmpty() && delegatedTo == currentUser && a.assignedUser != currentUser;
    };

    for (const AgvAgg &a : agg) {
        if (a.overdueCount > 0 && canViewerSee(a, a.bestOverdueDelegatedTo)) {
            QVariantMap item;
            item.insert(QStringLiteral("agvId"), a.agvId);
            item.insert(QStringLiteral("agvName"), a.agvName);
            item.insert(QStringLiteral("taskName"), a.bestOverdueTask);
            item.insert(QStringLiteral("date"), a.bestOverdueDate.toString(Qt::ISODate));
            item.insert(QStringLiteral("count"), a.overdueCount);
            item.insert(QStringLiteral("severity"), QStringLiteral("red"));
            item.insert(QStringLiteral("assignedInfo"), assignedInfo(a, a.bestOverdueDelegatedTo));
            item.insert(QStringLiteral("isDelegatedToMe"), delegatedToMe(a, a.bestOverdueDelegatedTo));
            out.push_back(item);
        }
        if (a.soonCount > 0 && canViewerSee(a, a.bestSoonDelegatedTo)) {
            QVariantMap item;
            item.insert(QStringLiteral("agvId"), a.agvId);
            item.insert(QStringLiteral("agvName"), a.agvName);
            item.insert(QStringLiteral("taskName"), a.bestSoonTask);
            item.insert(QStringLiteral("date"), a.bestSoonDate.toString(Qt::ISODate));
            item.insert(QStringLiteral("count"), a.soonCount);
            item.insert(QStringLiteral("severity"), QStringLiteral("orange"));
            item.insert(QStringLiteral("assignedInfo"), assignedInfo(a, a.bestSoonDelegatedTo));
            item.insert(QStringLiteral("isDelegatedToMe"), delegatedToMe(a, a.bestSoonDelegatedTo));
            out.push_back(item);
        }
    }

    std::sort(out.begin(), out.end(), [](const QVariant &va, const QVariant &vb) {
        const QDate a = QDate::fromString(va.toMap().value(QStringLiteral("date")).toString(), Qt::ISODate);
        const QDate b = QDate::fromString(vb.toMap().value(QStringLiteral("date")).toString(), Qt::ISODate);
        return a < b;
    });
    return out;
}

int MainShellBridge::unreadNotificationsCount() const
{
    const QString username = AppSession::currentUsername();
    if (username.isEmpty())
        return 0;
    return unreadCountForUser(username);
}

QVariantMap MainShellBridge::loadCurrentUserProfile() const
{
    QVariantMap out;
    const QString username = AppSession::currentUsername();
    if (username.isEmpty())
        return out;

    UserInfo info;
    if (!loadUserProfile(username, info))
        return out;

    out.insert(QStringLiteral("username"), info.username);
    out.insert(QStringLiteral("role"), info.role);
    out.insert(QStringLiteral("fullName"), info.fullName);
    out.insert(QStringLiteral("employeeId"), info.employeeId);
    out.insert(QStringLiteral("position"), info.position);
    out.insert(QStringLiteral("department"), info.department);
    out.insert(QStringLiteral("mobile"), info.mobile);
    out.insert(QStringLiteral("extPhone"), info.extPhone);
    out.insert(QStringLiteral("email"), info.email);
    out.insert(QStringLiteral("telegram"), info.telegram);
    return out;
}

bool MainShellBridge::saveCurrentUserProfile(const QVariantMap &profile)
{
    const QString username = AppSession::currentUsername();
    if (username.isEmpty())
        return false;

    UserInfo info;
    if (!loadUserProfile(username, info)) {
        info.username = username;
        info.role = getUserRole(username);
    }

    info.fullName = profile.value(QStringLiteral("fullName")).toString();
    info.employeeId = profile.value(QStringLiteral("employeeId")).toString();
    info.position = profile.value(QStringLiteral("position")).toString();
    info.department = profile.value(QStringLiteral("department")).toString();
    info.mobile = profile.value(QStringLiteral("mobile")).toString();
    info.extPhone = profile.value(QStringLiteral("extPhone")).toString();
    info.email = profile.value(QStringLiteral("email")).toString();
    info.telegram = profile.value(QStringLiteral("telegram")).toString();

    QString err;
    const bool ok = saveUserProfile(info, err);
    if (ok)
        emit profileUpdated();
    return ok;
}

bool MainShellBridge::switchAccount()
{
    AuthDialogQml dlg(nullptr);
    if (dlg.exec() != QDialog::Accepted)
        return false;

    const UserInfo user = dlg.user();
    if (user.username.isEmpty())
        return false;

    AppSession::setCurrentUsername(user.username);
    enableRememberMe(user.username);
    touchUserPresence(user.username);
    refreshRole();
    emit sessionChanged();
    return true;
}

void MainShellBridge::changeAvatar()
{
    const QString username = AppSession::currentUsername();
    if (username.isEmpty())
        return;

    const QString file = QFileDialog::getOpenFileName(
        nullptr,
        QStringLiteral("Выберите изображение"),
        QString(),
        QStringLiteral("Изображения (*.png *.jpg *.jpeg *.bmp)"));
    if (file.isEmpty())
        return;

    QPixmap pm(file);
    if (pm.isNull()) {
        QMessageBox::warning(nullptr, QStringLiteral("Ошибка"),
                             QStringLiteral("Не удалось загрузить изображение."));
        return;
    }

    QString err;
    if (!saveUserAvatarToDb(username, pm, err)) {
        QMessageBox::warning(nullptr, QStringLiteral("Ошибка"),
                             QStringLiteral("Не удалось сохранить аватар в базу данных."));
        return;
    }

    emit profileUpdated();
    QMessageBox::information(nullptr, QStringLiteral("Готово"),
                             QStringLiteral("Аватар успешно обновлён."));
}

void MainShellBridge::changeLanguage()
{
    QDialog dlg(nullptr);
    dlg.setWindowTitle(QStringLiteral("Сменить язык"));
    dlg.setModal(true);
    dlg.resize(320, 140);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    QComboBox *combo = new QComboBox(&dlg);
    combo->addItem(QStringLiteral("Русский"), QStringLiteral("ru"));
    combo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    combo->addItem(QStringLiteral("中文"), QStringLiteral("zh"));

    QString cfgPath = QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini");
    QSettings cfg(cfgPath, QSettings::IniFormat);
    const QString cur = cfg.value(QStringLiteral("language"), QStringLiteral("ru")).toString();
    const int idx = combo->findData(cur);
    if (idx >= 0)
        combo->setCurrentIndex(idx);
    layout->addWidget(combo);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(box);
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    cfg.setValue(QStringLiteral("language"), combo->currentData().toString());
    cfg.sync();

    const auto ans = QMessageBox::question(
        nullptr,
        QStringLiteral("Язык"),
        QStringLiteral("Язык сохранён. Перезапустить приложение сейчас?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (ans == QMessageBox::Yes) {
        const QString appPath = QCoreApplication::applicationFilePath();
        const QStringList args = QCoreApplication::arguments().mid(1);
        QProcess::startDetached(appPath, args);
        qApp->quit();
    }
}

void MainShellBridge::showAboutDialog()
{
    QMessageBox::information(
        nullptr,
        QStringLiteral("О программе"),
        QStringLiteral("AGV Manager\nВерсия 1.0.0\nГорьковский автомобильный завод"));
}

void MainShellBridge::showSettingsDialog()
{
    LeftMenuDialogs::showAppSettingsDialog(nullptr);
}
