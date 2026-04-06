#pragma once

#include <QWidget>
#include <QLabel>
#include <QVector>
#include "listagvinfo.h"   // для AgvInfo

class MultiSectionWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MultiSectionWidget(QWidget *parent = nullptr, qreal scaleFactor = 1.0);

    void setScaleFactor(qreal scaleFactor);

    void setActiveAGVCurrentCount(int value);
    void setActiveAGVTotalCount(int value);

    void setMaintenanceCurrentCount(int value);
    void setMaintenanceTotalCount(int value);

    void setErrorCurrentCount(int value);
    void setErrorTotalCount(int value);

    void setDisabledCurrentCount(int value);
    void setDisabledTotalCount(int value);

    // 🔥 Новый метод — обновление всех секций по списку AGV
    void updateFromList(const QVector<AgvInfo> &list);

private:
    void updateActiveAGVSection();
    void updateMaintenanceSection();
    void updateErrorSection();
    void updateDisabledSection();

private:
    qreal scaleFactor_;

    QLabel *activeAGVTitleLabel_;
    QLabel *activeAGVCountLabel_;

    QLabel *maintenanceTitleLabel_;
    QLabel *maintenanceCountLabel_;

    QLabel *errorTitleLabel_;
    QLabel *errorCountLabel_;

    QLabel *disabledTitleLabel_;
    QLabel *disabledCountLabel_;

    int activeAGVCurrentCount_;
    int activeAGVTotalCount_;

    int maintenanceCurrentCount_;
    int maintenanceTotalCount_;

    int errorCurrentCount_;
    int errorTotalCount_;

    int disabledCurrentCount_;
    int disabledTotalCount_;
};
