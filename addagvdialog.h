#pragma once

#include <QDialog>
#include <QString>
#include <functional>
#include <QLineEdit>

class QComboBox;
class QLabel;
class QPushButton;
class QEvent;

//
// ===== Структура данных =====
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
// ===== Кастомное поле AGV =====
//

class AgvMaskedEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit AgvMaskedEdit(QWidget *parent = nullptr);

    QString digits() const;
    QString letters() const;

signals:
    void contentChanged();

protected:
    void paintEvent(QPaintEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;

private:
    QVector<QChar> digitBuf;   // 4 цифры
    QVector<QChar> letterBuf;  // 10 букв
    int cursorIndex = 0;       // 0..14, -1 = скрыт

    void moveCursorLeft();
    void moveCursorRight();
    void updateCursor();
};

//
// ===== Кастомное поле SN =====
//

class SnMaskedEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit SnMaskedEdit(QWidget *parent = nullptr);

    QString digits() const;

signals:
    void contentChanged();

protected:
    void paintEvent(QPaintEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;

private:
    QVector<QChar> buf; // 10 цифр
    int cursorIndex = 0;

    void updateCursor();
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

    AgvMaskedEdit *nameEdit   = nullptr;
    SnMaskedEdit  *serialEdit = nullptr;
    QComboBox     *statusBox  = nullptr;
    QComboBox     *modelBox   = nullptr;
    QLineEdit     *aliasEdit  = nullptr;

    QLabel *nameError   = nullptr;
    QLabel *serialError = nullptr;
    QLabel *aliasError  = nullptr;

    QPushButton *addBtn = nullptr;

    void validateAll();
    bool validateName();
    bool validateSerial();
    bool validateAlias();
};
