#include "leftmenu.h"
#include "databus.h"
#include "diag_logger.h"

#include <QDate>
#include <QTimer>

leftMenu::leftMenu(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);

    scaleUpdateTimer_ = new QTimer(this);
    scaleUpdateTimer_->setSingleShot(true);
    scaleUpdateTimer_->setInterval(80);
    connect(scaleUpdateTimer_, &QTimer::timeout, this, [this]() {
        setScaleFactor(pendingScaleFactor_);
    });

    selectedDay_ = QDate(selectedYear_, selectedMonth_, 1);
    selectedWeek_ = 0;
    highlightWeek_ = false;

    initUI();
}

int leftMenu::s(int v) const
{
    return int(v * scaleFactor_);
}

void leftMenu::requestScaleFactorUpdate(qreal factor)
{
    if (factor <= 0)
        factor = 1.0;
    factor = qMin<qreal>(1.0, factor);

    pendingScaleFactor_ = factor;

    if (qAbs(scaleFactor_ - pendingScaleFactor_) < 0.01)
        return;

    if (scaleUpdateTimer_)
        scaleUpdateTimer_->start();
    else
        setScaleFactor(pendingScaleFactor_);
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

    destroyCalendarDayOverlay();

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
    requestScaleFactorUpdate(factor);
}
