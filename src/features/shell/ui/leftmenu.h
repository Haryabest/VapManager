#pragma once
#include "userspage.h"
#include <QWidget>
#include <QDate>
#include <QVector>
#include <QPixmap>
#include <QTableWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QScrollArea>
#include <QSizePolicy>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDebug>
#include <QHeaderView>
#include <QAbstractItemDelegate>
#include <QStyledItemDelegate>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTimer>
#include <QElapsedTimer>
#include <QClipboard>
#include <QFile>
#include <QFormLayout>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSettings>
#include <QTextStream>
#include <QBuffer>
#include <QMessageBox>
#include <QFileDialog>
#include <QStackedWidget>
#include <QHash>

#include "listagvinfo.h"
#include "agvsettingspage.h"
#include "multisectionwidget.h"
#include "maintenanceitemwidget.h"
#include "notifications_logs.h"
#include "modellistpage.h"

class TaskChatWidget;

// ★ Calendar event
struct CalendarEvent {
    QString agvId;
    QString taskTitle;
    QDate date;
    QString severity;
};

// ★ Maintenance item
struct MaintenanceItemData {
    QString agvId;
    QString agvName;
    QString type;
    QDate   date;
    QString details;
    QString severity;
    QString assignedInfo;   // "за X", "делегирована X" или "общая"
    QString assignedUser;  // username для уведомлений (если закреплён)
    bool isDelegatedToMe = false;  // true = разово делегировано (assigned_to=я, AGV за кем-то другим)
};

// ★ System status
struct SystemStatus {
    int active;
    int maintenance;
    int error;
    int disabled;
};

class leftMenu : public QWidget
{
    Q_OBJECT
public:
    explicit leftMenu(QWidget *parent = nullptr);

    int s(int v) const;
    void setScaleFactor(qreal factor);
    void requestScaleFactorUpdate(qreal factor);

    /// Стресс пересборки календаря (роль tech). showSummaryDialog=false — без окна (внутри полного автотеста).
    /// manageStressButtons=false — не трогать кнопки (когда запускает runFullStressAutotest).
    void runCalendarStressTest(int iterations = 500, bool showSummaryDialog = true, bool manageStressButtons = true);
    /// Комплексный тест (БД, UI, ввод, DataBus): всегда доходит до конца с отчётом PASS/SKIP; жёсткий лимит времени.
    void runFullStressAutotest();

signals:
    void addAgvRequested();
    void openAgvTaskRequested(const QString &agvId, const QString &task);
    void openUsersRequested();


protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class ActivePage {
        Calendar,
        AgvList,
        AgvDetails,
        ModelList,
        Logs,
        Profile,
        Chats,
        Users,
        UserProfile
    };

    void initUI();
    void restoreActivePage();
    void showAgvList();
    void showCalendar();
    void showAgvDetailInfo(const QString &agvId);
    void showModelList();
    void showLogs();
    void reloadLogs(int maxRows = 2000);
    void showProfile();
    void buildProfilePage();
    void showProfilePage();
    void hideAllPages();


    void showUsersPage();
    void showAnnualReportDialog();
    void showNotificationsPanel();
    void showChatsPage();
    void openEmbeddedDelegatorChatForAgv(const QString &agvId);
    void reloadChatsPageList();
    void onSearchTextChanged(const QString &text);
    void clearSearch();
    void showUserProfilePage(const QString &username);
    void updateNotifBadge();

    void setTechStressButtonsEnabled(bool enabled);

    void stressSuiteTick();
    void scheduleStressSuiteStep(int delayMs);
    void stressSuiteRecordCheck(const QString &name, bool pass, qint64 ms);
    void stressSuiteFinishAlwaysOk();
    /// В расширенный отчёт автотеста: время между входами на экраны (после предыдущего show*).
    void stressSuiteLogPageEntered(const QString &pageId);

    void changeMonth(int delta);
    void setSelectedMonthYear(int month, int year);
    void selectDay(int year, int month, int day);
    void refreshCalendarSelectionVisuals();
    QString monthYearLabelText(int month, int year) const;

    QPixmap makeRoundPixmap(const QPixmap &src, int size);
    QPixmap loadUserAvatarFromDb(const QString &userId);

    QVector<CalendarEvent> loadCalendarEvents(int month, int year);
    QVector<CalendarEvent> loadCalendarEventsRange(const QDate &from, const QDate &to);
    QVector<MaintenanceItemData> loadUpcomingMaintenance(int month, int year);
    SystemStatus loadSystemStatus();

