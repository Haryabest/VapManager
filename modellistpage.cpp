#include "modellistpage.h"
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QResizeEvent>
#include <QDialog>

ModelListPage::ModelListPage(std::function<int(int)> scale, QWidget *parent)
    : QFrame(parent), s(scale)
{
    setStyleSheet("background-color:#F1F2F4;border-radius:12px;");

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(s(10), s(10), s(10), s(10));
    root->setSpacing(s(12));

    // ===== Назад =====
    QPushButton *back = new QPushButton("   Назад", this);
    back->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    back->setIconSize(QSize(s(24), s(24)));
    back->setFixedSize(s(150), s(50));
    back->setStyleSheet(QString(
        "QPushButton { background:#E6E6E6; border-radius:%1px; "
        "font-family:Inter; font-size:%2px; font-weight:800; }"
        "QPushButton:hover { background:#D5D5D5; }"
    ).arg(s(10)).arg(s(16)));

    connect(back, &QPushButton::clicked, this, [this](){
        emit backRequested();
    });

    // ===== Верхняя строка =====
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->addWidget(back, 0, Qt::AlignLeft);

    QLabel *title = new QLabel("Модели AGV", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
    ).arg(s(26)));

    topRow->addWidget(title, 1, Qt::AlignCenter);
    topRow->addSpacing(back->width());

    root->addLayout(topRow);

    // ===== Скролл =====
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *content = new QWidget(scroll);
    layout = new QVBoxLayout(content);
    layout->setSpacing(s(8));
    layout->setContentsMargins(0,0,0,0);

    scroll->setWidget(content);
    root->addWidget(scroll);

    // ===== Пустой текст =====
    emptyLabel = new QLabel("Здесь пока ничего нет", this);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:700;color:#777;"
    ).arg(s(20)));

    layout->addWidget(emptyLabel);
    layout->addStretch();

    // ===== Плавающая кнопка =====
    addBtn = new QPushButton("+ Добавить модель", this);
    addBtn->setFixedSize(s(320), s(50));   // уменьшено на ~30%
    addBtn->raise();
    addBtn->setStyleSheet(QString(
        "QPushButton { background:#0F00DB; border-radius:%1px; "
        "font-family:Inter; font-size:%2px; font-weight:800; color:white; }"
        "QPushButton:hover { background:#1A4ACD; }"
    ).arg(s(14)).arg(s(20)));

    connect(addBtn, &QPushButton::clicked, this, [this](){
        openAddModelDialog();
    });
}

void ModelListPage::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);

    // Масштабируем кнопку при изменении разрешения
    addBtn->setFixedSize(s(320), s(50));

    if (addBtn) {
        int x = (width() - addBtn->width()) / 2;
        int y = height() - addBtn->height() - s(20);
        addBtn->move(x, y);
    }
}

void ModelListPage::openAddModelDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Добавить модель");
    dlg.setFixedSize(400, 200);

    // Пока пусто — ты сам скажешь, что нужно
    dlg.exec();
}

void ModelListPage::addModel(const ModelInfo &info)
{
    emptyLabel->hide();

    QFrame *item = new QFrame(this);
    item->setStyleSheet("background:white;border-radius:10px;border:1px solid #E0E0E0;");
    QHBoxLayout *h = new QHBoxLayout(item);
    h->setContentsMargins(s(12), s(10), s(12), s(10));

    QLabel *text = new QLabel(
        QString("%1\n%2\nГрузоподъёмность: %3 кг\nСкорость: %4 м/с")
            .arg(info.name)
            .arg(info.category)
            .arg(info.capacityKg)
            .arg(info.maxSpeed),
        item
    );
    text->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:700;color:#1A1A1A;"
    ).arg(s(16)));
    text->setWordWrap(true);

    h->addWidget(text);

    layout->insertWidget(layout->count() - 1, item);
}

void ModelListPage::reloadFromDatabase()
{
    QLayoutItem *child;
    while ((child = layout->takeAt(0)) != nullptr) {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }

    layout->addWidget(emptyLabel);
    emptyLabel->setVisible(true);
    layout->addStretch();
}
