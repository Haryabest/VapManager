#ifndef MAINTENANCEITEMWIDGET_H
#define MAINTENANCEITEMWIDGET_H

#include <QWidget>
#include <QColor>

class QLabel;
class QPushButton;
class QFrame;

class MaintenanceItemWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MaintenanceItemWidget(
        const QString &iconResPath,
        const QSize &iconSize,
        const QString &titleText,
        const QString &subtitleText,
        const QColor &rowBgColor,          // already includes alpha
        const QColor &buttonBgColor,       // already includes alpha
        const QColor &buttonTextColor,
        QWidget *parent = nullptr
    );

signals:
    void showClicked();

private:
    QFrame *root_;
    QLabel *iconLabel_;
    QLabel *titleLabel_;
    QLabel *subtitleLabel_;
    QPushButton *showButton_;
};

#endif // MAINTENANCEITEMWIDGET_H
