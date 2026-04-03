#pragma once
#include <QMainWindow>
#include <QStackedWidget>

class leftMenu;
class ListAgvInfo;
class AgvSettingsPage;
class ModelListPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    leftMenu *leftMenuWidget() const { return menu; }

private slots:
    void onAddAgvRequested();

private:
    QStackedWidget *stack;

    leftMenu *menu;
    ListAgvInfo *listPage;
    AgvSettingsPage *settingsPage;
    ModelListPage *modelPage;
};
