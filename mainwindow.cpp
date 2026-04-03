#include "mainwindow.h"
#include "leftmenu.h"
#include "addagvdialog.h"
#include "db_agv_tasks.h"

#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowIcon(QIcon(":/new/mainWindowIcons/noback/agvIcon.png"));
    setMinimumSize(800, 600);

    // Создаём меню и сохраняем указатель
    menu = new leftMenu(this);
    menu->setMinimumSize(800, 600);
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
    AddAgvDialog dlg([this](int v){ return menu ? menu->s(v) : v; }, this);
    if (qApp->property("autotest_running").toBool()) {
        for (int attempt = 0; attempt < 6; ++attempt) {
            QTimer::singleShot(140 + attempt * 120, &dlg, [&dlg]() {
                if (dlg.isVisible())
                    dlg.reject();
            });
        }
    }

    if (dlg.exec() != QDialog::Accepted)
        return;

    auto agv = dlg.result;

    AgvInfo info;

    // Генерация ID: имя + последние 4 цифры SN + модель в нижнем регистре
    QString digits;
    QRegularExpression re("\\d+");
    auto it = re.globalMatch(agv.serial);
    while (it.hasNext())
        digits += it.next().captured();

    QString last4 = digits.right(4);
    if (last4.isEmpty()) last4 = "0000";

    info.id = agv.name.trimmed() + "_" + last4 + "_" + agv.model.toLower();
    info.model = agv.model.toUpper();
    info.serial = agv.serial;
    info.status = agv.status;
    info.task = agv.alias.trimmed();
    info.kilometers = 0;
    info.blueprintPath = ":/new/mainWindowIcons/noback/blueprint.png";
    info.lastActive = QDate::currentDate();

    insertAgvToDb(info);   // ← запись в БД
    copyModelTasksToAgv(info.id, info.model);

    qDebug() << "AGV сохранён в БД:" << info.id;
}


