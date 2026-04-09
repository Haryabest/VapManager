#include "addagvdialog.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QFontMetrics>
#include <QStyle>
#include <QStyleOptionFrame>

AgvMaskedEdit::AgvMaskedEdit(QWidget *parent)
    : QLineEdit(parent),
      digitBuf(4, QChar('_')),
      letterBuf(10, QChar('_'))
{
    setReadOnly(true);
    setCursor(Qt::IBeamCursor);

    QFont f("Consolas");
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

    QString base = "AGV-____-__________";
    for (int i = 0; i < base.size(); ++i) {
        p.setPen(Qt::black);
        p.drawText(x, y, QString(base[i]));
        x += fm.horizontalAdvance(base[i]);
    }

    x = 8 + fm.horizontalAdvance("AGV-");
    for (int i = 0; i < 4; ++i) {
        if (digitBuf[i].isDigit()) {
            p.setPen(Qt::black);
            p.drawText(x, y, QString(digitBuf[i]));
        }
        x += fm.horizontalAdvance("_");
    }

    x = 8 + fm.horizontalAdvance("AGV-____-");
    for (int i = 0; i < 10; ++i) {
        if (letterBuf[i].isLetter()) {
            p.setPen(Qt::black);
            p.drawText(x, y, QString(letterBuf[i]));
        }
        x += fm.horizontalAdvance("_");
    }

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

    if (cursorIndex < 4 && c.isDigit()) {
        digitBuf[cursorIndex] = c;
        cursorIndex++;
        emit contentChanged();
        updateCursor();
        return;
    }

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
