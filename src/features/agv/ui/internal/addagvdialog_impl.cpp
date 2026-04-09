#include "addagvdialog.h"
#include "db_models.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QTimer>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QChar>

namespace {

QLabel *makeSectionTitle(QWidget *parent, const QString &text, int fontPx, int topPx, int bottomPx)
{
    QLabel *lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QString(
        "font-family:Inter;"
        "font-size:%1px;"
        "font-weight:600;"
        "color:#222;"
        "margin-top:%2px;"
        "margin-bottom:%3px;"
    ).arg(fontPx).arg(topPx).arg(bottomPx));
    return lbl;
}

QLabel *makeErrorLabel(QWidget *parent)
{
    QLabel *e = new QLabel(parent);
    e->setStyleSheet(
        "color:#D00000;"
        "font-size:13px;"
        "font-family:Inter;"
        "margin-top:2px;"
    );
    e->hide();
    return e;
}

QString addDialogLineEditStyle()
{
    return QStringLiteral(
        "padding:8px 10px;"
        "font-size:15px;"
        "border:1px solid #D0D0D0;"
        "border-radius:8px;"
        "background:white;");
}

QString addDialogComboStyle()
{
    return QStringLiteral(
        "QComboBox {"
        "   border:1px solid #D0D0D0;"
        "   padding:6px 8px;"
        "   background:white;"
        "   font-size:15px;"
        "}");
}

QString buildAgvName(const QString &digits, const QString &letters)
{
    QString finalName = QStringLiteral("AGV-") + digits;
    if (!letters.isEmpty())
        finalName += QStringLiteral("-") + letters.toUpper();
    return finalName;
}

void populateModelNames(QComboBox *modelBox)
{
    if (!modelBox)
        return;

    const QStringList names = loadModelNames();
    if (names.isEmpty()) {
        modelBox->addItem(QStringLiteral("Нет моделей"));
        return;
    }

    for (const QString &n : names)
        modelBox->addItem(n);
}

}

// ===============================
// Реализация AgvMaskedEdit (исправленная версия)
// ===============================

AgvMaskedEdit::AgvMaskedEdit(QWidget *parent)
    : QLineEdit(parent),
      digitBuf(4, QChar('_')),
      letterBuf(10, QChar('_'))
{
    setReadOnly(true);
    setCursor(Qt::IBeamCursor);

    // фиксируем шрифт, чтобы всё было ровно
    QFont f("Consolas");   // или "Courier New"
    f.setPointSize(15);
    setFont(f);

}

QString AgvMaskedEdit::digits() const
{
    QString out;
    for (auto c : digitBuf)
        if (c.isDigit())
            out += c;
    return out;
}

QString AgvMaskedEdit::letters() const
{
    QString out;
    for (auto c : letterBuf)
        if (c.isLetter())
            out += c;
    return out;
}

void AgvMaskedEdit::focusOutEvent(QFocusEvent *e)
{
    cursorIndex = -1;
    update();
    QLineEdit::focusOutEvent(e);
}

void AgvMaskedEdit::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QStyleOptionFrame opt;
    initStyleOption(&opt);
    style()->drawPrimitive(QStyle::PE_PanelLineEdit, &opt, &p, this);

    QFontMetrics fm(font());
    int x = 8;
    int y = height() / 2 + fm.ascent() / 2;

    // маска
    QString base = "AGV-____-__________";
    for (int i = 0; i < base.size(); ++i) {
        p.setPen(Qt::black);
        p.drawText(x, y, QString(base[i]));
        x += fm.horizontalAdvance(base[i]);
    }

    // цифры
    x = 8 + fm.horizontalAdvance("AGV-");
    for (int i = 0; i < 4; ++i) {
        if (digitBuf[i].isDigit()) {
            p.setPen(Qt::black);
            p.drawText(x, y, QString(digitBuf[i]));   // ← РОВНО
        }
        x += fm.horizontalAdvance("_");
    }

    // буквы
    x = 8 + fm.horizontalAdvance("AGV-____-");
    for (int i = 0; i < 10; ++i) {
        if (letterBuf[i].isLetter()) {
            p.setPen(Qt::black);
            p.drawText(x, y, QString(letterBuf[i]));  // ← РОВНО
        }
        x += fm.horizontalAdvance("_");
    }

    // курсор
    if (cursorIndex < 0)
        return;

    int cursorX;
    if (cursorIndex < 4)
        cursorX = 8 + fm.horizontalAdvance("AGV-") + fm.horizontalAdvance("_") * cursorIndex;
    else
        cursorX = 8 + fm.horizontalAdvance("AGV-____-") + fm.horizontalAdvance("_") * (cursorIndex - 4);

    p.setPen(QColor("#0F00DB"));
    p.drawLine(cursorX, y + 2, cursorX, y - fm.height());

    const_cast<AgvMaskedEdit*>(this)->setCursorPosition(0);
}

