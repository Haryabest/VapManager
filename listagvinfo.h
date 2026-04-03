#pragma once

#include <QFrame>
#include <QVector>
#include <QString>
#include <QDate>
#include <functional>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QCheckBox>
#include <QMouseEvent>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include "db_agv_tasks.h"

//
// ===== Структура AGV =====
//

struct AgvInfo
{
    QString id;
    QString model;
    QString serial;
    int     kilometers = 0;
    QString blueprintPath;
    QString status;
    QString maintenanceState; // green / orange / red
    bool hasOverdueMaintenance = false;
    bool hasSoonMaintenance = false;
    QString task;
    QDate   lastActive;
    QString assignedUser;  // за кем закреплена (для раздела Ваши/Общие)
};

//
// ===== Структура фильтров =====
//

struct FilterSettings
{
    enum Serv  { None, Asc, Desc } serv = None;
    enum Up    { UpNone, UpAsc, UpDesc } up = UpNone;
    enum Over  { OverNone, OverOld, OverNew } over = OverNone;
    enum Model { ModelNone, ModelAZ, ModelZA } modelSort = ModelNone;
    enum Km    { KmNone, KmAsc, KmDesc } km = KmNone;

    QString nameFilter;

    bool isActive() const
    {
        return serv != None ||
               up != UpNone ||
               over != OverNone ||
               modelSort != ModelNone ||
               km != KmNone ||
               !nameFilter.isEmpty();
    }
};

//
// ===== Свободная функция для записи AGV в БД =====
//

bool insertAgvToDb(const AgvInfo &info);

//
// ===== AgvItem =====
//

class AgvItem : public QFrame
{
    Q_OBJECT
public:
    AgvItem(const AgvInfo &info,
            std::function<int(int)> scale,
            QWidget *parent = nullptr);

    const QString &agvId() const { return agv.id; }

signals:
    void openDetailsRequested(const QString &id);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QString statusColor(const QString &st);
    QString maintenanceColor(const QString &state);

    AgvInfo agv;
    std::function<int(int)> s;

    QWidget     *header = nullptr;
    QWidget     *details = nullptr;
    QLabel      *arrowLabel = nullptr;
    QPushButton *detailsButton = nullptr;
};

//
// ===== ListAgvInfo =====
//

class ListAgvInfo : public QFrame
{
    Q_OBJECT
public:
    explicit ListAgvInfo(std::function<int(int)> scale,
                         QWidget *parent = nullptr);

    void addAgv(const AgvInfo &info);
    void removeAgvById(const QString &id);

    QVector<AgvInfo> loadAgvList();
    void rebuildList(const QVector<AgvInfo> &list);
    bool hasRenderedState() const;

signals:
    void backRequested();
    void openAgvDetails(const QString &id);

    // ★ ДОБАВЛЕНО — сигнал для leftMenu, чтобы обновлять счётчик AGV
    void agvListChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    std::function<int(int)> s;

    QWidget     *content = nullptr;
    QVBoxLayout *layout  = nullptr;

    QPushButton *addBtn_ = nullptr;
    QPushButton *loadMoreBtn_ = nullptr;
    QFrame *undoToast_ = nullptr;
    QPushButton *undoBtn_ = nullptr;
    QTimer *undoTimer_ = nullptr;

    FilterSettings currentFilter;
    QVector<AgvInfo> currentDisplayList_;
    int shownCount_ = 0;
    /// Размер плоской очереди карточек (ваши+общие+делегированные); для кнопки «Показать ещё».
    int displayQueueTotal_ = 0;
    int batchSize_ = 50;
    int appearRetryLeft_ = 0;
    bool hasRenderedState_ = false;

    struct DeletedHistoryRow {
        QString agvId;
        int taskId = 0;
        QString taskName;
        int intervalDays = 0;
        QDate completedAt;
        QDate nextDateAfter;
        QString performedBy;
    };
    QVector<AgvInfo> lastDeletedAgvs_;
    QVector<AgvTask> lastDeletedTasks_;
    QVector<DeletedHistoryRow> lastDeletedHistory_;

    void applyFilter(const FilterSettings &fs);
    void rebuildShownChunk();
    void runListAppearSmokeTest(int expectedVisible);
    void showUndoToast();
    void clearUndoSnapshot();
    void restoreDeletedAgvs();
};
