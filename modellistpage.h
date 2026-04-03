#ifndef MODELLISTPAGE_H
#define MODELLISTPAGE_H

#include <QFrame>
#include <QDialog>
#include <QVector>
#include <functional>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QIntValidator>
#include <QDoubleValidator>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QCheckBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDriver>
#include <QDebug>
#include <QTimer>
#include <QScrollBar>
#include <QMessageBox>
#include <QComboBox>
#include <QScrollArea>
#include <QPair>
#include <memory>

#include "db_models.h"   // ModelInfo, loadModelList(), insertModelToDb(), deleteModelByName()

class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QLineEdit;
class QCheckBox;
class QWidget;

// ---------------------- MaintenanceTask ----------------------

struct MaintenanceTask
{
    QString name;
    int intervalDays;
    int durationMinutes;
};

// ---------------------- AddModelDialog ----------------------

// ---------------------- AddModelDialog ----------------------

class AddModelDialog : public QDialog
{
    Q_OBJECT
public:
    struct Result {
        ModelInfo model;
        bool useTemplate = false;
    };

    explicit AddModelDialog(std::function<int(int)> scale, QWidget *parent = nullptr);

    Result result() const;

private:
    bool validateAll();
    void fillResultFromForm();

private:
    std::function<int(int)> s_;

    QLineEdit *nameEdit_        = nullptr;
    QWidget *versionPoWidget_   = nullptr;
    QVector<QLineEdit*> versionPoSegments_;
    QWidget *versionEplanWidget_ = nullptr;
    QVector<QLineEdit*> versionEplanSegments_;
    QLineEdit *capacityEdit_   = nullptr;

    QLabel *errName_  = nullptr;
    QLabel *errVersionPo_ = nullptr;
    QLabel *errVersionEplan_ = nullptr;
    QLabel *errCap_   = nullptr;

    QPushButton *tplBtn_ = nullptr;   // ← ДОБАВИТЬ ЭТУ СТРОКУ

    Result result_;
};

// ---------------------- TaskRow (новая строка) ----------------------

class TaskRow : public QWidget
{
    Q_OBJECT
public:
    QCheckBox *check = nullptr;
    QLineEdit *proc  = nullptr;
    QLineEdit *days  = nullptr;
    QLineEdit *mins  = nullptr;

    TaskRow(std::function<int(int)> s, bool checkboxVisible, QWidget *parent = nullptr);
};

// ---------------------- TemplatePageWidget (НОВЫЙ) ----------------------

class TemplatePageWidget : public QWidget
{
    Q_OBJECT
public:
    TemplatePageWidget(const ModelInfo &model,
                       std::function<int(int)> scale,
                       QWidget *parent = nullptr);

signals:
    void cancelRequested();
    void saveRequested(const ModelInfo &model,
                       const QVector<MaintenanceTask> &tasks);

private slots:
    void onSaveClicked();
    void toggleDeleteMode();
    void deleteSelected();
    void addRow();

private:
    void loadDefaultRows();

private:
    std::function<int(int)> s_;
    ModelInfo model_;

    QScrollArea *scrollArea_ = nullptr;
    QVBoxLayout *rowsLayout_ = nullptr;

    QVector<TaskRow*> rows_;

    QPushButton *addBtn_      = nullptr;
    QPushButton *delModeBtn_  = nullptr;

    QPushButton *saveBtn_     = nullptr;
    QPushButton *cancelBtn_   = nullptr;

    bool deleteMode_ = false;
};

// ---------------------- ModelDetailsPageWidget ----------------------

class ModelDetailsPageWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ModelDetailsPageWidget(const ModelInfo &model,
                                    std::function<int(int)> scale,
                                    QWidget *parent = nullptr);

signals:
    void backRequested();
};

// ---------------------- ModelListPage ----------------------

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

private:
    enum Mode {
        Mode_List,
        Mode_Template,
        Mode_Details
    };

    void reloadFromDatabase();
    void clearList();
    void addModel(const ModelInfo &m);
    void rebuildModelsVisible();

    void showListMode();
    void showTemplateMode(const ModelInfo &model);
    void showModelDetailsMode(const ModelInfo &model);

private:
    std::function<int(int)> s_;

    Mode mode_ = Mode_List;

    // Шапка
    QPushButton *backBtn_   = nullptr;
    QPushButton *deleteBtn_ = nullptr;
    QLabel      *titleLbl_  = nullptr;

    // Контент
    QScrollArea *scrollArea_    = nullptr;
    QWidget     *contentWidget_ = nullptr;
    QVBoxLayout *listLayout_    = nullptr;
    QLabel      *emptyLabel_    = nullptr;
    QPushButton *loadMoreModelsBtn_ = nullptr;

    QVector<ModelInfo> modelsAll_;
    int modelsShownCount_ = 0;
    static constexpr int kModelsPageBatch = 50;

    // Кнопка "Добавить модель"
    QPushButton *addBtn_ = nullptr;

    // Страница шаблона ТО
    TemplatePageWidget *templatePage_ = nullptr;
    ModelDetailsPageWidget *detailsPage_ = nullptr;
};

#endif // MODELLISTPAGE_H
