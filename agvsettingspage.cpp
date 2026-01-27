#include "agvsettingspage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

AgvSettingsPage::AgvSettingsPage(std::function<int(int)> scale, QWidget *parent)
    : QWidget(parent), s(scale)
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(s(20), s(20), s(20), s(20));
    root->setSpacing(s(15));

    //
    // Заголовок
    //
    titleLabel = new QLabel("AGV — супер‑настройки", this);
    titleLabel->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:black;"
    ).arg(s(26)));

    root->addWidget(titleLabel);

    //
    // Информационные блоки
    //
    firmwareLabel = new QLabel(this);
    batteryLabel = new QLabel(this);
    errorsLabel = new QLabel(this);
    maintenanceHistoryLabel = new QLabel(this);
    logsLabel = new QLabel(this);

    QList<QLabel*> labels = {
        firmwareLabel, batteryLabel, errorsLabel,
        maintenanceHistoryLabel, logsLabel
    };

    for (auto *lbl : labels)
        lbl->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;color:#333;"
        ).arg(s(18)));

    for (auto *lbl : labels)
        root->addWidget(lbl);

    //
    // Кнопка назад
    //
    backButton = new QPushButton("← Назад", this);
    backButton->setStyleSheet(QString(
        "QPushButton{background:transparent;font-family:Inter;font-size:%1px;"
        "font-weight:800;color:black;border:none;}"
        "QPushButton:hover{color:#0F00DB;}"
    ).arg(s(18)));

    connect(backButton, &QPushButton::clicked, this, [this](){
        emit backRequested();
    });

    root->addWidget(backButton, 0, Qt::AlignLeft);
}

//
// Загружаем данные AGV
//
void AgvSettingsPage::setAgv(const QString &agvId)
{
    currentAgvId = agvId;
    loadFromDatabase(agvId);
}

//
// Заглушка БД — заменишь на реальные запросы
//
void AgvSettingsPage::loadFromDatabase(const QString &agvId)
{
    titleLabel->setText(agvId + " — супер‑настройки");

    firmwareLabel->setText("Прошивка: v1.2.4");
    batteryLabel->setText("Батарея: 87%");
    errorsLabel->setText("Ошибки: нет");
    maintenanceHistoryLabel->setText("История ТО: 12.01.2026 — ТО‑1");
    logsLabel->setText("Логи: 0 критических событий");
}
