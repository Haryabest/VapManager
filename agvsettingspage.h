#ifndef AGVSETTINGSPAGE_H
#define AGVSETTINGSPAGE_H

#include <QWidget>
#include <QString>

class QLabel;
class QPushButton;

class AgvSettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit AgvSettingsPage(std::function<int(int)> scale, QWidget *parent = nullptr);

    // Загружаем данные AGV по ID
    void setAgv(const QString &agvId);

signals:
    void backRequested();

private:
    std::function<int(int)> s;

    QString currentAgvId;

    QLabel *titleLabel;
    QLabel *firmwareLabel;
    QLabel *batteryLabel;
    QLabel *errorsLabel;
    QLabel *maintenanceHistoryLabel;
    QLabel *logsLabel;

    QPushButton *backButton;

    void loadFromDatabase(const QString &agvId);
};

#endif // AGVSETTINGSPAGE_H
