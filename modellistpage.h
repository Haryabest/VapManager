#pragma once

#include <QFrame>
#include <QVector>
#include <functional>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QResizeEvent>
#include <QSqlQuery>
#include <QSqlError>
#include <QMap>
#include <QDebug>
#include <QPixmap>
#include <QIcon>
#include <QTimer>
#include <QScrollBar>

struct ModelInfo;
struct MaintenanceTask;

class QVBoxLayout;
class QLabel;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QWidget;

enum PageMode {
    MODE_LIST,
    MODE_TEMPLATE
};

class TemplatePageWidget;

class ModelListPage : public QFrame
{
    Q_OBJECT
public:
    explicit ModelListPage(std::function<int(int)> scale, QWidget *parent = nullptr);

signals:
    void backRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void openAddModelDialog();
    void openDeleteDialog();
    void showTemplatePage();

private:
    void reloadFromDatabase();
    void addModel(const ModelInfo &info);

private:
    std::function<int(int)> s;

    QScrollArea *scrollArea = nullptr;
    QWidget *contentWidget = nullptr;
    QVBoxLayout *layout = nullptr;

    QPushButton *addBtn = nullptr;

    QLabel *emptyLabel = nullptr;

    TemplatePageWidget *templatePage = nullptr;

    PageMode mode = MODE_LIST;
};
