#pragma once

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
#include <QPixmap>
#include <QPushButton>
#include <QLineEdit>
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
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemDelegate>
#include <QStyledItemDelegate>

#include "listagvinfo.h"
#include "agvdetailinfo.h"
#include "agvsettingspage.h"
#include "multisectionwidget.h"
#include "maintenanceitemwidget.h"
#include "notifications_logs.h"
#include "modellistpage.h"

// Больше никаких глобальных списков!

// Структуры данных
struct CalendarEvent {
    QString agvId;
    QString taskTitle;
    QDate date;
};

struct MaintenanceItemData {
    QString agvId;
    QString type;
    QDate date;
    QString details;
    QString severity;
};

struct SystemStatus {
    int active;
    int maintenance;
    int error;
    int disabled;
};

// Главный класс левого меню
class leftMenu : public QWidget
{
    Q_OBJECT
public:
    explicit leftMenu(QWidget *parent = nullptr);

    int s(int v) const;
    void setScaleFactor(qreal factor);

signals:
    void addAgvRequested();
    void openAgvTaskRequested(const QString &agvId, const QString &task);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void initUI();
    void showAgvList();
    void showCalendar();
    void showAgvDetailInfo(const QString &agvId);
    void showModelList();

    void changeMonth(int delta);
    void setSelectedMonthYear(int month, int year);
    void selectDay(int year, int month, int day);
    QString monthYearLabelText(int month, int year) const;

    QPixmap makeRoundPixmap(const QPixmap &src, int size);
    QPixmap loadUserAvatarFromDb(const QString &userId);

    QVector<CalendarEvent> loadCalendarEvents(int month, int year);
    QVector<MaintenanceItemData> loadUpcomingMaintenance(int month, int year);
    SystemStatus loadSystemStatus();

private:
    qreal scaleFactor_ = 1.0;

    int selectedMonth_ = QDate::currentDate().month();
    int selectedYear_  = QDate::currentDate().year();
    QDate selectedDay_;
    int selectedWeek_ = 0;
    bool highlightWeek_ = false;

    QWidget *rightCalendarFrame = nullptr;
    QWidget *rightUpcomingMaintenanceFrame = nullptr;

    ListAgvInfo *listAgvInfo = nullptr;
    AgvDetailInfo *agvDetailInfo = nullptr;
    AgvSettingsPage *agvSettingsPage = nullptr;

    ModelListPage *modelListPage = nullptr;

    QLabel *monthLabel = nullptr;
    QWidget *calendarActionsFrame = nullptr;

    QPushButton *backButton = nullptr;

    QTableWidget *calendarTablePtr = nullptr;
};
