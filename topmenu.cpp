#include "topmenu.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QDebug>

TopMenu::TopMenu(QFrame *firstFrame, QWidget *parent)
    : QWidget(parent), m_firstFrame(firstFrame)
{
    // Проверим доступность первого фрейма
    if (!m_firstFrame) {
        qDebug() << "Первый фрейм не передан.";
        return;
    }

    // Внешний фрейм
    QFrame *frame = new QFrame(this);
    frame->setFrameShape(QFrame::NoFrame);
    frame->setStyleSheet("background-color: #F8F9FB; border:none;");
    frame->setFixedHeight(40); // Заданная вами высота

    // Определим координаты
    int xPos = m_firstFrame->geometry().x();
    int yPos = m_firstFrame->geometry().bottom() + 5; // Сразу под первым фреймом

    // Перемещаем фрейм
    frame->move(xPos, yPos);

    // Надписи
    QLabel *titleLabel = new QLabel("Календарь технического обслуживания", frame);
    titleLabel->setStyleSheet("font-family: Inter; font-weight: 900; font-size: 20px; color: black;");
    titleLabel->setAlignment(Qt::AlignCenter);

    QLabel *subtitleLabel = new QLabel("Отслеживание графиков обслуживания AGV и истории технического обслуживания", frame);
    subtitleLabel->setStyleSheet("font-family: Inter; font-weight: bold; font-size: 16px; color: rgb(172, 172, 173);");
    subtitleLabel->setAlignment(Qt::AlignCenter);

    // Макет для надписей
    QVBoxLayout *labelLayout = new QVBoxLayout(frame);
    labelLayout->addWidget(titleLabel);
    labelLayout->addWidget(subtitleLabel);
    labelLayout->setAlignment(Qt::AlignCenter);
    labelLayout->setSpacing(5); // Пространство между строками

    // Присваиваем макет фрейму
    frame->setLayout(labelLayout);

    // Создаем основной макет
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(frame);
    layout->setMargin(0);
    layout->setSpacing(0);

    // Уточним политику размеров
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}
