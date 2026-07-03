    #include "multisectionwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

MultiSectionWidget::MultiSectionWidget(QWidget *parent, qreal scaleFactor)
    : QWidget(parent),
      scaleFactor_(scaleFactor),
      activeAGVCurrentCount_(0),
      activeAGVTotalCount_(0),
      maintenanceCurrentCount_(0),
      maintenanceTotalCount_(0),
      errorCurrentCount_(0),
      errorTotalCount_(0),
      disabledCurrentCount_(0),
      disabledTotalCount_(0)
{
    QLabel *statusSystemLabel = new QLabel("Статус системы", this);
    statusSystemLabel->setStyleSheet(QString(
        "font-family: Inter;"
        "font-weight: 600;"
        "font-size: %1px;"
        "color: #000000;"
    ).arg(int(16 * scaleFactor_)));

    activeAGVTitleLabel_ = new QLabel("Активные AGV:", this);
        activeAGVCountLabel_ = new QLabel(this);

        maintenanceTitleLabel_ = new QLabel("Обслуживание просрочено:", this);
        maintenanceCountLabel_ = new QLabel(this);

        errorTitleLabel_ = new QLabel("Пропущеное ТО:", this);
        errorCountLabel_ = new QLabel(this);

        disabledTitleLabel_ = new QLabel("Отключены:", this);
        disabledCountLabel_ = new QLabel(this);

        QString titleStyle = QString(
            "font-family: Inter;"
            "font-weight: 700;"
            "font-size: %1px;"
            "color: rgb(130,130,130);"
        ).arg(int(14 * scaleFactor_));

        activeAGVTitleLabel_->setStyleSheet(titleStyle);
        maintenanceTitleLabel_->setStyleSheet(titleStyle);
        errorTitleLabel_->setStyleSheet(titleStyle);
        disabledTitleLabel_->setStyleSheet(titleStyle);

        auto makeRow = [&](QLabel *title, QLabel *value) {
            QHBoxLayout *row = new QHBoxLayout();
            row->addWidget(title);
            row->addStretch();
            row->addWidget(value);
            return row;
        };

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(statusSystemLabel);
        mainLayout->addSpacing(int(10 * scaleFactor_));
        mainLayout->addLayout(makeRow(activeAGVTitleLabel_, activeAGVCountLabel_));
        mainLayout->addSpacing(int(8 * scaleFactor_));
        mainLayout->addLayout(makeRow(maintenanceTitleLabel_, maintenanceCountLabel_));
        mainLayout->addSpacing(int(8 * scaleFactor_));
        mainLayout->addLayout(makeRow(errorTitleLabel_, errorCountLabel_));
        mainLayout->addSpacing(int(8 * scaleFactor_));
        mainLayout->addLayout(makeRow(disabledTitleLabel_, disabledCountLabel_));
        mainLayout->addStretch();

        updateActiveAGVSection();
        updateMaintenanceSection();
        updateErrorSection();
        updateDisabledSection();
    }

    void MultiSectionWidget::setScaleFactor(qreal scaleFactor)
    {
        scaleFactor_ = scaleFactor;
    }

    void MultiSectionWidget::setActiveAGVCurrentCount(int value)
    {
        activeAGVCurrentCount_ = value;
        updateActiveAGVSection();
    }

    void MultiSectionWidget::setActiveAGVTotalCount(int value)
    {
        activeAGVTotalCount_ = value;
        updateActiveAGVSection();
    }

    void MultiSectionWidget::updateActiveAGVSection()
    {
        activeAGVCountLabel_->setText(QString(
            "<span style='color:#008000; font-family:Inter; font-size:%1px; font-weight:800;'>%2</span>"
            " / "
            "<span style='color:black; font-family:Inter; font-size:%1px; font-weight:800;'>%3</span>"
        ).arg(int(14 * scaleFactor_)).arg(activeAGVCurrentCount_).arg(activeAGVTotalCount_));
    }

    void MultiSectionWidget::setMaintenanceCurrentCount(int value)
    {
        maintenanceCurrentCount_ = value;
        updateMaintenanceSection();
    }

    void MultiSectionWidget::setMaintenanceTotalCount(int value)
    {
        maintenanceTotalCount_ = value;
        updateMaintenanceSection();
    }

    void MultiSectionWidget::updateMaintenanceSection()
    {
        maintenanceCountLabel_->setText(QString(
            "<span style='color:#FF8C00; font-family:Inter; font-size:%1px; font-weight:800;'>%2</span>"
            " / "
            "<span style='color:black; font-family:Inter; font-size:%1px; font-weight:800;'>%3</span>"
        ).arg(int(14 * scaleFactor_)).arg(maintenanceCurrentCount_).arg(maintenanceTotalCount_));
    }

    void MultiSectionWidget::setErrorCurrentCount(int value)
    {
        errorCurrentCount_ = value;
        updateErrorSection();
    }

    void MultiSectionWidget::setErrorTotalCount(int value)
    {
        errorTotalCount_ = value;
        updateErrorSection();
    }

    void MultiSectionWidget::updateErrorSection()
    {
        errorCountLabel_->setText(QString(
            "<span style='color:#FF0000; font-family:Inter; font-size:%1px; font-weight:800;'>%2</span>"
            " / "
            "<span style='color:black; font-family:Inter; font-size:%1px; font-weight:800;'>%3</span>"
        ).arg(int(14 * scaleFactor_)).arg(errorCurrentCount_).arg(errorTotalCount_));
    }

    void MultiSectionWidget::setDisabledCurrentCount(int value)
    {
        disabledCurrentCount_ = value;
        updateDisabledSection();
    }

    void MultiSectionWidget::setDisabledTotalCount(int value)
    {
        disabledTotalCount_ = value;
        updateDisabledSection();
    }

    void MultiSectionWidget::updateDisabledSection()
    {
        disabledCountLabel_->setText(QString(
            "<span style='color:#3B3B3B; font-family:Inter; font-size:%1px; font-weight:800;'>%2</span>"
            " / "
            "<span style='color:black; font-family:Inter; font-size:%1px; font-weight:800;'>%3</span>"
        ).arg(int(14 * scaleFactor_)).arg(disabledCurrentCount_).arg(disabledTotalCount_));
    }

    //
    // ======================= НОВЫЙ МЕТОД =======================
    //

    void MultiSectionWidget::updateFromList(const QVector<AgvInfo> &list)
    {
        int active = 0;
        int maintenance = 0;
        int error = 0;
        int disabled = 0;

        for (const auto &a : list)
        {
            if (a.status == "online") active++;
            else if (a.status == "maintenance") maintenance++;
            else if (a.status == "error") error++;
            else disabled++;
        }

        setActiveAGVCurrentCount(active);
        setActiveAGVTotalCount(list.size());

        setMaintenanceCurrentCount(maintenance);
        setMaintenanceTotalCount(list.size());

        setErrorCurrentCount(error);
        setErrorTotalCount(list.size());

        setDisabledCurrentCount(disabled);
        setDisabledTotalCount(list.size());
    }
