#pragma once

#include <QDialog>
#include <QString>
#include <functional>

class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;
class QEvent;

//
// ===== Структура данных, возвращаемая диалогом =====
//

struct NewAgvData
{
    QString name;
    QString serial;
    QString status;
    QString model;
    QString alias;
};

//
// ===== AddAgvDialog =====
//

class AddAgvDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AddAgvDialog(std::function<int(int)> scale,
                          QWidget *parent = nullptr);

    NewAgvData result;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    std::function<int(int)> s;

    QLineEdit  *nameEdit   = nullptr;
    QLineEdit  *serialEdit = nullptr;
    QComboBox  *statusBox  = nullptr;
    QComboBox  *modelBox   = nullptr;
    QLineEdit  *aliasEdit  = nullptr;

    QLabel *nameError   = nullptr;
    QLabel *serialError = nullptr;
    QLabel *aliasError  = nullptr;

    QPushButton *addBtn = nullptr;

    void validateAll();
    bool validateName();
    bool validateSerial();
    bool validateAlias();
};