void AgvMaskedEdit::mousePressEvent(QMouseEvent *e)
{
    Q_UNUSED(e);

    for (int i = 0; i < 4; i++)
        if (digitBuf[i] == '_') { cursorIndex = i; updateCursor(); return; }

    for (int i = 0; i < 10; i++)
        if (letterBuf[i] == '_') { cursorIndex = 4 + i; updateCursor(); return; }

    cursorIndex = 14;
    updateCursor();
}

void AgvMaskedEdit::moveCursorLeft()
{
    if (cursorIndex > 0)
        cursorIndex--;
    updateCursor();
}

void AgvMaskedEdit::moveCursorRight()
{
    if (cursorIndex < 14)
        cursorIndex++;
    updateCursor();
}

void AgvMaskedEdit::updateCursor()
{
    setCursorPosition(0);
    update();
}

void AgvMaskedEdit::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Left)  { moveCursorLeft();  return; }
    if (e->key() == Qt::Key_Right) { moveCursorRight(); return; }
    if (e->key() == Qt::Key_Tab)   { focusNextChild();  return; }

    // Backspace
    if (e->key() == Qt::Key_Backspace) {
        if (cursorIndex > 0) {
            cursorIndex--;
            if (cursorIndex < 4)
                digitBuf[cursorIndex] = '_';
            else
                letterBuf[cursorIndex - 4] = '_';

            emit contentChanged();
            updateCursor();
        }
        return;
    }

    // Delete
    if (e->key() == Qt::Key_Delete) {
        if (cursorIndex < 14) {
            if (cursorIndex < 4)
                digitBuf[cursorIndex] = '_';
            else
                letterBuf[cursorIndex - 4] = '_';

            emit contentChanged();
            updateCursor();
        }
        return;
    }

    QString t = e->text();
    if (t.isEmpty())
        return;

    QChar c = t[0];

    // цифры
    if (cursorIndex < 4 && c.isDigit()) {
        digitBuf[cursorIndex] = c;
        cursorIndex++;
        emit contentChanged();
        updateCursor();
        return;
    }

    // буквы (только латиница)
    if (cursorIndex >= 4 && cursorIndex < 14 &&
        c.isLetter() &&
        ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
    {
        letterBuf[cursorIndex - 4] = c;
        cursorIndex++;
        emit contentChanged();
        updateCursor();
        return;
    }
}

// ===============================
// Реализация SnMaskedEdit
// ===============================

// ===============================
// Реализация SnMaskedEdit (c "SN-")
// ===============================

SnMaskedEdit::SnMaskedEdit(QWidget *parent)
    : QLineEdit(parent),
      buf(10, QChar('_'))
{
    setReadOnly(true);
    setCursor(Qt::IBeamCursor);
    setObjectName("serialEdit");
}

QString SnMaskedEdit::digits() const
{
    QString out;
    for (auto c : buf)
        if (c.isDigit())
            out += c;
    return out;
}

void SnMaskedEdit::focusOutEvent(QFocusEvent *e)
{
    cursorIndex = -1; // скрываем кастомный курсор
    update();
    QLineEdit::focusOutEvent(e);
}

