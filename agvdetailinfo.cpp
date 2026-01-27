#include "agvdetailinfo.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QLineEdit>
#include <QComboBox>

AgvDetailInfo::AgvDetailInfo(std::function<int(int)> scale, QWidget *parent)
    : QWidget(parent), s(scale)
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(s(20), s(20), s(20), s(20));
    root->setSpacing(s(15));

    //
    // ======================= ЗАГОЛОВОК =======================
    //
    titleLabel = new QLabel("AGV", this);
    titleLabel->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:black;"
    ).arg(s(26)));

    root->addWidget(titleLabel);

    //
    // ======================= ОСНОВНАЯ ИНФОРМАЦИЯ =======================
    //
    modelLabel = new QLabel(this);
    serialLabel = new QLabel(this);
    kmLabel = new QLabel(this);
    statusLabel = new QLabel(this);
    taskLabel = new QLabel(this);
    lastActiveLabel = new QLabel(this);

    QList<QLabel*> labels = {modelLabel, serialLabel, kmLabel, statusLabel, taskLabel, lastActiveLabel};

    for (auto *lbl : labels)
        lbl->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:#333;").arg(s(18)));

    for (auto *lbl : labels)
        root->addWidget(lbl);

    //
    // ======================= ЧЕРТЁЖ =======================
    //
    blueprintLabel = new QLabel(this);
    blueprintLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(blueprintLabel);

    //
    // ======================= КНОПКА НАЗАД =======================
    //
    QHBoxLayout *btnRow = new QHBoxLayout();

    backButton = new QPushButton("← Назад", this);
    backButton->setStyleSheet(QString(
        "QPushButton{background:transparent;font-family:Inter;font-size:%1px;"
        "font-weight:800;color:black;border:none;}"
        "QPushButton:hover{color:#0F00DB;}"
    ).arg(s(18)));

    connect(backButton, &QPushButton::clicked, this, [this](){
        emit backRequested();
    });

    btnRow->addWidget(backButton);
    btnRow->addStretch();

    root->addLayout(btnRow);

}

//
// ======================= ЗАГРУЗКА ДАННЫХ AGV =======================
//
void AgvDetailInfo::setAgv(const QString &agvId)
{
    currentAgvId = agvId;
    loadFromDatabase(agvId);
}

//
// ======================= ЗАГЛУШКА БД =======================
//
void AgvDetailInfo::loadFromDatabase(const QString &agvId)
{
    titleLabel->setText(agvId);
    modelLabel->setText("Модель: X1");
    serialLabel->setText("Серийный номер: SN-101-0001");
    kmLabel->setText("Пробег: 5500 км");
    statusLabel->setText("Статус: online");
    taskLabel->setText("Текущая задача: Перевозка паллеты");
    lastActiveLabel->setText("Последняя активность: 20.02.2026");

    blueprintLabel->setPixmap(
        QPixmap(":/new/mainWindowIcons/noback/blueprint.png")
            .scaled(s(300), s(200), Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
}
