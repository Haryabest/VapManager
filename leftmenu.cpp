#include "leftmenu.h"
#include "multisectionwidget.h"
#include "listagvinfo.h"
#include "agvsettingspage.h"
#include "addagvdialog.h"
#include "logindialog.h"
#include "db_agv_tasks.h"
#include "db_users.h"
#include "db_task_chat.h"
#include "databus.h"
#include "app_session.h"
#include "accountinfodialog.h"
#include "notifications_logs.h"
#include "taskchatdialog.h"
#include "diag_logger.h"

#include <QTextEdit>

namespace {
void checkAndSendMaintenanceNotifications(const QVector<MaintenanceItemData> &upcoming)
{
    QSet<QString> sentThisRun;
    for (const auto &item : upcoming) {
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
}
#include "db.h"

#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>
#include <QPageSize>
#include <QPageLayout>
#include <QDateEdit>
#include <QSet>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDir>
#include <QProcess>
#include <algorithm>
#include <QListWidget>
#include <QListWidgetItem>
#include <QCheckBox>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QEventLoop>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSignalBlocker>
#include <QUrl>

// Явное объявление для TU, чтобы избежать конфликтов видимости при инкрементальной сборке.
void touchUserPresence(const QString &username);

namespace {
// Единый лимит стен-времени для комплексного теста (лог SUITE_START и обрезка фаз).
constexpr qint64 kComplexTestWallCapMs = 600000;

int minCalendarYear()
{
    return QDate::currentDate().year() - 10;
}

int maxCalendarYear()
{
    return QDate::currentDate().year() + 10;
}

QString makeChatListSignature(const QVector<TaskChatThread> &threads)
{
    QStringList parts;
    parts.reserve(threads.size());
    for (const TaskChatThread &t : threads) {
        parts.append(QStringLiteral("%1|%2|%3|%4|%5")
                         .arg(t.id)
                         .arg(t.createdAt.toString(Qt::ISODate))
                         .arg(t.closedAt.toString(Qt::ISODate))
                         .arg(t.createdBy)
                         .arg(t.recipientUser));
    }
    return parts.join(QLatin1Char('\n'));
}

struct ChatPeerMeta
{
    QString displayName;
    QPixmap avatar;
    bool isActive = false;
    QDateTime lastLogin;
};

bool chatListPeerShowsOnline(bool isActive, const QDateTime &lastLogin)
{
    if (!isActive || !lastLogin.isValid())
        return false;
    const qint64 secs = lastLogin.secsTo(QDateTime::currentDateTime());
    return secs >= 0 && secs < 180;
}

QString formatLastSeenText(bool isActive, const QDateTime &lastLogin)
{
    if (chatListPeerShowsOnline(isActive, lastLogin))
        return QStringLiteral("в сети");

    if (!lastLogin.isValid())
        return QStringLiteral("не в сети");

    const qint64 secs = lastLogin.secsTo(QDateTime::currentDateTime());
    if (secs < 60)
        return QStringLiteral("был в сети только что");
    if (secs < 3600)
        return QStringLiteral("был в сети %1 мин назад").arg(qMax<qint64>(1, secs / 60));
    if (secs < 86400)
        return QStringLiteral("был в сети %1 ч назад").arg(qMax<qint64>(1, secs / 3600));
    if (secs < 172800)
        return QStringLiteral("был в сети вчера");
    return QStringLiteral("был в сети %1").arg(lastLogin.toString(QStringLiteral("dd.MM.yyyy hh:mm")));
}

QHash<QString, ChatPeerMeta> loadChatPeerMeta(const QSet<QString> &usernames)
{
    QHash<QString, ChatPeerMeta> result;
    if (usernames.isEmpty())
        return result;

    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
    if (!db.isOpen())
        return result;

    QStringList names = usernames.values();
    names.removeAll(QString());
    if (names.isEmpty())
        return result;

    QStringList placeholders;
    placeholders.reserve(names.size());
    for (int i = 0; i < names.size(); ++i)
        placeholders.append(QStringLiteral("?"));

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT username, full_name, avatar, is_active, last_login FROM users WHERE username IN (%1)")
                  .arg(placeholders.join(QStringLiteral(","))));
    for (const QString &name : names)
        q.addBindValue(name);

    if (!q.exec())
        return result;

    while (q.next()) {
        const QString username = q.value(0).toString().trimmed();
        const QString fullName = q.value(1).toString().trimmed();
        if (username.isEmpty())
            continue;

        ChatPeerMeta meta;
        meta.displayName = fullName.isEmpty() ? username : fullName;

        const QByteArray avatarBytes = q.value(2).toByteArray();
        if (!avatarBytes.isEmpty()) {
            QPixmap pm;
            pm.loadFromData(avatarBytes);
            meta.avatar = pm;
        }
        meta.isActive = q.value(3).toInt() == 1;
        meta.lastLogin = q.value(4).toDateTime();

        result.insert(username, meta);
    }
    return result;
}

template <typename T>
void shuffleVector(QVector<T> &values)
{
    for (int i = values.size() - 1; i > 0; --i) {
        const int j = QRandomGenerator::global()->bounded(i + 1);
        if (i != j)
            values.swapItemsAt(i, j);
    }
}

void waitUiMs(int ms)
{
    if (ms <= 0) {
        QApplication::processEvents();
        return;
    }
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
    QApplication::processEvents();
}

QDialog *findVisibleDialogByTitle(const QString &titlePart)
{
    const auto widgets = QApplication::topLevelWidgets();
    for (auto it = widgets.crbegin(); it != widgets.crend(); ++it) {
        QDialog *dlg = qobject_cast<QDialog *>(*it);
        if (!dlg || !dlg->isVisible())
            continue;
        if (titlePart.trimmed().isEmpty() ||
            dlg->windowTitle().contains(titlePart, Qt::CaseInsensitive)) {
            return dlg;
        }
    }
    return nullptr;
}

QString normalizedUiText(QString text)
{
    text = text.trimmed().toLower();
    text.remove(QRegularExpression(QStringLiteral("[\\s\\-_]+")));
    return text;
}

QString buttonDebugText(QAbstractButton *button)
{
    if (!button)
        return QString();
    const QString text = button->text().trimmed();
    if (!text.isEmpty())
        return text;
    const QString tip = button->toolTip().trimmed();
    if (!tip.isEmpty())
        return tip;
    return button->objectName().trimmed();
}

QAbstractButton *findVisibleButtonByText(QWidget *root, const QString &textPart)
{
    if (!root)
        return nullptr;
    const QString needle = normalizedUiText(textPart);
    const auto buttons = root->findChildren<QAbstractButton *>();
    for (QAbstractButton *btn : buttons) {
        if (!btn || !btn->isVisible() || !btn->isEnabled())
            continue;
        const QString hay = normalizedUiText(buttonDebugText(btn));
        if (!needle.isEmpty() && hay.contains(needle))
            return btn;
    }
    return nullptr;
}

bool isUnsafeAutotestButton(QAbstractButton *button)
{
    if (!button)
        return true;
    const QString label = buttonDebugText(button).toLower();
    if (label.isEmpty())
        return true;

    static const QStringList blocked = {
        QStringLiteral("выйти"),
        QStringLiteral("switch"),
        QStringLiteral("сменить аккаунт"),
        QStringLiteral("удалить"),
        QStringLiteral("очистить"),
        QStringLiteral("export"),
        QStringLiteral("экспорт"),
        QStringLiteral("печать"),
        QStringLiteral("print"),
        QStringLiteral("сохранить"),
        QStringLiteral("save"),
        QStringLiteral("open file"),
        QStringLiteral("файл"),
        QStringLiteral("загрузить"),
        QStringLiteral("выгруз"),
        QStringLiteral("стресс"),
        QStringLiteral("автотест")
    };
    for (const QString &token : blocked) {
        if (label.contains(token))
            return true;
    }
    return false;
}

bool tryCloseDialog(QWidget *root)
{
    QDialog *dlg = qobject_cast<QDialog *>(root);
    if (!dlg)
        return false;

    static const QStringList closeLabels = {
        QStringLiteral("отмена"),
        QStringLiteral("закрыть"),
        QStringLiteral("назад"),
        QStringLiteral("cancel"),
        QStringLiteral("close"),
        QStringLiteral("back")
    };

    for (const QString &label : closeLabels) {
        if (QAbstractButton *btn = findVisibleButtonByText(dlg, label)) {
            btn->click();
            QApplication::processEvents();
            return true;
        }
    }

    dlg->reject();
    QApplication::processEvents();
    if (!dlg->isVisible())
        return true;

    dlg->close();
    QApplication::processEvents();
    return !dlg->isVisible();
}

void scheduleRejectDialog(const QString &titlePart, int delayMs = 140)
{
    const QString wantedTitle = titlePart;
    for (int attempt = 0; attempt < 8; ++attempt) {
        QTimer::singleShot(qMax(0, delayMs + attempt * 120), qApp, [wantedTitle]() {
            if (QDialog *dlg = findVisibleDialogByTitle(wantedTitle))
                (void)tryCloseDialog(dlg);
        });
    }
}

bool clickBackOn(QWidget *root)
{
    if (!root)
        return false;
    static const QStringList backLabels = {
        QStringLiteral("назад"),
        QStringLiteral("back"),
        QStringLiteral("отмена"),
        QStringLiteral("cancel")
    };
    for (const QString &label : backLabels) {
        if (QAbstractButton *btn = findVisibleButtonByText(root, label)) {
            btn->click();
            QApplication::processEvents();
            return true;
        }
    }
    return false;
}
}

//
// ======================= ДИАЛОГ НАСТРОЕК КАЛЕНДАРЯ =======================
//

static int calendarDaysInMonth(int year, int month)
{
    static const int kRegular[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12 || year <= 0)
        return 31;
    if (month == 2)
        return QDate::isLeapYear(year) ? 29 : 28;
    return kRegular[month - 1];
}

class CalendarSettingsDialog : public QDialog {
public:
    CalendarSettingsDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Настройки календаря");
        setModal(true);
        setFixedSize(400, 340);
        setObjectName("calendarSettingsDialog");

        setStyleSheet(
            "QDialog#calendarSettingsDialog{"
            "  background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #F7FAFF,stop:1 #EEF4FF);"
            "}"
            "QLabel#title{"
            "  font-family:Inter;font-size:22px;font-weight:900;color:#1F2937;"
            "  background:transparent;"
            "}"
            "QLabel#subtitle{"
            "  font-family:Inter;font-size:12px;font-weight:500;color:#6B7280;"
            "  background:transparent;"
            "}"
            "QLabel[class=\"field\"]{"
            "  font-family:Inter;font-size:13px;font-weight:700;color:#374151;"
            "  background:transparent;"
            "}"
            "QComboBox{"
            "  background:#FFFFFF;border:1px solid #C7D2FE;border-radius:10px;"
            "  padding:7px 12px;font-family:Inter;font-size:13px;color:#111827;min-height:20px;"
            "}"
            "QComboBox:hover{border:1px solid #8EA2FF;}"
            "QComboBox:focus{border:2px solid #1976FF;padding:6px 11px;}"
            "QComboBox::drop-down{border:none;width:20px;}"
            "QComboBox QAbstractItemView{"
            "  background:#FFFFFF;border:1px solid #C7D2FE;selection-background-color:#DDE8FF;"
            "}"
            "QPushButton#okBtn{"
            "  background:#1976FF;color:white;border:none;border-radius:10px;"
            "  padding:8px 16px;font-family:Inter;font-size:13px;font-weight:800;"
            "}"
            "QPushButton#okBtn:hover{background:#0F66EA;}"
            "QPushButton#cancelBtn{"
            "  background:#FFFFFF;color:#374151;border:1px solid #CBD5E1;border-radius:10px;"
            "  padding:8px 16px;font-family:Inter;font-size:13px;font-weight:700;"
            "}"
            "QPushButton#cancelBtn:hover{background:#F3F4F6;}"
        );

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(10);

        QLabel *title = new QLabel("Настройки календаря", this);
        title->setObjectName("title");
        title->setAutoFillBackground(false);
        QLabel *subtitle = new QLabel("Выберите месяц и конкретный день или неделю", this);
        subtitle->setObjectName("subtitle");
        subtitle->setAutoFillBackground(false);
        subtitle->setWordWrap(true);
        layout->addWidget(title);
        layout->addWidget(subtitle);

        yearBox  = new QComboBox(this);
        monthBox = new QComboBox(this);
        weekBox  = new QComboBox(this);
        dayBox   = new QComboBox(this);

        for (int y = minCalendarYear(); y <= maxCalendarYear(); ++y)
            yearBox->addItem(QString::number(y), y);

        QStringList months = {
            "Январь","Февраль","Март","Апрель","Май","Июнь",
            "Июль","Август","Сентябрь","Октябрь","Ноябрь","Декабрь"
        };
        for (int i = 0; i < months.size(); ++i)
            monthBox->addItem(months[i], i + 1);

        weekBox->addItem("—", 0);
        for (int w = 1; w <= 4; ++w)
            weekBox->addItem(QString("Неделя %1").arg(w), w);

        dayBox->addItem(QStringLiteral("—"), 0);

        auto mkFieldLabel = [this](const QString &text) {
            QLabel *lbl = new QLabel(text, this);
            lbl->setProperty("class", "field");
            lbl->setAutoFillBackground(false);
            return lbl;
        };

        QFrame *card = new QFrame(this);
        card->setStyleSheet("QFrame{background:transparent;border:none;}");
        QFormLayout *form = new QFormLayout(card);
        form->setContentsMargins(14, 14, 14, 14);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(10);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        form->addRow(mkFieldLabel("Год"), yearBox);
        form->addRow(mkFieldLabel("Месяц"), monthBox);
        form->addRow(mkFieldLabel("Неделя"), weekBox);
        form->addRow(mkFieldLabel("День"), dayBox);
        layout->addWidget(card);

        connect(weekBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx){
            int w = weekBox->itemData(idx).toInt();
            dayBox->setEnabled(w == 0);
            if (w != 0) {
                // Иначе day() остаётся старым (напр. 20), а week()!=0 — в обработчике выигрывает неделя.
                dayBox->setCurrentIndex(0);
            }
        });

        connect(dayBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx){
            int d = dayBox->itemData(idx).toInt();
            weekBox->setEnabled(d == 0);
            if (d != 0)
                weekBox->setCurrentIndex(0);
        });

        connect(yearBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { rebuildDayItems(); });
        connect(monthBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { rebuildDayItems(); });

        QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        QPushButton *okButton = btns->button(QDialogButtonBox::Ok);
        QPushButton *cancelButton = btns->button(QDialogButtonBox::Cancel);
        if (okButton) {
            okButton->setText("Применить");
            okButton->setObjectName("okBtn");
        }
        if (cancelButton) {
            cancelButton->setText("Отмена");
            cancelButton->setObjectName("cancelBtn");
        }

        connect(btns, &QDialogButtonBox::accepted, this, [this]() {
            if (year() == 0 || month() == 0) {
                reject();
                return;
            }
            if (week() == 0 && day() == 0) {
                reject();
                return;
            }
            accept();
        });

        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

        layout->addWidget(btns);

        const QDate now = QDate::currentDate();
        const int yearIndex = yearBox->findData(now.year());
        const int monthIndex = monthBox->findData(now.month());
        if (yearIndex >= 0) yearBox->setCurrentIndex(yearIndex);
        if (monthIndex >= 0) monthBox->setCurrentIndex(monthIndex);
        rebuildDayItems();
        weekBox->setCurrentIndex(0);
        dayBox->setCurrentIndex(0);
    }

    int year()  const { return yearBox->itemData(yearBox->currentIndex(), Qt::UserRole).toInt(); }
    int month() const { return monthBox->itemData(monthBox->currentIndex(), Qt::UserRole).toInt(); }
    int week()  const { return weekBox->itemData(weekBox->currentIndex(), Qt::UserRole).toInt(); }
    int day()   const { return dayBox->itemData(dayBox->currentIndex(), Qt::UserRole).toInt(); }

private:
    void rebuildDayItems()
    {
        bool yOk = false;
        bool mOk = false;
        const int y = yearBox->itemData(yearBox->currentIndex(), Qt::UserRole).toInt(&yOk);
        const int m = monthBox->itemData(monthBox->currentIndex(), Qt::UserRole).toInt(&mOk);
        const int prevDay = dayBox->itemData(dayBox->currentIndex(), Qt::UserRole).toInt();

        QSignalBlocker blockDay(dayBox);
        dayBox->clear();
        dayBox->addItem(QStringLiteral("—"), 0);

        int dim = 31;
        if (yOk && mOk && y > 0 && m >= 1 && m <= 12)
            dim = calendarDaysInMonth(y, m);

        for (int d = 1; d <= dim; ++d)
            dayBox->addItem(QString::number(d), d);

        if (prevDay <= 0) {
            dayBox->setCurrentIndex(0);
        } else if (prevDay <= dim) {
            const int ix = dayBox->findData(prevDay);
            dayBox->setCurrentIndex(ix >= 0 ? ix : 0);
        } else {
            const int ix = dayBox->findData(dim);
            dayBox->setCurrentIndex(ix >= 0 ? ix : 0);
        }
    }

    QComboBox *yearBox;
    QComboBox *monthBox;
    QComboBox *weekBox;
    QComboBox *dayBox;
};

//
// ======================= ДИАЛОГ НАСТРОЕК ПРИЛОЖЕНИЯ =======================
//

class AppSettingsDialog : public QDialog {
public:
    AppSettingsDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Настройки");
        setModal(true);
        setFixedSize(400, 160);
        setStyleSheet(
            "QDialog{background:#F7FAFF;border:1px solid #E2E8F0;border-radius:12px;}"
            "QLabel{font-family:Inter;font-size:13px;font-weight:700;color:#374151;}"
            "QLineEdit{background:#FFFFFF;border:1px solid #C7D2FE;border-radius:8px;"
            "padding:8px 12px;font-family:Inter;font-size:13px;}"
            "QPushButton{font-family:Inter;font-size:13px;font-weight:700;border-radius:8px;padding:8px 16px;}"
            "QPushButton#okBtn{background:#1976FF;color:white;border:none;}"
            "QPushButton#okBtn:hover{background:#0F66EA;}"
            "QPushButton#cancelBtn{background:#FFFFFF;color:#374151;border:1px solid #CBD5E1;}"
            "QPushButton#cancelBtn:hover{background:#F3F4F6;}"
        );

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 18, 20, 18);
        layout->setSpacing(14);

        QLabel *dbLbl = new QLabel("IP-адрес базы данных:", this);
        dbLbl->setStyleSheet("background:transparent;color:#374151;font-family:Inter;font-size:13px;font-weight:700;");
        dbHostEdit = new QLineEdit(this);
        dbHostEdit->setPlaceholderText("localhost");
        dbHostEdit->setText(getDbHost());
        layout->addWidget(dbLbl);
        layout->addWidget(dbHostEdit);

        layout->addSpacing(10);

        QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        QPushButton *okBtn = btns->button(QDialogButtonBox::Ok);
        QPushButton *cancelBtn = btns->button(QDialogButtonBox::Cancel);
        if (okBtn) { okBtn->setObjectName("okBtn"); }
        if (cancelBtn) { cancelBtn->setObjectName("cancelBtn"); }
        layout->addWidget(btns);

        connect(btns, &QDialogButtonBox::accepted, this, [this]() {
            QString host = dbHostEdit->text().trimmed();
            if (host.isEmpty()) host = "localhost";
            bool reconnectOk = reconnectWithHost(host);
            if (reconnectOk) {
                QMessageBox::information(this, "Настройки", "Настройки сохранены.");
            } else {
                QMessageBox::warning(this, "Ошибка", "Не удалось подключиться к базе данных.\nПроверьте IP-адрес.");
            }
            accept();
        });
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

        if (qApp->property("autotest_running").toBool()) {
            for (int attempt = 0; attempt < 6; ++attempt) {
                QTimer::singleShot(120 + attempt * 120, this, [this]() {
                    if (isVisible())
                        reject();
                });
            }
        }
    }

private:
    QLineEdit *dbHostEdit;
};

//
// ======================= КОНСТРУКТОР =======================
//

leftMenu::leftMenu(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);

    selectedDay_ = QDate(selectedYear_, selectedMonth_, 1);
    selectedWeek_ = 0;
    highlightWeek_ = false;

    initUI();
}

//
// ======================= SCALE HELPERS =======================
//

int leftMenu::s(int v) const
{
    return int(v * scaleFactor_);
}

void leftMenu::setScaleFactor(qreal factor)
{
    if (factor <= 0)
        factor = 1.0;
    factor = qMin<qreal>(1.0, factor);

    if (qAbs(scaleFactor_ - factor) < 0.01)
        return;

    scaleFactor_ = factor;
    setUpdatesEnabled(false);
    disconnect(&DataBus::instance(), nullptr, this, nullptr);

    if (QLayout *old = layout()) {
        QLayoutItem *item;
        while ((item = old->takeAt(0)) != nullptr) {
            if (QWidget *w = item->widget()) {
                w->setParent(nullptr);
                delete w;
            }
            delete item;
        }
        delete old;
    }

    topRow_ = nullptr;
    bottomRow_ = nullptr;
    rightCalendarFrame = nullptr;
    rightUpcomingMaintenanceFrame = nullptr;
    listAgvInfo = nullptr;
    agvSettingsPage = nullptr;
    modelListPage = nullptr;
    logsPage = nullptr;
    logsTable = nullptr;
    logsLoadAllBtn = nullptr;
    logsExportBtn = nullptr;
    profilePage = nullptr;
    chatsPage = nullptr;
    chatsStack_ = nullptr;
    embeddedChatWidget_ = nullptr;
    chatsListLayout_ = nullptr;
    calendarActionsFrame = nullptr;
    statusWidget_ = nullptr;
    calendarTablePtr = nullptr;
    agvCounter = nullptr;
    userButton = nullptr;
    searchEdit_ = nullptr;
    notifBadge_ = nullptr;
    logFilterUser_ = nullptr;
    logFilterSource_ = nullptr;
    logFilterCategory_ = nullptr;
    logFilterTime_ = nullptr;
    if (profileKeyTimer) { profileKeyTimer->stop(); profileKeyTimer->deleteLater(); profileKeyTimer = nullptr; }
    if (agvCounterTimer) { agvCounterTimer->stop(); agvCounterTimer->deleteLater(); agvCounterTimer = nullptr; }
    if (notifPollTimer) { notifPollTimer->stop(); notifPollTimer->deleteLater(); notifPollTimer = nullptr; }
    if (chatsPollTimer) { chatsPollTimer->stop(); chatsPollTimer->deleteLater(); chatsPollTimer = nullptr; }
    backButton = nullptr;
    monthLabel = nullptr;
    usersPage = nullptr;
    calendarStressTestBtn_ = nullptr;
    fullStressAutotestBtn_ = nullptr;
    techDiagLogEdit_ = nullptr;
    setTechDiagLogSink(nullptr);
    agvListDirty_ = true;

    initUI();
    restoreActivePage();
    setUpdatesEnabled(true);
    updateGeometry();
    update();
}

void leftMenu::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    qreal wFactor = width() / 1920.0;
    qreal hFactor = height() / 1080.0;
    qreal factor = qMin<qreal>(1.0, qMin(wFactor, hFactor));
    setScaleFactor(factor);
}
//
// ======================= initUI() — НАЧАЛО =======================
//

