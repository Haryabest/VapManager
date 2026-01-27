#include "mainwindow.h"
#include "leftmenu.h"
#include "addagvdialog.h"

#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Создаём меню и сохраняем указатель
    menu = new leftMenu(this);
    setCentralWidget(menu);

    // Ловим сигнал "Добавить AGV"
    connect(menu, &leftMenu::addAgvRequested,
            this, &MainWindow::onAddAgvRequested);

    setStyleSheet("background-color: #ffffff;");
    showMaximized();
}

MainWindow::~MainWindow()
{
}

void MainWindow::onAddAgvRequested()
{
    // Открываем диалог
    AddAgvDialog dlg([](int v){ return v; }, this);

    if (dlg.exec() != QDialog::Accepted)
        return;

    auto agv = dlg.result;

    // Пока просто выводим
    qDebug() << "Добавлен AGV:"
             << agv.name
             << agv.serial
             << agv.status
             << agv.model
             << agv.alias;

    // Здесь позже добавим:
    // listAgvInfo->addAgvItem(...)
}
