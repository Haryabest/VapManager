#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class leftMenu;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onAddAgvRequested();   // ← вот он

private:
    leftMenu *menu;
};

#endif // MAINWINDOW_H
