#pragma once

#include <QFrame>
#include <QVector>
#include <QString>
#include <QDate>
#include <functional>
#include <QRegularExpression>

class QVBoxLayout;
class QLabel;
class QPushButton;
class QWidget;

//
// ===== Структура AGV =====
//

struct AgvInfo
{
    QString id;
    QString model;
    QString serial;
    int kilometers;
    QString blueprintPath;
    QString status;
    QString task;
    QDate lastActive;
};

//
// ===== Структура фильтров =====
//

struct FilterSettings
{
    bool servAsc = false;
    bool servDesc = false;

    bool upAsc = false;
    bool upDesc = false;

    bool overOld = false;
    bool overNew = false;

    bool modelAZ = false;
    bool modelZA = false;

    bool kmAsc = false;
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
// ===== AgvItem =====
//

class AgvItem : public QFrame
{
    Q_OBJECT
public:
    AgvItem(const AgvInfo &info, std::function<int(int)> scale, QWidget *parent = nullptr);

    // Геттер для удаления
    const QString &agvId() const { return agv.id; }

signals:
    void openDetailsRequested(const QString &id);
    void deleteRequested(const QString &id);   // <<< ДОБАВЛЕНО

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QString statusColor(const QString &st);

    AgvInfo agv;
    std::function<int(int)> s;

    QWidget *header;
    QWidget *details;
    QLabel *arrowLabel;
    QPushButton *detailsButton;
};

//
// ===== ListAgvInfo =====
//

class ListAgvInfo : public QFrame
{
    Q_OBJECT
public:
    explicit ListAgvInfo(std::function<int(int)> scale, QWidget *parent = nullptr);

    void addAgv(const AgvInfo &info);
    void removeAgvById(const QString &id);   // <<< ДОБАВЛЕНО

signals:
    void backRequested();
    void openAgvDetails(const QString &id);

private:
    std::function<int(int)> s;

    QWidget *content;
    QVBoxLayout *layout;

    QPushButton *filterBtn;
    QLabel *filterCount;

    FilterSettings currentFilter;

    void applyFilter(const FilterSettings &fs);
    void rebuildList(const QVector<AgvInfo> &list);
    QVector<AgvInfo> loadAgvList();
};
