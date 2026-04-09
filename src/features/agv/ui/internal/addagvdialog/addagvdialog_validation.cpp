#include "addagvdialog.h"

#include <QEvent>
#include <QPushButton>

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

bool AddAgvDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == serialEdit)
    {
        if (event->type() == QEvent::InputMethod)
            return true;

        if (event->type() == QEvent::MouseButtonDblClick ||
            event->type() == QEvent::MouseButtonPress)
        {
            serialEdit->setFocus();
            return true;
        }
    }

    return QDialog::eventFilter(obj, event);
}
