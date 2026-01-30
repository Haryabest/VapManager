#pragma once

#include <QFrame>
#include <QVector>
#include <QString>
#include <QDate>
#include <functional>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QEvent>
#include <QPixmap>
#include <QIcon>
#include <QLayoutItem>
#include <QDialog>
#include <QFrame>
#include <QLineEdit>
#include <QPainter>
#include <QCheckBox>
#include <QMouseEvent>
#include <QComboBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

class QVBoxLayout;
class QLabel;
class QPushButton;
class QWidget;
class QMouseEvent;

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
    QString task;
    QDate   lastActive;
};

//
// ===== Структура фильтров =====
//

struct FilterSettings
{
    bool servAsc  = false;
    bool servDesc = false;

    bool upAsc  = false;
    bool upDesc = false;

    bool overOld = false;
    bool overNew = false;

    bool modelAZ = false;
    bool modelZA = false;

    bool kmAsc  = false;
    bool kmDesc = false;

    QString nameFilter;

    bool isActive() const
    {
        return servAsc || servDesc ||
               upAsc || upDesc ||
               overOld || overNew ||
               modelAZ || modelZA ||
               kmAsc || kmDesc ||
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
    void deleteRequested(const QString &id);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QString statusColor(const QString &st);

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

signals:
    void backRequested();
    void openAgvDetails(const QString &id);

private:
    std::function<int(int)> s;

    QWidget     *content = nullptr;
    QVBoxLayout *layout  = nullptr;

    QPushButton *filterBtn   = nullptr;
    QLabel      *filterCount = nullptr;

    FilterSettings currentFilter;

    void applyFilter(const FilterSettings &fs);
    void rebuildList(const QVector<AgvInfo> &list);
    QVector<AgvInfo> loadAgvList();   // тянет AGV из БД (реализация в .cpp)
};
