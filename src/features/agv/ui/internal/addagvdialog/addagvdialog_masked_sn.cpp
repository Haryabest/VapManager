#include "addagvdialog.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QFontMetrics>
#include <QStyle>
#include <QStyleOptionFrame>

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
    cursorIndex = -1;
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

    p.setPen(Qt::black);
    p.drawText(x, y, "SN-");
    x += fm.horizontalAdvance("SN-");

    for (int i = 0; i < 10; i++) {
        p.setPen(Qt::black);
        p.drawText(x, y, "_");

        if (buf[i].isDigit()) {
            p.drawText(x, y - fm.ascent() * 0.05, QString(buf[i]));
        }

        x += fm.horizontalAdvance("_");
    }

    if (cursorIndex < 0)
        return;

    int cursorX = 8 + fm.horizontalAdvance("SN-") + fm.horizontalAdvance("_") * cursorIndex;

    p.setPen(QColor("#0F00DB"));
    p.drawLine(cursorX, y + 2, cursorX, y - fm.height());

    const_cast<SnMaskedEdit*>(this)->setCursorPosition(0);
}

void SnMaskedEdit::mousePressEvent(QMouseEvent *e)
{
    Q_UNUSED(e);

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

    if (e->key() == Qt::Key_Backspace) {
        if (cursorIndex > 0) {
            cursorIndex--;
            buf[cursorIndex] = '_';
            emit contentChanged();
            updateCursor();
        }
        return;
    }

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

    if (c.isDigit() && cursorIndex < 10) {
        buf[cursorIndex] = c;
        cursorIndex++;
        emit contentChanged();
        updateCursor();
        return;
    }
}