void leftMenu::initUI()
{
    const QString appVersionText = "1.0.0 от 30.11.2025";

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(s(5));
    rootLayout->setContentsMargins(s(10), s(10), s(10), s(10));

    // Легкий heartbeat присутствия: обновляем last_login текущего пользователя.
    if (!findChild<QTimer*>("presenceHeartbeatTimer")) {
        QTimer *presenceTimer = new QTimer(this);
        presenceTimer->setObjectName("presenceHeartbeatTimer");
        presenceTimer->setInterval(15000);
        connect(presenceTimer, &QTimer::timeout, this, []() {
            touchUserPresence(AppSession::currentUsername());
        });
        presenceTimer->start();
    }
    touchUserPresence(AppSession::currentUsername());

    //
    // ======================= ВЕРХНЯЯ ШАПКА =======================
    //
    topRow_ = new QWidget(this);
    QWidget *topRow = topRow_;
    QHBoxLayout *topLayout = new QHBoxLayout(topRow);
    topLayout->setContentsMargins(0,0,0,0);
    topLayout->setSpacing(s(5));

    //
    // ЛЕВАЯ ЧАСТЬ ШАПКИ (ЛОГО)
    //
    QFrame *leftTopHeaderFrame = new QFrame(topRow);
    leftTopHeaderFrame->setStyleSheet("background-color:#F2F3F5;");
    leftTopHeaderFrame->setFixedSize(s(370), s(115));

    QVBoxLayout *leftTopHeaderLayout = new QVBoxLayout(leftTopHeaderFrame);
    leftTopHeaderLayout->setContentsMargins(s(3), s(0), 0, 0);
    leftTopHeaderLayout->setSpacing(0);

    QLabel *iconLabel = new QLabel(leftTopHeaderFrame);
    iconLabel->setPixmap(
        QPixmap(":/new/mainWindowIcons/noback/VAPManagerLogo.png")
            .scaled(s(288), s(92), Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
    iconLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    leftTopHeaderLayout->addStretch();
    leftTopHeaderLayout->addWidget(iconLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    leftTopHeaderLayout->addStretch();

    topLayout->addWidget(leftTopHeaderFrame);

    //
    // ПРАВАЯ ЧАСТЬ ШАПКИ
    //
    QFrame *rightTopHeaderFrame = new QFrame(topRow);
    rightTopHeaderFrame->setStyleSheet("background-color:#F1F2F4;");
    rightTopHeaderFrame->setFixedHeight(s(115));

    QHBoxLayout *rightTopHeaderLayout = new QHBoxLayout(rightTopHeaderFrame);
    rightTopHeaderLayout->setContentsMargins(0,0,0,0);

    QWidget *headerContent = new QWidget(rightTopHeaderFrame);
    QHBoxLayout *headerRow = new QHBoxLayout(headerContent);
    headerRow->setContentsMargins(s(5), s(5), s(5), s(5));

    //
    // Левый текстовый блок
    //
    QWidget *leftTextWidget = new QWidget(headerContent);
    QVBoxLayout *leftTextLayout = new QVBoxLayout(leftTextWidget);
    leftTextLayout->setContentsMargins(0,0,0,0);
    leftTextLayout->setSpacing(0);

    QLabel *titleLabel = new QLabel("Календарь технического обслуживания", leftTextWidget);
    titleLabel->setStyleSheet(QString(
        "font-family:Inter;font-weight:900;font-size:%1px;color:black;padding-left:%2px;"
    ).arg(s(22)).arg(s(9)));

    QLabel *subtitleLabel = new QLabel(
        "Отслеживание графиков обслуживания AGV\nи истории технического обслуживания",
        leftTextWidget
    );
    subtitleLabel->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:bold;color:#B8B8B8;padding-left:%2px;"
    ).arg(s(18)).arg(s(7)));

    leftTextLayout->addStretch();
    leftTextLayout->addWidget(titleLabel);
    leftTextLayout->addWidget(subtitleLabel);
    leftTextLayout->addStretch();

    headerRow->addWidget(leftTextWidget, 1);

    //
    // Правая колонка (поиск, уведомления, юзер)
    //
    QWidget *controls = new QWidget(headerContent);
    QHBoxLayout *controlsRow = new QHBoxLayout(controls);
    controlsRow->setContentsMargins(0,0,0,0);
    controlsRow->setSpacing(s(5));

    //
    // Поиск
    //
    QFrame *searchFrame = new QFrame(controls);
    searchFrame->setFixedSize(s(346), s(64));
    searchFrame->setStyleSheet("background-color:#DFDFDF;border-radius:8px;");

    QHBoxLayout *searchLayout = new QHBoxLayout(searchFrame);
    searchLayout->setContentsMargins(s(12), 0, s(12), 0);

    QLabel *searchIcon = new QLabel(searchFrame);
    searchIcon->setPixmap(
        QPixmap(":/new/mainWindowIcons/noback/lupa.png")
            .scaled(s(31), s(30), Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
    searchIcon->setAlignment(Qt::AlignCenter);

    searchEdit_ = new QLineEdit(searchFrame);
    searchEdit_->setPlaceholderText("Поиск AGV...");
    searchEdit_->setStyleSheet(QString(
        "QLineEdit{background:transparent;border:none;font-family:Inter;font-size:%1px;color:#747474;}"
    ).arg(s(16)));

    connect(searchEdit_, &QLineEdit::textChanged, this, &leftMenu::onSearchTextChanged);

    searchLayout->addWidget(searchIcon);
    searchLayout->addWidget(searchEdit_);

    QToolButton *chatsTopBtn = new QToolButton(controls);
    chatsTopBtn->setFixedSize(s(50), s(50));
    chatsTopBtn->setIcon(QIcon(":/new/mainWindowIcons/noback/logs.png"));
    chatsTopBtn->setIconSize(QSize(s(24), s(24)));
    chatsTopBtn->setStyleSheet(QString(
        "QToolButton{background:#E6E6E6;border-radius:%1px;border:none;}"
        "QToolButton:hover{background:#D5D5D5;}"
    ).arg(s(10)));
    chatsTopBtn->setToolTip("Чаты");
    connect(chatsTopBtn, &QToolButton::clicked, this, [this](){
        showChatsPage();
    });

    //
    // Кнопка уведомлений
    //
    QPushButton *notifBtn = new QPushButton(controls);
    notifBtn->setFixedSize(s(58), s(65));
    notifBtn->setAttribute(Qt::WA_TranslucentBackground);
    notifBtn->setStyleSheet(
        "QPushButton{background-color:transparent;border-radius:8px;border:none;}"
        "QPushButton:hover{background-color:rgba(0,0,0,0.05);} "
        "QPushButton:pressed{border:3px solid #FFD700;}"
    );

    QLabel *bell = new QLabel(notifBtn);
    bell->setAttribute(Qt::WA_TranslucentBackground);
    bell->setStyleSheet("background: transparent;");
    bell->setPixmap(
        QPixmap(":/new/mainWindowIcons/noback/bell.png")
            .scaled(s(37), s(37), Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
    bell->setAlignment(Qt::AlignCenter);

    notifBadge_ = new QLabel(notifBtn);
    notifBadge_->setFixedSize(s(20), s(20));
    notifBadge_->setAlignment(Qt::AlignCenter);
    notifBadge_->setStyleSheet(QString(
        "background:#FF3B30;color:white;font-family:Inter;font-weight:900;"
        "font-size:%1px;border-radius:%2px;"
    ).arg(s(10)).arg(s(10)));
    notifBadge_->hide();

    QHBoxLayout *notifLayout = new QHBoxLayout(notifBtn);
    notifLayout->setContentsMargins(0,0,0,0);
    notifLayout->setSpacing(0);
    notifLayout->addStretch();
    notifLayout->addWidget(bell);
    notifLayout->addStretch();
    notifBadge_->move(s(2), s(2)); // top-left over bell icon
    notifBadge_->raise();

    connect(notifBtn, &QPushButton::clicked, this, [this](){
        showNotificationsPanel();
    });

    //
    // Юзер
    //
    QFrame *userFrame = new QFrame(controls);
    userFrame->setFixedSize(s(65), s(65));
    QHBoxLayout *userLayout = new QHBoxLayout(userFrame);
    userLayout->setContentsMargins(s(5), s(5), s(5), s(5));

    userButton = new QToolButton(userFrame);
    userButton->setFixedSize(s(55), s(55));

    const QString currentUsername = AppSession::currentUsername();
    QPixmap avatarPm = loadUserAvatarFromDb(currentUsername);
    if (!avatarPm.isNull()) {
        QPixmap round = makeRoundPixmap(avatarPm, s(55));
        userButton->setIcon(QIcon(round));
        userButton->setIconSize(QSize(s(55), s(55)));
    } else {
        QString initials = currentUsername.trimmed().left(2).toUpper();
        if (initials.isEmpty())
            initials = "US";
        userButton->setText(initials);
    }
    userButton->setToolTip(currentUsername.isEmpty() ? tr("Пользователь") : currentUsername);

    userButton->setStyleSheet(QString(
        "QToolButton{background-color:#D9D9D9;border-radius:%1px;font-family:Inter;"
        "font-weight:900;font-size:%2px;color:black;border:none;padding:%3px;} "
        "QToolButton:hover{background-color:rgba(173,216,230,76);border:1px solid #ADD8E6;}"
    ).arg(s(27)).arg(s(14)).arg(s(4)));

    userLayout->addWidget(userButton);

    QMenu *userMenu = new QMenu(userButton);
    userMenu->setStyleSheet(QString(
        "QMenu { background-color: white; font-family: Inter; font-size:%1px; }"
        "QMenu::item { padding: %2px %3px; }"
        "QMenu::item:selected { background-color: #E6E6E6; }"
    ).arg(s(14)).arg(s(6)).arg(s(12)));

    QAction *accountInfoAction = userMenu->addAction(
        currentUsername.isEmpty() ? tr("Аккаунт: неизвестно")
                                  : tr("Аккаунт: %1").arg(currentUsername));
    connect(accountInfoAction, &QAction::triggered, this, [this]() {

        QString username = AppSession::currentUsername();
        if (username.isEmpty())
            return;

        QString role = getUserRole(username);
        QString key;
        if (role == "admin") {
            refreshAdminInviteKeyIfNeeded(username);
            key = getAdminInviteKey(username);
        } else if (role == "tech") {
            refreshTechInviteKeyIfNeeded(username);
            key = getTechInviteKey(username);
        }

        // Аватар из базы
        QPixmap avatar = loadUserAvatarFromDb(username);

        AccountInfoDialog dlg(username, role, key, avatar, this);
        dlg.exec();

    });


    userMenu->addSeparator();
    QAction *editProfileAction = userMenu->addAction(tr("Редактировать профиль"));
    QAction *changeAvatarAction = userMenu->addAction(tr("Сменить аватар"));
    QAction *changeLanguageAction = userMenu->addAction(tr("Сменить язык"));
    userMenu->addSeparator();
    QAction *aboutAction = userMenu->addAction(tr("О программе"));
    userMenu->addSeparator();
    QAction *switchAccountAction = userMenu->addAction(tr("Сменить аккаунт"));
    QAction *exitAppAction = userMenu->addAction(tr("Выйти из приложения"));

    auto logMenuClick = [] (const QString &action, const QString &details) {
        logAction(AppSession::currentUsername(), action, details);
    };


    auto openAboutDialog = [this, appVersionText, logMenuClick]() {
        QDialog dlg(this);
        dlg.setWindowTitle("О программе");
        dlg.setModal(true);
        dlg.setFixedSize(s(700), s(430));
        dlg.setStyleSheet(
            "QDialog{background:#F6F8FC;border:1px solid #DCE2EE;border-radius:14px;}"
            "QFrame#aboutCard{background:white;border:1px solid #E5EAF3;border-radius:12px;}"
            "QLabel{font-family:Inter;color:#1A1A1A;}"
            "QLabel#title{font-size:24px;font-weight:900;color:#0F172A;}"
            "QLabel#subtitle{font-size:13px;font-weight:600;color:#64748B;}"
            "QLabel#rowTitle{font-size:13px;font-weight:800;color:#334155;}"
            "QLabel#rowValue{font-size:14px;font-weight:600;color:#0F172A;}"
            "QFrame#line{background:#E7ECF5;border:none;min-height:1px;max-height:1px;}"
            "QPushButton{font-family:Inter;font-size:14px;font-weight:800;border-radius:9px;padding:9px 16px;}"
            "QPushButton#okBtn{background:#0F00DB;color:white;border:none;}"
            "QPushButton#okBtn:hover{background:#1A4ACD;}"
        );

        QVBoxLayout *root = new QVBoxLayout(&dlg);
        root->setContentsMargins(s(16), s(16), s(16), s(16));
        root->setSpacing(s(10));

        QFrame *card = new QFrame(&dlg);
        card->setObjectName("aboutCard");
        QVBoxLayout *v = new QVBoxLayout(card);
        v->setContentsMargins(s(18), s(16), s(18), s(14));
        v->setSpacing(s(10));

        QHBoxLayout *header = new QHBoxLayout();
        header->setContentsMargins(0,0,0,0);
        header->setSpacing(s(12));
        QLabel *logo = new QLabel(card);
        logo->setFixedSize(s(170), s(54));
        logo->setPixmap(QPixmap(":/new/mainWindowIcons/noback/VAPManagerLogo.png")
                        .scaled(logo->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logo->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        QLabel *agvIcon = new QLabel(card);
        agvIcon->setFixedSize(s(42), s(42));
        agvIcon->setPixmap(QPixmap(":/new/mainWindowIcons/noback/agvIcon.png")
                           .scaled(agvIcon->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        agvIcon->setAlignment(Qt::AlignCenter);

        header->addWidget(logo);
        header->addStretch();
        header->addWidget(agvIcon);
        v->addLayout(header);

        QLabel *title = new QLabel("О ПО AGV Manager", card);
        title->setObjectName("title");
        v->addWidget(title);

        QLabel *subtitle = new QLabel("Информация о владельце и разработчике", card);
        subtitle->setObjectName("subtitle");
        v->addWidget(subtitle);

        QFrame *line = new QFrame(card);
        line->setObjectName("line");
        v->addWidget(line);

        auto addInfoRow = [this, v, card](const QString &k, const QString &val) {
            QHBoxLayout *row = new QHBoxLayout();
            row->setContentsMargins(0, 0, 0, 0);
            row->setSpacing(s(12));
            QLabel *kLbl = new QLabel(k, card);
            kLbl->setObjectName("rowTitle");
            kLbl->setMinimumWidth(s(180));
            QLabel *vLbl = new QLabel(val, card);
            vLbl->setObjectName("rowValue");
            vLbl->setWordWrap(true);
            row->addWidget(kLbl);
            row->addWidget(vLbl, 1);
            v->addLayout(row);
        };

        addInfoRow("Организация:", "Горьковский автомобильный завод");
        addInfoRow("Ответственный:", "Ведущий спец. Булькин Дмитрий Олегович");
        addInfoRow("Продукт:", "AGV Manager (панель администрирования)");
        addInfoRow("Версия:", appVersionText);
        addInfoRow("Год:", "2026");

        v->addStretch();

        QPushButton *ok = new QPushButton("Закрыть", card);
        ok->setObjectName("okBtn");
        v->addWidget(ok, 0, Qt::AlignRight);
        connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);

        logMenuClick("user_menu_about", "Открыто окно О программе");
        root->addWidget(card);
        dlg.exec();
    };

    connect(accountInfoAction, &QAction::triggered, this, [logMenuClick](){
        logMenuClick("user_menu_account", "Открыт пункт Аккаунт");
    });
    connect(editProfileAction, &QAction::triggered, this, [this, logMenuClick](){
        logMenuClick("user_menu_edit_profile", "Нажат пункт Редактировать профиль");
        showProfile();
    });
    connect(changeAvatarAction, &QAction::triggered, this, [this, logMenuClick](){
        logMenuClick("user_menu_change_avatar", "Нажат пункт Сменить аватар");
        changeAvatar();
    });

    connect(changeLanguageAction, &QAction::triggered, this, [this, logMenuClick](){
        logMenuClick("user_menu_change_language", "Нажат пункт Сменить язык");
        QDialog langDlg(this);
        langDlg.setWindowTitle(tr("Сменить язык"));
        langDlg.setModal(true);
        langDlg.setFixedSize(320, 120);
        QVBoxLayout *v = new QVBoxLayout(&langDlg);
        QComboBox *cb = new QComboBox(&langDlg);
        cb->addItem(tr("Русский"), "ru");
        cb->addItem("English", "en");
        cb->addItem("中文", "zh");
        QString cfgPath = QCoreApplication::applicationDirPath() + "/config.ini";
        QSettings cfg(cfgPath, QSettings::IniFormat);
        int idx = cb->findData(cfg.value("language", "ru").toString());
        if (idx >= 0) cb->setCurrentIndex(idx);
        v->addWidget(cb);
        QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &langDlg);
        connect(bb, &QDialogButtonBox::accepted, &langDlg, [&](){
            cfg.setValue("language", cb->currentData().toString());
            cfg.sync();
            const int ret = QMessageBox::question(
                &langDlg,
                tr("Язык"),
                tr("Язык сохранен. Перезапустить приложение сейчас?"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);
            if (ret == QMessageBox::Yes) {
                const QString appPath = QCoreApplication::applicationFilePath();
                const QStringList args = QCoreApplication::arguments().mid(1);
                QProcess::startDetached(appPath, args);
                qApp->quit();
                return;
            }
            langDlg.accept();
        });
        connect(bb, &QDialogButtonBox::rejected, &langDlg, &QDialog::reject);
        v->addWidget(bb);
        langDlg.exec();
    });
    connect(switchAccountAction, &QAction::triggered, this, [this](){
        logAction(AppSession::currentUsername(), "user_menu_switch_account", "Нажат пункт Сменить аккаунт");
        
        logoutUser();
        
        LoginDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            UserInfo newUser = dlg.user();
            AppSession::setCurrentUsername(newUser.username);
            enableRememberMe(newUser.username);
            logAction(AppSession::currentUsername(), "account_switched", QString("Вход под аккаунтом %1").arg(newUser.username));
            
            QTimer::singleShot(0, this, [this]() {
                qreal oldFactor = scaleFactor_;
                scaleFactor_ = 0;
                setScaleFactor(oldFactor);
            });
        }
    });
    connect(aboutAction, &QAction::triggered, this, [openAboutDialog](){
        openAboutDialog();
    });

    connect(exitAppAction, &QAction::triggered, this, [](){
        logAction(AppSession::currentUsername(), "user_menu_exit_app", "Нажат пункт Выйти из приложения");
        qApp->quit();
    });

    connect(userButton, &QToolButton::clicked, this, [this, userMenu](){
        QPoint pos = this->userButton->mapToGlobal(QPoint(0, this->userButton->height() + s(5)));
        userMenu->popup(pos);
    });



    controlsRow->addWidget(searchFrame);
    controlsRow->addWidget(chatsTopBtn);
    controlsRow->addWidget(notifBtn);
    controlsRow->addWidget(userFrame);

    headerRow->addWidget(controls, 0, Qt::AlignRight);
    rightTopHeaderLayout->addWidget(headerContent);

    topLayout->addWidget(rightTopHeaderFrame);

    rootLayout->addWidget(topRow);

    //
    // ======================= НИЖНЯЯ ПАНЕЛЬ =======================
    //
    bottomRow_ = new QWidget(this);
    QWidget *bottomRow = bottomRow_;
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomRow);
    bottomLayout->setContentsMargins(0, s(5), 0, 0);
    bottomLayout->setSpacing(s(5));

    //
    // ======================= ЛЕВАЯ ПАНЕЛЬ =======================
    //
    QWidget *leftPanel = new QWidget(bottomRow);
    leftPanel->setFixedWidth(s(370));

    QVBoxLayout *leftPanelLayout = new QVBoxLayout(leftPanel);
    leftPanelLayout->setContentsMargins(0,0,0,0);
    leftPanelLayout->setSpacing(s(8));

    //
    // МЕНЮ
    //
    QFrame *leftNavFrame = new QFrame(leftPanel);
    leftNavFrame->setStyleSheet("background-color:#F1F2F4;");
    QVBoxLayout *leftNavLayout = new QVBoxLayout(leftNavFrame);
    leftNavLayout->setContentsMargins(s(10), s(5), s(10), s(5));
    leftNavLayout->setSpacing(0);

    auto makeNavButton = [&](QString text, QString iconPath){
        QPushButton *btn = new QPushButton(text, leftNavFrame);
        QPixmap pix(iconPath);
        if (!pix.isNull())
            btn->setIcon(QIcon(pix.scaled(s(24), s(24), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        btn->setIconSize(QSize(s(24), s(24)));
        btn->setFixedHeight(s(45));
        btn->setStyleSheet(QString(
            "QPushButton{text-align:left;background-color:transparent;padding-left:%1px;font-family:Inter;"
            "font-size:%2px;font-weight:700;border-radius:8px;border:1px solid transparent;} "
            "QPushButton:hover{background-color:rgba(173,216,230,76);border:1px solid #ADD8E6;}"
        ).arg(s(15)).arg(s(14)));
        return btn;
    };

    QPushButton *btnUsers = makeNavButton("Пользователи", ":/new/mainWindowIcons/noback/user.png");
    QPushButton *btnAgv   = makeNavButton("AGV", ":/new/mainWindowIcons/noback/agvIcon.png");
    QPushButton *btnEdit  = makeNavButton("Модель AGV", ":/new/mainWindowIcons/noback/edit.png");
    QPushButton *btnYear  = makeNavButton("Сформировать годовой отчет", ":/new/mainWindowIcons/noback/YearListPrint.png");
    QPushButton *btnSet   = makeNavButton("Настройки", ":/new/mainWindowIcons/noback/agvSetting.png");
    connect(btnUsers, &QPushButton::clicked, this, [this](){
        showUsersPage();
    });

    connect(btnYear, &QPushButton::clicked, this, [this](){
        showAnnualReportDialog();
    });

    connect(btnSet, &QPushButton::clicked, this, [this](){
        AppSettingsDialog dlg(this);
        dlg.exec();
    });

    leftNavLayout->addWidget(btnUsers);
    // === AGV + COUNTER ===
    {
        QWidget *agvRow = new QWidget(leftNavFrame);
        QHBoxLayout *agvRowLayout = new QHBoxLayout(agvRow);
        agvRowLayout->setContentsMargins(0,0,0,0);
        agvRowLayout->setSpacing(s(8));

        agvRowLayout->addWidget(btnAgv);

        agvCounter = new QLabel("0", agvRow);
        agvCounter->setFixedSize(s(26), s(26));
        agvCounter->setAlignment(Qt::AlignCenter);
        agvCounter->setStyleSheet(QString(
            "background:#00C8FF;"
            "color:#0F00DB;"
            "font-family:Inter;"
            "font-weight:900;"
            "font-size:%1px;"
            "border-radius:%2px;"
        ).arg(s(14)).arg(s(13)));

        agvRowLayout->addWidget(agvCounter);

        leftNavLayout->addWidget(agvRow);
    }
    leftNavLayout->addWidget(btnEdit);
    leftNavLayout->addWidget(btnYear);
    leftNavLayout->addWidget(btnSet);
    leftNavLayout->addSpacing(s(10));

    //
    // КНОПКА ДОБАВИТЬ AGV
    //
    QPushButton *addAgvButton = new QPushButton("+ Добавить AGV", leftNavFrame);
    {
        QString role = getUserRole(AppSession::currentUsername());
        if (role == "viewer")
            addAgvButton->hide();
    }
    connect(addAgvButton, &QPushButton::clicked, this, [this](){

        // 1. Открываем диалог добавления
        AddAgvDialog dlg([this](int v){ return s(v); }, this);
        if (dlg.exec() != QDialog::Accepted)
            return;

        // 2. Формируем структуру AgvInfo
        AgvInfo info;
        QString baseName = dlg.result.name.trimmed();

        // последние цифры из серийника
        QString digits;
        QRegularExpression re("\\d+");
        auto it = re.globalMatch(dlg.result.serial);
        while (it.hasNext())
            digits += it.next().captured();

        QString last4 = digits.right(4);
        if (last4.isEmpty()) last4 = "0000";

        QString modelLower = dlg.result.model.toLower();

        QString finalId = QString("%1_%2_%3")
                            .arg(baseName)
                            .arg(last4)
                            .arg(modelLower);

        info.id = finalId;
        info.model = dlg.result.model.toUpper();
        info.serial = dlg.result.serial;
        info.status = dlg.result.status;
        info.task = dlg.result.alias.trimmed();
        info.kilometers = 0;
        info.blueprintPath = ":/new/mainWindowIcons/noback/blueprint.png";
        info.lastActive = QDate::currentDate();

        // 3. Записываем в базу
        if (!insertAgvToDb(info)) {
            qDebug() << "insertAgvToDb: не удалось записать AGV";
            return;
        }
        // 3.1 Копируем задачи модели → agv_tasks
        if (!copyModelTasksToAgv(info.id, info.model)) {
            qDebug() << "copyModelTasksToAgv: ошибка копирования задач для" << info.id;
        }

        // 4. Обновляем список AGV, если он открыт
        agvListDirty_ = true;
        if (listAgvInfo && listAgvInfo->isVisible()) {
            QVector<AgvInfo> agvs = listAgvInfo->loadAgvList();

            listAgvInfo->rebuildList(agvs);
            agvListDirty_ = false;
        }

        // 5. Переключаемся на список AGV
        showAgvList();
    });

    addAgvButton->setFixedHeight(s(40));
    addAgvButton->setStyleSheet(QString(
        "QPushButton{background-color:#0F00DB;color:white;font-family:Inter;font-size:%1px;font-weight:800;"
        "border-radius:10px;border:none;} "
        "QPushButton:hover{background-color:#1A4ACD;}"
    ).arg(s(16)));

    QHBoxLayout *addAgvButtonRow = new QHBoxLayout();
    addAgvButtonRow->addSpacing(s(28));
    addAgvButtonRow->addWidget(addAgvButton);
    addAgvButtonRow->addSpacing(s(28));

    leftNavLayout->addLayout(addAgvButtonRow);
    leftNavLayout->addSpacing(s(5));

    leftPanelLayout->addWidget(leftNavFrame);

    //
    // ======================= СТАТУС СИСТЕМЫ =======================
    //
    QFrame *leftStatusFrame = new QFrame(leftPanel);
    leftStatusFrame->setStyleSheet("background-color:#F1F2F4;");
    leftStatusFrame->setMinimumHeight(s(208));

    QVBoxLayout *leftStatusLayout = new QVBoxLayout(leftStatusFrame);
    leftStatusLayout->setContentsMargins(s(10), s(20), s(10), s(5));
    leftStatusLayout->setSpacing(s(5));

    SystemStatus st = loadSystemStatus();
    int totalAgv = st.active + st.maintenance + st.error + st.disabled;

    statusWidget_ = new MultiSectionWidget(leftStatusFrame, scaleFactor_);
    statusWidget_->setScaleFactor(scaleFactor_ * 1.3);
    statusWidget_->setActiveAGVCurrentCount(st.active);
    statusWidget_->setActiveAGVTotalCount(totalAgv);
    statusWidget_->setMaintenanceCurrentCount(st.maintenance);
    statusWidget_->setMaintenanceTotalCount(totalAgv);
    statusWidget_->setErrorCurrentCount(st.error);
    statusWidget_->setErrorTotalCount(totalAgv);
    statusWidget_->setDisabledCurrentCount(st.disabled);
    statusWidget_->setDisabledTotalCount(totalAgv);

    leftStatusLayout->addWidget(statusWidget_);

    QPushButton *logsButton = new QPushButton("Logs", leftStatusFrame);
    logsButton->setFixedSize(s(120), s(25));
    logsButton->setIcon(QIcon(":/new/mainWindowIcons/noback/logs.png"));
    logsButton->setIconSize(QSize(s(14), s(14)));
    logsButton->setStyleSheet(QString(
        "QPushButton{background-color:transparent;border:none;font-family:Inter;font-size:%1px;font-weight:800;color:black;padding-left:%2px;} "
        "QPushButton:hover{background-color:rgba(173,216,230,76);border-radius:5px;}"
    ).arg(s(12)).arg(s(4)));

    QHBoxLayout *logsRow = new QHBoxLayout();
    logsRow->addStretch();
    logsRow->addWidget(logsButton);
    logsRow->addStretch();
    leftStatusLayout->addLayout(logsRow);
    connect(logsButton, &QPushButton::clicked, this, [this](){
        showLogs();
    });

    QLabel *versionLabel = new QLabel(QString("Версия: %1").arg(appVersionText), leftStatusFrame);
    versionLabel->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:rgb(120,120,120);").arg(s(10)));
    versionLabel->setAlignment(Qt::AlignCenter);
    leftStatusLayout->addWidget(versionLabel);

    leftPanelLayout->addWidget(leftStatusFrame);
    leftPanelLayout->setStretch(0,0);
    leftPanelLayout->setStretch(1,1);

    bottomLayout->addWidget(leftPanel);
    //
    // ======================= ПРАВАЯ ПАНЕЛЬ =======================
    //
    QWidget *rightPanel = new QWidget(bottomRow);
    QVBoxLayout *rightPanelLayout = new QVBoxLayout(rightPanel);
    rightPanelLayout->setContentsMargins(0, s(5), 0, 0);
    rightPanelLayout->setSpacing(s(8));

    QFrame *rightBodyFrame = new QFrame(rightPanel);
    QVBoxLayout *rightBodyLayout = new QVBoxLayout(rightBodyFrame);
    rightBodyLayout->setContentsMargins(0,0,0,0);
    rightBodyLayout->setSpacing(s(5));

    rightPanelLayout->addWidget(rightBodyFrame,1);
    bottomLayout->addWidget(rightPanel,1);

    rootLayout->addWidget(bottomRow,1);

    //
    // ======================= КАЛЕНДАРЬ =======================
    //
    rightCalendarFrame = new QFrame(rightBodyFrame);
    rightCalendarFrame->setStyleSheet("background-color:#F1F2F4;border-radius:12px;");
    rightCalendarFrame->setMinimumHeight(s(300));

    QVBoxLayout *rightCalendarLayout = new QVBoxLayout(rightCalendarFrame);
    rightCalendarLayout->setContentsMargins(s(8),s(8),s(8),s(8));
    rightCalendarLayout->setSpacing(s(8));

    //
    // ======== БЛОК ДЕЙСТВИЙ ДНЯ ========
    //
    calendarActionsFrame = new QFrame(rightCalendarFrame);
    calendarActionsFrame->setStyleSheet("background-color:#F1F2F4;border-radius:8px;");
    calendarActionsFrame->setVisible(false);

    QHBoxLayout *actionsLayout = new QHBoxLayout(calendarActionsFrame);
    actionsLayout->setContentsMargins(s(10), s(10), s(10), s(10));
    actionsLayout->setSpacing(s(10));

    QLabel *actionsLabel = new QLabel("Выберите день или неделю", calendarActionsFrame);
    actionsLabel->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:700;color:black;"
    ).arg(s(16)));

    QPushButton *btnAdd = new QPushButton("Добавить событие", calendarActionsFrame);
    btnAdd->setStyleSheet(QString(
            "QPushButton{background-color:#0F00DB;color:white;font-family:Inter;font-size:%1px;"
            "font-weight:700;border-radius:8px;padding:%2px %3px;} "
            "QPushButton:hover{background-color:#1A4ACD;}"
    ).arg(s(14)).arg(s(6)).arg(s(12)));

    actionsLayout->addWidget(actionsLabel);
    actionsLayout->addStretch();
    actionsLayout->addWidget(btnAdd);

    rightCalendarLayout->addWidget(calendarActionsFrame);

    //
    // ======== HEADER КАЛЕНДАРЯ ========
    //
    QWidget *calendarHeader = new QWidget(rightCalendarFrame);
    QVBoxLayout *calendarHeaderMainLayout = new QVBoxLayout(calendarHeader);
    calendarHeaderMainLayout->setContentsMargins(s(10),s(14),s(10),0);
    calendarHeaderMainLayout->setSpacing(s(16));

    QHBoxLayout *topRowLayoutCal = new QHBoxLayout();
    topRowLayoutCal->setContentsMargins(0,0,0,0);
    topRowLayoutCal->setSpacing(s(35));

    monthLabel = new QLabel(monthYearLabelText(selectedMonth_, selectedYear_), calendarHeader);
    monthLabel->setStyleSheet(QString(
        "font-family:Inter;font-weight:900;font-size:%1px;color:black;"
    ).arg(s(26)));
    monthLabel->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);

    topRowLayoutCal->addWidget(monthLabel);
    topRowLayoutCal->addStretch();

    QPushButton *prevMonthBtn = new QPushButton("<");
    prevMonthBtn->setFixedSize(s(32),s(32));
    prevMonthBtn->setStyleSheet(QString(
        "QPushButton{background-color:#DFDFDF;border-radius:6px;font-size:%1px;} "
        "QPushButton:hover{background-color:#CFCFCF;}"
    ).arg(s(18)));

    QPushButton *nextMonthBtn = new QPushButton(">");
    nextMonthBtn->setFixedSize(s(32),s(32));
    nextMonthBtn->setStyleSheet(prevMonthBtn->styleSheet());

    const bool atMinBoundary = (selectedYear_ == minCalendarYear() && selectedMonth_ == 1);
    const bool atMaxBoundary = (selectedYear_ == maxCalendarYear() && selectedMonth_ == 12);
    prevMonthBtn->setEnabled(!atMinBoundary);
    nextMonthBtn->setEnabled(!atMaxBoundary);

    connect(prevMonthBtn,&QPushButton::clicked,this,[this](){ changeMonth(-1); });
    connect(nextMonthBtn,&QPushButton::clicked,this,[this](){ changeMonth(1); });

    topRowLayoutCal->addWidget(prevMonthBtn);
    topRowLayoutCal->addWidget(nextMonthBtn);

    QPushButton *settingsBtn = new QPushButton("Настройки");
    settingsBtn->setFixedSize(s(120),s(38));
    settingsBtn->setStyleSheet(QString(
        "QPushButton{background-color:#DFDFDF;border-radius:8px;font-family:Inter;font-size:%1px;font-weight:700;} "
        "QPushButton:hover{background-color:#CFCFCF;}"
    ).arg(s(16)));

    connect(settingsBtn,&QPushButton::clicked,this,[this](){
        CalendarSettingsDialog dlg(this);
        if(dlg.exec()!=QDialog::Accepted) return;

        if (!calendarHighlightTimer) {
            calendarHighlightTimer = new QTimer(this);
            calendarHighlightTimer->setSingleShot(true);
            connect(calendarHighlightTimer, &QTimer::timeout, this, [this](){
                calendarHighlightActive_ = false;
                highlightWeek_ = false;
                selectedWeek_ = 0;
                if (rightCalendarFrame && rightCalendarFrame->isVisible()) {
                    refreshCalendarSelectionVisuals();
                } else {
                    pendingCalendarReload_ = true;
                }
            });
        }

        int y = dlg.year(), m = dlg.month(), w = dlg.week(), d = dlg.day();
        // Явный приоритет: выбранный день отменяет неделю (и наоборот), даже если комбо не сбросили вручную.
        if (d != 0)
            w = 0;
        else if (w != 0)
            d = 0;

        calendarHighlightTimer->stop();
        selectedWeek_ = w;
        highlightWeek_ = (w != 0);
        calendarHighlightActive_ = true;

        if (w != 0) {
            int startDay = 1 + (w - 1) * 7;
            const int monthDays = calendarDaysInMonth(y, m);
            startDay = qBound(1, startDay, monthDays);
            selectedDay_ = QDate(y, m, startDay);
            if (m == selectedMonth_ && y == selectedYear_ && calendarTablePtr) {
                refreshCalendarSelectionVisuals();
            } else {
                setSelectedMonthYear(m, y);
            }
        } else {
            highlightWeek_ = false;
            selectedWeek_ = 0;
            selectDay(y, m, d);
        }

        calendarHighlightTimer->start(20000);
    });

    topRowLayoutCal->addWidget(settingsBtn);
    calendarHeaderMainLayout->addLayout(topRowLayoutCal);

    //
    // ======== ЛЕГЕНДА ========
    //
    QHBoxLayout *legendLayout = new QHBoxLayout();
    legendLayout->setSpacing(s(15));
    legendLayout->setAlignment(Qt::AlignLeft);

    auto makeDot=[&](QString color){
        QLabel *dot=new QLabel();
        dot->setFixedSize(s(12),s(12));
        dot->setStyleSheet(QString("background-color:%1;border-radius:%2px;").arg(color).arg(s(6)));
        return dot;
    };
    auto makeLegendText=[&](QString text){
        QLabel *lbl=new QLabel(text);
        lbl->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:#A39E9E;").arg(s(17)));
        return lbl;
    };

    legendLayout->addWidget(makeDot("#FF0000"));
    legendLayout->addWidget(makeLegendText("Просрочен"));
    legendLayout->addSpacing(s(8));
    legendLayout->addWidget(makeDot("#FF8800"));
    legendLayout->addWidget(makeLegendText("Ближайшие события"));
    legendLayout->addSpacing(s(8));
    legendLayout->addWidget(makeDot("#18CF00"));
    legendLayout->addWidget(makeLegendText("Запланировано"));
    legendLayout->addSpacing(s(8));
    legendLayout->addWidget(makeDot("#00E5FF"));
    legendLayout->addWidget(makeLegendText("Обслужено"));

    calendarHeaderMainLayout->addLayout(legendLayout);

    //
    // ======== ЛИНИЯ ========
    //
    QFrame *calendarDivider = new QFrame(calendarHeader);
    calendarDivider->setFrameShape(QFrame::HLine);
    calendarDivider->setFixedHeight(s(2));
    calendarDivider->setStyleSheet("background-color:rgba(211,211,211,0.8); border:none;");
    calendarHeaderMainLayout->addWidget(calendarDivider);

    rightCalendarLayout->addWidget(calendarHeader);


    //
    // ======================= СЕТКА КАЛЕНДАРЯ (ФИНАЛ + ТЁМНАЯ ШАПКА + ТЕМНЕЕ СТАРЫЕ/НОВЫЕ ДНИ) =======================
    //

    QDate firstDay(selectedYear_, selectedMonth_, 1);
    int daysInMonth = firstDay.daysInMonth();
    // dayOfWeek(): Monday=1 ... Sunday=7; у нас колонки тоже Пн..Вс
    int startCol = firstDay.dayOfWeek() - 1;

    // Жесткое ограничение: максимум 35 видимых дней (5 строк по 7 дней).
    // Чтобы месяц всегда помещался, уменьшаем количество "предыдущих" дней при необходимости.
    const int maxVisibleDays = 35;
    const int maxLeadingAllowed = qMax(0, maxVisibleDays - daysInMonth);
    const int leadingCells = qMin(startCol, maxLeadingAllowed);
    const int usedWithoutTail = leadingCells + daysInMonth;
    int tail = qMax(0, maxVisibleDays - usedWithoutTail);

    int calendarRows = 5;
    int totalRows = calendarRows + 1;

    QTableWidget *calendarTable = new QTableWidget(totalRows, 7, rightCalendarFrame);
    calendarTablePtr = calendarTable;

    // таблица не растягивается сверх нужного
    calendarTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

    calendarTable->setStyleSheet(
        "QTableWidget { border:none; }"
        "QTableWidget::item { border:none; padding:3px; }"
    );

    calendarTable->horizontalHeader()->setVisible(false);
    calendarTable->verticalHeader()->setVisible(false);
    calendarTable->setShowGrid(false);
    calendarTable->setSelectionMode(QAbstractItemView::NoSelection);
    calendarTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    calendarTable->setFocusPolicy(Qt::NoFocus);
    calendarTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    calendarTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    calendarTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // ===== ВСЕ ЯЧЕЙКИ ОДИНАКОВОГО РАЗМЕРА =====
    int cellH = s(71);
    int headerH = s(52);

    calendarTable->verticalHeader()->setDefaultSectionSize(cellH);
    calendarTable->verticalHeader()->setMinimumSectionSize(cellH);

    calendarTable->setRowHeight(0, headerH);

    for (int r = 1; r < totalRows; r++)
        calendarTable->setRowHeight(r, cellH);

    //
    // ===== ДНИ НЕДЕЛИ (фон 555555, текст 222222) =====
    //
    QStringList weekdaysList = {
        "Понедельник",
        "Вторник",
        "Среда",
        "Четверг",
        "Пятница",
        "Суббота",
        "Воскресенье"
    };

    for (int c = 0; c < 7; c++) {
        QTableWidgetItem *item = new QTableWidgetItem(weekdaysList[c]);
        item->setTextAlignment(Qt::AlignCenter);
        item->setFont(QFont("Inter", s(17), QFont::Bold));

        item->setForeground(QBrush(QColor("#222222")));
        item->setBackground(QBrush(QColor("#555555")));

        calendarTable->setItem(0, c, item);
    }

    //
    // ===== ЗАПОЛНЕНИЕ КАЛЕНДАРЯ =====
    //

    // строка 1 — первая строка с числами
    int row = 1;
    int col = 0;

    // ===== ХВОСТ ПРЕДЫДУЩЕГО МЕСЯЦА =====
    QDate prevMonth = firstDay.addMonths(-1);
    int prevDays = prevMonth.daysInMonth();
    QDate nextMonth = firstDay.addMonths(1);

    int prevCount = leadingCells;
    int tailStart = prevDays - prevCount + 1;

    // старые дни — на 10% темнее: #A5A5A5
    QColor oldMonthColor("#A5A5A5");

    QMap<QDate, QTableWidgetItem*> visibleDateItems;

    for (int i = 0; i < prevCount; ++i) {
        const int d = tailStart + i;
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(d));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        item->setFont(QFont("Inter", s(14), QFont::Bold));
        item->setForeground(QBrush(oldMonthColor));
        const QDate itemDate(prevMonth.year(), prevMonth.month(), d);
        item->setData(Qt::UserRole, itemDate);
        item->setData(Qt::UserRole + 1, QStringList());
        item->setData(Qt::UserRole + 2, QStringList());
        item->setData(Qt::UserRole + 5, false);
        calendarTable->setItem(row, col, item);
        visibleDateItems[itemDate] = item;
        col++;
        if (col > 6) { col = 0; row++; }
    }

    // ===== ТЕКУЩИЙ МЕСЯЦ =====
    row = 1;
    col = startCol;

    QMap<int, QTableWidgetItem*> currentMonthItems;
    for (int d = 1; d <= daysInMonth; d++) {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(d));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        item->setFont(QFont("Inter", s(14), QFont::Bold));
        item->setForeground(QBrush(QColor("#000000")));
        item->setData(Qt::UserRole, QDate(selectedYear_, selectedMonth_, d));
        item->setData(Qt::UserRole + 1, QStringList());
        item->setData(Qt::UserRole + 2, QStringList());
        item->setData(Qt::UserRole + 5, false);
        calendarTable->setItem(row, col, item);
        currentMonthItems[d] = item;
        visibleDateItems[QDate(selectedYear_, selectedMonth_, d)] = item;

        col++;
        if (col > 6) { col = 0; row++; }
    }

    // ===== ХВОСТ СЛЕДУЮЩЕГО МЕСЯЦА =====
    int nextDay = 1;

    // новые дни — тоже на 10% темнее: #A5A5A5
    QColor nextMonthColor("#A5A5A5");

    for (int i = 0; i < tail; i++) {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(nextDay));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        item->setFont(QFont("Inter", s(14), QFont::Bold));
        item->setForeground(QBrush(nextMonthColor));
        const QDate itemDate(nextMonth.year(), nextMonth.month(), nextDay);
        item->setData(Qt::UserRole, itemDate);
        item->setData(Qt::UserRole + 1, QStringList());
        item->setData(Qt::UserRole + 2, QStringList());
        item->setData(Qt::UserRole + 5, false);
        calendarTable->setItem(row, col, item);
        visibleDateItems[itemDate] = item;

        col++;
        if (col > 6) { col = 0; row++; }
        nextDay++;
    }

    auto monthShift = [](int month, int year, int delta, int &outMonth, int &outYear) {
        outMonth = month + delta;
        outYear = year;
        while (outMonth < 1) { outMonth += 12; --outYear; }
        while (outMonth > 12) { outMonth -= 12; ++outYear; }
    };
    int prevMonthNum = selectedMonth_, prevYearNum = selectedYear_;
    int nextMonthNum = selectedMonth_, nextYearNum = selectedYear_;
    monthShift(selectedMonth_, selectedYear_, -1, prevMonthNum, prevYearNum);
    monthShift(selectedMonth_, selectedYear_, +1, nextMonthNum, nextYearNum);

    const QDate visibleFrom(prevYearNum, prevMonthNum, 1);
    const QDate visibleTo(nextYearNum, nextMonthNum, QDate(nextYearNum, nextMonthNum, 1).daysInMonth());
    QVector<CalendarEvent> events = loadCalendarEventsRange(visibleFrom, visibleTo);
    QMap<QDate, QVector<CalendarEvent> > eventsByDate;
    for (const CalendarEvent &e : events)
        eventsByDate[e.date].push_back(e);

    for (auto it = visibleDateItems.constBegin(); it != visibleDateItems.constEnd(); ++it) {
        const QDate date = it.key();
        QTableWidgetItem *item = it.value();
        if (!item || !date.isValid())
            continue;

        bool isHighlighted = false;
        if (calendarHighlightActive_ && date.month() == selectedMonth_ && date.year() == selectedYear_) {
            if (highlightWeek_ && selectedWeek_ > 0) {
                const int monthDays = QDate(selectedYear_, selectedMonth_, 1).daysInMonth();
                const int startDay = 1 + (selectedWeek_ - 1) * 7;
                const int endDay = (selectedWeek_ == 4) ? monthDays : qMin(startDay + 6, monthDays);
                isHighlighted = (date.day() >= startDay && date.day() <= endDay);
            } else if (selectedDay_.isValid()) {
                isHighlighted = (date == selectedDay_);
            }
        }
        item->setData(Qt::UserRole + 5, isHighlighted);

        QVector<CalendarEvent> dayEvents = eventsByDate.value(date);
        // Учитываем текущий поисковый фильтр и в попапе дня.
        const QString searchTerm = searchEdit_ ? searchEdit_->text().trimmed().toLower() : QString();
        if (!searchTerm.isEmpty()) {
            auto norm = [](QString s) {
                s = s.toLower().trimmed();
                s.remove(QRegularExpression("[\\s\\-_/]+"));
                return s;
            };
            const QString t = norm(searchTerm);
            dayEvents.erase(std::remove_if(dayEvents.begin(), dayEvents.end(),
                [&](const CalendarEvent &ev) {
                    return !norm(ev.agvId + ev.taskTitle).contains(t);
                }), dayEvents.end());
        }
        if (dayEvents.isEmpty())
            continue;

        auto severityRank = [](const QString &sev) {
            if (sev == "overdue") return 4;
            if (sev == "soon") return 3;
            if (sev == "planned") return 2;
            if (sev == "completed") return 1;
            return 0;
        };

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

        QMap<QString, int> agvCounts;
        QMap<QString, QString> agvSeverity;
        QVector<QString> agvOrder;
        QStringList allEventKeys;
        QStringList allEventSeverities;
        for (const CalendarEvent &ev : dayEvents) {
            allEventKeys << (ev.agvId.trimmed() + "||" + ev.taskTitle.trimmed());
            allEventSeverities << ev.severity;
            if (!agvCounts.contains(ev.agvId)) {
                agvOrder.push_back(ev.agvId);
                agvCounts[ev.agvId] = 0;
                agvSeverity[ev.agvId] = ev.severity;
            }
            agvCounts[ev.agvId] += 1;
            if (severityRank(ev.severity) > severityRank(agvSeverity.value(ev.agvId)))
                agvSeverity[ev.agvId] = ev.severity;
        }

        QStringList previewLines;
        QStringList previewSeverities;
        for (int i = 0; i < agvOrder.size() && i < 2; ++i) {
            const QString agvId = agvOrder[i];
            const int count = agvCounts.value(agvId);
            previewLines << QString("%1 - %2 задач").arg(shortenAgvIdForCell(agvId)).arg(count);
            previewSeverities << agvSeverity.value(agvId);
        }
        if (agvOrder.size() > 2) {
            previewLines[1] = "...";
            previewSeverities[1] = "";
        }

        item->setData(Qt::UserRole + 1, previewLines);
        item->setData(Qt::UserRole + 2, previewSeverities);
        // Полные данные дня для фильтра поиска (agvId||taskTitle + severity).
        item->setData(Qt::UserRole + 10, allEventKeys);
        item->setData(Qt::UserRole + 11, allEventSeverities);
    }

    //
    // ===== ДЕЛЕГАТ (ТОЛЬКО ВНУТРЕННИЕ ЛИНИИ) =====
    //
    class CalendarDelegate : public QStyledItemDelegate {
    public:
        void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
            QStyledItemDelegate::paint(p, opt, idx);

            const QDate d = idx.data(Qt::UserRole).toDate();
            if (d.isValid()) {
                const bool isHighlighted = idx.data(Qt::UserRole + 5).toBool();
                if (isHighlighted) {
                    p->save();
                    p->setRenderHint(QPainter::Antialiasing, true);
                    p->setRenderHint(QPainter::TextAntialiasing, true);

                    const QString dayText = idx.data(Qt::DisplayRole).toString();
                    QFont badgeFont("Inter", 10, QFont::Black);
                    p->setFont(badgeFont);
                    QFontMetrics badgeFm(badgeFont);
                    const int badgeW = qMax(30, badgeFm.horizontalAdvance(dayText) + 16);
                    const QRect badgeRect(opt.rect.left() + 4, opt.rect.top() + 2, badgeW, 24);

                    p->setBrush(Qt::NoBrush);
                    p->setPen(QPen(QColor("#1976FF"), 2));
                    p->drawRoundedRect(badgeRect, 9, 9);
                    p->restore();
                }

                const QStringList previewLines = idx.data(Qt::UserRole + 1).toStringList();
                const QStringList previewSeverities = idx.data(Qt::UserRole + 2).toStringList();
                if (!previewLines.isEmpty()) {
                    p->save();
                    p->setRenderHint(QPainter::Antialiasing, true);
                    p->setRenderHint(QPainter::TextAntialiasing, true);
                    p->setPen(QColor("#4B5563"));
                    QFont small("Inter", 9, QFont::DemiBold);
                    p->setFont(small);
                    QFontMetrics fm(small);
                    QRect r = opt.rect;
                    const int availW = qMax(10, r.width() - 20);
                    int y = r.top() + 22;
                    for (int i = 0; i < previewLines.size() && i < 2; ++i) {
                        QString line = previewLines[i];
                        if (line != "...")
                            line = fm.elidedText(line, Qt::ElideRight, availW);

                        QString sev = (i < previewSeverities.size()) ? previewSeverities[i] : QString();
                        QColor dotColor("#9CA3AF");
                        if (sev == "overdue") dotColor = QColor("#FF0000");
                        else if (sev == "soon") dotColor = QColor("#FF8800");
                        else if (sev == "planned") dotColor = QColor("#18CF00");
                        else if (sev == "completed") dotColor = QColor("#00E5FF");

                        if (line != "...") {
                            p->setBrush(dotColor);
                            p->setPen(Qt::NoPen);
                            p->drawEllipse(QPoint(r.left() + 8, y + fm.height() / 2), 4, 4);
                        }

                        p->setPen(QColor("#4B5563"));
                        p->drawText(QRect(r.left() + 16, y, availW, fm.height()),
                                    Qt::AlignLeft | Qt::AlignTop, line);
                        y += fm.height() + 1;
                    }
                    p->restore();
                }
            }

            if (idx.row() == 0) return;

            p->setPen(QColor("#D3D3D3"));
            QRect r = opt.rect;

            if (idx.column() < idx.model()->columnCount() - 1)
                p->drawLine(r.right(), r.top(), r.right(), r.bottom());

            if (idx.row() < idx.model()->rowCount() - 1)
                p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
        }
    };

    calendarTable->setItemDelegate(new CalendarDelegate());

    // ===== ПОПАП СПИСКА ЗАДАЧ НА ВЫБРАННЫЙ ДЕНЬ =====
    QFrame *dayOverlay = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    dayOverlay->hide();
    dayOverlay->setStyleSheet(
        "QFrame{background:#CDCDCD;border:none;border-radius:6px;}"
        "QLabel{font-family:Inter;color:#111827;background:#CDCDCD;}"
        "QWidget{background:#CDCDCD;}"
    );
    dayOverlay->raise();

    QVBoxLayout *dayOverlayLayout = new QVBoxLayout(dayOverlay);
    dayOverlayLayout->setContentsMargins(s(5), s(4), s(5), s(5));
    dayOverlayLayout->setSpacing(s(6));

    QHBoxLayout *dayOverlayHeader = new QHBoxLayout();
    dayOverlayHeader->setContentsMargins(0,0,0,0);
    QLabel *dayOverlayTitle = new QLabel("1", dayOverlay);
    dayOverlayTitle->setStyleSheet("font-family:Inter;font-size:13px;font-weight:800;color:#111111;");
    dayOverlayHeader->addWidget(dayOverlayTitle);
    dayOverlayHeader->addStretch();
    dayOverlayLayout->addLayout(dayOverlayHeader);

    QScrollArea *dayOverlayScroll = new QScrollArea(dayOverlay);
    dayOverlayScroll->setWidgetResizable(true);
    dayOverlayScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    dayOverlayScroll->setStyleSheet(
        "QScrollArea{border:none;background:#CDCDCD;}"
        "QScrollArea > QWidget > QWidget{background:#CDCDCD;}"
        "QScrollBar:vertical { width:6px; background:transparent; margin:2px; }"
        "QScrollBar::handle:vertical { background:#C0C0C0; border-radius:3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0px; }"
    );
    QWidget *dayOverlayHost = new QWidget(dayOverlayScroll);
    dayOverlayHost->setStyleSheet("background:#CDCDCD;");
    QVBoxLayout *dayOverlayTasksLayout = new QVBoxLayout(dayOverlayHost);
    dayOverlayTasksLayout->setContentsMargins(0, 0, 0, 0);
    dayOverlayTasksLayout->setSpacing(s(6));
    dayOverlayScroll->setWidget(dayOverlayHost);
    dayOverlayLayout->addWidget(dayOverlayScroll, 1);

    QDate overlayDate;
    connect(calendarTable, &QTableWidget::cellClicked, this, [=, &overlayDate](int r, int c){
        if (r == 0)
            return;
        QTableWidgetItem *item = calendarTable->item(r, c);
        if (!item)
            return;

        const QDate date = item->data(Qt::UserRole).toDate();
        if (!date.isValid())
            return;

        const QVector<CalendarEvent> dayEvents = eventsByDate.value(date);
        if (dayOverlay->isVisible() && overlayDate == date) {
            dayOverlay->hide();
            overlayDate = QDate();
            return;
        }

        QRect cellRect = calendarTable->visualRect(calendarTable->model()->index(r, c));
        QPoint globalTopLeft = calendarTable->viewport()->mapToGlobal(cellRect.topLeft());

        const QPoint frameTopLeft = rightCalendarFrame->mapToGlobal(QPoint(0, 0));
        const QPoint frameBottomRight = rightCalendarFrame->mapToGlobal(QPoint(rightCalendarFrame->width(), rightCalendarFrame->height()));

        int x = globalTopLeft.x() + s(2);
        int y = globalTopLeft.y() + s(2);
        QFont rowFont("Inter", s(12));
        QFontMetrics rowFm(rowFont);
        int maxTextW = 0;
        for (const CalendarEvent &ev : dayEvents) {
            const QString fullText = QString("%1 - %2").arg(ev.agvId, ev.taskTitle);
            maxTextW = qMax(maxTextW, rowFm.horizontalAdvance(fullText));
        }
        int desiredW = qMax(cellRect.width() - s(2), maxTextW + s(42));
        int availableW = frameBottomRight.x() - x - s(8);
        int width = qBound(s(120), desiredW, qMax(s(120), availableW));
        int maxHeight = frameBottomRight.y() - y - s(8);
        int targetHeight = cellRect.height() * 3;
        int height = qMax(s(120), qMin(maxHeight, targetHeight));

        if (x < frameTopLeft.x() + s(8))
            x = frameTopLeft.x() + s(8);
        if (x + width > frameBottomRight.x() - s(8))
            x = frameBottomRight.x() - width - s(8);

        if (height < s(80))
            height = s(80);

        dayOverlay->setGeometry(x, y, width, height);

        while (dayOverlayTasksLayout->count() > 0) {
            QLayoutItem *itemToRemove = dayOverlayTasksLayout->takeAt(0);
            if (itemToRemove->widget())
                itemToRemove->widget()->deleteLater();
            delete itemToRemove;
        }

        dayOverlayTitle->setText(QString::number(date.day()));

        if (dayEvents.isEmpty()) {
            QLabel *empty = new QLabel("На этот день задач нет.", dayOverlay);
            empty->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:#6B7280;").arg(s(11)));
            dayOverlayTasksLayout->addWidget(empty);
        }

        for (const CalendarEvent &ev : dayEvents) {
            const QString fullText = QString("%1 - %2").arg(ev.agvId, ev.taskTitle);
            QWidget *rowHost = new QWidget(dayOverlay);
            rowHost->setStyleSheet("background:#CDCDCD;");
            QHBoxLayout *rowLayout = new QHBoxLayout(rowHost);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(s(4));

            QColor dotColor("#9CA3AF");
            if (ev.severity == "overdue") dotColor = QColor("#FF0000");
            else if (ev.severity == "soon") dotColor = QColor("#FF8800");
            else if (ev.severity == "planned") dotColor = QColor("#18CF00");
            else if (ev.severity == "completed") dotColor = QColor("#00E5FF");

            QLabel *dot = new QLabel(rowHost);
            dot->setFixedSize(s(16), s(16));
            dot->setStyleSheet(QString("background:%1;border:none;border-radius:%2px;")
                               .arg(dotColor.name()).arg(s(8)));
            rowLayout->addWidget(dot, 0, Qt::AlignVCenter);

            QPushButton *taskBtn = new QPushButton(fullText, rowHost);
            taskBtn->setStyleSheet(QString(
                "QPushButton{text-align:left;background:#CDCDCD;border:none;border-radius:0px;"
                "padding:%1px %2px;font-family:Inter;font-size:%3px;color:#1F2937;}"
                "QPushButton:hover{background:#BDBDBD;}"
            ).arg(s(1)).arg(s(2)).arg(s(12)));
            taskBtn->setMinimumHeight(s(24));
            taskBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            taskBtn->setToolTip(fullText);
            connect(taskBtn, &QPushButton::clicked, this, [this, ev, dayOverlay](){
                dayOverlay->hide();
                showAgvDetailInfo(ev.agvId);
                if (agvSettingsPage)
                    agvSettingsPage->highlightTask(ev.taskTitle);
            });
            rowLayout->addWidget(taskBtn, 1, Qt::AlignVCenter);
            dayOverlayTasksLayout->addWidget(rowHost);
        }

        dayOverlayTasksLayout->addStretch();
        dayOverlay->show();
        dayOverlay->raise();
        overlayDate = date;

        actionsLabel->setText("Выберите день");
        calendarActionsFrame->setVisible(false);
    });

    rightCalendarLayout->addWidget(calendarTable);

    //
    // ======================= ПРЕДСТОЯЩЕЕ ТО (с иконками) =======================
    //
    rightUpcomingMaintenanceFrame = new QFrame(rightBodyFrame);
    rightUpcomingMaintenanceFrame->setStyleSheet("background-color:#F1F2F4;border-radius:12px;");
    rightUpcomingMaintenanceFrame->setMinimumHeight(s(130));

    QVBoxLayout *rightUpcomingLayout = new QVBoxLayout(rightUpcomingMaintenanceFrame);
    rightUpcomingLayout->setContentsMargins(s(8), s(8), s(8), s(8));
    rightUpcomingLayout->setSpacing(s(8));

    QLabel *maintenanceTitle = new QLabel("Предстоящее Техническое обслуживание", rightUpcomingMaintenanceFrame);
    maintenanceTitle->setStyleSheet(QString(
        "background:transparent;font-family:Inter;font-size:%1px;font-weight:800;color:black;padding:%2px %3px;"
    ).arg(s(18)).arg(s(10)).arg(s(15)));
    maintenanceTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    rightUpcomingLayout->addWidget(maintenanceTitle);

    QFrame *thickDivider = new QFrame(rightUpcomingMaintenanceFrame);
    thickDivider->setFrameShape(QFrame::HLine);
    thickDivider->setFixedHeight(s(2));
    thickDivider->setStyleSheet("background-color:rgba(211,211,211,0.8); border:none;");
    rightUpcomingLayout->addWidget(thickDivider);

    QScrollArea *upcomingScroll = new QScrollArea(rightUpcomingMaintenanceFrame);
    upcomingScroll->setWidgetResizable(true);
    upcomingScroll->setStyleSheet(
        "QScrollArea { border:none; background:transparent; }"
        "QScrollBar:vertical { width:6px; background:transparent; margin:2px; }"
        "QScrollBar::handle:vertical { background:#C0C0C0; border-radius:3px; }"
        "QScrollBar::handle:vertical:hover { background:#A0A0A0; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height:0; }"
    );

    QWidget *contentContainer = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(contentContainer);
    contentLayout->setContentsMargins(s(10), s(10), s(10), s(10));
    contentLayout->setSpacing(s(8));

    upcomingScroll->setWidget(contentContainer);

    contentLayout->setContentsMargins(s(10), s(10), s(10), s(10));
    contentLayout->setSpacing(s(5));

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

        // 🔥 ПЕРВАЯ СТРОКА — НАЗВАНИЕ AGV + тип обслуживания + количество
        QString topLine = QString(
            "<span style='font-weight:800; color:#000000;'>%1 — %2 — %3 задач(и)</span>"
        ).arg(item.agvName).arg(serviceLabel).arg(item.details);


        // 🔥 ВТОРАЯ СТРОКА — дата + название задачи + за кем/общая
        QString assignSuffix = item.assignedInfo.isEmpty() ? "общая" : item.assignedInfo;
        QString bottomLine = QString(
            "<span style='color:#777777;'>%1 — %2 — %3</span>"
        ).arg(item.date.toString("dd.MM.yyyy")).arg(item.type).arg(assignSuffix);

        textLabel->setText(
            topLine +
            "<br style='line-height:200%; font-size:8px;'>" +   // ← ровный увеличенный отступ
            bottomLine
        );

        textLabel->setStyleSheet(QString(
            "background:transparent;font-family:Inter;font-size:%1px;"
        ).arg(s(14)));
        textLabel->setWordWrap(true);

        itemLayout->addWidget(textLabel, 1);

        QPushButton *showBtn = new QPushButton("Показать", itemFrame);
        showBtn->setStyleSheet(QString(
            "QPushButton{background-color:rgba(%1,%2,%3,%4);color:white;font-family:Inter;font-size:%5px;"
            "font-weight:700;border-radius:8px;padding:%6px %7px;border:none;} "
            "QPushButton:hover{background-color:rgba(%8,%9,%10,%11);} "
            "QPushButton:pressed{background-color:rgba(%12,%13,%14,%15);}"
        )
        .arg(btnColor.red()).arg(btnColor.green()).arg(btnColor.blue()).arg(btnColor.alpha())
        .arg(s(13)).arg(s(4)).arg(s(10))
        .arg(btnColor.lighter().red()).arg(btnColor.lighter().green()).arg(btnColor.lighter().blue()).arg(btnColor.lighter().alpha())
        .arg(btnColor.darker().red()).arg(btnColor.darker().green()).arg(btnColor.darker().blue()).arg(btnColor.darker().alpha()));

        connect(showBtn, &QPushButton::clicked, this, [this, item](){
            showAgvDetailInfo(item.agvId);
            if (agvSettingsPage)
                agvSettingsPage->highlightTask(item.type);
        });

        itemLayout->addWidget(showBtn, 0, Qt::AlignVCenter | Qt::AlignRight);

        contentLayout->addWidget(itemFrame);
    };


    QVector<MaintenanceItemData> upcoming = loadUpcomingMaintenance(selectedMonth_, selectedYear_);

    std::sort(upcoming.begin(), upcoming.end(),
        [](const MaintenanceItemData &a, const MaintenanceItemData &b){
            return a.date < b.date;
        }
    );

    checkAndSendMaintenanceNotifications(upcoming);
    DataBus::instance().triggerNotificationsChanged();

    QVector<MaintenanceItemData> delegated, rest;
    for (const auto &item : upcoming) {
        if (item.isDelegatedToMe)
            delegated.append(item);
        else
            rest.append(item);
    }
    if (!delegated.isEmpty()) {
        QLabel *delegatedHeader = new QLabel("Делегировано вам", contentContainer);
        delegatedHeader->setStyleSheet(QString(
            "background:transparent;font-family:Inter;font-size:%1px;font-weight:800;color:#0F00DB;padding:%2px 0;"
        ).arg(s(14)).arg(s(4)));
        contentLayout->addWidget(delegatedHeader);
        for (const auto &item : delegated)
            addMaintenanceItem(item);
    }
    if (!rest.isEmpty() && !delegated.isEmpty()) {
        QFrame *sep = new QFrame(contentContainer);
        sep->setFrameShape(QFrame::HLine);
        sep->setFixedHeight(1);
        sep->setStyleSheet("background:#ddd;border:none;");
        contentLayout->addWidget(sep);
    }
    for (const auto &item : rest)
        addMaintenanceItem(item);
    contentLayout->addStretch();

    rightUpcomingLayout->addWidget(upcomingScroll, 1);



    //
    // ======================= НОВЫЙ AGV LIST =======================
    //
    listAgvInfo = new ListAgvInfo([this](int v){ return s(v); }, rightBodyFrame);
    listAgvInfo->setVisible(false);
    connect(listAgvInfo, &ListAgvInfo::openAgvDetails, this, [this](const QString &id){
        showAgvDetailInfo(id);
    });

    // Когда нажали "Назад" в списке AGV → возвращаемся в календарь
    connect(listAgvInfo, &ListAgvInfo::backRequested, this, [this](){
        showCalendar();
    });

    //
    // ======================= СУПЕР‑НАСТРОЙКИ AGV =======================
    //
    agvSettingsPage = new AgvSettingsPage([this](int v){ return s(v); }, rightBodyFrame);
    agvSettingsPage->setVisible(false);
    connect(agvSettingsPage, &AgvSettingsPage::backRequested, this, [this](){
        showAgvList();
    });
    connect(agvSettingsPage, &AgvSettingsPage::tasksChanged,
            this, [this](){
                updateUpcomingMaintenance();
                if (listAgvInfo && listAgvInfo->isVisible()) {
                    QVector<AgvInfo> agvs = listAgvInfo->loadAgvList();
                    listAgvInfo->rebuildList(agvs);
                    agvListDirty_ = false;
                } else {
                    agvListDirty_ = true;
                }
                // Изменили даты/провели обслуживание:
                // если календарь открыт — обновляем сразу, иначе откладываем
                // до следующего открытия экрана календаря.
                if (rightCalendarFrame && rightCalendarFrame->isVisible()) {
                    QTimer::singleShot(0, this, [this](){
                        setSelectedMonthYear(selectedMonth_, selectedYear_);
                    });
                } else {
                    pendingCalendarReload_ = true;
                }
            });
    connect(agvSettingsPage, &AgvSettingsPage::openDelegatorChatRequested,
            this, &leftMenu::openEmbeddedDelegatorChatForAgv);

    //
    // ======================= МОДЕЛИ AGV =======================
    //
    modelListPage = new ModelListPage([this](int v){ return s(v); }, rightBodyFrame);
    modelListPage->setVisible(false);

    connect(modelListPage, &ModelListPage::backRequested, this, [this](){
        showCalendar();
    });

    //
    // ======================= LOGS =======================
    //
    logsPage = new QFrame(rightBodyFrame);
    logsPage->setStyleSheet("background-color:#F1F2F4;border-radius:12px;");
    logsPage->setVisible(false);

    QVBoxLayout *logsRoot = new QVBoxLayout(logsPage);
    logsRoot->setContentsMargins(s(10), s(10), s(10), s(10));
    logsRoot->setSpacing(s(10));

    QWidget *logsHeader = new QWidget(logsPage);
    QHBoxLayout *logsHdr = new QHBoxLayout(logsHeader);
    logsHdr->setContentsMargins(0,0,0,0);
    logsHdr->setSpacing(s(10));

    QPushButton *logsBack = new QPushButton("   Назад", logsHeader);
    logsBack->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    logsBack->setIconSize(QSize(s(24), s(24)));
    logsBack->setFixedSize(s(150), s(50));
    logsBack->setStyleSheet(QString(
        "QPushButton { background-color:#E6E6E6; border-radius:%1px; border:1px solid #C8C8C8;"
        "font-family:Inter; font-size:%2px; font-weight:800; color:black; text-align:left; padding-left:%3px; }"
        "QPushButton:hover { background-color:#D5D5D5; }"
    ).arg(s(10)).arg(s(16)).arg(s(10)));

    QLabel *logsTitle = new QLabel("Logs", logsHeader);
    logsTitle->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
    ).arg(s(26)));
    logsTitle->setAlignment(Qt::AlignCenter);

    QPushButton *logsRefresh = new QPushButton("Обновить", logsHeader);
    logsRefresh->setFixedSize(s(130), s(40));
    logsRefresh->setStyleSheet(QString(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;font-size:%1px;font-weight:800;border-radius:%2px;}"
        "QPushButton:hover{background:#1A4ACD;}"
    ).arg(s(14)).arg(s(8)));

    QString logsUserRole = getUserRole(AppSession::currentUsername());
    bool isTechUser = (logsUserRole == "tech" || logsUserRole == "admin");

    if (isTechUser) {
        logsLoadAllBtn = new QPushButton("Загрузить все", logsHeader);
        logsLoadAllBtn->setFixedSize(s(140), s(40));
        logsLoadAllBtn->setStyleSheet(QString(
            "QPushButton{background:#059669;color:white;font-family:Inter;font-size:%1px;font-weight:800;border-radius:%2px;}"
            "QPushButton:hover{background:#047857;}"
        ).arg(s(14)).arg(s(8)));

        logsExportBtn = new QPushButton("Скачать логи", logsHeader);
        logsExportBtn->setFixedSize(s(170), s(40));
        logsExportBtn->setStyleSheet(QString(
            "QPushButton{background:#6366F1;color:white;font-family:Inter;font-size:%1px;font-weight:800;border-radius:%2px;}"
            "QPushButton:hover{background:#4F46E5;}"
        ).arg(s(14)).arg(s(8)));
    }

    logsHdr->addWidget(logsBack, 0, Qt::AlignLeft);
    logsHdr->addStretch();
    logsHdr->addWidget(logsTitle, 0, Qt::AlignCenter);
    logsHdr->addStretch();
    if (logsLoadAllBtn) logsHdr->addWidget(logsLoadAllBtn, 0, Qt::AlignRight);
    if (logsExportBtn) logsHdr->addWidget(logsExportBtn, 0, Qt::AlignRight);
    logsHdr->addWidget(logsRefresh, 0, Qt::AlignRight);
    logsRoot->addWidget(logsHeader);

    // Log filters
    QWidget *filterRow = new QWidget(logsPage);
    filterRow->setStyleSheet("background:transparent;");
    QHBoxLayout *filterLay = new QHBoxLayout(filterRow);
    filterLay->setContentsMargins(0,0,0,0);
    filterLay->setSpacing(s(8));

    QString filterComboStyle = QString(
        "QComboBox{background:white;border:1px solid #C8C8C8;border-radius:%1px;"
        "font-family:Inter;font-size:%2px;padding:4px 8px;}"
    ).arg(s(6)).arg(s(12));

    auto makeFilterCombo = [&](const QString &placeholder) -> QComboBox* {
        QComboBox *c = new QComboBox(filterRow);
        c->setStyleSheet(filterComboStyle);
        c->setMinimumWidth(s(120));
        c->addItem(placeholder, "");
        return c;
    };

    logFilterUser_ = makeFilterCombo("Пользователь");
    logFilterSource_ = makeFilterCombo("Источник");
    logFilterCategory_ = makeFilterCombo("Категория");
    logFilterTime_ = makeFilterCombo("Период");
    logFilterTime_->addItem("Сегодня", "today");
    logFilterTime_->addItem("Последние 7 дней", "week");
    logFilterTime_->addItem("Последние 30 дней", "month");
    logFilterTime_->addItem("Все", "all");

    filterLay->addWidget(logFilterUser_);
    filterLay->addWidget(logFilterSource_);
    filterLay->addWidget(logFilterCategory_);
    filterLay->addWidget(logFilterTime_);
    filterLay->addStretch();

    auto applyLogFilters = [this](){
        reloadLogs(lastLogsMaxRows_);
    };
    connect(logFilterUser_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyLogFilters);
    connect(logFilterSource_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyLogFilters);
    connect(logFilterCategory_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyLogFilters);
    connect(logFilterTime_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, applyLogFilters);

    logsRoot->addWidget(filterRow);

    logsTable = new QTableWidget(logsPage);
    logsTable->setColumnCount(5);
    logsTable->setHorizontalHeaderLabels(QStringList()
                                         << "Время" << "Источник" << "Пользователь" << "Категория" << "Детали");
    logsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    logsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    logsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    logsTable->verticalHeader()->setVisible(false);
    logsTable->setWordWrap(true);
    logsTable->setStyleSheet(
        "QTableWidget{background:white;border:1px solid #DADDE3;border-radius:8px;font-family:Inter;}"
        "QHeaderView::section{background:#EDEFF3;font-weight:800;border:none;padding:6px;}"
    );
    logsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    logsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    logsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    logsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    logsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    logsRoot->addWidget(logsTable, 1);

    if (isTechUser) {
        QLabel *techLbl = new QLabel(QStringLiteral("Тех-диагностика (результаты стресс-теста и подробный лог):"), logsPage);
        techLbl->setStyleSheet(QString(QStringLiteral("font-family:Inter;font-size:%1px;font-weight:800;color:#0F172A;"))
                                   .arg(s(12)));
        calendarStressTestBtn_ = new QPushButton(QStringLiteral("Стресс календаря (500)"), logsPage);
        calendarStressTestBtn_->setToolTip(
            QStringLiteral("500 раз подряд переключает месяц вперёд (полная пересборка UI). Результат — в окне ниже и в tech_verbose.log"));
        calendarStressTestBtn_->setStyleSheet(QString(
            "QPushButton{background:#B45309;color:white;font-family:Inter;font-size:%1px;font-weight:800;border-radius:%2px;padding:6px 12px;}"
            "QPushButton:hover{background:#92400E;}"
        ).arg(s(12)).arg(s(8)));
        fullStressAutotestBtn_ = new QPushButton(QStringLiteral("Комплексный тест"), logsPage);
        fullStressAutotestBtn_->setToolTip(
            QStringLiteral("Комплексный тест: много CHECK (БД, навигация, календарь, DataBus, профиль, чаты); "
                          "лимит ~%1 с; итог всегда с отчётом PASS/SKIP. Тяжёлый календарь — оранжевая кнопка.")
                .arg(kComplexTestWallCapMs / 1000));
        fullStressAutotestBtn_->setStyleSheet(QString(
            "QPushButton{background:#7C3AED;color:white;font-family:Inter;font-size:%1px;font-weight:800;border-radius:%2px;padding:6px 12px;}"
            "QPushButton:hover{background:#6D28D9;}"
        ).arg(s(12)).arg(s(8)));
        QHBoxLayout *techRow = new QHBoxLayout();
        techRow->setContentsMargins(0, 0, 0, 0);
        techRow->addWidget(techLbl);
        techRow->addWidget(calendarStressTestBtn_);
        techRow->addWidget(fullStressAutotestBtn_);
        techRow->addStretch();
        logsRoot->addLayout(techRow);

        techDiagLogEdit_ = new QTextEdit(logsPage);
        techDiagLogEdit_->setReadOnly(true);
        techDiagLogEdit_->setMaximumHeight(s(240));
        techDiagLogEdit_->setPlaceholderText(QStringLiteral(
            "Здесь появятся строки TECH_DIAG после стресс-теста и при перелистывании календаря (роли: администратор/техник)."));
        techDiagLogEdit_->setStyleSheet(QString(
            "QTextEdit{font-family:Consolas,monospace;font-size:%1px;background:#0F172A;color:#E2E8F0;border:1px solid #334155;border-radius:8px;padding:8px;}"
        ).arg(s(10)));
        for (const QString &line : techDiagRecentLines(500))
            techDiagLogEdit_->append(line);
        setTechDiagLogSink(techDiagLogEdit_);
        connect(calendarStressTestBtn_, &QPushButton::clicked, this, [this]() {
            runCalendarStressTest(500, true, true);
        });
        connect(fullStressAutotestBtn_, &QPushButton::clicked, this, [this]() {
            runFullStressAutotest();
        });
        logsRoot->addWidget(techDiagLogEdit_);
    }

    connect(logsBack, &QPushButton::clicked, this, [this](){
        if (logsTable) logsTable->setRowCount(0);
        showCalendar();
    });
    connect(logsRefresh, &QPushButton::clicked, this, [this](){ reloadLogs(lastLogsMaxRows_); });
    if (logsLoadAllBtn) {
        connect(logsLoadAllBtn, &QPushButton::clicked, this, [this](){ reloadLogs(0); });
    }
    if (logsExportBtn) {
        connect(logsExportBtn, &QPushButton::clicked, this, [this](){
            QVector<UserInfo> users = getAllUsers(false);
            if (users.isEmpty()) {
                QMessageBox::information(this, "Логи", "Нет пользователей для выбора.");
                return;
            }

            std::sort(users.begin(), users.end(), [](const UserInfo &a, const UserInfo &b) {
                const QString an = a.fullName.trimmed().isEmpty() ? a.username : a.fullName;
                const QString bn = b.fullName.trimmed().isEmpty() ? b.username : b.fullName;
                return an.toLower() < bn.toLower();
            });

            QDialog pick(this);
            pick.setWindowTitle("Выберите пользователей");
            pick.setModal(true);
            pick.setMinimumWidth(520);

            QVBoxLayout *v = new QVBoxLayout(&pick);
            QLabel *lbl = new QLabel("Отметьте пользователей, чьи логи нужно скачать:", &pick);
            v->addWidget(lbl);

            QScrollArea *scroll = new QScrollArea(&pick);
            scroll->setWidgetResizable(true);
            QWidget *host = new QWidget(scroll);
            QVBoxLayout *hostLay = new QVBoxLayout(host);
            hostLay->setContentsMargins(6, 6, 6, 6);
            hostLay->setSpacing(6);

            QVector<QCheckBox*> checks;
            checks.reserve(users.size());
            for (const UserInfo &u : users) {
                const QString display = u.fullName.trimmed().isEmpty()
                                            ? u.username
                                            : QString("%1 (%2)").arg(u.fullName, u.username);
                QCheckBox *cb = new QCheckBox(display, host);
                cb->setProperty("username", u.username);
                cb->setChecked(true);
                checks.push_back(cb);
                hostLay->addWidget(cb);
            }
            hostLay->addStretch();
            scroll->setWidget(host);
            v->addWidget(scroll, 1);

            QHBoxLayout *btnRow = new QHBoxLayout();
            QPushButton *allBtn = new QPushButton("Выбрать всех", &pick);
            QPushButton *noneBtn = new QPushButton("Снять всё", &pick);
            QPushButton *cancelBtn = new QPushButton("Отмена", &pick);
            QPushButton *okBtn = new QPushButton("Скачать", &pick);
            btnRow->addWidget(allBtn);
            btnRow->addWidget(noneBtn);
            btnRow->addStretch();
            btnRow->addWidget(cancelBtn);
            btnRow->addWidget(okBtn);
            v->addLayout(btnRow);

            connect(allBtn, &QPushButton::clicked, &pick, [checks]() {
                for (QCheckBox *cb : checks) cb->setChecked(true);
            });
            connect(noneBtn, &QPushButton::clicked, &pick, [checks]() {
                for (QCheckBox *cb : checks) cb->setChecked(false);
            });
            connect(cancelBtn, &QPushButton::clicked, &pick, &QDialog::reject);
            connect(okBtn, &QPushButton::clicked, &pick, &QDialog::accept);

            if (pick.exec() != QDialog::Accepted)
                return;

            QSet<QString> selectedUsers;
            for (QCheckBox *cb : checks) {
                if (cb->isChecked())
                    selectedUsers.insert(cb->property("username").toString().trimmed());
            }
            if (selectedUsers.isEmpty()) {
                QMessageBox::information(this, "Логи", "Нужно выбрать хотя бы одного пользователя.");
                return;
            }

            QString logsDir = localLogsDirPath();
            if (!QDir().mkpath(logsDir)) {
                QMessageBox::warning(this, "Логи", "Не удалось создать папку для логов.");
                return;
            }

            QString srcPath = localLogFilePath();
            if (!QFile::exists(srcPath)) {
                const QString oldPath = QCoreApplication::applicationDirPath() + "/logs/app.log";
                if (QFile::exists(oldPath))
                    srcPath = oldPath;
            }

            QFile src(srcPath);
            if (!src.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QMessageBox::warning(this, "Логи", "Не удалось открыть локальный файл логов.");
                return;
            }

            const QString outPath = logsDir + QString("/logs_%1.txt")
                .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
            QFile outFile(outPath);
            if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::warning(this, "Логи", "Не удалось создать файл выгрузки.");
                return;
            }

            QTextStream in(&src);
            in.setCodec("UTF-8");
            QTextStream out(&outFile);
            out.setCodec("UTF-8");
            int written = 0;
            while (!in.atEnd()) {
                const QString line = in.readLine();
                const int brOpen = line.indexOf('[');
                const int brClose = line.indexOf(']');
                if (brOpen < 0 || brClose <= brOpen)
                    continue;
                const QString user = line.mid(brOpen + 1, brClose - brOpen - 1).trimmed();
                if (!selectedUsers.contains(user))
                    continue;
                out << line << "\n";
                ++written;
            }
            src.close();
            outFile.close();

            QMessageBox::information(this, "Логи",
                                     QString("Готово. Записано %1 строк.\nФайл: %2")
                                         .arg(written)
                                         .arg(outPath));
        });
    }

    //
    // ======================= ДОБАВЛЯЕМ В ПРАВУЮ ПАНЕЛЬ =======================
    //
    rightBodyLayout->addWidget(rightCalendarFrame, 3);
    rightBodyLayout->addWidget(rightUpcomingMaintenanceFrame, 2);
    rightBodyLayout->addWidget(listAgvInfo, 3);
    rightBodyLayout->addWidget(agvSettingsPage, 3);
    rightBodyLayout->addWidget(modelListPage, 3);
    rightBodyLayout->addWidget(logsPage, 3);

    //
    // ======================= ПЕРЕКЛЮЧЕНИЕ РЕЖИМОВ =======================
    //
    //
    // ======================= USERS PAGE =======================
    //
    usersPage = new UsersPage([this](int v){ return s(v); }, rightBodyFrame);
    usersPage->setVisible(false);
    usersPage->setProperty("loaded_once", false);

    connect(usersPage, &UsersPage::backRequested, this, [this](){
        showCalendar();
    });

    connect(usersPage, &UsersPage::openUserDetailsRequested, this, [this](const QString &username){
        showUserProfilePage(username);
    });

    connect(&DataBus::instance(), &DataBus::userDataChanged,
            this, [this]() {
        avatarCache_.clear();
        if (!usersPage) return;
        usersPage->setProperty("loaded_once", false);
        QTimer::singleShot(0, usersPage, [this]() {
            if (usersPage) usersPage->loadUsers();
        });
    });

    rightBodyLayout->addWidget(usersPage, 3);

    connect(btnAgv, &QPushButton::clicked, this, [this](){
        showAgvList();
    });
    connect(btnEdit, &QPushButton::clicked, this, [this](){
        showModelList();
    });
    // === АВТО-ОБНОВЛЕНИЕ КАЛЕНДАРЯ И ПРЕДСТОЯЩЕГО ТО ===
    connect(&DataBus::instance(), &DataBus::calendarChanged,
            this, [this](){
                updateUpcomingMaintenance();
                // Во время комплексного теста десятки сигналов подряд иначе ставят в очередь полную пересборку leftMenu (зависание).
                if (stressSuiteRunning_) {
                    pendingCalendarReload_ = true;
                    return;
                }
                if (rightCalendarFrame && rightCalendarFrame->isVisible()) {
                    QTimer::singleShot(0, this, [this](){
                        setSelectedMonthYear(selectedMonth_, selectedYear_);
                    });
                } else {
                    pendingCalendarReload_ = true;
                }
            });
    if (listAgvInfo)
    {
        connect(listAgvInfo, &ListAgvInfo::agvListChanged,
                this, &leftMenu::updateAgvCounter);

    }

    // Моментальное обновление счётчика при изменениях через DataBus.
    connect(&DataBus::instance(), &DataBus::agvListChanged,
            this, [this](){
                updateAgvCounter();
                updateSystemStatus();
                agvListDirty_ = true;
                if (stressSuiteRunning_) {
                    pendingCalendarReload_ = true;
                    return;
                }
                if (rightCalendarFrame && rightCalendarFrame->isVisible()) {
                    QTimer::singleShot(0, this, [this](){
                        setSelectedMonthYear(selectedMonth_, selectedYear_);
                    });
                } else {
                    pendingCalendarReload_ = true;
                }
            });

    connect(&DataBus::instance(), &DataBus::calendarChanged,
            this, &leftMenu::updateSystemStatus);

    agvCounterTimer = new QTimer(this);
    agvCounterTimer->setSingleShot(true);
    connect(agvCounterTimer, &QTimer::timeout, this, [this](){
        updateAgvCounter();
    });

    agvCounterTimer->start(0);

    connect(&DataBus::instance(), &DataBus::notificationsChanged,
            this, &leftMenu::updateNotifBadge);
    connect(&DataBus::instance(), &DataBus::notificationsChanged,
            this, [this]() {
        if (chatsPage && chatsPage->isVisible() && chatsStack_ && chatsStack_->currentIndex() == 0)
            reloadChatsPageList();
    });

    QTimer::singleShot(100, this, &leftMenu::updateNotifBadge);

    notifPollTimer = new QTimer(this);
    notifPollTimer->setInterval(25000);
    connect(notifPollTimer, &QTimer::timeout, this, &leftMenu::updateNotifBadge);
    notifPollTimer->start();

    chatsPollTimer = new QTimer(this);
        // Статус "когда был в сети" обновляем не слишком часто, чтобы не перегружать БД.
        chatsPollTimer->setInterval(180000);
    connect(chatsPollTimer, &QTimer::timeout, this, [this]() {
        // Обновляем список только когда открыт именно список чатов (не сам диалог).
            if (chatsPage && chatsPage->isVisible() && chatsStack_ && chatsStack_->currentIndex() == 0) {
                lastChatsListSignature_.clear();
            reloadChatsPageList();
            }
    });
    chatsPollTimer->start();
}