void SnMaskedEdit::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QStyleOptionFrame opt;
    initStyleOption(&opt);
    style()->drawPrimitive(QStyle::PE_PanelLineEdit, &opt, &p, this);

    QFontMetrics fm(font());
    int x = 8;
    int y = height() / 2 + fm.ascent() / 2;

    // рисуем "SN-"
    p.setPen(Qt::black);
    p.drawText(x, y, "SN-");
    x += fm.horizontalAdvance("SN-");

    // подчёркивания + цифры
    for (int i = 0; i < 10; i++) {
        p.setPen(Qt::black);
        p.drawText(x, y, "_");

        if (buf[i].isDigit()) {
            p.drawText(x, y - fm.ascent() * 0.05, QString(buf[i]));
        }

        x += fm.horizontalAdvance("_");
    }

    // если курсор скрыт — выходим
    if (cursorIndex < 0)
        return;

    // курсор после "SN-"
    int cursorX = 8 + fm.horizontalAdvance("SN-") + fm.horizontalAdvance("_") * cursorIndex;

    p.setPen(QColor("#0F00DB"));
    p.drawLine(cursorX, y + 2, cursorX, y - fm.height());

    // скрываем системный курсор
    const_cast<SnMaskedEdit*>(this)->setCursorPosition(0);
}

void SnMaskedEdit::mousePressEvent(QMouseEvent *e)
{
    Q_UNUSED(e);

    // курсор всегда в конец
    for (int i = 0; i < 10; i++)
        if (buf[i] == '_') { cursorIndex = i; updateCursor(); return; }

    cursorIndex = 10;
    updateCursor();
}

void SnMaskedEdit::updateCursor()
{
    setCursorPosition(0);
    update();
}

void SnMaskedEdit::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Left) {
        if (cursorIndex > 0) cursorIndex--;
        updateCursor();
        return;
    }

    if (e->key() == Qt::Key_Right) {
        if (cursorIndex < 10) cursorIndex++;
        updateCursor();
        return;
    }

    if (e->key() == Qt::Key_Tab) {
        focusNextChild();
        return;
    }

    // Backspace
    if (e->key() == Qt::Key_Backspace) {
        if (cursorIndex > 0) {
            cursorIndex--;
            buf[cursorIndex] = '_';
            emit contentChanged();
            updateCursor();
        }
        return;
    }

    // Delete
    if (e->key() == Qt::Key_Delete) {
        if (cursorIndex < 10) {
            buf[cursorIndex] = '_';
            emit contentChanged();
            updateCursor();
        }
        return;
    }

    QString t = e->text();
    if (t.isEmpty())
        return;

    QChar c = t[0];

    // цифры
    if (c.isDigit() && cursorIndex < 10) {
        buf[cursorIndex] = c;
        cursorIndex++;
        emit contentChanged();
        updateCursor();
        return;
    }
}

// ===============================
// Реализация AddAgvDialog
// ===============================

