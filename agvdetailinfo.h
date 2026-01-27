#ifndef AGVDETAILINFO_H
#define AGVDETAILINFO_H

#include <QWidget>
#include <QString>
#include <functional>

class QLabel;
class QPushButton;

class AgvDetailInfo : public QWidget
{
    Q_OBJECT

public:
    explicit AgvDetailInfo(std::function<int(int)> scale, QWidget *parent = nullptr);

    // Загружаем данные AGV по ID
    void setAgv(const QString &agvId);

signals:
    void backRequested();   // ← осталось только "Назад"

private:
    std::function<int(int)> s;

    QString currentAgvId;

    // UI элементы
    QLabel *titleLabel;
    QLabel *modelLabel;
    QLabel *serialLabel;
    QLabel *kmLabel;
    QLabel *statusLabel;
    QLabel *taskLabel;
    QLabel *lastActiveLabel;
    QLabel *blueprintLabel;

    QPushButton *backButton;

    // Метод загрузки данных из БД
    void loadFromDatabase(const QString &agvId);
};

#endif // AGVDETAILINFO_H