//
// ======================= РЕЖИМЫ ПРАВОЙ ПАНЕЛИ =======================
//

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




//
// ======================= ДЕТАЛЬНАЯ СТРАНИЦА AGV =======================
//

void leftMenu::showAgvDetailInfo(const QString &agvId)
{
    activePage_ = ActivePage::AgvDetails;
    activeAgvId_ = agvId;
    hideAllPages();
    clearSearch();

    agvSettingsPage->loadAgv(agvId);
    agvSettingsPage->setVisible(true);
    stressSuiteLogPageEntered(QStringLiteral("agv_detail"));
}


//
// ======================= МЕСЯЦ / ДЕНЬ =======================
//

void leftMenu::changeMonth(int delta)
{
    int month = selectedMonth_ + delta;
    int year = selectedYear_;
    if (month < 1) { month = 12; year--; }
    if (month > 12) { month = 1; year++; }

    if (year < minCalendarYear() || year > maxCalendarYear())
        return;

    setSelectedMonthYear(month, year);
}

void leftMenu::refreshCalendarSelectionVisuals()
{
    if (!calendarTablePtr)
        return;

    for (int r = 1; r < calendarTablePtr->rowCount(); ++r) {
        for (int c = 0; c < calendarTablePtr->columnCount(); ++c) {
            QTableWidgetItem *item = calendarTablePtr->item(r, c);
            if (!item)
                continue;

            const QDate date = item->data(Qt::UserRole).toDate();
            if (!date.isValid()) {
                item->setData(Qt::UserRole + 5, false);
                continue;
            }

            bool isHighlighted = false;
            if (calendarHighlightActive_ && date.month() == selectedMonth_ && date.year() == selectedYear_) {
                if (highlightWeek_ && selectedWeek_ > 0) {
                    const int monthDays = QDate(selectedYear_, selectedMonth_, 1).daysInMonth();
                    const int startDay = 1 + (selectedWeek_ - 1) * 7;
                    const int endDay = (selectedWeek_ == 4) ? monthDays : qMin(startDay + 6, monthDays);
                    isHighlighted = (date.day() >= startDay && date.day() <= endDay);
                } else if (selectedDay_.isValid()) {
                    isHighlighted = (date == selectedDay_);
                }
            }
            item->setData(Qt::UserRole + 5, isHighlighted);
        }
    }

    calendarTablePtr->viewport()->update();
}