AddAgvDialog::AddAgvDialog(std::function<int(int)> scale, QWidget *parent)
    : QDialog(parent), s(scale)
{
    setWindowTitle("Добавить AGV");
    setMinimumSize(s(460), s(560));
    resize(s(520), s(640));
    setSizeGripEnabled(true);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(s(20), s(20), s(20), s(20));
    root->setSpacing(s(10));

    const QString lineEditStyle = addDialogLineEditStyle();
    const QString comboStyle = addDialogComboStyle();

    //
    // ===== NAME (AGV) =====
    //
    root->addWidget(makeSectionTitle(this, "Название AGV", s(15), s(7), s(4)));
    nameEdit = new AgvMaskedEdit(this);
    nameEdit->setStyleSheet(lineEditStyle);
    root->addWidget(nameEdit);

    nameError = makeErrorLabel(this);
    root->addWidget(nameError);

    //
    // ===== SERIAL (SN) =====
    //
    root->addWidget(makeSectionTitle(this, "Серийный номер (10 цифр)", s(15), s(7), s(4)));
    serialEdit = new SnMaskedEdit(this);
    serialEdit->setStyleSheet(lineEditStyle);
    root->addWidget(serialEdit);

    serialError = makeErrorLabel(this);
    root->addWidget(serialError);

    //
    // ===== STATUS =====
    //
    root->addWidget(makeSectionTitle(this, "Статус", s(15), s(7), s(4)));
    statusBox = new QComboBox(this);
    statusBox->addItem("online");
    statusBox->addItem("offline");
    statusBox->setStyleSheet(comboStyle);
    root->addWidget(statusBox);

    //
    // ===== MODEL =====
    //
    root->addWidget(makeSectionTitle(this, "Модель AGV", s(15), s(7), s(4)));
    modelBox = new QComboBox(this);
    modelBox->setStyleSheet(comboStyle);

    populateModelNames(modelBox);

    root->addWidget(modelBox);


    //
    // ===== ALIAS =====
    //
    root->addWidget(makeSectionTitle(this, "Заметка (необязательно)", s(15), s(7), s(4)));
    aliasEdit = new QLineEdit(this);
    aliasEdit->setPlaceholderText("...");
    aliasEdit->setMaxLength(40);
    aliasEdit->setStyleSheet(lineEditStyle);
    root->addWidget(aliasEdit);

    aliasError = makeErrorLabel(this);
    root->addWidget(aliasError);

    //
    // ===== BUTTONS =====
    //
    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *cancel = new QPushButton("Отмена", this);
    addBtn = new QPushButton("Добавить", this);

    cancel->setStyleSheet(
        "padding:10px 22px;"
        "font-size:16px;"
        "border:1px solid #C8C8C8;"
        "border-radius:10px;"
        "background:#F7F7F7;"
        "color:#333;"
    );

    addBtn->setStyleSheet(
        "padding:10px 22px;"
        "font-size:16px;"
        "border-radius:10px;"
        "background:#28A745;"
        "color:white;"
    );

    addBtn->setEnabled(false);

    btns->addWidget(cancel);
    btns->addStretch();
    btns->addWidget(addBtn);

    root->addStretch();
    root->addLayout(btns);

    //
    // SIGNALS
    //
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    connect(nameEdit,   &AgvMaskedEdit::contentChanged, this, &AddAgvDialog::validateAll);
    connect(serialEdit, &SnMaskedEdit::contentChanged,  this, &AddAgvDialog::validateAll);
    connect(aliasEdit,  &QLineEdit::textChanged,        this, &AddAgvDialog::validateAll);

    connect(addBtn, &QPushButton::clicked, this, [this](){
        const QString digits = nameEdit->digits();
        const QString letters = nameEdit->letters();

        result.name   = buildAgvName(digits, letters);
        result.serial = serialEdit->digits();
        result.status = statusBox->currentText();
        result.model  = modelBox->currentText();
        result.alias  = aliasEdit->text().trimmed();

        accept();
    });

    if (qApp->property("autotest_running").toBool()) {
        for (int attempt = 0; attempt < 8; ++attempt) {
            QTimer::singleShot(140 + attempt * 120, this, [this]() {
                if (isVisible())
                    reject();
            });
        }
    }

    validateAll();
}
// ===============================
// Валидация
// ===============================

void AddAgvDialog::validateAll()
{
    bool ok =
        validateName() &
        validateSerial() &
        validateAlias();

    addBtn->setEnabled(ok);
}

bool AddAgvDialog::validateName()
{
    QString digits  = nameEdit->digits();
    QString letters = nameEdit->letters();

    if (digits.isEmpty()) {
        nameError->setText("Нужно ввести хотя бы одну цифру");
        nameError->show();
        return false;
    }

    if (digits.size() > 4) {
        nameError->setText("Максимум 4 цифры");
        nameError->show();
        return false;
    }

    if (letters.size() > 10) {
        nameError->setText("Максимум 10 букв");
        nameError->show();
        return false;
    }

    nameError->hide();
    return true;
}

bool AddAgvDialog::validateSerial()
{
    QString d = serialEdit->digits();

    if (d.isEmpty()) {
        serialError->setText("Введите хотя бы одну цифру");
        serialError->show();
        return false;
    }

    if (d.size() > 10) {
        serialError->setText("Максимум 10 цифр");
        serialError->show();
        return false;
    }

    serialError->hide();
    return true;
}

bool AddAgvDialog::validateAlias()
{
    if (aliasEdit->text().size() > 40) {
        aliasError->setText("Максимум 40 символов");
        aliasError->show();
        return false;
    }

    aliasError->hide();
    return true;
}
// ===============================
// eventFilter
// ===============================

bool AddAgvDialog::eventFilter(QObject *obj, QEvent *event)
{
    // SN теперь кастомный, но оставляем защиту от IME
    if (obj == serialEdit)
    {
        // Блокируем ввод через IME (китайская/японская клавиатура)
        if (event->type() == QEvent::InputMethod)
            return true;

        // Блокируем попытки выделить текст мышью
        if (event->type() == QEvent::MouseButtonDblClick ||
            event->type() == QEvent::MouseButtonPress)
        {
            serialEdit->setFocus();
            return true;
        }
    }

    return QDialog::eventFilter(obj, event);

}