    void updateAgvCounter();
    void updateUpcomingMaintenance();
    void updateSystemStatus();

private slots:
    void changeAvatar();

private:
    /// Сброс подсветки дня/недели из «Настройки календаря» (таймер и флаги).
    void clearCalendarSettingsHighlight();

    qreal scaleFactor_ = 1.0;
    qreal pendingScaleFactor_ = 1.0;
    QTimer *scaleUpdateTimer_ = nullptr;

    int selectedMonth_ = QDate::currentDate().month();
    int selectedYear_  = QDate::currentDate().year();
    QDate selectedDay_;
    int selectedWeek_ = 0;
    bool highlightWeek_ = false;

    QWidget *topRow_ = nullptr;
    QWidget *bottomRow_ = nullptr;

    QWidget *rightCalendarFrame = nullptr;
    QWidget *rightUpcomingMaintenanceFrame = nullptr;

    ListAgvInfo *listAgvInfo = nullptr;
    AgvSettingsPage *agvSettingsPage = nullptr;
    ModelListPage *modelListPage = nullptr;
    QWidget *logsPage = nullptr;
    QTableWidget *logsTable = nullptr;
    QPushButton *logsLoadAllBtn = nullptr;
    QPushButton *logsExportBtn = nullptr;
    QPushButton *calendarStressTestBtn_ = nullptr;
    QPushButton *fullStressAutotestBtn_ = nullptr;
    QTextEdit *techDiagLogEdit_ = nullptr;
    QWidget *profilePage = nullptr;
    QWidget *chatsPage = nullptr;
    QStackedWidget *chatsStack_ = nullptr;
    TaskChatWidget *embeddedChatWidget_ = nullptr;
    QVBoxLayout *chatsListLayout_ = nullptr;
    QTimer *profileKeyTimer = nullptr;

    QLabel *monthLabel = nullptr;
    QWidget *calendarActionsFrame = nullptr;

    QPushButton *backButton = nullptr;

    QTableWidget *calendarTablePtr = nullptr;

    QLabel *agvCounter = nullptr;
    MultiSectionWidget *statusWidget_ = nullptr;

    QTimer *agvCounterTimer = nullptr;
    QTimer *maintenanceTimer = nullptr;
    QTimer *calendarHighlightTimer = nullptr;
    QTimer *notifPollTimer = nullptr;
    QTimer *chatsPollTimer = nullptr;

    bool calendarHighlightActive_ = false;
    /// Подавляет подробный CALENDAR-лог при массовом стресс-тесте (оставляются только строки STRESS).
    bool calendarStressDiagQuiet_ = false;
    bool pendingCalendarReload_ = false;
    bool agvListDirty_ = true;
    ActivePage activePage_ = ActivePage::Calendar;
    QString activeAgvId_;
    QString activeUsername_;

    QToolButton *userButton = nullptr;
    QLineEdit *searchEdit_ = nullptr;
    QLabel *notifBadge_ = nullptr;
    QHash<QString, QPixmap> avatarCache_;
    int lastUnreadChatNotifCount_ = -1;
    int lastUnreadAnyNotifCount_ = -1;

    QComboBox *logFilterUser_ = nullptr;
    QComboBox *logFilterSource_ = nullptr;
    QComboBox *logFilterCategory_ = nullptr;
    QComboBox *logFilterTime_ = nullptr;
    bool reloadingLogs_ = false;
    int lastLogsMaxRows_ = 2000;
    QElapsedTimer lastLogsReloadTimer_;
    QString lastChatsListSignature_;

    bool stressSuiteRunning_ = false;
    int stressSuitePhase_ = 0;
    QString stressSuiteReportPath_;
    QElapsedTimer stressSuiteTotalTimer_;
    int stressSuitePassCount_ = 0;
    int stressSuiteSkipCount_ = 0;
    int stressSuiteInner_ = 0;
    ActivePage stressSuiteSavedActivePage_ = ActivePage::Calendar;
    int stressSuiteSelY_ = 0;
    int stressSuiteSelM_ = 0;
    QElapsedTimer stressSuiteStepTimer_;
    QString stressSuiteChatPeer_;
    int stressSuiteChatThreadId_ = 0;
    QVector<int> stressSuiteOrder_;
    QVector<int> stressSuiteRandomPickDays_;

    QElapsedTimer stressNavTimer_;
    QString stressNavLastLabel_;

    UsersPage *usersPage = nullptr;
};