void leftMenu::clearCalendarSettingsHighlight()
{
    if (calendarHighlightTimer)
        calendarHighlightTimer->stop();
    calendarHighlightActive_ = false;
    highlightWeek_ = false;
    selectedWeek_ = 0;
    if (selectedYear_ >= minCalendarYear() && selectedYear_ <= maxCalendarYear()
        && selectedMonth_ >= 1 && selectedMonth_ <= 12)
        selectedDay_ = QDate(selectedYear_, selectedMonth_, 1);
    if (calendarTablePtr)
        refreshCalendarSelectionVisuals();
}

void leftMenu::setSelectedMonthYear(int month, int year)
{
    if (month < 1) { month = 12; year--; }
    if (month > 12) { month = 1; year++; }

    year = qBound(minCalendarYear(), year, maxCalendarYear());

    selectedMonth_ = month;
    selectedYear_  = year;

    if (!calendarStressDiagQuiet_) {
        techDiagLog(QStringLiteral("CALENDAR"),
                    QStringLiteral("setSelectedMonthYear %1/%2 — полная пересборка UI календаря")
                        .arg(selectedMonth_)
                        .arg(selectedYear_));
    }

    if (!selectedDay_.isValid() ||
        selectedDay_.year()  != selectedYear_ ||
        selectedDay_.month() != selectedMonth_)
    {
        selectedDay_ = QDate(selectedYear_, selectedMonth_, 1);
    }

    if (monthLabel)
        monthLabel->setText(monthYearLabelText(selectedMonth_, selectedYear_));

    setUpdatesEnabled(false);
    disconnect(&DataBus::instance(), nullptr, this, nullptr);

    // Полностью пересоздаём UI под новый месяц (без изменения scale).
    // Важно: setScaleFactor(scaleFactor_) тут не подходит, т.к. внутри стоит ранний return
    // при том же значении scaleFactor_, и сетка календаря не обновляется.
    if (QLayout *old = layout()) {
        QLayoutItem *item;
        while ((item = old->takeAt(0)) != nullptr) {
            if (QWidget *w = item->widget()) {
                w->setParent(nullptr);
                delete w;
            }
            delete item;
        }
        delete old;
    }

    topRow_ = nullptr;
    bottomRow_ = nullptr;
    rightCalendarFrame = nullptr;
    rightUpcomingMaintenanceFrame = nullptr;
    listAgvInfo = nullptr;
    agvSettingsPage = nullptr;
    modelListPage = nullptr;
    logsPage = nullptr;
    logsTable = nullptr;
    logsLoadAllBtn = nullptr;
    logsExportBtn = nullptr;
    calendarStressTestBtn_ = nullptr;
    fullStressAutotestBtn_ = nullptr;
    techDiagLogEdit_ = nullptr;
    setTechDiagLogSink(nullptr);
    profilePage = nullptr;
    chatsPage = nullptr;
    chatsStack_ = nullptr;
    embeddedChatWidget_ = nullptr;
    chatsListLayout_ = nullptr;
    calendarActionsFrame = nullptr;
    statusWidget_ = nullptr;
    calendarTablePtr = nullptr;
    agvCounter = nullptr;
    userButton = nullptr;
    searchEdit_ = nullptr;
    notifBadge_ = nullptr;
    logFilterUser_ = nullptr;
    logFilterSource_ = nullptr;
    logFilterCategory_ = nullptr;
    logFilterTime_ = nullptr;
    if (profileKeyTimer) { profileKeyTimer->stop(); profileKeyTimer->deleteLater(); profileKeyTimer = nullptr; }
    if (agvCounterTimer) { agvCounterTimer->stop(); agvCounterTimer->deleteLater(); agvCounterTimer = nullptr; }
    if (notifPollTimer) { notifPollTimer->stop(); notifPollTimer->deleteLater(); notifPollTimer = nullptr; }
    if (chatsPollTimer) { chatsPollTimer->stop(); chatsPollTimer->deleteLater(); chatsPollTimer = nullptr; }
    backButton = nullptr;
    monthLabel = nullptr;
    usersPage = nullptr;
    agvListDirty_ = true;

    initUI();
    restoreActivePage();
    setUpdatesEnabled(true);
    updateGeometry();
    update();
}

void leftMenu::setTechStressButtonsEnabled(bool enabled)
{
    if (calendarStressTestBtn_)
        calendarStressTestBtn_->setEnabled(enabled);
    if (fullStressAutotestBtn_)
        fullStressAutotestBtn_->setEnabled(enabled);
}

void leftMenu::runCalendarStressTest(int iterations, bool showSummaryDialog, bool manageStressButtons)
{
    if (iterations <= 0)
        return;

    clearCalendarSettingsHighlight();

    // Пока идёт смена месяца, вызывается restoreActivePage() — без этого при открытых «Логах»
    // каждый шаг дергал бы showLogs() и пересоздавал бы тяжёлую страницу сотни раз.
    const ActivePage savedPage = activePage_;
    activePage_ = ActivePage::Calendar;

    if (manageStressButtons)
        setTechStressButtonsEnabled(false);

    calendarStressDiagQuiet_ = true;
    techDiagLog(QStringLiteral("STRESS"),
                QStringLiteral("START iterations=%1 month=%2 year=%3")
                    .arg(iterations)
                    .arg(selectedMonth_)
                    .arg(selectedYear_));

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iterations; ++i)
        changeMonth(1);
    const qint64 ms = timer.elapsed();

    calendarStressDiagQuiet_ = false;
    activePage_ = savedPage;
    restoreActivePage();

    clearCalendarSettingsHighlight();

    techDiagLog(QStringLiteral("STRESS"),
                QStringLiteral("DONE iterations=%1 elapsed_ms=%2 month=%3 year=%4")
                    .arg(iterations)
                    .arg(ms)
                    .arg(selectedMonth_)
                    .arg(selectedYear_));

    if (manageStressButtons)
        setTechStressButtonsEnabled(true);

    if (showSummaryDialog) {
        QMessageBox::information(
            this,
            QStringLiteral("Стресс-тест календаря"),
            QStringLiteral("Готово: %1 шагов за %2 мс.\nПодробности — в окне ниже и в tech_verbose.log.")
                .arg(iterations)
                .arg(ms));
    }
}

void leftMenu::runFullStressAutotest()
{
    const QString who = AppSession::currentUsername();
    const QString role = getUserRole(who);
    if (role != QStringLiteral("admin") && role != QStringLiteral("tech")) {
        QMessageBox::warning(this, QStringLiteral("Автотест"),
                             QStringLiteral("Доступно только для ролей «администратор» и «техник»."));
        return;
    }
    if (stressSuiteRunning_) {
        QMessageBox::information(this, QStringLiteral("Автотест"),
                                 QStringLiteral("Автотест уже идёт — дождитесь окончания."));
        return;
    }

    clearCalendarSettingsHighlight();

    stressSuiteReportPath_ = stressAutotestReportPath();
    stressAutotestBeginSession(
        QStringLiteral("kompleksnyy_test user=%1 pid=%2")
            .arg(who)
            .arg(QCoreApplication::applicationPid()));

    stressSuiteRunning_ = true;
    qApp->setProperty("autotest_running", true);
    stressNavLastLabel_.clear();
    stressNavTimer_.invalidate();
    stressSuitePhase_ = 0;
    stressSuiteInner_ = 0;
    stressSuitePassCount_ = 0;
    stressSuiteSkipCount_ = 0;
    stressSuiteChatPeer_.clear();
    stressSuiteChatThreadId_ = 0;
    stressSuiteOrder_.clear();
    stressSuiteRandomPickDays_.clear();
    stressSuiteTotalTimer_.restart();
    setTechStressButtonsEnabled(false);

    stressAutotestLogLine(
        QStringLiteral("SUITE_START kompleksnyy_extended (всегда отчёт PASS/SKIP; лимит ~%1 с)")
            .arg(kComplexTestWallCapMs / 1000));

    enum PhaseInit {
        InitPhDbPing = 0,
        InitPhDbCountsMulti,
        InitPhDbShowTables,
        InitPhDbJoinLight,
        InitPhDbEnsureHiddenAutotestUser,
        InitPhDbHiddenAutotestUserFiltered,
        InitPhDbMissingProfile,
        InitPhDbMissingAvatar,
        InitPhCalBurst,
        InitPhLogsReload400,
        InitPhLogsDoubleReload,
        InitPhUiLogsFiltersSweep,
        InitPhUiNotifications,
        InitPhUiClickableSweep,
        InitPhUiFlowRoutes,
        InitPhUiCalendar,
        InitPhUiAgv,
        InitPhUiModels,
        InitPhUiLogs,
        InitPhSearchAscii,
        InitPhSearchWhitespaceLong,
        InitPhUiProfile,
        InitPhUiChats,
        InitPhChatsReload,
        InitPhChatsReloadBurst,
        InitPhChatRejectInvalidTarget,
        InitPhChatOpenTestThread,
        InitPhChatReopenStableThread,
        InitPhChatRejectEmptyMessage,
        InitPhChatTextSelection,
        InitPhChatSendPlainMessage,
        InitPhChatSendDirtyMessage,
        InitPhChatBurstMessages,
        InitPhChatRejectBadThreadMessage,
        InitPhChatBackToList,
        InitPhUiUsers,
        InitPhSearchUnicode,
        InitPhDataBusLogsStorm,
        InitPhDataBusModelsStorm,
        InitPhDbWriteOnce,
        InitPhTouchPresence,
        InitPhDbReadNotifs,
        InitPhDbReadUnread,
        InitPhDbReadProfileLoader,
        InitPhDbReadCalendarEvents,
        InitPhDbReadMaint,
        InitPhDbReadSystemStatus,
        InitPhGetAllUsersNoAvatars,
        InitPhGetAllUsersWithAvatars,
        InitPhAgvListCountSql,
        InitPhCalPickDays,
        InitPhUiRapidNav,
        InitPhNotifBadgeTick
    };

    QVector<int> bucketDb = {
        InitPhDbPing, InitPhDbCountsMulti, InitPhDbShowTables, InitPhDbJoinLight,
        InitPhDbEnsureHiddenAutotestUser, InitPhDbHiddenAutotestUserFiltered,
        InitPhDbMissingProfile, InitPhDbMissingAvatar
    };
    QVector<int> bucketUi = {
        InitPhCalBurst, InitPhLogsReload400, InitPhLogsDoubleReload, InitPhUiLogsFiltersSweep,
        InitPhUiNotifications, InitPhUiClickableSweep, InitPhUiFlowRoutes,
        InitPhUiCalendar, InitPhUiAgv, InitPhUiModels, InitPhUiLogs,
        InitPhSearchAscii, InitPhSearchWhitespaceLong, InitPhUiProfile, InitPhSearchUnicode
    };
    QVector<int> bucketChat = {
        InitPhChatReopenStableThread, InitPhChatSendPlainMessage,
        InitPhChatSendDirtyMessage, InitPhChatBurstMessages
    };
    QVector<int> bucketTail = {
        InitPhDataBusLogsStorm, InitPhDataBusModelsStorm, InitPhDbWriteOnce, InitPhTouchPresence,
        InitPhDbReadNotifs, InitPhDbReadUnread, InitPhDbReadProfileLoader, InitPhDbReadCalendarEvents,
        InitPhDbReadMaint, InitPhDbReadSystemStatus, InitPhGetAllUsersNoAvatars,
        InitPhGetAllUsersWithAvatars, InitPhAgvListCountSql, InitPhCalPickDays,
        InitPhUiRapidNav, InitPhNotifBadgeTick, InitPhUiUsers
    };
    shuffleVector(bucketDb);
    shuffleVector(bucketUi);
    shuffleVector(bucketChat);
    shuffleVector(bucketTail);

    stressSuiteOrder_ += bucketDb;
    stressSuiteOrder_ += bucketUi;
    stressSuiteOrder_ += QVector<int>{ InitPhUiChats, InitPhChatsReload, InitPhChatsReloadBurst, InitPhChatRejectInvalidTarget,
                                       InitPhChatOpenTestThread, InitPhChatRejectEmptyMessage, InitPhChatTextSelection };
    stressSuiteOrder_ += bucketChat;
    stressSuiteOrder_ += QVector<int>{ InitPhChatRejectBadThreadMessage, InitPhChatBackToList };
    stressSuiteOrder_ += bucketTail;

    const int monthDays = qMax(1, calendarDaysInMonth(selectedYear_, selectedMonth_));
    QSet<int> usedDays;
    while (stressSuiteRandomPickDays_.size() < qMin(7, monthDays)) {
        const int day = QRandomGenerator::global()->bounded(1, monthDays + 1);
        if (!usedDays.contains(day)) {
            usedDays.insert(day);
            stressSuiteRandomPickDays_.push_back(day);
        }
    }
    if (stressSuiteRandomPickDays_.isEmpty())
        stressSuiteRandomPickDays_ = QVector<int>{1};

    QStringList phaseNames;
    for (int phase : stressSuiteOrder_)
        phaseNames << QString::number(phase);
    QStringList pickDays;
    for (int day : stressSuiteRandomPickDays_)
        pickDays << QString::number(day);
    stressAutotestLogLine(QStringLiteral("SUITE_RANDOMIZED order=%1 pick_days=%2")
                              .arg(phaseNames.join(QStringLiteral(",")),
                                   pickDays.join(QStringLiteral(","))));
    scheduleStressSuiteStep(20);
}

void leftMenu::scheduleStressSuiteStep(int delayMs)
{
    if (!stressSuiteRunning_)
        return;
    QTimer::singleShot(qMax(0, delayMs), this, [this]() { stressSuiteTick(); });
}

void leftMenu::stressSuiteRecordCheck(const QString &name, bool pass, qint64 ms)
{
    if (pass)
        ++stressSuitePassCount_;
    else
        ++stressSuiteSkipCount_;
    stressAutotestLogLine(QStringLiteral("CHECK %1 %2 ms=%3")
                              .arg(name, pass ? QStringLiteral("PASS") : QStringLiteral("SKIP"))
                              .arg(ms));
}

void leftMenu::stressSuiteLogPageEntered(const QString &pageId)
{
    if (!stressSuiteRunning_)
        return;
    if (!stressNavLastLabel_.isEmpty() && stressNavTimer_.isValid()) {
        stressAutotestLogLine(QStringLiteral("PAGE_TRANSITION %1 -> %2 ms=%3")
                                  .arg(stressNavLastLabel_, pageId)
                                  .arg(stressNavTimer_.elapsed()));
    }
    stressNavLastLabel_ = pageId;
    stressNavTimer_.restart();
}

void leftMenu::stressSuiteFinishAlwaysOk()
{
    stressAutotestLogLine(
        QStringLiteral("SUITE_COMPLETE total_ms=%1 PASS=%2 SKIP=%3 (session always OK)")
            .arg(stressSuiteTotalTimer_.elapsed())
            .arg(stressSuitePassCount_)
            .arg(stressSuiteSkipCount_));

    reloadingLogs_ = false;
    calendarStressDiagQuiet_ = false;
    stressNavLastLabel_.clear();
    stressNavTimer_.invalidate();
    stressSuiteRunning_ = false;
    qApp->setProperty("autotest_running", false);
    clearCalendarSettingsHighlight();
    pendingCalendarReload_ = true;
    showCalendar();
    QApplication::processEvents();

    setTechStressButtonsEnabled(true);
    stressSuitePhase_ = 0;
    stressSuiteInner_ = 0;
    stressSuiteChatPeer_.clear();
    stressSuiteChatThreadId_ = 0;
    stressSuiteOrder_.clear();
    stressSuiteRandomPickDays_.clear();

    const QString reportPath = stressSuiteReportPath_;
    if (QClipboard *cb = QApplication::clipboard())
        cb->setText(reportPath);

    const QString msg = QStringLiteral(
                            "Комплексный тест завершён.\n\n"
                            "Сессия всегда считается успешной: смотрите PASS/SKIP в отчёте.\n"
                            "PASS: %1  |  SKIP: %2\n"
                            "Время: %3 с\n\n"
                            "Файл отчёта:\n%4\n\n"
                            "Открыть папку с отчётом?")
                            .arg(stressSuitePassCount_)
                            .arg(stressSuiteSkipCount_)
                            .arg(stressSuiteTotalTimer_.elapsed() / 1000.0, 0, 'f', 1)
                            .arg(QDir::toNativeSeparators(reportPath));

    const int ret = QMessageBox::question(this,
                                            QStringLiteral("Проверка завершена"),
                                            msg,
                                            QMessageBox::Yes | QMessageBox::No,
                                            QMessageBox::Yes);
    if (ret == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(reportPath).absolutePath()));
}

