#ifndef MODELLISTPAGE_H
#define MODELLISTPAGE_H

#include <QFrame>
#include <QVector>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <functional>

struct ModelInfo {
    QString name;
    QString category;
    int capacityKg;
    double maxSpeed;
    QString blueprintPath;
};

class ModelListPage : public QFrame
{
    Q_OBJECT
public:
    explicit ModelListPage(std::function<int(int)> scale, QWidget *parent = nullptr);

    void addModel(const ModelInfo &info);
    void reloadFromDatabase();

signals:
    void backRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    std::function<int(int)> s;

    QVBoxLayout *layout = nullptr;
    QLabel *emptyLabel = nullptr;

    QPushButton *addBtn = nullptr;

    void openAddModelDialog();
};

#endif // MODELLISTPAGE_H
