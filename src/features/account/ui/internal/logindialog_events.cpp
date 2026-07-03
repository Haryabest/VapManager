#include "../logindialog.h"

#include <QCloseEvent>
#include <QKeyEvent>

void LoginDialog::closeEvent(QCloseEvent *event)
{
    if (stack->currentIndex() == 3) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void LoginDialog::keyPressEvent(QKeyEvent *event)
{
    if (stack->currentIndex() == 3 && event->key() == Qt::Key_Escape) {
        event->ignore();
        return;
    }
    QDialog::keyPressEvent(event);
}