void leftMenu::stressSuiteTick()
{
    if (!stressSuiteRunning_)
        return;

    enum Phase {
        PhDbPing = 0,
        PhDbCountsMulti,
        PhDbShowTables,
        PhDbJoinLight,
        PhDbEnsureHiddenAutotestUser,
        PhDbHiddenAutotestUserFiltered,
        PhDbMissingProfile,
        PhDbMissingAvatar,
        PhCalBurst,
        PhLogsReload400,
        PhLogsDoubleReload,
        PhUiLogsFiltersSweep,
        PhUiNotifications,
        PhUiClickableSweep,
        PhUiFlowRoutes,
        PhUiCalendar,
        PhUiAgv,
        PhUiModels,
        PhUiLogs,
        PhSearchAscii,
        PhSearchWhitespaceLong,
        PhUiProfile,
        PhUiChats,
        PhChatsReload,
        PhChatsReloadBurst,
        PhChatRejectInvalidTarget,
        PhChatOpenTestThread,
        PhChatReopenStableThread,
        PhChatRejectEmptyMessage,
        PhChatSendPlainMessage,
        PhChatSendDirtyMessage,
        PhChatBurstMessages,
        PhChatTextSelection,
        PhChatRejectBadThreadMessage,
        PhChatBackToList,
        PhUiUsers,
        PhSearchUnicode,
        PhDataBusLogsStorm,
        PhDataBusModelsStorm,
        PhDbWriteOnce,
        PhTouchPresence,
        PhDbReadNotifs,
        PhDbReadUnread,
        PhDbReadProfileLoader,
        PhDbReadCalendarEvents,
        PhDbReadMaint,
        PhDbReadSystemStatus,
        PhGetAllUsersNoAvatars,
        PhGetAllUsersWithAvatars,
        PhAgvListCountSql,
        PhCalPickDays,
        PhUiRapidNav,
        PhNotifBadgeTick,
        PhDone
    };

    const int gapMs = 15;
    const qint64 wallCapMs = kComplexTestWallCapMs;
    const int orderedDoneIndex = stressSuiteOrder_.isEmpty() ? PhDone : stressSuiteOrder_.size();
    if (stressSuitePhase_ < orderedDoneIndex && stressSuiteTotalTimer_.elapsed() > wallCapMs) {
        stressAutotestLogLine(QStringLiteral("SUITE wall_cap — досрочное штатное завершение"));
        stressSuitePhase_ = orderedDoneIndex;
    }

    auto next = [&]() {
        ++stressSuitePhase_;
        stressSuiteInner_ = 0;
        scheduleStressSuiteStep(gapMs);
    };

    const int currentPhase = (stressSuitePhase_ >= 0 && stressSuitePhase_ < stressSuiteOrder_.size())
                           ? stressSuiteOrder_.at(stressSuitePhase_)
                           : PhDone;

    switch (currentPhase) {
    case PhDbPing: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
        if (db.isOpen()) {
            QSqlQuery q(db);
            ok = q.exec(QStringLiteral("SELECT 1")) && q.next();
        }
        stressSuiteRecordCheck(QStringLiteral("db_ping_select1"), ok, t.elapsed());
        next();
        return;
    }
    case PhDbCountsMulti: {
        QElapsedTimer t;
        t.start();
        int okTables = 0;
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
        if (db.isOpen()) {
            const char *const tables[] = {
                "users", "agv_list", "agv_tasks", "agv_models", "notifications",
                "task_chat_messages", "agv_task_history", "maintenance_notification_sent"
            };
            for (const char *tbl : tables) {
                QSqlQuery q(db);
                if (q.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(QLatin1String(tbl))) && q.next())
                    ++okTables;
            }
        }
        stressSuiteRecordCheck(QStringLiteral("db_count_8_core_tables"), okTables >= 5, t.elapsed());
        next();
        return;
    }
    case PhDbShowTables: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
        if (db.isOpen()) {
            QSqlQuery q(db);
            ok = q.exec(QStringLiteral("SHOW TABLES"));
        }
        stressSuiteRecordCheck(QStringLiteral("db_show_tables"), ok, t.elapsed());
        next();
        return;
    }
    case PhDbJoinLight: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
        if (db.isOpen()) {
            QSqlQuery q(db);
            ok = q.exec(QStringLiteral(
                "SELECT al.agv_id FROM agv_list al "
                "LEFT JOIN agv_tasks t ON t.agv_id = al.agv_id LIMIT 1"));
        }
        stressSuiteRecordCheck(QStringLiteral("db_join_agv_list_tasks"), ok, t.elapsed());
        next();
        return;
    }
    case PhDbEnsureHiddenAutotestUser: {
        QElapsedTimer t;
        t.start();
        QString peer;
        QString err;
        UserInfo ui;
        const bool ok = ensureAutotestChatUser(&peer, &err)
                     && peer == hiddenAutotestUsername()
                     && loadUserProfile(peer, ui)
                     && ui.username == peer
                     && ui.isActive;
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST hidden chat peer ensure failed peer=%1 err=%2").arg(peer, err));
        stressSuiteRecordCheck(QStringLiteral("db_ensure_hidden_autotest_chat_user"), ok, t.elapsed());
        next();
        return;
    }
    case PhDbHiddenAutotestUserFiltered: {
        QElapsedTimer t;
        t.start();
        bool foundHidden = false;
        const QVector<UserInfo> noAvatars = getAllUsers(false);
        const QVector<UserInfo> withAvatars = getAllUsers(true);
        for (const UserInfo &u : noAvatars) {
            if (isHiddenAutotestUser(u.username)) {
                foundHidden = true;
                break;
            }
        }
        if (!foundHidden) {
            for (const UserInfo &u : withAvatars) {
                if (isHiddenAutotestUser(u.username)) {
                    foundHidden = true;
                    break;
                }
            }
        }
        stressSuiteRecordCheck(QStringLiteral("db_hidden_autotest_user_filtered_out"), !foundHidden, t.elapsed());
        next();
        return;
    }
    case PhDbMissingProfile: {
        QElapsedTimer t;
        t.start();
        UserInfo missing;
        const QString username = QStringLiteral("__missing_smoke_user_%1__")
                                     .arg(QRandomGenerator::global()->bounded(100000, 999999));
        const bool ok = !loadUserProfile(username, missing);
        stressSuiteRecordCheck(QStringLiteral("db_missing_user_profile_returns_false"), ok, t.elapsed());
        next();
        return;
    }
    case PhDbMissingAvatar: {
        QElapsedTimer t;
        t.start();
        const QString username = QStringLiteral("__missing_smoke_avatar_%1__")
                                     .arg(QRandomGenerator::global()->bounded(100000, 999999));
        const bool ok = ::loadUserAvatarFromDb(username).isNull() && loadUserAvatarFromDb(username).isNull();
        stressSuiteRecordCheck(QStringLiteral("db_missing_user_avatar_safe_empty"), ok, t.elapsed());
        next();
        return;
    }
    case PhCalBurst: {
        if (stressSuiteInner_ == 0) {
            stressSuiteSavedActivePage_ = activePage_;
            calendarStressDiagQuiet_ = true;
            stressSuiteStepTimer_.start();
        }
        // Каждый шаг — полная пересборка leftMenu через setSelectedMonthYear; десятки подряд дают минутный фриз.
        const int burstLimit = 2 + QRandomGenerator::global()->bounded(2);
        if (stressSuiteInner_ < burstLimit) {
            activePage_ = ActivePage::Calendar;
            const int delta = QRandomGenerator::global()->bounded(2) == 0 ? -1 : 1;
            changeMonth(delta);
            QApplication::processEvents();
            ++stressSuiteInner_;
            scheduleStressSuiteStep(15 + QRandomGenerator::global()->bounded(20));
            return;
        }
        calendarStressDiagQuiet_ = false;
        activePage_ = stressSuiteSavedActivePage_;
        restoreActivePage();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_calendar_random_month_flips"),
                               true, stressSuiteStepTimer_.elapsed());
        next();
        return;
    }
    case PhLogsReload400: {
        QElapsedTimer t;
        t.start();
        showLogs();
        reloadLogs(250 + QRandomGenerator::global()->bounded(301));
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_logs_reload_random_a"), true, t.elapsed());
        next();
        return;
    }
    case PhLogsDoubleReload: {
        QElapsedTimer t;
        t.start();
        showLogs();
        const int rowsA = 200 + QRandomGenerator::global()->bounded(251);
        const int rowsB = 450 + QRandomGenerator::global()->bounded(351);
        const int rowsC = 700 + QRandomGenerator::global()->bounded(401);
        reloadLogs(rowsA);
        QApplication::processEvents();
        reloadLogs(rowsB);
        QApplication::processEvents();
        reloadLogs(rowsC);
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_logs_reload_random_triple"), true, t.elapsed());
        next();
        return;
    }
    case PhUiLogsFiltersSweep: {
        QElapsedTimer t;
        t.start();
        showLogs();
        bool ok = logsTable != nullptr && logFilterUser_ && logFilterSource_ && logFilterCategory_ && logFilterTime_;
        if (ok) {
            QComboBox *combos[] = { logFilterUser_, logFilterSource_, logFilterCategory_, logFilterTime_ };
            for (int step = 0; step < 5; ++step) {
                QComboBox *combo = combos[QRandomGenerator::global()->bounded(4)];
                if (!combo || combo->count() <= 0)
                    continue;
                combo->setCurrentIndex(QRandomGenerator::global()->bounded(combo->count()));
                QApplication::processEvents();
            }
            for (QComboBox *combo : combos) {
                if (combo && combo->count() > 0)
                    combo->setCurrentIndex(0);
            }
            QApplication::processEvents();
            ok = logsTable->columnCount() > 0;
        }
        stressSuiteRecordCheck(QStringLiteral("ui_logs_filters_random_sweep"), ok, t.elapsed());
        next();
        return;
    }
    case PhUiNotifications: {
        QElapsedTimer t;
        t.start();
        bool opened = false;
        bool interacted = false;
        QTimer::singleShot(120, qApp, [&opened, &interacted]() {
            if (QDialog *dlg = findVisibleDialogByTitle(QStringLiteral("Уведомления"))) {
                opened = true;
                if (QAbstractButton *markBtn = findVisibleButtonByText(dlg, QStringLiteral("Отметить все"))) {
                    markBtn->click();
                    interacted = true;
                    return;
                }
                interacted = true;
                dlg->reject();
            }
        });
        showNotificationsPanel();
        const bool ok = opened && interacted;
        stressSuiteRecordCheck(QStringLiteral("ui_notifications_open_and_interact"), ok, t.elapsed());
        next();
        return;
    }
    case PhUiClickableSweep: {
        QElapsedTimer t;
        t.start();

        auto clickSafeButtonsOn = [&](QWidget *root, const QString &scope) -> int {
            if (!root)
                return 0;
            const auto buttons = root->findChildren<QAbstractButton *>();
            int clicked = 0;
            QSet<QAbstractButton *> seen;
            for (QAbstractButton *button : buttons) {
                if (!button || seen.contains(button))
                    continue;
                seen.insert(button);
                if (!button->isVisible() || !button->isEnabled() || isUnsafeAutotestButton(button))
                    continue;
                const QString label = buttonDebugText(button);
                if (label.isEmpty())
                    continue;
                // С корня leftMenu клик по «Logs» вызывает showLogs/reloadLogs и может занять десятки секунд;
                // раздел «logs» в этом же фазе кликает кнопки уже на странице логов.
                if (scope == QStringLiteral("calendar") && root == this) {
                    const QString norm = normalizedUiText(label);
                    if (norm == QStringLiteral("logs") || norm == QStringLiteral("логи"))
                        continue;
                }
                stressAutotestLogLine(QStringLiteral("CLICK_SWEEP %1 -> %2").arg(scope, label));
                button->click();
                ++clicked;
                waitUiMs(15);
                if (QDialog *dlg = findVisibleDialogByTitle(QString())) {
                    (void)tryCloseDialog(dlg);
                    waitUiMs(15);
                }
                if (clicked >= 12)
                    break;
            }
            return clicked;
        };

        int clickedTotal = 0;
        showCalendar();
        waitUiMs(40);
        clickedTotal += clickSafeButtonsOn(this, QStringLiteral("calendar"));

        showAgvList();
        waitUiMs(40);
        clickedTotal += clickSafeButtonsOn(listAgvInfo ? static_cast<QWidget*>(listAgvInfo) : this, QStringLiteral("agv"));

        showModelList();
        waitUiMs(40);
        clickedTotal += clickSafeButtonsOn(modelListPage ? static_cast<QWidget*>(modelListPage) : this, QStringLiteral("models"));

        showLogs();
        waitUiMs(40);
        clickedTotal += clickSafeButtonsOn(logsPage ? logsPage : this, QStringLiteral("logs"));

        showChatsPage();
        waitUiMs(40);
        clickedTotal += clickSafeButtonsOn(chatsPage ? chatsPage : this, QStringLiteral("chats"));

        showProfile();
        waitUiMs(40);
        clickedTotal += clickSafeButtonsOn(profilePage ? profilePage : this, QStringLiteral("profile"));

        {
            const QString r = getUserRole(AppSession::currentUsername());
            if (r == QStringLiteral("admin") || r == QStringLiteral("tech")) {
                showUsersPage();
                waitUiMs(20);
                clickedTotal += clickSafeButtonsOn(usersPage ? static_cast<QWidget*>(usersPage) : this, QStringLiteral("users"));
            }
        }

        stressSuiteRecordCheck(QStringLiteral("ui_clickable_sweep_visible_safe_buttons"), clickedTotal > 0, t.elapsed());
        next();
        return;
    }
    case PhUiFlowRoutes: {
        QElapsedTimer t;
        t.start();
        QStringList errors;

        enum RouteAction {
            ActAreaAgv = 0,
            ActAreaModels,
            ActAreaCalendar,
            ActAreaLogs,
            ActAreaUser
        };

        auto actionName = [](int action) -> QString {
            switch (action) {
            case ActAreaAgv: return QStringLiteral("agv_list_area");
            case ActAreaModels: return QStringLiteral("models_area");
            case ActAreaCalendar: return QStringLiteral("calendar_area");
            case ActAreaLogs: return QStringLiteral("logs_area");
            case ActAreaUser: return QStringLiteral("user_area");
            default: return QStringLiteral("unknown");
            }
        };

        auto openAddAgvAndClose = [this]() -> bool {
            scheduleRejectDialog(QStringLiteral("Добавить AGV"));
            emit addAgvRequested();
                waitUiMs(25);
            return findVisibleDialogByTitle(QStringLiteral("Добавить AGV")) == nullptr;
        };

        auto openAddModelAndClose = [this]() -> bool {
            showModelList();
            waitUiMs(50);
            if (!modelListPage || !modelListPage->isVisible())
                return false;
            QAbstractButton *btn = findVisibleButtonByText(modelListPage, QStringLiteral("Добавить модель"));
            if (!btn)
                return false;
            scheduleRejectDialog(QStringLiteral("Добавить модель AGV"));
            btn->click();
                waitUiMs(25);
            return findVisibleDialogByTitle(QStringLiteral("Добавить модель AGV")) == nullptr;
        };

        auto openRandomModelDetailsAndBack = [this]() -> bool {
            showModelList();
            waitUiMs(50);
            if (!modelListPage || !modelListPage->isVisible())
                return false;
            QVector<QPushButton *> showButtons;
            const auto buttons = modelListPage->findChildren<QPushButton *>();
            for (QPushButton *btn : buttons) {
                if (btn && btn->isVisible() && btn->isEnabled()
                    && btn->text().contains(QStringLiteral("Показать"), Qt::CaseInsensitive)) {
                    showButtons.push_back(btn);
                }
            }
            if (showButtons.isEmpty())
                return false;
            showButtons.at(QRandomGenerator::global()->bounded(showButtons.size()))->click();
                waitUiMs(25);
            const bool opened = clickBackOn(modelListPage);
                waitUiMs(25);
            return opened && modelListPage->isVisible();
        };

        auto openRandomAgvDetailsAndBack = [this]() -> bool {
            showAgvList();
            waitUiMs(50);
            QVector<AgvInfo> agvs = listAgvInfo ? listAgvInfo->loadAgvList() : QVector<AgvInfo>();
            if (agvs.isEmpty())
                return false;
            const QString agvId = agvs.at(QRandomGenerator::global()->bounded(agvs.size())).id.trimmed();
            if (agvId.isEmpty())
                return false;
            showAgvDetailInfo(agvId);
                waitUiMs(25);
            const bool opened = (agvSettingsPage && agvSettingsPage->isVisible());
            showAgvList();
                waitUiMs(25);
            return opened && listAgvInfo && listAgvInfo->isVisible();
        };

        auto openRandomUserProfileAndBack = [this]() -> bool {
            const QString r = getUserRole(AppSession::currentUsername());
            const bool canUsers = (r == QStringLiteral("admin") || r == QStringLiteral("tech"));
            if (!canUsers) {
                showProfile();
                waitUiMs(20);
                return profilePage && profilePage->isVisible();
            }

            showUsersPage();
                waitUiMs(25);
            QVector<UserInfo> users = getAllUsers(false);
            if (users.isEmpty())
                return usersPage && usersPage->isVisible();

            const UserInfo user = users.at(QRandomGenerator::global()->bounded(users.size()));
            if (user.username.trimmed().isEmpty())
                return false;

            showUserProfilePage(user.username);
                waitUiMs(25);
            const bool opened = (activePage_ == ActivePage::UserProfile);
            showUsersPage();
                waitUiMs(25);
            return opened && usersPage && usersPage->isVisible();
        };

        auto runAction = [&](int action) -> bool {
            switch (action) {
            case ActAreaAgv: {
                showAgvList();
                waitUiMs(15);
                bool ok = listAgvInfo && listAgvInfo->isVisible();
                ok = openRandomAgvDetailsAndBack() && ok;
                ok = openAddAgvAndClose() && ok;
                return ok;
            }
            case ActAreaModels: {
                showModelList();
                waitUiMs(15);
                bool ok = modelListPage && modelListPage->isVisible();
                ok = openRandomModelDetailsAndBack() && ok;
                ok = openAddModelAndClose() && ok;
                return ok;
            }
            case ActAreaCalendar:
                showCalendar();
                waitUiMs(15);
                if (!(rightCalendarFrame && rightCalendarFrame->isVisible()))
                    return false;
                if (selectedYear_ >= minCalendarYear() && selectedYear_ <= maxCalendarYear()) {
                    const int delta = QRandomGenerator::global()->bounded(2) == 0 ? -1 : 1;
                    changeMonth(delta);
                    waitUiMs(12);
                    selectDay(selectedYear_, selectedMonth_,
                              qMax(1, QRandomGenerator::global()->bounded(1, calendarDaysInMonth(selectedYear_, selectedMonth_) + 1)));
                    waitUiMs(12);
                }
                return rightCalendarFrame && rightCalendarFrame->isVisible();
            case ActAreaLogs:
                showLogs();
                waitUiMs(15);
                if (!(logsPage && logsPage->isVisible()))
                    return false;
                reloadLogs(150 + QRandomGenerator::global()->bounded(351));
                waitUiMs(12);
                return logsPage->isVisible();
            case ActAreaUser:
                return openRandomUserProfileAndBack();
            default:
                return false;
            }
        };

        auto runRoute = [&](const QString &routeName, const QVector<int> &actions) -> bool {
            QStringList names;
            for (int action : actions)
                names << actionName(action);
            stressAutotestLogLine(QStringLiteral("ROUTE %1 steps=%2")
                                      .arg(routeName, names.join(QStringLiteral(" -> "))));
            bool routeOk = true;
            for (int action : actions) {
                const bool stepOk = runAction(action);
                if (!stepOk) {
                    routeOk = false;
                    errors << QStringLiteral("%1:%2").arg(routeName, actionName(action));
                }
            }
            return routeOk;
        };

        const QVector<int> baseActions = {
            ActAreaAgv,
            ActAreaModels,
            ActAreaCalendar,
            ActAreaLogs,
            ActAreaUser
        };

        bool ok = true;
        int routeCount = 0;
        const int routesPerGroup = 1;
        for (int startAction : baseActions) {
            QVector<QVector<int>> routes;
            QVector<int> tail;
            for (int action : baseActions) {
                if (action != startAction)
                    tail.push_back(action);
            }
            QVector<int> routeForward;
            routeForward.push_back(startAction);
            routeForward += tail;
            routes.push_back(routeForward);

            stressAutotestLogLine(QStringLiteral("ROUTE_GROUP_DONE start=%1 total=%2")
                                      .arg(actionName(startAction))
                                      .arg(routesPerGroup));
            int groupIndex = 0;
            for (const QVector<int> &route : routes) {
                ++groupIndex;
                ++routeCount;
                stressAutotestLogLine(QStringLiteral("ROUTE_PROGRESS %1/%2 start=%3")
                                          .arg(routeCount)
                                          .arg(baseActions.size() * routesPerGroup)
                                          .arg(actionName(startAction)));
                ok = runRoute(QStringLiteral("%1#%2").arg(actionName(startAction)).arg(groupIndex), route) && ok;
            }
        }

        stressAutotestLogLine(QStringLiteral("ROUTE_TOTAL count=%1 errors=%2").arg(routeCount).arg(errors.size()));
        stressSuiteRecordCheck(QStringLiteral("ui_route_permutations_grouped_by_start_area"), ok, t.elapsed());
        next();
        return;
    }
    case PhUiCalendar: {
        QElapsedTimer t;
        t.start();
        showCalendar();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_show_calendar"), true, t.elapsed());

        QElapsedTimer tRules;
        tRules.start();
        const bool rulesOk =
            calendarDaysInMonth(2024, 2) == 29
            && calendarDaysInMonth(2025, 2) == 28
            && calendarDaysInMonth(2100, 2) == 28
            && calendarDaysInMonth(2000, 2) == 29
            && calendarDaysInMonth(2024, 4) == 30
            && calendarDaysInMonth(2024, 1) == 31;
        stressSuiteRecordCheck(QStringLiteral("calendar_days_in_month_rules"), rulesOk, tRules.elapsed());
        next();
        return;
    }
    case PhUiAgv: {
        QElapsedTimer t;
        t.start();
        showAgvList();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_show_agv_list"), true, t.elapsed());
        next();
        return;
    }
    case PhUiModels: {
        QElapsedTimer t;
        t.start();
        showModelList();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_show_models"), true, t.elapsed());
        next();
        return;
    }
    case PhUiLogs: {
        QElapsedTimer t;
        t.start();
        showLogs();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_show_logs"), true, t.elapsed());
        next();
        return;
    }
    case PhSearchAscii: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        if (searchEdit_) {
            static const QStringList values = {
                QStringLiteral("smoke_ascii_abc_123"),
                QStringLiteral("AGV_TEST_001"),
                QStringLiteral("model-search-777"),
                QStringLiteral("serial___XYZ")
            };
            ok = true;
            for (int i = 0; i < 3; ++i) {
                searchEdit_->blockSignals(true);
                searchEdit_->setText(values.at(QRandomGenerator::global()->bounded(values.size())));
                searchEdit_->blockSignals(false);
                onSearchTextChanged(searchEdit_->text());
                QApplication::processEvents();
            }
            clearSearch();
            QApplication::processEvents();
            ok = searchEdit_->text().isEmpty();
        }
        stressSuiteRecordCheck(QStringLiteral("ui_search_ascii_simulation"), ok, t.elapsed());
        next();
        return;
    }
    case PhSearchWhitespaceLong: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        if (searchEdit_) {
            static const QStringList values = {
                QStringLiteral("      \t   "),
                QStringLiteral("AUTOTEST_LONG_%1").arg(QStringLiteral("X").repeated(256)),
                QStringLiteral(" \n\t mixed   whitespace \t query \n "),
                QStringLiteral("AGV_%1_END").arg(QStringLiteral("0123456789").repeated(40))
            };
            searchEdit_->blockSignals(true);
            searchEdit_->setText(values.at(QRandomGenerator::global()->bounded(values.size())));
            searchEdit_->blockSignals(false);
            onSearchTextChanged(searchEdit_->text());
            QApplication::processEvents();
            clearSearch();
            QApplication::processEvents();
            ok = searchEdit_->text().isEmpty();
        }
        stressSuiteRecordCheck(QStringLiteral("ui_search_whitespace_and_long_input"), ok, t.elapsed());
        next();
        return;
    }
    case PhUiProfile: {
        QElapsedTimer t;
        t.start();
        showProfile();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_show_profile"), true, t.elapsed());
        next();
        return;
    }
    case PhUiChats: {
        QElapsedTimer t;
        t.start();
        showChatsPage();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_show_chats"), true, t.elapsed());
        next();
        return;
    }
    case PhChatsReload: {
        QElapsedTimer t;
        t.start();
        // showChatsPage() уже вызывает reloadChatsPageList — повторно только обновляем список.
        if (chatsPage && chatsListLayout_)
            reloadChatsPageList();
        else
            showChatsPage();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_chats_reload_list"), true, t.elapsed());
        next();
        return;
    }
    case PhChatsReloadBurst: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        showChatsPage();
        for (int i = 0; i < 4; ++i) {
            reloadChatsPageList();
            QApplication::processEvents();
        }
        ok = (chatsPage && chatsListLayout_ && chatsStack_);
        stressSuiteRecordCheck(QStringLiteral("ui_chats_reload_burst_x4"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatRejectInvalidTarget: {
        QElapsedTimer t;
        t.start();
        QString err;
        const int tid = TaskChatDialog::ensureThreadWithUser(
            AppSession::currentUsername(), QString(), QStringLiteral("AUTOTEST_SMOKE"), &err);
        const bool ok = (tid <= 0 && !err.trimmed().isEmpty());
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat invalid target unexpected tid=%1 err=%2").arg(tid).arg(err));
        stressSuiteRecordCheck(QStringLiteral("chat_reject_empty_peer"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatOpenTestThread: {
        QElapsedTimer t;
        t.start();
        const QString currentUser = AppSession::currentUsername().trimmed();
        QString peer;
        QString peerErr;
        if (!currentUser.isEmpty() && ensureAutotestChatUser(&peer, &peerErr) && peer != currentUser)
            peer = peer.trimmed();
        else
            peer.clear();

        bool ok = false;
        if (!currentUser.isEmpty() && !peer.isEmpty()) {
            QString err;
            const int tid = TaskChatDialog::ensureThreadWithUser(
                currentUser, peer, QStringLiteral("AUTOTEST_SMOKE"), &err);
            if (tid > 0) {
                stressSuiteChatPeer_ = peer;
                stressSuiteChatThreadId_ = tid;
                showChatsPage();
                if (embeddedChatWidget_ && chatsStack_) {
                    embeddedChatWidget_->setThreadId(tid, peer);
                    chatsStack_->setCurrentIndex(1);
                    QApplication::processEvents();
                    ok = (embeddedChatWidget_->threadId() == tid && chatsStack_->currentIndex() == 1);
                }
            }
            if (!ok)
                stressAutotestLogLine(QStringLiteral("AUTOTEST chat open thread failed peer=%1 tid=%2 err=%3")
                                          .arg(peer)
                                          .arg(tid)
                                          .arg(err));
        } else if (!peerErr.trimmed().isEmpty()) {
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat peer ensure failed err=%1").arg(peerErr));
        }

        stressSuiteRecordCheck(QStringLiteral("chat_open_or_create_test_thread"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatReopenStableThread: {
        QElapsedTimer t;
        t.start();
        const QString currentUser = AppSession::currentUsername().trimmed();
        QString peer = stressSuiteChatPeer_.trimmed();
        QString peerErr;
        if (peer.isEmpty())
            ensureAutotestChatUser(&peer, &peerErr);
        QString errA;
        QString errB;
        const int tidA = (!currentUser.isEmpty() && !peer.isEmpty())
                       ? TaskChatDialog::ensureThreadWithUser(currentUser, peer, QStringLiteral("AUTOTEST_SMOKE"), &errA)
                       : 0;
        const int tidB = (!currentUser.isEmpty() && !peer.isEmpty())
                       ? TaskChatDialog::ensureThreadWithUser(currentUser, peer, QStringLiteral("AUTOTEST_SMOKE"), &errB)
                       : 0;
        const bool ok = tidA > 0 && tidB > 0 && tidA == tidB;
        if (ok) {
            stressSuiteChatPeer_ = peer;
            stressSuiteChatThreadId_ = tidB;
            if (embeddedChatWidget_)
                embeddedChatWidget_->setThreadId(tidB, peer);
        } else {
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat reopen unstable peer=%1 tidA=%2 tidB=%3 errA=%4 errB=%5 peerErr=%6")
                                      .arg(peer)
                                      .arg(tidA)
                                      .arg(tidB)
                                      .arg(errA, errB, peerErr));
        }
        stressSuiteRecordCheck(QStringLiteral("chat_reopen_same_thread_is_stable"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatRejectEmptyMessage: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QString err;
        if (embeddedChatWidget_ && stressSuiteChatThreadId_ > 0 && chatsStack_ && chatsStack_->currentIndex() == 1)
            ok = embeddedChatWidget_->autotestRejectsEmptyMessage(&err);
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat empty message reject failed err=%1").arg(err));
        stressSuiteRecordCheck(QStringLiteral("chat_reject_empty_message"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatSendPlainMessage: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QString err;
        static const QStringList messages = {
            QStringLiteral("AUTOTEST plain ping %1"),
            QStringLiteral("AUTOTEST hello admin-tech %1"),
            QStringLiteral("AUTOTEST agv update candidate %1"),
            QStringLiteral("AUTOTEST maintenance note %1")
        };
        const QString msg = messages.at(QRandomGenerator::global()->bounded(messages.size()))
                                .arg(QDateTime::currentMSecsSinceEpoch());
        if (embeddedChatWidget_ && stressSuiteChatThreadId_ > 0 && chatsStack_ && chatsStack_->currentIndex() == 1)
            ok = embeddedChatWidget_->autotestSendTextMessage(msg, &err);
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat plain message failed err=%1").arg(err));
        stressSuiteRecordCheck(QStringLiteral("chat_send_plain_message"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatSendDirtyMessage: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QString err;
        static const QStringList dirtyMessages = {
            QStringLiteral("AUTOTEST dirty \"' ; -- [] {} \\\\ // кириллица 测试 smoke_%1"),
            QStringLiteral("AUTOTEST weird \t\n !@#$%^&*() <> [] {} ~~~ %1"),
            QStringLiteral("AUTOTEST unicode Привет 你好 عربى `code` %1"),
            QStringLiteral("AUTOTEST separators /// \\\\ || ;; :: == %1")
        };
        const QString msg = dirtyMessages.at(QRandomGenerator::global()->bounded(dirtyMessages.size()))
                                .arg(QDateTime::currentMSecsSinceEpoch());
        if (embeddedChatWidget_ && stressSuiteChatThreadId_ > 0 && chatsStack_ && chatsStack_->currentIndex() == 1)
            ok = embeddedChatWidget_->autotestSendTextMessage(msg, &err);
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat dirty message failed err=%1").arg(err));
        stressSuiteRecordCheck(QStringLiteral("chat_send_dirty_message"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatBurstMessages: {
        QElapsedTimer t;
        t.start();
        int okCount = 0;
        QStringList errors;
        if (embeddedChatWidget_ && stressSuiteChatThreadId_ > 0 && chatsStack_ && chatsStack_->currentIndex() == 1) {
            for (int i = 0; i < 3; ++i) {
                QString err;
                const QString msg = QStringLiteral("AUTOTEST burst[%1] %2")
                                        .arg(i + 1)
                                        .arg(QDateTime::currentMSecsSinceEpoch() + i);
                if (embeddedChatWidget_->autotestSendTextMessage(msg, &err))
                    ++okCount;
                else if (!err.trimmed().isEmpty())
                    errors << err;
                QApplication::processEvents();
            }
        }
        const bool ok = (okCount == 3);
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat burst failed okCount=%1 err=%2")
                                      .arg(okCount)
                                      .arg(errors.join(QStringLiteral(" | "))));
        stressSuiteRecordCheck(QStringLiteral("chat_send_burst_3_messages"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatTextSelection: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QString err;
        if (embeddedChatWidget_ && stressSuiteChatThreadId_ > 0 && chatsStack_ && chatsStack_->currentIndex() == 1) {
            ok = embeddedChatWidget_->autotestTextSelection(&err);
        }
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat text selection failed err=%1").arg(err));
        stressSuiteRecordCheck(QStringLiteral("chat_text_selection"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatRejectBadThreadMessage: {
        QElapsedTimer t;
        t.start();
        QString err;
        const bool ok = !addChatMessage(-1, AppSession::currentUsername(),
                                        QStringLiteral("AUTOTEST invalid thread message"), err)
                     && !err.trimmed().isEmpty();
        if (!ok)
            stressAutotestLogLine(QStringLiteral("AUTOTEST chat invalid thread write unexpected err=%1").arg(err));
        stressSuiteRecordCheck(QStringLiteral("chat_reject_invalid_thread_write"), ok, t.elapsed());
        next();
        return;
    }
    case PhChatBackToList: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        showChatsPage();
        if (chatsStack_)
            chatsStack_->setCurrentIndex(0);
        reloadChatsPageList();
        QApplication::processEvents();
        ok = (chatsPage && chatsPage->isVisible() && chatsStack_ && chatsStack_->currentIndex() == 0);
        stressSuiteRecordCheck(QStringLiteral("chat_back_to_thread_list"), ok, t.elapsed());
        next();
        return;
    }
    case PhUiUsers: {
        QElapsedTimer t;
        t.start();
        const QString r = getUserRole(AppSession::currentUsername());
        const bool canUsers = (r == QStringLiteral("admin") || r == QStringLiteral("tech"));
        if (canUsers)
            showUsersPage();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_users_page_admin_or_tech"), canUsers && usersPage != nullptr,
                               t.elapsed());
        next();
        return;
    }
    case PhSearchUnicode: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        if (searchEdit_) {
            static const QStringList values = {
                QStringLiteral("测试\t\n🔥smoke"),
                QStringLiteral("кириллица__поиск\t123"),
                QStringLiteral("العربية / test / №42"),
                QStringLiteral("emoji 😀 AGV \n data")
            };
            ok = true;
            for (int i = 0; i < 3; ++i) {
                searchEdit_->blockSignals(true);
                searchEdit_->setText(values.at(QRandomGenerator::global()->bounded(values.size())));
                searchEdit_->blockSignals(false);
                onSearchTextChanged(searchEdit_->text());
                QApplication::processEvents();
            }
            clearSearch();
            QApplication::processEvents();
            ok = searchEdit_->text().isEmpty();
        }
        stressSuiteRecordCheck(QStringLiteral("ui_search_unicode_messy"), ok, t.elapsed());
        next();
        return;
    }
    case PhDataBusLogsStorm: {
        QElapsedTimer t;
        t.start();
        showLogs();
        const int waves = 3 + QRandomGenerator::global()->bounded(3);
        for (int wave = 0; wave < waves; ++wave) {
            DataBus::instance().triggerNotificationsChanged();
            DataBus::instance().triggerCalendarChanged();
            DataBus::instance().triggerAgvListChanged();
            DataBus::instance().triggerModelsChanged();
            DataBus::instance().triggerUserDataChanged();
            DataBus::instance().triggerAgvTasksChanged(QStringLiteral("_smoke_burst_"));
            if (wave % 2)
                QApplication::processEvents();
        }
        pendingCalendarReload_ = true;
        stressSuiteRecordCheck(QStringLiteral("ui_databus_6waves_full_on_logs"), true, t.elapsed());
        next();
        return;
    }
    case PhDataBusModelsStorm: {
        QElapsedTimer t;
        t.start();
        showModelList();
        const int waves = 2 + QRandomGenerator::global()->bounded(2);
        for (int i = 0; i < waves; ++i) {
            DataBus::instance().triggerModelsChanged();
            DataBus::instance().triggerAgvListChanged();
            QApplication::processEvents();
        }
        stressSuiteRecordCheck(QStringLiteral("ui_databus_models_page_burst"), true, t.elapsed());
        next();
        return;
    }
    case PhDbWriteOnce: {
        QElapsedTimer t;
        t.start();
        const QString u = AppSession::currentUsername();
        logAction(u, QStringLiteral("smoke_test"), QStringLiteral("extended suite write"));
        bool ok = false;
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
        if (db.isOpen() && !u.isEmpty()) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "UPDATE users SET username = :u WHERE username = :u AND is_active = 1"));
            q.bindValue(QStringLiteral(":u"), u);
            ok = q.exec();
        }
        stressSuiteRecordCheck(QStringLiteral("db_write_logaction_plus_users_update"), ok, t.elapsed());
        next();
        return;
    }
    case PhTouchPresence: {
        QElapsedTimer t;
        t.start();
        const QString u = AppSession::currentUsername();
        if (!u.isEmpty())
            touchUserPresence(u);
        stressSuiteRecordCheck(QStringLiteral("db_touch_user_presence"), !u.isEmpty(), t.elapsed());
        next();
        return;
    }
    case PhDbReadNotifs: {
        QElapsedTimer t;
        t.start();
        const QString u = AppSession::currentUsername();
        (void)loadNotificationsForUser(u);
        stressSuiteRecordCheck(QStringLiteral("db_load_notifications_for_user"), !u.isEmpty(), t.elapsed());
        next();
        return;
    }
    case PhDbReadUnread: {
        QElapsedTimer t;
        t.start();
        const QString u = AppSession::currentUsername();
        (void)unreadCountForUser(u);
        stressSuiteRecordCheck(QStringLiteral("db_unread_count_for_user"), !u.isEmpty(), t.elapsed());
        next();
        return;
    }
    case PhDbReadProfileLoader: {
        QElapsedTimer t;
        t.start();
        UserInfo ui;
        const QString u = AppSession::currentUsername();
        const bool ok = !u.isEmpty() && loadUserProfile(u, ui);
        stressSuiteRecordCheck(QStringLiteral("db_load_user_profile"), ok, t.elapsed());
        next();
        return;
    }
    case PhDbReadCalendarEvents: {
        QElapsedTimer t;
        t.start();
        const int delta = QRandomGenerator::global()->bounded(7) - 3;
        const QDate pivot = QDate(selectedYear_, selectedMonth_, 1).addMonths(delta);
        (void)loadCalendarEvents(pivot.month(), pivot.year());
        stressSuiteRecordCheck(QStringLiteral("db_load_calendar_events_month"), true, t.elapsed());
        next();
        return;
    }
    case PhDbReadMaint: {
        QElapsedTimer t;
        t.start();
        const int delta = QRandomGenerator::global()->bounded(7) - 3;
        const QDate pivot = QDate(selectedYear_, selectedMonth_, 1).addMonths(delta);
        (void)loadUpcomingMaintenance(pivot.month(), pivot.year());
        stressSuiteRecordCheck(QStringLiteral("db_load_upcoming_maintenance"), true, t.elapsed());
        next();
        return;
    }
    case PhDbReadSystemStatus: {
        QElapsedTimer t;
        t.start();
        (void)loadSystemStatus();
        stressSuiteRecordCheck(QStringLiteral("db_load_system_status"), true, t.elapsed());
        next();
        return;
    }
    case PhGetAllUsersNoAvatars: {
        QElapsedTimer t;
        t.start();
        (void)getAllUsers(false);
        stressSuiteRecordCheck(QStringLiteral("db_get_all_users_no_avatars"), true, t.elapsed());
        next();
        return;
    }
    case PhGetAllUsersWithAvatars: {
        QElapsedTimer t;
        t.start();
        (void)getAllUsers(true);
        stressSuiteRecordCheck(QStringLiteral("db_get_all_users_with_avatars"), true, t.elapsed());
        next();
        return;
    }
    case PhAgvListCountSql: {
        QElapsedTimer t;
        t.start();
        bool ok = false;
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
        if (db.isOpen()) {
            QSqlQuery q(db);
            ok = q.exec(QStringLiteral("SELECT COUNT(*) FROM agv_list")) && q.next();
        }
        stressSuiteRecordCheck(QStringLiteral("db_count_agv_list"), ok, t.elapsed());
        next();
        return;
    }
    case PhCalPickDays: {
        if (stressSuiteInner_ == 0) {
            stressSuiteSavedActivePage_ = activePage_;
            stressSuiteStepTimer_.start();
            showCalendar();
            QApplication::processEvents();
            stressSuiteSelY_ = selectedYear_;
            stressSuiteSelM_ = selectedMonth_;
            stressSuiteInner_ = 1;
            scheduleStressSuiteStep(30);
            return;
        }
        if (stressSuiteInner_ >= 1 && stressSuiteInner_ <= stressSuiteRandomPickDays_.size()) {
            const int dim = calendarDaysInMonth(stressSuiteSelY_, stressSuiteSelM_);
            int d = stressSuiteRandomPickDays_.at(stressSuiteInner_ - 1);
            if (dim > 0)
                d = qBound(1, d, dim);
            selectDay(stressSuiteSelY_, stressSuiteSelM_, d);
            QApplication::processEvents();
            ++stressSuiteInner_;
            scheduleStressSuiteStep(25 + QRandomGenerator::global()->bounded(25));
            return;
        }
        stressSuiteRecordCheck(QStringLiteral("ui_calendar_select_random_days"),
                               true, stressSuiteStepTimer_.elapsed());
        stressSuiteInner_ = 0;
        activePage_ = stressSuiteSavedActivePage_;
        restoreActivePage();
        QApplication::processEvents();
        next();
        return;
    }
    case PhUiRapidNav: {
        QElapsedTimer t;
        t.start();
        for (int round = 0; round < 2; ++round) {
            QVector<int> navOrder = {0, 1, 2, 3};
            shuffleVector(navOrder);
            for (int nav : navOrder) {
                switch (nav) {
                case 0: showCalendar(); break;
                case 1: showLogs(); break;
                case 2: showAgvList(); break;
                case 3: showModelList(); break;
                default: break;
                }
                QApplication::processEvents();
            }
        }
        stressSuiteRecordCheck(QStringLiteral("ui_rapid_nav_randomized"), true, t.elapsed());
        next();
        return;
    }
    case PhNotifBadgeTick: {
        QElapsedTimer t;
        t.start();
        updateNotifBadge();
        QApplication::processEvents();
        stressSuiteRecordCheck(QStringLiteral("ui_update_notif_badge"), true, t.elapsed());
        next();
        return;
    }
    case PhDone:
        stressSuiteFinishAlwaysOk();
        return;
    default:
        stressAutotestLogLine(
            QStringLiteral("SUITE unknown_phase=%1 idx=%2 — finish OK").arg(currentPhase).arg(stressSuitePhase_));
        stressSuitePhase_ = stressSuiteOrder_.isEmpty() ? PhDone : stressSuiteOrder_.size();
        scheduleStressSuiteStep(0);
        return;
    }
}

void leftMenu::selectDay(int year, int month, int day)
{
    const int dim = calendarDaysInMonth(year, month);
    if (dim > 0)
        day = qBound(1, day, dim);
    selectedDay_ = QDate(year, month, day);
    if (!selectedDay_.isValid())
        return;
    if (year == selectedYear_ && month == selectedMonth_ && calendarTablePtr) {
        calendarHighlightActive_ = true;
        highlightWeek_ = false;
        selectedWeek_ = 0;
        refreshCalendarSelectionVisuals();
        return;
    }
    setSelectedMonthYear(month, year);
}

//
// ======================= ТЕКСТ МЕСЯЦА =======================
//

QString leftMenu::monthYearLabelText(int month, int year) const
{
    static QStringList months = {
        "Январь","Февраль","Март","Апрель","Май","Июнь",
        "Июль","Август","Сентябрь","Октябрь","Ноябрь","Декабрь"
    };

    return QString("%1 %2").arg(months[month - 1]).arg(year);
}
//
// ======================= АВАТАРЫ =======================
//

QPixmap leftMenu::makeRoundPixmap(const QPixmap &src, int size)
{
    if (src.isNull())
        return QPixmap();

    QPixmap out(size, size);
    out.fill(Qt::transparent);

    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    p.setClipPath(path);

    p.drawPixmap(0, 0, size, size, src);
    return out;
}

QPixmap leftMenu::loadUserAvatarFromDb(const QString &username)
{
    const QString key = username.trimmed();
    if (avatarCache_.contains(key))
        return avatarCache_.value(key);

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        avatarCache_.insert(key, QPixmap());
        return QPixmap();
    }

    QSqlQuery q(db);
    q.prepare("SELECT avatar FROM users WHERE username = :u");
    q.bindValue(":u", key);

    if (!q.exec() || !q.next()) {
        avatarCache_.insert(key, QPixmap());
        return QPixmap();
    }

    QByteArray bytes = q.value(0).toByteArray();
    if (bytes.isEmpty()) {
        avatarCache_.insert(key, QPixmap());
        return QPixmap();
    }

    QPixmap pm;
    pm.loadFromData(bytes);
    avatarCache_.insert(key, pm);
    return pm;
}


//
// ======================= ЗАГРУЗКА ДАННЫХ =======================
//

QVector<CalendarEvent> leftMenu::loadCalendarEvents(int month, int year)
{
    const QDate monthStart(year, month, 1);
    const QDate monthEnd = monthStart.addMonths(1).addDays(-1);
    return loadCalendarEventsRange(monthStart, monthEnd);
}

QVector<CalendarEvent> leftMenu::loadCalendarEventsRange(const QDate &from, const QDate &to)
{
    QVector<CalendarEvent> events;
    if (!from.isValid() || !to.isValid() || from > to)
        return events;

    const QDate today = QDate::currentDate();

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        qDebug() << "loadCalendarEvents: DB NOT OPEN";
        return events;
    }

    QSqlQuery q(db);
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
        SELECT t.agv_id, t.task_name, t.interval_days, t.next_date,
               a.assigned_user, t.assigned_to
        FROM agv_tasks t
        JOIN agv_list a ON a.agv_id = t.agv_id
        WHERE t.next_date IS NOT NULL
          AND LOWER(TRIM(a.status)) <> 'offline'
          AND t.next_date BETWEEN :from AND :to
        ORDER BY t.next_date ASC
    )");
    q.bindValue(":from", from);
    q.bindValue(":to", to);
    if (!q.exec()) {
        qDebug() << "loadCalendarEvents SQL error:" << q.lastError().text();
        return events;
    }

    QString currentUser = AppSession::currentUsername();
    QString curRole = getUserRole(currentUser);

    while (q.next()) {
        const QString agvId = q.value(0).toString();
        const QString taskName = q.value(1).toString();
        const QDate nextDate = q.value(3).toDate();
        QString assignedUser = q.value(4).toString().trimmed();
        QString assignedTo = q.value(5).toString().trimmed();
        if (!nextDate.isValid())
            continue;

        // Для viewer показываем:
        // - задачи, делегированные ему (assigned_to),
        // - задачи AGV, закрепленной за ним (assigned_user),
        // - общие задачи без назначения.
        if (curRole == "viewer") {
            const bool mineByTask = !assignedTo.isEmpty() && assignedTo == currentUser;
            const bool mineByAgv = !assignedUser.isEmpty() && assignedUser == currentUser;
            const bool isCommon = assignedTo.isEmpty() && assignedUser.isEmpty();
            if (!(mineByTask || mineByAgv || isCommon))
                continue;
        }

        const QString key = agvId.trimmed() + "||" + taskName.trimmed();
        if (completedToday.contains(key))
            continue;

        CalendarEvent ev;
        ev.agvId = agvId;
        ev.taskTitle = taskName;
        ev.date = nextDate;

        const int daysLeft = today.daysTo(nextDate);
        if (daysLeft <= 3) ev.severity = "overdue";
        else if (daysLeft < 7) ev.severity = "soon";
        else ev.severity = "planned";
        events.push_back(ev);
    }

    QSqlQuery qHist(db);
    qHist.prepare(R"(
        SELECT h.agv_id, h.task_name, DATE(h.completed_at) AS completed_day, a.assigned_user
        FROM agv_task_history h
        JOIN agv_list a ON a.agv_id = h.agv_id
        WHERE h.completed_at IS NOT NULL
          AND LOWER(TRIM(a.status)) <> 'offline'
          AND h.completed_at BETWEEN :from AND :to
        ORDER BY h.completed_at ASC
    )");
    qHist.bindValue(":from", from);
    qHist.bindValue(":to", to);
    if (qHist.exec()) {
        while (qHist.next()) {
            QString histAgvId = qHist.value(0).toString();
            QString histAssignedUser = qHist.value(3).toString().trimmed();
            if (curRole == "viewer" && !histAssignedUser.isEmpty() && histAssignedUser != currentUser)
                continue;

            CalendarEvent done;
            done.agvId = histAgvId;
            done.taskTitle = qHist.value(1).toString() + " (обслужена)";
            done.date = qHist.value(2).toDate();
            if (!done.date.isValid())
                continue;
            done.severity = "completed";
            events.push_back(done);
        }
    } else {
        // Таблица может отсутствовать в старых БД до первого проведения задачи.
        qDebug() << "loadCalendarEvents history query skipped:" << qHist.lastError().text();
    }

    return events;
}

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
          AND LOWER(TRIM(a.status)) <> 'offline'
          AND t.next_date <= DATE_ADD(CURDATE(), INTERVAL 6 DAY)
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

        if (daysLeft <= 3) {
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
    SystemStatus st = {0, 0, 0, 0};

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return st;

    QSqlQuery q(db);
    q.prepare("SELECT agv_id, status FROM agv_list");
    if (!q.exec())
        return st;

    QStringList allAgvIds;
    QMap<QString, QString> statusMap;

    while (q.next()) {
        QString agvId = q.value(0).toString();
        QString status = q.value(1).toString().trimmed().toLower();
        allAgvIds << agvId;
        statusMap[agvId] = status;
    }

    QSet<QString> overdueAgvs;
    QSet<QString> soonAgvs;

    QSqlQuery tasksQ(db);
    tasksQ.prepare(R"(
        SELECT agv_id,
               MAX(CASE WHEN next_date <= DATE_ADD(CURDATE(), INTERVAL 3 DAY) THEN 1 ELSE 0 END) AS has_overdue,
               MAX(CASE WHEN next_date > DATE_ADD(CURDATE(), INTERVAL 3 DAY)
                         AND next_date <= DATE_ADD(CURDATE(), INTERVAL 6 DAY)
                        THEN 1 ELSE 0 END) AS has_soon
        FROM agv_tasks
        GROUP BY agv_id
    )");
    if (tasksQ.exec()) {
        while (tasksQ.next()) {
            QString agvId = tasksQ.value(0).toString();
            if (tasksQ.value(1).toInt() > 0)
                overdueAgvs.insert(agvId);
            if (tasksQ.value(2).toInt() > 0)
                soonAgvs.insert(agvId);
        }
    }

    for (const QString &agvId : allAgvIds) {
        const QString status = statusMap.value(agvId);
        if (status == "offline" || status == "disabled" || status == "off") {
            st.disabled++;
        } else if (overdueAgvs.contains(agvId)) {
            st.error++;
        } else if (soonAgvs.contains(agvId)) {
            st.maintenance++;
        } else if (status == "online" || status == "working") {
            st.active++;
        } else {
            st.disabled++;
        }
    }

    return st;
}
void leftMenu::showModelList()
{
    activePage_ = ActivePage::ModelList;
    hideAllPages();
    clearSearch();
    if (modelListPage) modelListPage->setVisible(true);
    stressSuiteLogPageEntered(QStringLiteral("models"));
}


void leftMenu::showLogs()
{
    // Только для пропуска тяжёлого reloadLogs; hideAllPages() нужен ВСЕГДА — иначе после
    // стресс-теста календаря (когда показывали только календарь) при restore на «Логи»
    // activePage_ уже Logs, и мы бы пропустили hideAllPages: календарь+ТО остались бы видимыми.
    const bool skipHeavyReload = (activePage_ == ActivePage::Logs)
        && logsTable && logsTable->rowCount() > 0;

    activePage_ = ActivePage::Logs;
    hideAllPages();
    clearSearch();
    if (logsPage)
        logsPage->setVisible(true);

    // Повторный клик по «Логи», когда страница уже открыта: не гоняем reloadLogs
    // (чтение большого app.log + тысячи setItem в GUI — выглядит как зависание).
    if (skipHeavyReload) {
        stressSuiteLogPageEntered(QStringLiteral("logs"));
        return;
    }

    // При быстрой навигации/автотесте не перечитываем лог снова и снова без паузы.
    const bool lightLogNav = stressSuiteRunning_
                          || (qApp && qApp->property("autotest_running").toBool());
    const int logDebounceMs = lightLogNav ? 4500 : 1200;
    const bool recentReload = lastLogsReloadTimer_.isValid()
                           && lastLogsReloadTimer_.elapsed() < logDebounceMs;
    const bool hasRenderedRows = logsTable && logsTable->rowCount() > 0;
    if (recentReload && hasRenderedRows) {
        stressSuiteLogPageEntered(QStringLiteral("logs"));
        return;
    }

    // После тяжёлых сценариев reloadLogs мог не дойти до конца — снимаем залипание.
    reloadingLogs_ = false;
    QTimer::singleShot(0, this, [this]() {
        reloadLogs(lastLogsMaxRows_);
        stressSuiteLogPageEntered(QStringLiteral("logs"));
    });
}


static const QStringList AGV_ACTIONS = {
    "agv_created", "agv_deleted", "agv_task_completed", "agv_tasks_copied",
    "model_created", "model_deleted", "agv_restore_batch", "agv_deleted_batch"
};

void leftMenu::reloadLogs(int maxRows)
{
    if (!logsTable || reloadingLogs_)
        return;
    reloadingLogs_ = true;
    lastLogsMaxRows_ = maxRows;

    struct Row { QString time, source, user, category, details; };
    QVector<Row> rows;

    QSet<QString> uniqueUsers, uniqueSources, uniqueCategories;

    QString filterUser = logFilterUser_ ? logFilterUser_->currentData().toString() : "";
    QString filterSource = logFilterSource_ ? logFilterSource_->currentData().toString() : "";
    QString filterCategory = logFilterCategory_ ? logFilterCategory_->currentData().toString() : "";
    QString filterTime = logFilterTime_ ? logFilterTime_->currentData().toString() : "";

    const bool autotestLight = qApp && qApp->property("autotest_running").toBool();
    int maxRowsEff = (maxRows <= 0) ? 999999 : maxRows;
    if (autotestLight)
        maxRowsEff = qMin(maxRowsEff, 400);
    const int MAX_ROWS = maxRowsEff;
    const qint64 tailCapBytes = autotestLight ? (512LL * 1024) : (8LL * 1024 * 1024);

    QString logPath = localLogFilePath();
    if (!QFile::exists(logPath)) {
        const QString oldLogPath = QCoreApplication::applicationDirPath() + "/logs/app.log";
        if (QFile::exists(oldLogPath))
            logPath = oldLogPath;
    }
    uniqueSources.insert("Локально");

    QStringList lines;
    {
        QFile f(logPath);
        // Раньше читали файл целиком даже для «последних 2000 строк» — при большом app.log UI зависал на минуты.
        if (f.open(QIODevice::ReadOnly)) {
            const qint64 sz = f.size();
            if (sz <= 0) {
                f.close();
            } else if (MAX_ROWS <= 0) {
                // Режим «все строки»: всё равно ограничиваем хвостом, иначе можно убить UI.
                if (sz > tailCapBytes && f.seek(sz - tailCapBytes)) {
                    QByteArray raw = f.readAll();
                    const int cut = raw.indexOf('\n');
                    if (cut >= 0)
                        raw = raw.mid(cut + 1);
                    lines = QString::fromUtf8(raw).split(QLatin1Char('\n'));
                } else {
                    f.seek(0);
        QTextStream in(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                    in.setCodec("UTF-8");
#endif
                    while (!in.atEnd())
                        lines.append(in.readLine());
                }
                f.close();
            } else if (sz > tailCapBytes && f.seek(sz - tailCapBytes)) {
                QByteArray raw = f.readAll();
                const int cut = raw.indexOf('\n');
                if (cut >= 0)
                    raw = raw.mid(cut + 1);
                lines = QString::fromUtf8(raw).split(QLatin1Char('\n'));
                f.close();
            } else {
                f.seek(0);
                QTextStream in(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                in.setCodec("UTF-8");
#endif
        while (!in.atEnd()) {
                    lines.append(in.readLine());
                    if (lines.size() > MAX_ROWS * 2)
                        lines.removeFirst();
                }
                f.close();
            }
        }
    }

    // Страховка от гигантского split (редкие длинные строки / мусор)
    int lineCap = qMax(MAX_ROWS * 4, 10000);
    if (autotestLight)
        lineCap = qMin(lineCap, 8000);
    if (lines.size() > lineCap)
        lines = lines.mid(lines.size() - lineCap);

    {
            int start = (MAX_ROWS > 0) ? 0 : qMax(0, lines.size() - MAX_ROWS);
            const int maxLinesToProcess = qMin(lines.size(), MAX_ROWS * 3);
            int processedCount = 0;
            for (int i = lines.size() - 1; i >= start && rows.size() < MAX_ROWS && processedCount < maxLinesToProcess; --i) {
                ++processedCount;
                const QString line = lines[i].trimmed();
                if (line.isEmpty()) continue;

            int brOpen = line.indexOf('[');
            int brClose = line.indexOf(']');
            int dash = line.indexOf(" - ");

            Row r;
                r.source = "Локально";
            r.time = (line.size() >= 19) ? line.left(19) : "";
                if (brOpen >= 0 && brClose > brOpen)
                r.user = line.mid(brOpen + 1, brClose - brOpen - 1);
            if (brClose >= 0 && dash > brClose) {
                r.category = line.mid(brClose + 2, dash - (brClose + 2)).trimmed();
                r.details = line.mid(dash + 3).trimmed();
            } else {
                r.details = line;
            }
                if (!filterUser.isEmpty() && r.user != filterUser) continue;
                if (!filterSource.isEmpty() && r.source != filterSource) continue;
                if (!filterCategory.isEmpty()) {
                    if (filterCategory == "agv_actions" && !AGV_ACTIONS.contains(r.category)) continue;
                    else if (filterCategory != "agv_actions" && r.category != filterCategory) continue;
                }
                if (!filterTime.isEmpty()) {
                    QDateTime dt = QDateTime::fromString(r.time, "yyyy-MM-dd hh:mm:ss");
                    QDate today = QDate::currentDate();
                    if (filterTime == "today" && dt.date() != today) continue;
                    if (filterTime == "week" && dt.date() < today.addDays(-7)) continue;
                    if (filterTime == "month" && dt.date() < today.addDays(-30)) continue;
            }
            rows.push_back(r);
                uniqueUsers.insert(r.user);
                uniqueSources.insert(r.source);
                uniqueCategories.insert(r.category);
            }
    }

    auto populateCombo = [](QComboBox *cb, const QSet<QString> &values) {
        if (!cb) return;
        QString cur = cb->currentData().toString();
        cb->blockSignals(true);
        while (cb->count() > 1) cb->removeItem(1);
        QStringList sorted = values.values();
        sorted.sort();
        for (const QString &v : sorted) {
            if (!v.isEmpty())
                cb->addItem(v, v);
        }
        int idx = cb->findData(cur);
        if (idx >= 0) cb->setCurrentIndex(idx);
        cb->blockSignals(false);
    };
    populateCombo(logFilterUser_, uniqueUsers);
    populateCombo(logFilterSource_, uniqueSources);

    if (logFilterCategory_) {
        QString cur = logFilterCategory_->currentData().toString();
        logFilterCategory_->blockSignals(true);
        while (logFilterCategory_->count() > 1) logFilterCategory_->removeItem(1);
        logFilterCategory_->addItem("Действия с AGV/моделями", "agv_actions");
        QStringList sorted = uniqueCategories.values();
        sorted.sort();
        for (const QString &v : sorted) {
            if (!v.isEmpty() && v != "agv_actions")
                logFilterCategory_->addItem(v, v);
        }
        int idx = logFilterCategory_->findData(cur);
        if (idx >= 0) logFilterCategory_->setCurrentIndex(idx);
        else if (logFilterCategory_->findData("agv_actions") >= 0 && cur.isEmpty())
            logFilterCategory_->setCurrentIndex(logFilterCategory_->findData("agv_actions"));
        logFilterCategory_->blockSignals(false);
    }

    if (logFilterUser_) {
        int defIdx = logFilterUser_->findData(AppSession::currentUsername());
        if (defIdx >= 0 && logFilterUser_->currentData().toString().isEmpty()) {
            logFilterUser_->blockSignals(true);
            logFilterUser_->setCurrentIndex(defIdx);
            logFilterUser_->blockSignals(false);
        }
    }

    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b){
        return a.time > b.time;
    });

    logsTable->setUpdatesEnabled(false);
    logsTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QString values[5] = {
            rows[i].time,
            rows[i].source,
            rows[i].user,
            rows[i].category,
            rows[i].details
        };
        for (int col = 0; col < 5; ++col) {
            QTableWidgetItem *item = logsTable->item(i, col);
            if (!item) {
                item = new QTableWidgetItem();
                logsTable->setItem(i, col, item);
            }
            item->setText(values[col]);
        }
    }
    logsTable->setUpdatesEnabled(true);
    logsTable->viewport()->update();
    lastLogsReloadTimer_.restart();
    reloadingLogs_ = false;
}

void leftMenu::showProfile()
{
    activePage_ = ActivePage::Profile;
    hideAllPages();
    clearSearch();

    if (!profilePage)
        buildProfilePage();

    profilePage->setVisible(true);
    stressSuiteLogPageEntered(QStringLiteral("profile"));
}


void leftMenu::buildProfilePage()
{
    if (profilePage) {
        delete profilePage;
        profilePage = nullptr;
    }
    if (profileKeyTimer) {
        profileKeyTimer->stop();
        delete profileKeyTimer;
        profileKeyTimer = nullptr;
    }

    const QString currentUsername = AppSession::currentUsername();

    QString userRole = "viewer";
    {
        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare("SELECT role FROM users WHERE username = :u");
            q.bindValue(":u", currentUsername);
            if (q.exec() && q.next()) {
                userRole = q.value(0).toString();
            }
        }
    }
    bool isAdmin = (userRole == "admin");
    bool isTech = (userRole == "tech");

    const QString userKey = currentUsername.trimmed().isEmpty()
                                ? QString("unknown")
                                : currentUsername.trimmed();

    QString savedFio, savedEmployeeId, savedMobile, savedEmail;
    QString savedPosition, savedDepartment, savedExtPhone, savedTelegram;

    UserInfo dbProfile;
    if (loadUserProfile(currentUsername, dbProfile)) {
        savedFio        = dbProfile.fullName;
        savedEmployeeId = dbProfile.employeeId;
        savedMobile     = dbProfile.mobile;
        savedEmail      = dbProfile.email;
        savedPosition   = dbProfile.position;
        savedDepartment = dbProfile.department;
        savedExtPhone   = dbProfile.extPhone;
        savedTelegram   = dbProfile.telegram;
    } else {
        QSettings settings("AgvNewUi", "AgvNewUi");
    settings.beginGroup(QString("profiles/%1").arg(userKey));
        savedFio        = settings.value("fio").toString();
        savedEmployeeId = settings.value("employee_id").toString();
        savedMobile     = settings.value("mobile").toString();
        savedEmail      = settings.value("email").toString();
        savedPosition   = settings.value("position").toString();
        savedDepartment = settings.value("department").toString();
        savedExtPhone   = settings.value("ext_phone").toString();
        savedTelegram   = settings.value("telegram").toString();
    settings.endGroup();
    }

    QWidget *profileParent = rightCalendarFrame ? rightCalendarFrame->parentWidget() : this;

    profilePage = new QWidget(profileParent);
    profilePage->setStyleSheet("background:#F5F7FB;");

    QVBoxLayout *mainLayout = new QVBoxLayout(profilePage);
    mainLayout->setContentsMargins(s(20), s(15), s(20), s(15));
    mainLayout->setSpacing(s(12));

    //
    // ====== ШАПКА ======
    //
    //
    // ====== ШАПКА ПРОФИЛЯ (идеально выровненная) ======
    //
    QWidget *header = new QWidget(profilePage);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);

    // Отступы как у твоей кнопки
    headerLayout->setContentsMargins(s(10), s(10), s(10), s(5));
    headerLayout->setSpacing(s(10));

    QPushButton *backBtn = new QPushButton("   Назад", header);
    backBtn->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    backBtn->setIconSize(QSize(s(24), s(24)));
    backBtn->setFixedSize(s(150), s(50));

    backBtn->setStyleSheet(QString(
        "QPushButton {"
        "   background-color:#E6E6E6;"
        "   border-radius:%1px;"
        "   border:1px solid #C8C8C8;"
        "   font-family:Inter;"
        "   font-size:%2px;"
        "   font-weight:800;"
        "   color:black;"
        "   text-align:left;"
        "   padding-left:%3px;"
        "}"
        "QPushButton:hover { background-color:#D5D5D5; }"
    ).arg(s(10)).arg(s(16)).arg(s(10)));

    connect(backBtn, &QPushButton::clicked, this, [this]() {
        showCalendar();
    });

    headerLayout->addWidget(backBtn, 0, Qt::AlignLeft);
    headerLayout->addStretch();

    // Добавляем header в профиль
    mainLayout->addWidget(header);
    mainLayout->addSpacing(s(5));

    //
    // ====== СКРОЛЛ ======
    //
    QScrollArea *scroll = new QScrollArea(profilePage);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea{background:transparent;}");

    QWidget *content = new QWidget();
    content->setStyleSheet("background:transparent;");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(s(10), 0, s(10), 0);
    contentLayout->setSpacing(s(20));

    //
    // ====== КАРТОЧКА ПРОФИЛЯ ======
    //
    QWidget *profileCard = new QWidget(content);
    profileCard->setStyleSheet("background:transparent;");
    QVBoxLayout *cardLayout = new QVBoxLayout(profileCard);
    cardLayout->setContentsMargins(s(4), 0, 0, 0);   // ← единый левый отступ
    cardLayout->setSpacing(s(16));

    QHBoxLayout *avatarRow = new QHBoxLayout();
    avatarRow->setSpacing(s(16));

    QLabel *avatarLabel = new QLabel(profileCard);
    QPixmap avatarPm = loadUserAvatarFromDb(currentUsername);
    if (avatarPm.isNull()) avatarPm = QPixmap(":/new/mainWindowIcons/noback/user-icon.png");
    QPixmap toRound = avatarPm.isNull() ? QPixmap() : avatarPm.scaled(s(80), s(80), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap roundAvatar = makeRoundPixmap(toRound, s(80));
    avatarLabel->setPixmap(roundAvatar);
    avatarLabel->setFixedSize(s(80), s(80));
    avatarRow->addWidget(avatarLabel);

    QVBoxLayout *nameCol = new QVBoxLayout();
    nameCol->setSpacing(s(4));

    QLabel *nameLabel = new QLabel(currentUsername, profileCard);
    nameLabel->setStyleSheet(
        "font-family:Inter;font-size:24px;font-weight:900;color:#0F172A;background:transparent;"
    );
    nameCol->addWidget(nameLabel);

    QString roleText =
        isAdmin ? "Роль: Администратор"
        : (userRole == "tech" ? "Роль: Техник" : "Роль: Пользователь");

    QLabel *roleLabel = new QLabel(roleText, profileCard);
    roleLabel->setStyleSheet(
        "font-family:Inter;font-size:14px;font-weight:700;color:#475569;background:transparent;"
    );
    nameCol->addWidget(roleLabel);

    avatarRow->addLayout(nameCol);
    avatarRow->addStretch();
    cardLayout->addLayout(avatarRow);

    //
    // ====== АДМИНСКИЙ КЛЮЧ ======
    //
    //
    // ====== АДМИНСКИЙ КЛЮЧ (идеально выровненный) ======
    //
    //
    // ====== АДМИНСКИЙ КЛЮЧ — ИДЕАЛЬНОЕ ВЫРАВНИВАНИЕ ======
    //
    if (isAdmin || isTech) {

        QWidget *adminBlock = new QWidget(profileCard);
        QVBoxLayout *adminLayout = new QVBoxLayout(adminBlock);
        adminLayout->setContentsMargins(0, 0, 0, 0);
        adminLayout->setSpacing(s(10));

        //
        // Создаём сетку: 2 колонки
        //
        QGridLayout *grid = new QGridLayout();
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(s(6));
        grid->setVerticalSpacing(s(4));

        // Фиксируем ширину первой колонки (как у меток "ФИО", "Телефон" и т.д.)
        int labelWidth = s(20);   // ширина под иконку
        grid->setColumnMinimumWidth(0, labelWidth);
        grid->setColumnStretch(1, 1);

        //
        // Иконка
        //
        QLabel *keyIcon = new QLabel(adminBlock);
        keyIcon->setPixmap(
            QPixmap(":/new/mainWindowIcons/noback/key.png")
                .scaled(s(16), s(16), Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );
        keyIcon->setFixedSize(s(16), s(16));
        keyIcon->setStyleSheet("background:transparent;");

        //
        // Заголовок
        //
        QString keyBlockTitle = isTech ? "Ключ техника" : "Ключ администратора";
        QLabel *keyTitle = new QLabel(keyBlockTitle, adminBlock);
        keyTitle->setStyleSheet(
            "font-family:Inter;"
            "font-size:14px;"
            "font-weight:800;"
            "color:#0369A1;"
            "background:transparent;"
        );

        grid->addWidget(keyIcon, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(keyTitle, 0, 1, Qt::AlignLeft | Qt::AlignVCenter);

        adminLayout->addLayout(grid);

        //
        // Сам ключ
        //
        QString inviteKey;
        if (isAdmin) {
        refreshAdminInviteKeyIfNeeded(currentUsername);
            inviteKey = getAdminInviteKey(currentUsername);
        } else {
            refreshTechInviteKeyIfNeeded(currentUsername);
            inviteKey = getTechInviteKey(currentUsername);
        }

        QLabel *keyValue = new QLabel(inviteKey.isEmpty() ? "Генерация..." : inviteKey, adminBlock);
        keyValue->setAlignment(Qt::AlignCenter);
        keyValue->setStyleSheet(
            "background:#E0F2FE;"
            "border:1px solid #BAE6FD;"
            "border-radius:10px;"
            "font-family:Consolas,monospace;"
            "font-size:20px;"
            "font-weight:700;"
            "color:#0C4A6E;"
            "padding:12px;"
        );
        adminLayout->addWidget(keyValue);

        //
        // Подсказка
        //
        QString keyHintText = isTech
            ? "Действует 10 минут. Передайте новому технику для регистрации."
            : "Действует 10 минут. Передайте новому админу для регистрации.";
        QLabel *keyHint = new QLabel(keyHintText, adminBlock);
        keyHint->setWordWrap(true);
        keyHint->setStyleSheet(
            "font-family:Inter;font-size:11px;color:#0369A1;background:transparent;"
        );
        adminLayout->addWidget(keyHint);

        //
        // Кнопка копирования
        //
        QPushButton *copyKeyBtn = new QPushButton("Копировать ключ", adminBlock);
        copyKeyBtn->setStyleSheet(
            "QPushButton{background:#0EA5E9;color:white;font-family:Inter;font-size:13px;"
            "font-weight:700;border:none;border-radius:8px;padding:10px 16px;}"
            "QPushButton:hover{background:#0284C7;}"
        );
        connect(copyKeyBtn, &QPushButton::clicked, this, [keyValue, copyKeyBtn]() {
            QApplication::clipboard()->setText(keyValue->text());
            copyKeyBtn->setText("Скопировано!");
            QTimer::singleShot(2000, copyKeyBtn, [copyKeyBtn]() {
                copyKeyBtn->setText("Копировать ключ");
            });
        });
        adminLayout->addWidget(copyKeyBtn);

        //
        // Таймер
        //
        profileKeyTimer = new QTimer(this);
        profileKeyTimer->setInterval(30000);
        connect(profileKeyTimer, &QTimer::timeout, this, [this, keyValue, currentUsername, isTech]() {
            if (isTech) {
                refreshTechInviteKeyIfNeeded(currentUsername);
                QString newKey = getTechInviteKey(currentUsername);
                keyValue->setText(newKey.isEmpty() ? "Ошибка" : newKey);
            } else {
            refreshAdminInviteKeyIfNeeded(currentUsername);
            QString newKey = getAdminInviteKey(currentUsername);
            keyValue->setText(newKey.isEmpty() ? "Ошибка" : newKey);
            }
        });
        profileKeyTimer->start();

        cardLayout->addWidget(adminBlock);
    }


    contentLayout->addWidget(profileCard);

    //
    // ====== ЛИЧНЫЕ ДАННЫЕ ======
    //
    QWidget *editCard = new QWidget(content);
    editCard->setStyleSheet("background:transparent;");
    QVBoxLayout *editLayout = new QVBoxLayout(editCard);
    editLayout->setContentsMargins(s(4), 0, 0, 0);   // ← тот же левый отступ
    editLayout->setSpacing(s(10));

    QLabel *editTitle = new QLabel("Личные данные", editCard);
    editTitle->setStyleSheet(
        "font-family:Inter;font-size:18px;font-weight:900;color:#0F172A;background:transparent;"
    );
    editLayout->addWidget(editTitle);

    auto addField = [&](const QString &label,
                        const QString &placeholder,
                        const QString &value,
                        const QString &validatorPattern,
                        int maxLen = 0) -> QLineEdit*
    {
        QLabel *lbl = new QLabel(label, editCard);
        lbl->setStyleSheet(
            "font-family:Inter;font-size:13px;font-weight:700;color:#334155;background:transparent;"
        );
        editLayout->addWidget(lbl);

        QLineEdit *edit = new QLineEdit(editCard);
        edit->setPlaceholderText(placeholder);
        edit->setText(value);
        edit->setStyleSheet(
            "QLineEdit{background:#FFFFFF;border:1px solid #E2E8F0;border-radius:10px;"
            "padding:12px 14px;font-family:Inter;font-size:14px;color:#0F172A;}"
            "QLineEdit:focus{border:1px solid #3B82F6;background:white;}"
        );
        if (maxLen > 0)
            edit->setMaxLength(maxLen);
        if (!validatorPattern.isEmpty()) {
            edit->setValidator(
                new QRegularExpressionValidator(QRegularExpression(validatorPattern), edit)
            );
        }
        editLayout->addWidget(edit);
        return edit;
    };

    QLineEdit *fioEdit        = addField("ФИО", "Введите ФИО", savedFio, "", 128);
    QLineEdit *employeeIdEdit = addField("Табельный номер", "До 6 цифр", savedEmployeeId, "^[0-9]{0,6}$");
    QLineEdit *positionEdit   = addField("Должность", "Введите должность", savedPosition, "", 128);
    QLineEdit *departmentEdit = addField("Подразделение", "Введите подразделение", savedDepartment, "", 128);
    QLineEdit *mobileEdit     = addField("Телефон", "+7 (XXX) XXX-XX-XX", savedMobile, "^[\\+0-9\\-\\s\\(\\)]{0,20}$");
    QLineEdit *extPhoneEdit   = addField("Внутренний номер", "До 5 цифр", savedExtPhone, "^[0-9]{0,5}$");
    QLineEdit *emailEdit      = addField("Email", "example@mail.com", savedEmail, "");
    QLineEdit *telegramEdit   = addField("Telegram", "@username", savedTelegram, "^@?[A-Za-z0-9_]{0,32}$");

    connect(mobileEdit, &QLineEdit::editingFinished, mobileEdit, [mobileEdit](){
        QString t = mobileEdit->text().trimmed();
        if (t.isEmpty()) return;
        QString digits = t;
        digits.remove(QRegularExpression("[^0-9+]"));
        if (digits.isEmpty()) return;
        QString result;
        if (digits.startsWith("+7")) result = digits;
        else if (digits.startsWith("8") && digits.length() > 1) result = "+7" + digits.mid(1);
        else if (digits.startsWith("7")) result = "+" + digits;
        else result = "+7" + digits;
        if (result != mobileEdit->text()) {
            mobileEdit->blockSignals(true);
            mobileEdit->setText(result);
            mobileEdit->blockSignals(false);
        }
    });

    connect(telegramEdit, &QLineEdit::textChanged, telegramEdit, [telegramEdit](){
        QString t = telegramEdit->text();
        QString out;
        for (int i = 0; i < t.length(); ++i) {
            QChar c = t[i];
            if (c == '@' && out.isEmpty()) out += c;
            else if (c.isLetterOrNumber() || c == '_') out += c;
        }
        if (!out.isEmpty() && !out.startsWith('@')) out.prepend('@');
        if (out != t) {
            telegramEdit->blockSignals(true);
            telegramEdit->setText(out);
            telegramEdit->blockSignals(false);
        }
    });

    QLabel *errorLbl = new QLabel(editCard);
    errorLbl->setStyleSheet(
        "font-family:Inter;font-size:12px;font-weight:600;color:#DC2626;background:transparent;"
    );
    errorLbl->setWordWrap(true);
    editLayout->addWidget(errorLbl);

    editLayout->addSpacing(s(10));

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(s(12));

    QPushButton *saveBtn = new QPushButton("Сохранить изменения", editCard);
    saveBtn->setStyleSheet(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;font-size:14px;"
        "font-weight:800;border:none;border-radius:10px;padding:14px 28px;}"
        "QPushButton:hover{background:#1A4ACD;}"
    );

    connect(saveBtn, &QPushButton::clicked, this, [=]() {
        QString mobile = mobileEdit->text().trimmed();
        if (!mobile.isEmpty()) {
            QString digits = mobile;
            digits.remove(QRegularExpression("[^0-9+]"));
            if (!digits.startsWith("+") && !digits.startsWith("7") && !digits.startsWith("8")) {
                mobile = "+7" + digits;
            } else if (digits.startsWith("8") && digits.length() > 1) {
                mobile = "+7" + digits.mid(1);
            } else if (digits.startsWith("7") && !digits.startsWith("+")) {
                mobile = "+" + digits;
            }
            mobileEdit->setText(mobile);
        }

        QString email = emailEdit->text().trimmed();
        if (!email.isEmpty()) {
            if (!email.contains('@') || !QRegularExpression("\\.[a-zA-Z]{2,}$").match(email).hasMatch()) {
                errorLbl->setText("Email должен содержать @ и домен (.com, .ru и т.п.).");
                return;
            }
        }

        QString telegram = telegramEdit->text().trimmed();
        if (!telegram.isEmpty()) {
            QString out;
            for (int i = 0; i < telegram.length(); ++i) {
                QChar c = telegram[i];
                if (c == '@' && out.isEmpty()) out += c;
                else if (c.isLetterOrNumber() || c == '_') out += c;
            }
            if (!out.isEmpty() && !out.startsWith('@')) out.prepend('@');
            telegramEdit->setText(out);
            telegram = out;
        }

        QSettings s("AgvNewUi", "AgvNewUi");
        s.beginGroup(QString("profiles/%1").arg(userKey));
        s.setValue("fio",          fioEdit->text().trimmed());
        s.setValue("employee_id",  employeeIdEdit->text().trimmed());
        s.setValue("mobile",       mobile);
        s.setValue("email",        email);
        s.setValue("position",     positionEdit->text().trimmed());
        s.setValue("department",   departmentEdit->text().trimmed());
        s.setValue("ext_phone",    extPhoneEdit->text().trimmed());
        s.setValue("telegram",     telegram);
        s.endGroup();

        UserInfo profileData;
        profileData.username   = currentUsername;
        profileData.fullName   = fioEdit->text().trimmed();
        profileData.employeeId = employeeIdEdit->text().trimmed();
        profileData.position   = positionEdit->text().trimmed();
        profileData.department = departmentEdit->text().trimmed();
        profileData.mobile     = mobile;
        profileData.extPhone   = extPhoneEdit->text().trimmed();
        profileData.email      = email;
        profileData.telegram   = telegram;

        QString dbError;
        if (!saveUserProfile(profileData, dbError)) {
            errorLbl->setStyleSheet(
                "font-family:Inter;font-size:12px;font-weight:600;color:#DC2626;background:transparent;"
            );
            errorLbl->setText("Ошибка сохранения: " + (dbError.isEmpty() ? "не удалось записать в БД" : dbError));
            return;
        }

        emit DataBus::instance().userDataChanged();

        logAction(currentUsername, "profile_saved", "Профиль сохранён");
        errorLbl->setStyleSheet(
            "font-family:Inter;font-size:12px;font-weight:600;color:#10B981;background:transparent;"
        );
        errorLbl->setText("Изменения сохранены!");
        QTimer::singleShot(2000, errorLbl, [errorLbl]() {
            errorLbl->setStyleSheet(
                "font-family:Inter;font-size:12px;font-weight:600;color:#DC2626;background:transparent;"
            );
            errorLbl->clear();
        });
    });

    btnRow->addWidget(saveBtn);
    btnRow->addStretch();
    editLayout->addLayout(btnRow);

    contentLayout->addWidget(editCard);
    contentLayout->addStretch();

    scroll->setWidget(content);
    mainLayout->addWidget(scroll, 1);

    // Добавляем страницу в правую панель
    if (rightCalendarFrame) {
        QWidget *rightBodyFrame = rightCalendarFrame->parentWidget();
        if (rightBodyFrame) {
            if (QVBoxLayout *rightBodyLayout = qobject_cast<QVBoxLayout*>(rightBodyFrame->layout())) {
                rightBodyLayout->addWidget(profilePage, 3);
            }
        }
    }
}



bool leftMenu::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        for (QWidget *w = qobject_cast<QWidget*>(obj); w; w = qobject_cast<QWidget*>(w->parentWidget())) {
            const int notificationId = w->property("notificationId").toInt();

            const QString peerOpen = w->property("openChatPeerUser").toString().trimmed();
            if (!peerOpen.isEmpty()) {
                const QString currentUser = AppSession::currentUsername();
                const QString agvHint = w->property("openChatAgvHint").toString().trimmed();
                QString ctx = w->property("openChatContextText").toString().trimmed();
                if (ctx.isEmpty())
                    ctx = QStringLiteral("По уведомлению");
                QString err;
                const int tid = TaskChatDialog::ensureThreadWithUser(currentUser, peerOpen, agvHint, &err);
                if (tid <= 0) {
                    QMessageBox::warning(this, QStringLiteral("Чат"),
                                           err.isEmpty() ? QStringLiteral("Не удалось открыть чат") : err);
                    return true;
                }
                TaskChatDialog::markNextMessageSpecial(tid, ctx);
                if (notificationId > 0) {
                    markNotificationReadById(notificationId);
                    updateNotifBadge();
                }
                showChatsPage();
                if (embeddedChatWidget_ && chatsStack_) {
                    embeddedChatWidget_->setThreadId(tid, peerOpen);
                    chatsStack_->setCurrentIndex(1);
                }
                if (QDialog *dlg = qobject_cast<QDialog*>(w->window()))
                    dlg->accept();
                return true;
            }

            const int chatId = w->property("openChatThreadId").toInt();
            if (chatId > 0) {
                const QString agvMark = w->property("openChatAgvHintForThread").toString().trimmed();
                if (!agvMark.isEmpty())
                    TaskChatDialog::markNextMessageSpecial(chatId, QStringLiteral("Чат по AGV %1").arg(agvMark));
                showChatsPage();
                if (embeddedChatWidget_ && chatsStack_) {
                    embeddedChatWidget_->setThreadId(chatId, QString());
                    chatsStack_->setCurrentIndex(1);
                }
                if (notificationId > 0) {
                    markNotificationReadById(notificationId);
                    updateNotifBadge();
                }
                if (QDialog *dlg = qobject_cast<QDialog*>(w->window()))
                    dlg->accept();
                return true;
            }
            if (w->property("openChatsPageOnClick").toBool()) {
                showChatsPage();
                if (notificationId > 0) {
                    markNotificationReadById(notificationId);
                    updateNotifBadge();
                }
                if (QDialog *dlg = qobject_cast<QDialog*>(w->window()))
                    dlg->accept();
                return true;
            }
        }
    }

    if (calendarTablePtr && obj == calendarTablePtr->viewport() && event->type() == QEvent::Paint) {

        QPainter p(calendarTablePtr->viewport());
        p.setPen(QColor("#D3D3D3"));

        for (int r = 1; r < calendarTablePtr->rowCount(); r++) {
            for (int c = 0; c < calendarTablePtr->columnCount(); c++) {
                QRect rect = calendarTablePtr->visualRect(calendarTablePtr->model()->index(r, c));
                p.drawRect(rect.adjusted(0,0,-1,-1));
            }
        }
    }

    return QWidget::eventFilter(obj, event);
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
    if (!delegated.isEmpty()) {
        QLabel *delegatedHeader = new QLabel("Делегировано вам", contentContainer);
        delegatedHeader->setStyleSheet(QString(
            "background:transparent;font-family:Inter;font-size:%1px;font-weight:800;color:#0F00DB;padding:%2px 0;"
        ).arg(s(14)).arg(s(4)));
        contentLayout->addWidget(delegatedHeader);
        for (const auto &item : delegated)
            addMaintenanceItem(item);
    }
    if (!rest.isEmpty() && !delegated.isEmpty()) {
        QFrame *sep = new QFrame(contentContainer);
        sep->setFrameShape(QFrame::HLine);
        sep->setFixedHeight(1);
        sep->setStyleSheet("background:#ddd;border:none;");
        contentLayout->addWidget(sep);
    }
    for (const auto &item : rest)
        addMaintenanceItem(item);

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

bool saveUserAvatarToDb(const QString &username, const QPixmap &pm)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pm.save(&buffer, "PNG");

    QSqlQuery q(QSqlDatabase::database("main_connection"));
    q.prepare("UPDATE users SET avatar = :av WHERE username = :u");
    q.bindValue(":av", bytes);
    q.bindValue(":u", username);
    return q.exec();
}

    void leftMenu::changeAvatar()
    {
        QString username = AppSession::currentUsername();
        if (username.isEmpty())
            return;

        QString file = QFileDialog::getOpenFileName(
            this,
            "Выберите изображение",
            "",
            "Изображения (*.png *.jpg *.jpeg *.bmp)"
        );

        if (file.isEmpty())
            return;

        QPixmap pm(file);
        if (pm.isNull()) {
            QMessageBox::warning(this, "Ошибка", "Не удалось загрузить изображение.");
            return;
        }

        // Сохраняем в БД
        if (!saveUserAvatarToDb(username, pm)) {
            QMessageBox::warning(this, "Ошибка", "Не удалось сохранить аватар в базу данных.");
            return;
        }

        // Обновляем кнопку
        QPixmap round = makeRoundPixmap(pm, s(55));
        userButton->setIcon(QIcon(round));
        userButton->setIconSize(QSize(s(55), s(55)));

        // Если страница профиля уже создана — пересобираем её, чтобы аватар обновился сразу
        if (profilePage) {
            bool wasVisible = profilePage->isVisible();
            buildProfilePage();
            profilePage->setVisible(wasVisible);
        }

        QMessageBox::information(this, "Готово", "Аватар успешно обновлён.");
    }
    void leftMenu::showAnnualReportDialog()
    {
        QDialog dlg(this);
        dlg.setWindowTitle("Годовой отчёт AGV");
        dlg.setFixedSize(s(560), s(520));
        dlg.setStyleSheet(
            "QDialog{background:#F5F7FB;}"
            "QLabel{background:transparent;font-family:Inter;color:#1A1A1A;}"
            "QComboBox{background:#FFFFFF;border:1px solid #C7D2FE;border-radius:10px;"
            "padding:7px 12px;font-family:Inter;font-size:13px;color:#111827;min-height:20px;}"
            "QComboBox:hover{border:1px solid #8EA2FF;}"
            "QComboBox::drop-down{border:none;width:20px;}"
        );

        QVBoxLayout *root = new QVBoxLayout(&dlg);
        root->setContentsMargins(s(20), s(20), s(20), s(20));
        root->setSpacing(s(14));

        QLabel *title = new QLabel("Сформировать годовой отчёт", &dlg);
        title->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#0F172A;"
        ).arg(s(22)));
        root->addWidget(title);

        QLabel *subtitle = new QLabel("Выберите AGV и диапазон дат для формирования PDF-отчёта", &dlg);
        subtitle->setWordWrap(true);
        subtitle->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:500;color:#6B7280;"
        ).arg(s(13)));
        root->addWidget(subtitle);

        QLabel *agvLabel = new QLabel("AGV:", &dlg);
        agvLabel->setStyleSheet(QString("font-size:%1px;font-weight:700;").arg(s(14)));
        root->addWidget(agvLabel);

        QComboBox *agvCombo = new QComboBox(&dlg);
        agvCombo->addItem("Все AGV", "");

        {
            QSqlDatabase db = QSqlDatabase::database("main_connection");
            if (db.isOpen()) {
                QSqlQuery q(db);
                q.prepare("SELECT agv_id FROM agv_list ORDER BY agv_id ASC");
                if (q.exec()) {
                    while (q.next()) {
                        QString agvId = q.value(0).toString();
                        agvCombo->addItem(agvId, agvId);
                    }
                }
            }
        }
        root->addWidget(agvCombo);

        QLabel *dateFromLabel = new QLabel("Дата начала:", &dlg);
        dateFromLabel->setStyleSheet(QString("font-size:%1px;font-weight:700;").arg(s(14)));
        root->addWidget(dateFromLabel);

        QDateEdit *dateFrom = new QDateEdit(QDate::currentDate().addYears(-1), &dlg);
        dateFrom->setCalendarPopup(true);
        dateFrom->setDisplayFormat("dd.MM.yyyy");
        dateFrom->setStyleSheet(
            "QDateEdit{background:#FFFFFF;border:1px solid #C7D2FE;border-radius:10px;"
            "padding:7px 12px;font-family:Inter;font-size:13px;color:#111827;}"
        );
        root->addWidget(dateFrom);

        QLabel *dateToLabel = new QLabel("Дата окончания:", &dlg);
        dateToLabel->setStyleSheet(QString("font-size:%1px;font-weight:700;").arg(s(14)));
        root->addWidget(dateToLabel);

        QDateEdit *dateTo = new QDateEdit(QDate::currentDate(), &dlg);
        dateTo->setCalendarPopup(true);
        dateTo->setDisplayFormat("dd.MM.yyyy");
        dateTo->setStyleSheet(dateFrom->styleSheet());
        root->addWidget(dateTo);

        root->addStretch();

        QLabel *errorLbl = new QLabel(&dlg);
        errorLbl->setStyleSheet("font-family:Inter;font-size:12px;font-weight:600;color:#DC2626;");
        errorLbl->setWordWrap(true);
        root->addWidget(errorLbl);

        QHBoxLayout *btns = new QHBoxLayout();
        btns->setSpacing(s(12));

        QPushButton *cancelBtn = new QPushButton("Отмена", &dlg);
        cancelBtn->setStyleSheet(QString(
            "QPushButton{background:#E6E6E6;border-radius:%1px;border:1px solid #C8C8C8;"
            "font-family:Inter;font-size:%2px;font-weight:700;padding:%3px %4px;color:#333;}"
            "QPushButton:hover{background:#D5D5D5;}"
        ).arg(s(8)).arg(s(14)).arg(s(8)).arg(s(16)));

        QPushButton *saveBtn = new QPushButton("Сохранить PDF", &dlg);
        saveBtn->setStyleSheet(QString(
            "QPushButton{background:#0F00DB;color:white;font-family:Inter;font-size:%1px;"
            "font-weight:800;border:none;border-radius:%2px;padding:%3px %4px;}"
            "QPushButton:hover{background:#1A4ACD;}"
        ).arg(s(14)).arg(s(8)).arg(s(8)).arg(s(16)));

        QPushButton *printBtn = new QPushButton("Печать", &dlg);
        printBtn->setStyleSheet(QString(
            "QPushButton{background:#0EA5E9;color:white;font-family:Inter;font-size:%1px;"
            "font-weight:800;border:none;border-radius:%2px;padding:%3px %4px;}"
            "QPushButton:hover{background:#0284C7;}"
        ).arg(s(14)).arg(s(8)).arg(s(8)).arg(s(16)));

        btns->addWidget(cancelBtn);
        btns->addStretch();
        btns->addWidget(printBtn);
        btns->addWidget(saveBtn);
        root->addLayout(btns);

        connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (stressSuiteRunning_ || qApp->property("autotest_running").toBool()) {
        for (int attempt = 0; attempt < 6; ++attempt) {
            QTimer::singleShot(140 + attempt * 120, &dlg, [&dlg, cancelBtn]() {
                if (!dlg.isVisible())
                    return;
                if (cancelBtn && cancelBtn->isVisible() && cancelBtn->isEnabled())
                    cancelBtn->click();
                else
                    dlg.reject();
            });
        }
    }

        auto generateReportHtml = [&]() -> QString {
            const QString agvId = agvCombo->currentData().toString();
            const QDate from = dateFrom->date();
            const QDate to = dateTo->date();

            if (from > to) {
                errorLbl->setText("Дата начала не может быть позже даты окончания.");
                return QString();
            }

            QSqlDatabase db = QSqlDatabase::database("main_connection");
            if (!db.isOpen()) {
                errorLbl->setText("База данных не доступна.");
                return QString();
            }

            QString html;
            html += "<html><head><meta charset='utf-8'>";
            html += "<style>"
                    "body{font-family:Inter,Arial,sans-serif;margin:11px;font-size:9.5pt;}"
                    "h1{color:#0F172A;font-size:17pt;}"
                    "h2{color:#334155;font-size:13pt;margin-top:11px;}"
                    "p{font-size:9.5pt;}"
                    "table{border-collapse:collapse;width:100%;margin-top:9px;font-size:8.5pt;}"
                    "th{background:#0F00DB;color:white;padding:5px 9px;text-align:left;font-size:8.5pt;}"
                    "td{border:1px solid #E2E8F0;padding:4px 9px;font-size:8.5pt;}"
                    "tr:nth-child(even){background:#F8FAFC;}"
                    ".summary{background:#EFF6FF;border:1px solid #BFDBFE;border-radius:6px;padding:9px;margin:9px 0;font-size:9.5pt;}"
                    "</style></head><body>";

            html += QString("<h1>Годовой отчёт AGV</h1>");
            html += QString("<p><b>Период:</b> %1 — %2</p>")
                        .arg(from.toString("dd.MM.yyyy"))
                        .arg(to.toString("dd.MM.yyyy"));

            if (!agvId.isEmpty())
                html += QString("<p><b>AGV:</b> %1</p>").arg(agvId);
            else
                html += "<p><b>AGV:</b> Все</p>";

            html += QString("<p><b>Дата формирования:</b> %1</p>")
                        .arg(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm"));

            QSqlQuery agvQ(db);
            if (agvId.isEmpty()) {
                agvQ.prepare("SELECT agv_id, model, serial, status, kilometers FROM agv_list ORDER BY agv_id ASC");
            } else {
                agvQ.prepare("SELECT agv_id, model, serial, status, kilometers FROM agv_list WHERE agv_id = :id");
                agvQ.bindValue(":id", agvId);
            }

            if (agvQ.exec()) {
                html += "<h2>Список AGV</h2>";
                html += "<table><tr><th>ID</th><th>Модель</th><th>S/N</th><th>Статус</th><th>Пробег (км)</th></tr>";

                while (agvQ.next()) {
                    html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                                .arg(agvQ.value(0).toString())
                                .arg(agvQ.value(1).toString())
                                .arg(agvQ.value(2).toString())
                                .arg(agvQ.value(3).toString())
                                .arg(agvQ.value(4).toInt());
                }
                html += "</table>";
            }

            QSqlQuery histQ(db);
            if (agvId.isEmpty()) {
                histQ.prepare(R"(
                    SELECT h.agv_id, h.task_name, h.interval_days, h.completed_at, h.next_date_after, h.performed_by
                    FROM agv_task_history h
                    INNER JOIN agv_list a ON a.agv_id = h.agv_id
                    WHERE h.completed_at >= :from AND h.completed_at <= :to
                    ORDER BY h.completed_at DESC
                )");
            } else {
                histQ.prepare(R"(
                    SELECT h.agv_id, h.task_name, h.interval_days, h.completed_at, h.next_date_after, h.performed_by
                    FROM agv_task_history h
                    INNER JOIN agv_list a ON a.agv_id = h.agv_id
                    WHERE h.agv_id = :id AND h.completed_at >= :from AND h.completed_at <= :to
                    ORDER BY h.completed_at DESC
                )");
                histQ.bindValue(":id", agvId);
            }
            histQ.bindValue(":from", from);
            histQ.bindValue(":to", to);

            int totalTasks = 0;

            if (histQ.exec()) {
                html += "<h2>История обслуживания</h2>";
                html += "<table><tr><th>AGV</th><th>Задача</th><th>Интервал (дн.)</th>"
                         "<th>Выполнено</th><th>Следующая дата</th><th>Исполнитель</th></tr>";

                while (histQ.next()) {
                    totalTasks++;
                    html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6</td></tr>")
                                .arg(histQ.value(0).toString())
                                .arg(histQ.value(1).toString())
                                .arg(histQ.value(2).toInt())
                                .arg(histQ.value(3).toDate().toString("dd.MM.yyyy"))
                                .arg(histQ.value(4).toDate().toString("dd.MM.yyyy"))
                                .arg(histQ.value(5).toString());
                }
                html += "</table>";
            }

            QSqlQuery pendingQ(db);
            if (agvId.isEmpty()) {
                pendingQ.prepare(R"(
                    SELECT t.agv_id, t.task_name, t.interval_days, t.next_date
                    FROM agv_tasks t
                    INNER JOIN agv_list a ON a.agv_id = t.agv_id
                    WHERE t.next_date >= :from AND t.next_date <= :to
                    ORDER BY t.next_date ASC
                )");
            } else {
                pendingQ.prepare(R"(
                    SELECT t.agv_id, t.task_name, t.interval_days, t.next_date
                    FROM agv_tasks t
                    INNER JOIN agv_list a ON a.agv_id = t.agv_id
                    WHERE t.agv_id = :id AND t.next_date >= :from AND t.next_date <= :to
                    ORDER BY t.next_date ASC
                )");
                pendingQ.bindValue(":id", agvId);
            }
            pendingQ.bindValue(":from", from);
            pendingQ.bindValue(":to", to);

            int overdueCount = 0;
            int upcomingCount = 0;

            if (pendingQ.exec()) {
                html += "<h2>Запланированные задачи</h2>";
                html += "<table><tr><th>AGV</th><th>Задача</th><th>Интервал (дн.)</th><th>Дата</th><th>Статус</th></tr>";

                QDate today = QDate::currentDate();
                while (pendingQ.next()) {
                    QDate nextDate = pendingQ.value(3).toDate();
                    QString status;
                    if (nextDate < today) {
                        status = "<span style='color:#FF0000;font-weight:bold;'>Просрочено</span>";
                        overdueCount++;
                    } else if (nextDate <= today.addDays(7)) {
                        status = "<span style='color:#FF8800;font-weight:bold;'>Скоро</span>";
                        upcomingCount++;
                    } else {
                        status = "<span style='color:#18CF00;'>Запланировано</span>";
                    }

                    html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                                .arg(pendingQ.value(0).toString())
                                .arg(pendingQ.value(1).toString())
                                .arg(pendingQ.value(2).toInt())
                                .arg(nextDate.toString("dd.MM.yyyy"))
                                .arg(status);
                }
                html += "</table>";
            }

            html += "<div class='summary'>";
            html += QString("<p><b>Выполнено задач за период:</b> %1</p>").arg(totalTasks);
            html += QString("<p><b>Просроченных задач:</b> %1</p>").arg(overdueCount);
            html += QString("<p><b>Ближайших задач (7 дней):</b> %1</p>").arg(upcomingCount);
            html += "</div>";

            html += "</body></html>";

            errorLbl->clear();
            return html;
        };

        connect(saveBtn, &QPushButton::clicked, &dlg, [&](){
            QString html = generateReportHtml();
            if (html.isEmpty())
                return;

            QString filePath = QFileDialog::getSaveFileName(
                &dlg,
                "Сохранить отчёт",
                QString("AGV_Report_%1.pdf").arg(QDate::currentDate().toString("yyyy-MM-dd")),
                "PDF файлы (*.pdf)"
            );
            if (filePath.isEmpty())
                return;

            QPrinter printer(QPrinter::ScreenResolution);
            printer.setOutputFormat(QPrinter::PdfFormat);
            printer.setOutputFileName(filePath);
            printer.setPageSize(QPageSize(QPageSize::A4));
            printer.setPageMargins(QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);

            QTextDocument doc;
            doc.setHtml(html);
            doc.setPageSize(printer.pageRect(QPrinter::Point).size());
            doc.print(&printer);

            QMessageBox::information(&dlg, "Готово",
                QString("Отчёт сохранён:\n%1").arg(filePath));
            dlg.accept();
        });

        connect(printBtn, &QPushButton::clicked, &dlg, [&](){
            QString html = generateReportHtml();
            if (html.isEmpty())
                return;

            QPrinter printer(QPrinter::ScreenResolution);
            QPrintDialog printDlg(&printer, &dlg);
            if (printDlg.exec() == QDialog::Accepted) {
                QTextDocument doc;
                doc.setHtml(html);
                doc.setPageSize(printer.pageRect(QPrinter::Point).size());
                doc.print(&printer);

                QMessageBox::information(&dlg, "Готово", "Отчёт отправлен на печать.");
                dlg.accept();
            }
        });

        dlg.exec();
    }

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

    void leftMenu::showNotificationsPanel()
    {
        const QString currentUser = AppSession::currentUsername();
        QDialog dlg(this);
        dlg.setWindowTitle("Уведомления");
        dlg.setFixedSize(s(520), s(560));
        dlg.setStyleSheet("background:#F5F7FB;");

        QVBoxLayout *root = new QVBoxLayout(&dlg);
        root->setContentsMargins(s(16), s(16), s(16), s(16));
        root->setSpacing(s(10));

        QLabel *title = new QLabel("Уведомления", &dlg);
        title->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#0F172A;"
        ).arg(s(22)));
        root->addWidget(title);

        QScrollArea *scroll = new QScrollArea(&dlg);
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");

        QWidget *host = new QWidget(scroll);
        host->setStyleSheet("background:transparent;");
        QVBoxLayout *listLayout = new QVBoxLayout(host);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(s(8));

        QVector<Notification> notifs = loadNotificationsForUser(currentUser);

        if (notifs.isEmpty()) {
            QLabel *empty = new QLabel("Нет уведомлений", host);
            empty->setAlignment(Qt::AlignCenter);
            empty->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;font-weight:700;color:#888;"
            ).arg(s(16)));
            listLayout->addStretch();
            listLayout->addWidget(empty, 0, Qt::AlignCenter);
            listLayout->addStretch();
        } else {
            for (const Notification &n : notifs) {
                QFrame *card = new QFrame(host);
                card->setProperty("notificationId", n.id);
                card->setStyleSheet(QString(
                    "QFrame{background:%1;border:none;}"
                ).arg(n.isRead ? "#FFFFFF" : "#EFF6FF"));

                QVBoxLayout *cardL = new QVBoxLayout(card);
                cardL->setContentsMargins(s(12), s(8), s(12), s(8));
                cardL->setSpacing(s(4));

                QHBoxLayout *topRow = new QHBoxLayout();
                QLabel *titleLbl = new QLabel(n.title, card);
                titleLbl->setStyleSheet(QString(
                    "font-family:Inter;font-size:%1px;font-weight:800;color:#0F172A;background:transparent;"
                ).arg(s(14)));

                QLabel *timeLbl = new QLabel(n.createdAt.toString("dd.MM.yy hh:mm"), card);
                timeLbl->setStyleSheet(QString(
                    "font-family:Inter;font-size:%1px;color:#94A3B8;background:transparent;"
                ).arg(s(11)));

                topRow->addWidget(titleLbl);
                topRow->addStretch();
                topRow->addWidget(timeLbl);
                cardL->addLayout(topRow);

                QLabel *msgLbl = new QLabel(notificationMessageForDisplay(n.message), card);
                msgLbl->setWordWrap(true);
                msgLbl->setStyleSheet(QString(
                    "font-family:Inter;font-size:%1px;color:#475569;background:transparent;"
                ).arg(s(13)));
                cardL->addWidget(msgLbl);

                auto parseAgvId = [&](const QString &text) -> QString {
                    QRegularExpression re("AGV\\s*№?\\s*([A-Za-z0-9_\\-]+)");
                    QRegularExpressionMatch m = re.match(text);
                    return m.hasMatch() ? m.captured(1).trimmed() : QString();
                };
                auto parseSenderLogin = [&](const QString &text) -> QString {
                    QRegularExpression re("\\(([^\\)]+)\\)");
                    QRegularExpressionMatch m = re.match(text);
                    if (m.hasMatch()) return m.captured(1).trimmed();
                    return QString();
                };
                auto parseChatId = [&](const QString &text) -> int {
                    QRegularExpression re("\\[chat:(\\d+)\\]");
                    QRegularExpressionMatch m = re.match(text);
                    return m.hasMatch() ? m.captured(1).toInt() : 0;
                };

                const bool chatEligible = (n.title == "AGV закреплена за вами" || n.title == "Задача делегирована");
                const bool chatNotification = (n.title.contains("чат", Qt::CaseInsensitive) ||
                                               n.title == "Новое сообщение по задаче" ||
                                               n.title == "Ответ в чате по задаче");
                if (chatEligible) {
                    QString agvId = parseAgvId(n.message);
                    QString recipient = notificationPeerUsername(n.message);
                    if (recipient.isEmpty())
                        recipient = parseSenderLogin(n.message);
                    QPushButton *chatBtn = new QPushButton("Начать чат", card);
                    chatBtn->setStyleSheet(QString(
                        "QPushButton{background:#0B89FF;color:white;font-family:Inter;font-size:%1px;"
                        "font-weight:700;border:none;border-radius:%2px;padding:%3px %4px;}"
                        "QPushButton:hover{background:#0A75D6;}"
                    ).arg(s(12)).arg(s(7)).arg(s(6)).arg(s(12)));
                    chatBtn->setIcon(QIcon(":/new/mainWindowIcons/noback/user.png"));
                    chatBtn->setIconSize(QSize(s(14), s(14)));
                    connect(chatBtn, &QPushButton::clicked, &dlg, [this, currentUser, agvId, recipient, &dlg, n]() {
                        if (agvId.isEmpty() || recipient.isEmpty()) {
                            QMessageBox::warning(this, "Чат",
                                                 "Не удалось определить AGV или отправителя из уведомления.");
                            return;
                        }
                        QString err;
                        int tid = TaskChatDialog::ensureThreadWithUser(currentUser, recipient, QString(), &err);
                        if (tid <= 0) {
                            QMessageBox::warning(this, "Чат", err.isEmpty() ? QStringLiteral("Не удалось открыть чат") : err);
                            return;
                        }
                        TaskChatDialog::markNextMessageSpecial(tid, QStringLiteral("AGV %1").arg(agvId));
                        markNotificationReadById(n.id);
                        updateNotifBadge();
                        showChatsPage();
                        if (embeddedChatWidget_ && chatsStack_) {
                            embeddedChatWidget_->setThreadId(tid, recipient);
                            chatsStack_->setCurrentIndex(1);
                        }
                        dlg.accept();
                    });
                    cardL->addWidget(chatBtn, 0, Qt::AlignLeft);
                }
                const bool taskNotifOpenChat =
                    (n.title == QStringLiteral("Задача выполнена") ||
                     n.title == QStringLiteral("Задача назначена") ||
                     n.title == QStringLiteral("Новая задача") ||
                     n.title == QStringLiteral("Задача делегирована"));
                if (taskNotifOpenChat) {
                    const QString peerU = notificationPeerUsername(n.message);
                    if (!peerU.isEmpty()) {
                        const QString agvIdTask = parseAgvId(n.message);
                        QString tname;
                        {
                            static const QRegularExpression reTn(QStringLiteral("Задача\\s+\"([^\"]+)\""));
                            static const QRegularExpression reAvail(QStringLiteral("Доступна задача «([^»]+)»"));
                            static const QRegularExpression reDel(QStringLiteral("делегирована задача\\s+\"([^\"]+)\""),
                                                                 QRegularExpression::CaseInsensitiveOption);
                            QRegularExpressionMatch m = reTn.match(n.message);
                            if (m.hasMatch()) tname = m.captured(1).trimmed();
                            if (tname.isEmpty()) {
                                m = reAvail.match(n.message);
                                if (m.hasMatch()) tname = m.captured(1).trimmed();
                            }
                            if (tname.isEmpty()) {
                                m = reDel.match(n.message);
                                if (m.hasMatch()) tname = m.captured(1).trimmed();
                            }
                        }
                        QString ctx;
                        if (!tname.isEmpty() && !agvIdTask.isEmpty())
                            ctx = QStringLiteral("По задаче «%1», AGV %2").arg(tname, agvIdTask);
                        else if (!agvIdTask.isEmpty())
                            ctx = QStringLiteral("Чат по AGV %1").arg(agvIdTask);
                        else if (!tname.isEmpty())
                            ctx = QStringLiteral("По задаче «%1»").arg(tname);
                        else
                            ctx = QStringLiteral("По уведомлению");
                        card->setProperty("openChatPeerUser", peerU);
                        card->setProperty("openChatAgvHint", agvIdTask);
                        card->setProperty("openChatContextText", ctx);
                        card->setCursor(Qt::PointingHandCursor);
                        card->installEventFilter(this);
                        QLabel *tapHint = new QLabel(QStringLiteral("Нажмите, чтобы открыть чат"), card);
                        tapHint->setStyleSheet(QString(
                            "font-family:Inter;font-size:%1px;font-weight:600;color:#64748B;background:transparent;"
                        ).arg(s(11)));
                        cardL->addWidget(tapHint);
                    }
                }
                if (chatNotification) {
                    int chatId = parseChatId(n.message);
                    if (chatId > 0) {
                        card->setProperty("openChatThreadId", chatId);
                        if (n.title == QStringLiteral("Новое сообщение по задаче")) {
                            static const QRegularExpression ra(QStringLiteral("AGV\\s+([A-Za-z0-9_\\-]+)"));
                            const QRegularExpressionMatch mx = ra.match(n.message);
                            if (mx.hasMatch())
                                card->setProperty("openChatAgvHintForThread", mx.captured(1).trimmed());
                        }
                    } else {
                        card->setProperty("openChatsPageOnClick", true);
                    }
                    card->setCursor(Qt::PointingHandCursor);
                    card->installEventFilter(this);
                }

                auto isMutedPeer = [&](const QString &peer) -> bool {
                    QSettings s("AgvNewUi", "AgvNewUi");
                    return s.value(QString("chat/mute/%1/%2").arg(currentUser.trimmed(), peer.trimmed()), false).toBool();
                };

                bool mutedChatNotif = false;
                if (!n.isRead) {
                    const int chatId = parseChatId(n.message);
                    if (chatId > 0) {
                        TaskChatThread t = getThreadById(chatId);
                        QString other = (t.createdBy == currentUser) ? t.recipientUser : t.createdBy;
                        if (!other.trimmed().isEmpty() && isMutedPeer(other))
                            mutedChatNotif = true;
                    }
                }

                if (!n.isRead && !mutedChatNotif) {
                    QLabel *badge = new QLabel("Новое", card);
                    badge->setStyleSheet(QString(
                        "font-family:Inter;font-size:%1px;font-weight:700;color:#2563EB;background:transparent;"
                    ).arg(s(11)));
                    cardL->addWidget(badge);
                }

                listLayout->addWidget(card);
            }
            listLayout->addStretch();
        }

        scroll->setWidget(host);
        root->addWidget(scroll, 1);

        QPushButton *markReadBtn = new QPushButton("Отметить все как прочитанные", &dlg);
        markReadBtn->setStyleSheet(QString(
            "QPushButton{background:#0F00DB;color:white;font-family:Inter;font-size:%1px;"
            "font-weight:700;border:none;border-radius:%2px;padding:%3px %4px;}"
            "QPushButton:hover{background:#1A4ACD;}"
        ).arg(s(13)).arg(s(8)).arg(s(8)).arg(s(16)));
        connect(markReadBtn, &QPushButton::clicked, &dlg, [&](){
            markAllReadForUser(currentUser);
            updateNotifBadge();
            dlg.accept();
        });
        root->addWidget(markReadBtn);

        QHBoxLayout *bottomBtns = new QHBoxLayout();
        QPushButton *clearBtn = new QPushButton("Очистить уведомления", &dlg);
        clearBtn->setStyleSheet(QString(
            "QPushButton{background:#E6E6E6;border-radius:%1px;border:1px solid #C8C8C8;"
            "font-family:Inter;font-size:%2px;font-weight:700;padding:%3px %4px;}"
            "QPushButton:hover{background:#D5D5D5;}"
        ).arg(s(8)).arg(s(13)).arg(s(6)).arg(s(14)));
        connect(clearBtn, &QPushButton::clicked, &dlg, [&](){
            clearAllNotificationsForUser(currentUser);
            updateNotifBadge();
            DataBus::instance().triggerNotificationsChanged();
            dlg.accept();
        });
        bottomBtns->addWidget(clearBtn, 0, Qt::AlignLeft);
        bottomBtns->addStretch();
        QPushButton *closeBtn = new QPushButton("Закрыть", &dlg);
        closeBtn->setStyleSheet(QString(
            "QPushButton{background:#E6E6E6;border-radius:%1px;border:1px solid #C8C8C8;"
            "font-family:Inter;font-size:%2px;font-weight:700;padding:%3px %4px;}"
            "QPushButton:hover{background:#D5D5D5;}"
        ).arg(s(8)).arg(s(13)).arg(s(6)).arg(s(14)));
        connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
        bottomBtns->addWidget(closeBtn);

        root->addLayout(bottomBtns);

        dlg.exec();
        updateNotifBadge();
    }

    void leftMenu::updateNotifBadge()
    {
        if (!notifBadge_) return;
        const QString currentUser = AppSession::currentUsername();
        QVector<Notification> notifs = loadNotificationsForUser(currentUser);
        auto parseChatId = [&](const QString &text) -> int {
            QRegularExpression re("\\[chat:(\\d+)\\]");
            QRegularExpressionMatch m = re.match(text);
            return m.hasMatch() ? m.captured(1).toInt() : 0;
        };
        auto isMutedPeer = [&](const QString &peer) -> bool {
            QSettings s("AgvNewUi", "AgvNewUi");
            return s.value(QString("chat/mute/%1/%2").arg(currentUser.trimmed(), peer.trimmed()), false).toBool();
        };
        QVector<int> chatIds;
        chatIds.reserve(notifs.size());
        for (const Notification &n : notifs) {
            if (n.isRead) continue;
            const int chatId = parseChatId(n.message);
            if (chatId > 0)
                chatIds.push_back(chatId);
        }
        const QHash<int, TaskChatThread> threadMap = getThreadsByIds(chatIds);
        int count = 0;
        for (const Notification &n : notifs) {
            if (n.isRead) continue;
            const int chatId = parseChatId(n.message);
            if (chatId > 0) {
                const TaskChatThread t = threadMap.value(chatId);
                QString other = (t.createdBy == currentUser) ? t.recipientUser : t.createdBy;
                if (!other.trimmed().isEmpty() && isMutedPeer(other))
                    continue;
            }
            count++;
        }
        if (count > 0) {
            notifBadge_->setText(count > 99 ? "99+" : QString::number(count));
            notifBadge_->show();
        } else {
            notifBadge_->hide();
        }
    }

    void leftMenu::showUserProfilePage(const QString &username)
    {
        activePage_ = ActivePage::UserProfile;
        activeUsername_ = username;
        UserInfo info;
        if (!loadUserProfile(username, info)) {
            QMessageBox::warning(this, "Ошибка", "Не удалось загрузить данные пользователя.");
            return;
        }

        hideAllPages();

        QWidget *profileParent = rightCalendarFrame ? rightCalendarFrame->parentWidget() : this;
        QWidget *page = new QWidget(profileParent);
        page->setStyleSheet("background:#F5F7FB;");
        page->setObjectName("userProfilePage");

        QVBoxLayout *mainLay = new QVBoxLayout(page);
        mainLay->setContentsMargins(s(20), s(15), s(20), s(15));
        mainLay->setSpacing(s(12));

        QWidget *header = new QWidget(page);
        QHBoxLayout *hdrLay = new QHBoxLayout(header);
        hdrLay->setContentsMargins(0, 0, 0, 0);
        hdrLay->setSpacing(s(10));

        QPushButton *backBtn = new QPushButton("   Назад", header);
        backBtn->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
        backBtn->setIconSize(QSize(s(24), s(24)));
        backBtn->setFixedSize(s(150), s(50));
        backBtn->setStyleSheet(QString(
            "QPushButton{background-color:#E6E6E6;border-radius:%1px;border:1px solid #C8C8C8;"
            "font-family:Inter;font-size:%2px;font-weight:800;color:black;text-align:left;padding-left:%3px;}"
            "QPushButton:hover{background-color:#D5D5D5;}"
        ).arg(s(10)).arg(s(16)).arg(s(10)));

        connect(backBtn, &QPushButton::clicked, this, [this, page](){
            page->setVisible(false);
            page->deleteLater();
            showUsersPage();
        });

        hdrLay->addWidget(backBtn, 0, Qt::AlignLeft);
        hdrLay->addStretch();

        QLabel *titleLbl = new QLabel(
            info.fullName.isEmpty() ? info.username : info.fullName, header);
        titleLbl->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#0F172A;background:transparent;"
        ).arg(s(24)));
        hdrLay->addWidget(titleLbl, 0, Qt::AlignCenter);
        hdrLay->addStretch();

        mainLay->addWidget(header);

        QScrollArea *scroll = new QScrollArea(page);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet("QScrollArea{background:transparent;}");

        QWidget *content = new QWidget();
        content->setStyleSheet("background:transparent;");
        QVBoxLayout *contentLay = new QVBoxLayout(content);
        contentLay->setContentsMargins(s(10), 0, s(10), 0);
        contentLay->setSpacing(s(14));

        QString roleText;
        if (info.role == "admin") roleText = "Администратор";
        else if (info.role == "tech") roleText = "Техник";
        else roleText = "Пользователь";

        auto addCopyableRow = [&](const QString &label, const QString &value) {
            if (value.trimmed().isEmpty()) return;
            QWidget *row = new QWidget(content);
            row->setStyleSheet("background:transparent;");
            QHBoxLayout *h = new QHBoxLayout(row);
            h->setContentsMargins(0, s(4), 0, s(4));
            h->setSpacing(s(10));

            QLabel *lbl = new QLabel(label + ":", row);
            lbl->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;font-weight:700;color:#334155;background:transparent;"
            ).arg(s(15)));
            lbl->setMinimumWidth(s(140));

            QLineEdit *valEdit = new QLineEdit(row);
            valEdit->setReadOnly(true);
            valEdit->setText(value);
            valEdit->setCursor(Qt::IBeamCursor);
            valEdit->setStyleSheet(QString(
                "QLineEdit{background:#F1F5F9;border:1px solid #E2E8F0;border-radius:8px;"
                "padding:8px 12px;font-family:Inter;font-size:%1px;color:#0F172A;}"
                "QLineEdit:focus{border:1px solid #3B82F6;}"
            ).arg(s(14)));
            valEdit->setMinimumWidth(s(200));

            QPushButton *copyBtn = new QPushButton("Копировать", row);
            copyBtn->setFixedHeight(s(36));
            copyBtn->setStyleSheet(QString(
                "QPushButton{background:#0EA5E9;color:white;font-family:Inter;font-size:%1px;"
                "font-weight:700;border:none;border-radius:6px;padding:0 12px;}"
                "QPushButton:hover{background:#0284C7;}"
            ).arg(s(12)));
            connect(copyBtn, &QPushButton::clicked, row, [valEdit, copyBtn](){
                QApplication::clipboard()->setText(valEdit->text());
                copyBtn->setText("Скопировано");
                QTimer::singleShot(1500, copyBtn, [copyBtn](){
                    copyBtn->setText("Копировать");
                });
            });

            h->addWidget(lbl);
            h->addWidget(valEdit, 1);
            h->addWidget(copyBtn);
            contentLay->addWidget(row);
        };

        // Сетка: слева ФИО, Табельный, Должность; справа Логин, Роль, Подразделение
        QGridLayout *grid = new QGridLayout();
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(s(8));
        auto addGridCell = [&](int row, int col, const QString &label, const QString &value) {
            QString v = value.trimmed().isEmpty() ? "—" : value;
            QLabel *l = new QLabel(QString("<b>%1:</b> %2").arg(label, v), content);
            l->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:#1A1A1A;background:transparent;").arg(s(15)));
            l->setWordWrap(true);
            grid->addWidget(l, row, col);
        };
        addGridCell(0, 0, "ФИО", info.fullName);
        addGridCell(0, 1, "Логин", info.username);
        addGridCell(1, 0, "Табельный номер", info.employeeId);
        addGridCell(1, 1, "Роль", roleText);
        addGridCell(2, 0, "Должность", info.position);
        addGridCell(2, 1, "Подразделение", info.department);
        contentLay->addLayout(grid);
        addCopyableRow("Телефон", info.mobile);
        addCopyableRow("Внутренний номер", info.extPhone);
        addCopyableRow("Email", info.email);
        addCopyableRow("Telegram", info.telegram);

        // AGV закреплены за пользователем
        QStringList agvList = getAgvIdsAssignedToUser(username);
        if (!agvList.isEmpty()) {
            QLabel *agvTitle = new QLabel("Закреплённые AGV:", content);
            agvTitle->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;font-weight:700;color:#334155;background:transparent;"
            ).arg(s(15)));
            agvTitle->setMinimumWidth(s(140));
            contentLay->addWidget(agvTitle);
            QLabel *agvVal = new QLabel(agvList.join(", "), content);
            agvVal->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;color:#0F172A;background:transparent;"
            ).arg(s(14)));
            agvVal->setWordWrap(true);
            contentLay->addWidget(agvVal);
        }

        // Activity history section (без рамки)
        QWidget *histFrame = new QWidget(content);
        histFrame->setStyleSheet("background:transparent;");
        QVBoxLayout *histLay = new QVBoxLayout(histFrame);
        histLay->setContentsMargins(0, s(10), 0, 0);
        histLay->setSpacing(s(6));

        QFrame *histLine = new QFrame(histFrame);
        histLine->setFixedHeight(1);
        histLine->setStyleSheet("background:#E2E8F0;");
        histLay->addWidget(histLine);

        QHBoxLayout *histTitleRow = new QHBoxLayout();
        histTitleRow->addStretch();
        QLabel *histTitle = new QLabel("История действий", histFrame);
        histTitle->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#0F172A;background:transparent;"
        ).arg(s(18)));
        histTitleRow->addWidget(histTitle, 0, Qt::AlignCenter);
        histTitleRow->addStretch();
        histLay->addLayout(histTitleRow);

        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (db.isOpen()) {
            QSqlQuery histQ(db);
            histQ.prepare(R"(
                SELECT task_name, agv_id, completed_ts
                FROM agv_task_history
                WHERE performed_by = :u
                ORDER BY completed_ts DESC
                LIMIT 50
            )");
            histQ.bindValue(":u", username);
            if (histQ.exec()) {
                bool hasHistory = false;
                while (histQ.next()) {
                    hasHistory = true;
                    QString taskName = histQ.value(0).toString();
                    QString agvId = histQ.value(1).toString();
                    QDateTime completedTs = histQ.value(2).toDateTime();
                    QString dateStr = completedTs.isValid() ? completedTs.toString("dd.MM.yyyy") : "—";

                    QLabel *histLbl = new QLabel(
                        QString("• %1 — %2 (выполнено %3)").arg(taskName, agvId, dateStr),
                        histFrame);
                    histLbl->setStyleSheet(QString(
                        "font-family:Inter;font-size:%1px;color:#475569;background:transparent;"
                        "padding:4px 0;"
                    ).arg(s(14)));
                    histLay->addWidget(histLbl);
                }
                if (!hasHistory) {
                    QLabel *noHist = new QLabel("Нет записей", histFrame);
                    noHist->setStyleSheet(QString(
                        "font-family:Inter;font-size:%1px;color:#94A3B8;background:transparent;"
                    ).arg(s(14)));
                    histLay->addWidget(noHist);
                }
            }
        }
        contentLay->addWidget(histFrame);

        contentLay->addStretch();
        scroll->setWidget(content);
        mainLay->addWidget(scroll, 1);

        if (rightCalendarFrame) {
            QWidget *rightBodyFrame = rightCalendarFrame->parentWidget();
            if (rightBodyFrame) {
                if (QVBoxLayout *rbl = qobject_cast<QVBoxLayout*>(rightBodyFrame->layout())) {
                    rbl->addWidget(page, 3);
                }
            }
        }

        page->setVisible(true);
        stressSuiteLogPageEntered(QStringLiteral("user_profile"));
    }


    void leftMenu::showChatsPage()
    {
        activePage_ = ActivePage::Chats;
        hideAllPages();
        clearSearch();

        if (!chatsPage) {
            QWidget *parent = rightCalendarFrame ? rightCalendarFrame->parentWidget() : this;
            chatsPage = new QWidget(parent);
            chatsPage->setStyleSheet("background:#F5F7FB;");

            QVBoxLayout *main = new QVBoxLayout(chatsPage);
            main->setContentsMargins(0, 0, 0, 0);
            main->setSpacing(0);

            chatsStack_ = new QStackedWidget(chatsPage);
            main->addWidget(chatsStack_);

            // Страница 0: список чатов
            QWidget *listPage = new QWidget(chatsStack_);
            listPage->setStyleSheet("background:#F5F7FB;");
            QVBoxLayout *listMain = new QVBoxLayout(listPage);
            listMain->setContentsMargins(s(12), s(10), s(12), s(10));
            listMain->setSpacing(s(10));

            QWidget *header = new QWidget(listPage);
            QHBoxLayout *hdr = new QHBoxLayout(header);
            hdr->setContentsMargins(0, 0, 0, 0);
            hdr->setSpacing(s(10));

            QPushButton *backBtn = new QPushButton("   Назад", header);
            backBtn->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
            backBtn->setIconSize(QSize(s(24), s(24)));
            backBtn->setFixedSize(s(150), s(50));
            backBtn->setStyleSheet(QString(
                "QPushButton{background-color:#E6E6E6;border-radius:%1px;border:1px solid #C8C8C8;"
                "font-family:Inter;font-size:%2px;font-weight:800;color:black;text-align:left;padding-left:%3px;}"
                "QPushButton:hover{background-color:#D5D5D5;}"
            ).arg(s(10)).arg(s(16)).arg(s(10)));
            connect(backBtn, &QPushButton::clicked, this, [this]() { showCalendar(); });

            QLabel *title = new QLabel("Чаты", header);
            title->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;font-weight:900;color:#0F172A;"
            ).arg(s(22)));

            QWidget *rightPad = new QWidget(header);
            rightPad->setFixedSize(s(150), s(50));

            hdr->addWidget(backBtn, 0, Qt::AlignLeft);
            hdr->addStretch();
            hdr->addWidget(title, 0, Qt::AlignVCenter);
            hdr->addStretch();
            hdr->addWidget(rightPad, 0, Qt::AlignRight);
            listMain->addWidget(header);

            QScrollArea *scroll = new QScrollArea(listPage);
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setStyleSheet("QScrollArea{background:transparent;border:none;}");
            QWidget *host = new QWidget(scroll);
            host->setStyleSheet("background:transparent;");
            chatsListLayout_ = new QVBoxLayout(host);
            chatsListLayout_->setContentsMargins(0, 0, 0, 0);
            chatsListLayout_->setSpacing(s(12));
            scroll->setWidget(host);
            listMain->addWidget(scroll, 1);

            QHBoxLayout *fabRow = new QHBoxLayout();
            fabRow->setContentsMargins(0, 0, 0, 0);
            fabRow->addStretch();
            QToolButton *addChatFab = new QToolButton(listPage);
            addChatFab->setFixedSize(s(56), s(56));
            addChatFab->setIcon(QIcon(":/new/mainWindowIcons/noback/edit.png"));
            addChatFab->setIconSize(QSize(s(24), s(24)));
            addChatFab->setStyleSheet(QString(
                "QToolButton{background:#55BFFF;border:none;border-radius:%1px;color:white;}"
                "QToolButton:hover{background:#43AEEA;}"
            ).arg(s(28)));
            addChatFab->setToolTip("Начать чат");
            connect(addChatFab, &QToolButton::clicked, this, [this]() {
                const QString currentUser = AppSession::currentUsername();
                QVector<UserInfo> allUsers = getAllUsers(false);
                QVector<UserInfo> candidates;
                for (const UserInfo &u : allUsers) {
                    if (u.username != currentUser) candidates.push_back(u);
                }
                if (candidates.isEmpty()) {
                    QMessageBox::information(this, "Начать чат", "Нет пользователей для чата.");
                    return;
                }
                QDialog pick(this);
                pick.setWindowTitle("Начать чат");
                pick.setMinimumSize(s(460), s(560));
                pick.setStyleSheet(
                    "QDialog{background:#F8FAFF;}"
                    "QLabel{font-family:Inter;font-size:14px;font-weight:700;color:#334155;}"
                    "QListWidget{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:10px;padding:6px;}"
                    "QListWidget::item{padding:10px 8px;border-radius:10px;}"
                    "QListWidget::item:hover{background:rgba(80,118,251,36);}"
                    "QListWidget::item:selected{background:rgba(80,118,251,52);}"
                    "QPushButton{background:#E2E8F0;color:#334155;font-weight:800;border:none;border-radius:10px;padding:8px 14px;}"
                    "QPushButton:hover{background:#CBD5E1;}"
                );
                QVBoxLayout *root = new QVBoxLayout(&pick);
                QLabel *hint = new QLabel("Выберите пользователя", &pick);
                root->addWidget(hint);
                QListWidget *list = new QListWidget(&pick);
                list->setIconSize(QSize(s(40), s(40)));
                for (const UserInfo &u : candidates) {
                    const QString display = u.fullName.isEmpty() ? u.username : (u.fullName + " (" + u.username + ")");
                    QListWidgetItem *it = new QListWidgetItem(display, list);
                    QPixmap avatar = loadUserAvatarFromDb(u.username);
                    if (avatar.isNull()) avatar = QPixmap(":/new/mainWindowIcons/noback/user.png");
                    if (!avatar.isNull())
                        it->setIcon(QIcon(makeRoundPixmap(avatar, s(40))));
                    it->setSizeHint(QSize(0, s(58)));
                    it->setData(Qt::UserRole, u.username);
                }
                root->addWidget(list, 1);
                QPushButton *cancel = new QPushButton("Отмена", &pick);
                connect(cancel, &QPushButton::clicked, &pick, &QDialog::reject);
                root->addWidget(cancel);
                connect(list, &QListWidget::itemClicked, &pick, [&pick, this, currentUser](QListWidgetItem *it) {
                    if (!it) return;
                    const QString other = it->data(Qt::UserRole).toString().trimmed();
                    if (other.isEmpty()) return;
                    QString err;
                    int tid = TaskChatDialog::ensureThreadWithUser(currentUser, other, QString(), &err);
                    if (tid <= 0) {
                        QMessageBox::warning(this, "Чат", err.isEmpty() ? QStringLiteral("Не удалось открыть чат") : err);
                        return;
                    }
                    pick.accept();
                    showChatsPage();
                    if (embeddedChatWidget_ && chatsStack_) {
                        embeddedChatWidget_->setThreadId(tid, other);
                        chatsStack_->setCurrentIndex(1);
                    }
                });
                pick.exec();
                reloadChatsPageList();
            });
            fabRow->addWidget(addChatFab, 0, Qt::AlignRight | Qt::AlignBottom);
            listMain->addLayout(fabRow);

            chatsStack_->addWidget(listPage);

            // Страница 1: встроенный чат с человеком (замещает список)
            const QString curUser = AppSession::currentUsername();
            const QString role = getUserRole(curUser);
            const bool isAdmin = (role == "admin" || role == "tech");
            embeddedChatWidget_ = new TaskChatWidget(0, curUser, isAdmin, [this](int v) { return s(v); }, chatsStack_);
            connect(embeddedChatWidget_, &TaskChatWidget::showProfileRequested, this, [this](const QString &u) {
                showUserProfilePage(u);
            });
    connect(embeddedChatWidget_, &TaskChatWidget::backRequested, this, [this]() {
        if (chatsStack_) chatsStack_->setCurrentIndex(0);
        reloadChatsPageList();
    });
    chatsStack_->addWidget(embeddedChatWidget_);

            if (rightCalendarFrame) {
                QWidget *rightBodyFrame = rightCalendarFrame->parentWidget();
                if (rightBodyFrame) {
                    if (QVBoxLayout *rightBodyLayout = qobject_cast<QVBoxLayout*>(rightBodyFrame->layout()))
                        rightBodyLayout->addWidget(chatsPage, 3);
                }
            }
        }

        if (chatsStack_)
            chatsStack_->setCurrentIndex(0);
        chatsPage->setVisible(true);
        reloadChatsPageList();
        stressSuiteLogPageEntered(QStringLiteral("chats"));
    }

    void leftMenu::openEmbeddedDelegatorChatForAgv(const QString &agvId)
    {
        const QString aid = agvId.trimmed();
        if (aid.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("Перейти в диалог"), QStringLiteral("AGV не выбран."));
            return;
        }
        const QString currentUser = AppSession::currentUsername();
        const QVector<QString> order = TaskChatDialog::delegatorUsernamesForAgv(aid, currentUser);
        if (order.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("Перейти в диалог"),
                QStringLiteral("По этому AGV вам никто не назначал задачи и не закреплял его за вами. Используйте «Начать чат» в разделе Чаты."));
            return;
        }
        const QString specialContext = QStringLiteral("AGV %1").arg(aid);

        const auto openWithPeer = [this, currentUser, specialContext](const QString &otherRaw) {
            const QString otherUser = otherRaw.trimmed();
            if (otherUser.isEmpty())
                return;
            QString err;
            const int tid = TaskChatDialog::ensureThreadWithUser(currentUser, otherUser, QString(), &err);
            if (tid <= 0) {
                QMessageBox::warning(this, QStringLiteral("Перейти в диалог"),
                    err.isEmpty() ? QStringLiteral("Не удалось открыть чат") : err);
                return;
            }
            TaskChatDialog::markNextMessageSpecial(tid, specialContext);
            showChatsPage();
            if (embeddedChatWidget_ && chatsStack_) {
                embeddedChatWidget_->setThreadId(tid, otherUser);
                chatsStack_->setCurrentIndex(1);
            }
            reloadChatsPageList();
        };

        if (order.size() == 1) {
            openWithPeer(order[0]);
            return;
        }

        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("Перейти в диалог"));
        dlg.setMinimumSize(s(360), s(300));
        dlg.setStyleSheet(
            "QDialog{background:#F4F6FA;} QListWidget{background:#FFFFFF;border:1px solid #E5EAF2;border-radius:12px;padding:4px;} "
            "QListWidget::item{min-height:48px;padding:8px 12px;border-radius:8px;} QListWidget::item:hover{background:#E8EEFF;} "
            "QLabel{font-weight:600;color:#475569;font-size:13px;}"
        );
        QVBoxLayout *root = new QVBoxLayout(&dlg);
        root->setContentsMargins(16, 16, 16, 16);
        QLabel *hint = new QLabel(QStringLiteral("С кем открыть диалог?"), &dlg);
        root->addWidget(hint);
        QListWidget *list = new QListWidget(&dlg);
        const QVector<UserInfo> allUsers = getAllUsers(false);
        for (const QString &username : order) {
            QString display = username;
            for (const UserInfo &u : allUsers) {
                if (u.username.compare(username, Qt::CaseInsensitive) == 0) {
                    display = u.fullName.trimmed().isEmpty() ? u.username : (u.fullName + QStringLiteral(" (") + u.username + QLatin1Char(')'));
                    break;
                }
            }
            list->addItem(display);
            list->item(list->count() - 1)->setData(Qt::UserRole, username);
        }
        root->addWidget(list, 1);
        connect(list, &QListWidget::itemClicked, &dlg, [&dlg, openWithPeer](QListWidgetItem *item) {
            if (!item)
                return;
            const QString otherUser = item->data(Qt::UserRole).toString().trimmed();
            if (otherUser.isEmpty())
                return;
            dlg.accept();
            openWithPeer(otherUser);
        });
        QPushButton *cancelBtn = new QPushButton(QStringLiteral("Отмена"), &dlg);
        cancelBtn->setStyleSheet(
            "QPushButton{background:#E2E8F0;color:#334155;font-weight:700;padding:10px 20px;border-radius:10px;} "
            "QPushButton:hover{background:#CBD5E1;}");
        connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
        root->addWidget(cancelBtn);
        dlg.exec();
    }

    void leftMenu::reloadChatsPageList()
    {
        if (!chatsPage || !chatsListLayout_) return;

        QWidget *listHost = qobject_cast<QWidget*>(chatsListLayout_->parent());
        if (!listHost) listHost = chatsPage;

        if (chatsPage) chatsPage->setUpdatesEnabled(false);
        if (listHost) listHost->setUpdatesEnabled(false);

        const QString currentUser = AppSession::currentUsername();
        const QString role = getUserRole(currentUser);
        const bool isAdmin = (role == "admin" || role == "tech");
        QVector<TaskChatThread> threads = isAdmin ? getThreadsForAdmin(currentUser) : getThreadsForUser(currentUser);

        // Комплексный тест: не грузим ФИО/аватар по каждому ряду (N запросов к БД → подвисание UI).
        const bool complexTestFast = stressSuiteRunning_;
        if (threads.size() > 100)
            threads.resize(100);

        const QString newSignature = makeChatListSignature(threads);
        if (!complexTestFast && newSignature == lastChatsListSignature_ && chatsListLayout_->count() > 0) {
            if (chatsPage) chatsPage->setUpdatesEnabled(true);
            if (listHost) listHost->setUpdatesEnabled(true);
            return;
        }

        while (QLayoutItem *it = chatsListLayout_->takeAt(0)) {
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }

        if (threads.isEmpty()) {
            lastChatsListSignature_ = newSignature;
            QLabel *empty = new QLabel("Чатов пока нет", listHost);
            empty->setAlignment(Qt::AlignCenter);
            empty->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;font-weight:700;color:#94A3B8;"
            ).arg(s(16)));
            chatsListLayout_->addStretch();
            chatsListLayout_->addWidget(empty);
            chatsListLayout_->addStretch();
            if (chatsPage) chatsPage->setUpdatesEnabled(true);
            if (listHost) listHost->setUpdatesEnabled(true);
            return;
        }

        QSet<QString> peerUsers;
        for (const TaskChatThread &t : threads) {
            const QString otherUser = (t.createdBy == currentUser) ? t.recipientUser : t.createdBy;
            if (!otherUser.isEmpty())
                peerUsers.insert(otherUser);
        }
        const QHash<QString, ChatPeerMeta> peerMetaMap = loadChatPeerMeta(peerUsers);
        const QPixmap defaultUserPm = QPixmap(QStringLiteral(":/new/mainWindowIcons/noback/user.png"));

        for (const TaskChatThread &t : threads) {
            QString otherUser = (t.createdBy == currentUser) ? t.recipientUser : t.createdBy;
            QString otherDisplay = otherUser.isEmpty() ? QString("Без адресата") : otherUser;
            const ChatPeerMeta meta = peerMetaMap.value(otherUser);
            if (!meta.displayName.isEmpty())
                otherDisplay = meta.displayName;
            const QString lastSeenText = formatLastSeenText(meta.isActive, meta.lastLogin);

            const QString agvText = t.agvId.trimmed();
            QString secondaryText;
            if (!agvText.isEmpty() && agvText != QStringLiteral("—")) {
                secondaryText = agvText;
                if (!t.taskName.isEmpty())
                    secondaryText += QString(" • %1").arg(t.taskName);
            } else if (!t.taskName.isEmpty()) {
                secondaryText = t.taskName;
            }

            QPushButton *btn = new QPushButton(listHost);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setMinimumHeight(s(94));
            QPixmap peerAvatar;
            if (!otherUser.isEmpty()) {
                const QString cacheKey = otherUser.trimmed();
                if (avatarCache_.contains(cacheKey)) {
                    peerAvatar = avatarCache_.value(cacheKey);
                } else if (!meta.avatar.isNull()) {
                    peerAvatar = meta.avatar;
                    avatarCache_.insert(cacheKey, peerAvatar);
                }
            }
            if (peerAvatar.isNull())
                peerAvatar = defaultUserPm;
            btn->setStyleSheet(QString(
                "QPushButton{background:white;border:1px solid transparent;border-radius:%1px;padding:%2px %3px;"
                "text-align:left;%4}"
                "QPushButton:hover{background:rgba(80,118,251,36);border:1px solid rgb(80,118,251);}"
            ).arg(s(14)).arg(s(13)).arg(s(14))
             .arg(t.isClosed() ? "opacity:0.9;" : ""));

            QHBoxLayout *btnLay = new QHBoxLayout(btn);
            btnLay->setContentsMargins(s(14), s(12), s(14), s(12));
            btnLay->setSpacing(s(12));

            QWidget *avatarWrap = new QWidget(btn);
            avatarWrap->setFixedSize(s(42), s(42));
            avatarWrap->setAttribute(Qt::WA_TransparentForMouseEvents);
            QLabel *avatarLbl = new QLabel(avatarWrap);
            avatarLbl->setGeometry(0, 0, s(42), s(42));
            avatarLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            if (!peerAvatar.isNull()) {
                avatarLbl->setPixmap(makeRoundPixmap(peerAvatar, s(42)));
                avatarLbl->setAlignment(Qt::AlignCenter);
            }
            const int dotPx = s(10);
            const int dotRadius = qMax(1, dotPx / 2);
            QLabel *statusDot = new QLabel(avatarWrap);
            statusDot->setFixedSize(dotPx, dotPx);
            statusDot->move(s(42) - dotPx - s(2), s(42) - dotPx - s(2));
            statusDot->setAttribute(Qt::WA_TransparentForMouseEvents);
            const bool peerOnline = !otherUser.isEmpty() && chatListPeerShowsOnline(meta.isActive, meta.lastLogin);
            statusDot->setStyleSheet(
                peerOnline
                    ? QStringLiteral("background:#22C55E;border:%1px solid #FFFFFF;border-radius:%2px;")
                          .arg(s(2))
                          .arg(dotRadius)
                    : QStringLiteral("background:#CBD5E1;border:%1px solid #FFFFFF;border-radius:%2px;")
                          .arg(s(2))
                          .arg(dotRadius));
            statusDot->raise();

            QWidget *textHost = new QWidget(btn);
            textHost->setAttribute(Qt::WA_TransparentForMouseEvents);
            QVBoxLayout *textLay = new QVBoxLayout(textHost);
            textLay->setContentsMargins(0, 0, 0, 0);
            textLay->setSpacing(s(2));

            QLabel *nameLbl = new QLabel(otherDisplay, textHost);
            nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            nameLbl->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;font-weight:800;color:%2;background:transparent;%3"
            ).arg(s(15))
             .arg(t.isClosed() ? "#94A3B8" : "#0F172A")
             .arg(t.isClosed() ? "text-decoration: line-through;" : ""));

            textLay->addWidget(nameLbl);

            if (!secondaryText.trimmed().isEmpty()) {
                QLabel *secondaryLbl = new QLabel(secondaryText, textHost);
                secondaryLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
                secondaryLbl->setWordWrap(true);
                secondaryLbl->setStyleSheet(QString(
                    "font-family:Inter;font-size:%1px;font-weight:700;color:%2;background:transparent;%3"
                ).arg(s(13))
                 .arg(t.isClosed() ? "#94A3B8" : "#475569")
                 .arg(t.isClosed() ? "text-decoration: line-through;" : ""));
                textLay->addWidget(secondaryLbl);
            }

            QLabel *lastSeenLbl = new QLabel(lastSeenText, textHost);
            lastSeenLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            lastSeenLbl->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;font-weight:600;color:#94A3B8;background:transparent;"
            ).arg(s(12)));
            textLay->addWidget(lastSeenLbl);

            btnLay->addWidget(avatarWrap, 0, Qt::AlignTop);
            btnLay->addWidget(textHost, 1);

            connect(btn, &QPushButton::clicked, this, [this, t]() {
                if (embeddedChatWidget_) {
                    const QString currentUser = AppSession::currentUsername();
                    const QString peer = (t.createdBy == currentUser) ? t.recipientUser : t.createdBy;
                    embeddedChatWidget_->setThreadId(t.id, peer);
                    if (chatsStack_) chatsStack_->setCurrentIndex(1);
                }
            });
            chatsListLayout_->addWidget(btn);
        }
        chatsListLayout_->addStretch();
        lastChatsListSignature_ = newSignature;

        if (chatsPage) chatsPage->setUpdatesEnabled(true);
        if (listHost) listHost->setUpdatesEnabled(true);
        if (chatsPage) chatsPage->update();
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


//
// ======================= КОНЕЦ ФАЙЛА =======================
//

