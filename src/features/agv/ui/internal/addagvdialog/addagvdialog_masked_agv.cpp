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
    caretIndex = -1;
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

    if (caretIndex < 0)
        return;

    int caretX;
    if (caretIndex < 4)
        caretX = 8 + fm.horizontalAdvance("AGV-") + fm.horizontalAdvance("_") * caretIndex;
    else
        caretX = 8 + fm.horizontalAdvance("AGV-____-") + fm.horizontalAdvance("_") * (caretIndex - 4);

    p.setPen(QColor("#0F00DB"));
    p.drawLine(caretX, y + 2, caretX, y - fm.height());

    const_cast<AgvMaskedEdit*>(this)->setCursorPosition(0);
}

void AgvMaskedEdit::mousePressEvent(QMouseEvent *e)
{
    Q_UNUSED(e);

    for (int i = 0; i < 4; i++)
        if (digitBuf[i] == '_') { caretIndex = i; updateCaret(); return; }

    for (int i = 0; i < 10; i++)
        if (letterBuf[i] == '_') { caretIndex = 4 + i; updateCaret(); return; }

    caretIndex = 14;
    updateCaret();
}

void AgvMaskedEdit::moveCaretLeft()
{
    if (caretIndex > 0)
        caretIndex--;
    updateCaret();
}

void AgvMaskedEdit::moveCaretRight()
{
    if (caretIndex < 14)
        caretIndex++;
    updateCaret();
}

void AgvMaskedEdit::updateCaret()
{
    setCursorPosition(0);
    update();
}

void AgvMaskedEdit::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Left)  { moveCaretLeft();  return; }
    if (e->key() == Qt::Key_Right) { moveCaretRight(); return; }
    if (e->key() == Qt::Key_Tab)   { focusNextChild();  return; }

    if (e->key() == Qt::Key_Backspace) {
        if (caretIndex > 0) {
            caretIndex--;
            if (caretIndex < 4)
                digitBuf[caretIndex] = '_';
            else
                letterBuf[caretIndex - 4] = '_';

            emit contentChanged();
            updateCaret();
        }
        return;
    }

    if (e->key() == Qt::Key_Delete) {
        if (caretIndex < 14) {
            if (caretIndex < 4)
                digitBuf[caretIndex] = '_';
            else
                letterBuf[caretIndex - 4] = '_';

            emit contentChanged();
            updateCaret();
        }
        return;
    }

    QString t = e->text();
    if (t.isEmpty())
        return;

    QChar c = t[0];

    if (caretIndex < 4 && c.isDigit()) {
        digitBuf[caretIndex] = c;
        caretIndex++;
        emit contentChanged();
        updateCaret();
        return;
    }

    if (caretIndex >= 4 && caretIndex < 14 &&
        c.isLetter() &&
        ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
    {
        letterBuf[caretIndex - 4] = c;
        caretIndex++;
        emit contentChanged();
        updateCaret();
        return;
    }
}
