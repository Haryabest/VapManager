#include "addagvdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QTimer>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

static QStringList loadAgvModels()
{
    QStringList list;

    QSqlQuery q("SELECT name FROM agv_models ORDER BY name ASC");

    if (!q.isActive()) {
        qDebug() << "Ошибка загрузки моделей:" << q.lastError().text();
        return list;
    }

    while (q.next())
        list << q.value(0).toString();

    return list;
}

AddAgvDialog::AddAgvDialog(std::function<int(int)> scale, QWidget *parent)
    : QDialog(parent), s(scale)
{
    setWindowTitle("Добавить AGV");
    setFixedSize(s(520), s(640));

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(s(20), s(20), s(20), s(20));
    root->setSpacing(s(10));

    auto makeTitle = [&](const QString &t){
        QLabel *lbl = new QLabel(t, this);
        lbl->setStyleSheet(QString(
            "font-family:Inter;"
            "font-size:%1px;"
            "font-weight:600;"
            "color:#222;"
            "margin-top:%2px;"
            "margin-bottom:%3px;"
        ).arg(s(15)).arg(s(7)).arg(s(4)));
        return lbl;
    };

    auto makeError = [&](){
        QLabel *e = new QLabel(this);
        e->setStyleSheet(
            "color:#D00000;"
            "font-size:13px;"
            "font-family:Inter;"
            "margin-top:2px;"
        );
        e->hide();
        return e;
    };

    QString lineEditStyle =
        "padding:8px 10px;"
        "font-size:15px;"
        "border:1px solid #D0D0D0;"
        "border-radius:8px;"
        "background:white;"
        "selection-background-color:#0F00DB22;"
        "selection-color:black;";

    QString comboStyle =
        "QComboBox {"
        "   border:1px solid #D0D0D0;"
        "   border-radius:0px;"
        "   padding:6px 8px;"
        "   background:white;"
        "   font-size:15px;"
        "}"
        "QComboBox QAbstractItemView {"
        "   selection-background-color:#0F00DB22;"
        "   selection-color:black;"
        "   outline:0;"
        "}";

    // ===== NAME (AGV_) =====
    root->addWidget(makeTitle("Название (AGV_###)"));
    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText("AGV______");
    nameEdit->setText("AGV_");
    nameEdit->setCursorPosition(4);
    nameEdit->setStyleSheet(lineEditStyle);
    root->addWidget(nameEdit);

    nameError = makeError();
    root->addWidget(nameError);

    // ===== SERIAL (SN-) =====
    root->addWidget(makeTitle("Серийный номер"));
    serialEdit = new QLineEdit(this);
    serialEdit->setText("SN-");
    serialEdit->setCursorPosition(3);
    serialEdit->setStyleSheet(lineEditStyle);
    root->addWidget(serialEdit);

    serialError = makeError();
    root->addWidget(serialError);

    // ===== STATUS =====
    root->addWidget(makeTitle("Статус"));
    statusBox = new QComboBox(this);
    statusBox->addItem("online");
    statusBox->addItem("offline");
    statusBox->setStyleSheet(comboStyle);
    root->addWidget(statusBox);

    // ===== MODEL (из БД agv_models.name) =====
    root->addWidget(makeTitle("Модель AGV"));
    modelBox = new QComboBox(this);
    modelBox->setStyleSheet(comboStyle);

    QStringList models = loadAgvModels();
    if (models.isEmpty())
        modelBox->addItem("Нет моделей");
    else
        modelBox->addItems(models);

    root->addWidget(modelBox);

    // ===== ALIAS =====
    root->addWidget(makeTitle("Добавить заметку (необязательно)"));
    aliasEdit = new QLineEdit(this);
    aliasEdit->setPlaceholderText("Добавить заметку");
    aliasEdit->setStyleSheet(lineEditStyle);
    root->addWidget(aliasEdit);

    aliasError = makeError();
    root->addWidget(aliasError);

    // ===== BUTTONS =====
    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *cancel = new QPushButton("Отмена", this);
    addBtn = new QPushButton("Добавить", this);

    QString cancelStyle =
        "QPushButton {"
        "   padding:10px 22px;"
        "   font-size:16px;"
        "   border:1px solid #C8C8C8;"
        "   border-radius:10px;"
        "   background:#F7F7F7;"
        "   color:#333;"
        "}"
        "QPushButton:hover { background:#EDEDED; }"
        "QPushButton:pressed { background:#DCDCDC; }";

    QString addStyle =
        "QPushButton {"
        "   padding:10px 22px;"
        "   font-size:16px;"
        "   border-radius:10px;"
        "   background:#A0DFA0;"
        "   color:white;"
        "}"
        "QPushButton:enabled { background:#28A745; }"
        "QPushButton:hover:enabled { background:#2EC24F; }"
        "QPushButton:pressed:enabled { background:#1F8A36; }";

    cancel->setStyleSheet(cancelStyle);
    addBtn->setStyleSheet(addStyle);
    addBtn->setEnabled(false);

    btns->addWidget(cancel);
    btns->addStretch();
    btns->addWidget(addBtn);

    root->addStretch();
    root->addLayout(btns);

    // SIGNALS
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    connect(nameEdit,   &QLineEdit::textChanged, this, &AddAgvDialog::validateAll);
    connect(serialEdit, &QLineEdit::textChanged, this, &AddAgvDialog::validateAll);
    connect(aliasEdit,  &QLineEdit::textChanged, this, &AddAgvDialog::validateAll);

    connect(addBtn, &QPushButton::clicked, this, [this](){
        result.name   = nameEdit->text().toUpper();
        result.serial = serialEdit->text();
        result.status = statusBox->currentText();
        result.model  = modelBox->currentText().toUpper();
        result.alias  = aliasEdit->text();
        accept();
    });

    // EVENT FILTERS
    nameEdit->installEventFilter(this);
    serialEdit->installEventFilter(this);
    aliasEdit->installEventFilter(this);
}

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
    QString t = nameEdit->text();

    if (!t.contains(QRegularExpression("^AGV_[0-9]{3,}$",
        QRegularExpression::CaseInsensitiveOption)))
    {
        nameError->setText("Формат: AGV_ + минимум 3 цифры");
        nameError->show();
        return false;
    }

    nameError->hide();
    return true;
}

bool AddAgvDialog::validateSerial()
{
    QString t = serialEdit->text();

    if (!t.contains(QRegularExpression("^SN-[0-9]+$")))
    {
        serialError->setText("После SN- можно вводить только цифры");
        serialError->show();
        return false;
    }

    serialError->hide();
    return true;
}

bool AddAgvDialog::validateAlias()
{
    QString t = aliasEdit->text();

    if (t.contains(" "))
    {
        aliasError->setText("Пробелы запрещены");
        aliasError->show();
        return false;
    }

    aliasError->hide();
    return true;
}

bool AddAgvDialog::eventFilter(QObject *obj, QEvent *event)
{
    //
    // ===== AGV_ (nameEdit) =====
    //
    if (obj == nameEdit)
    {
        if (event->type() == QEvent::FocusIn ||
            event->type() == QEvent::MouseButtonPress)
        {
            QTimer::singleShot(0, [this](){
                if (nameEdit->cursorPosition() < 4)
                    nameEdit->setCursorPosition(4);
            });
        }

        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent *key = static_cast<QKeyEvent*>(event);

            // Запрет удаления префикса
            if ((key->key() == Qt::Key_Backspace && nameEdit->cursorPosition() <= 4) ||
                (key->key() == Qt::Key_Delete   && nameEdit->cursorPosition() <  4))
            {
                return true;
            }

            // После AGV_ — только цифры
            if (nameEdit->cursorPosition() >= 4)
            {
                if (!key->text().contains(QRegularExpression("[0-9]")))
                    return true;
            }
        }

        // Автовосстановление префикса
        if (event->type() == QEvent::KeyRelease)
        {
            if (!nameEdit->text().startsWith("AGV_"))
            {
                QString t = nameEdit->text();
                t.remove(QRegularExpression("^[^0-9]*"));
                nameEdit->setText("AGV_" + t);
                nameEdit->setCursorPosition(nameEdit->text().length());
            }
        }
    }

    //
    // ===== SN- (serialEdit) =====
    //
    if (obj == serialEdit)
    {
        if (event->type() == QEvent::FocusIn ||
            event->type() == QEvent::MouseButtonPress)
        {
            QTimer::singleShot(0, [this](){
                if (serialEdit->cursorPosition() < 3)
                    serialEdit->setCursorPosition(3);
            });
        }

        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent *key = static_cast<QKeyEvent*>(event);

            if ((key->key() == Qt::Key_Backspace && serialEdit->cursorPosition() <= 3) ||
                (key->key() == Qt::Key_Delete   && serialEdit->cursorPosition() <  3))
            {
                return true;
            }

            if (serialEdit->cursorPosition() >= 3)
            {
                if (!key->text().contains(QRegularExpression("[0-9]")))
                    return true;
            }
        }

        if (event->type() == QEvent::KeyRelease)
        {
            if (!serialEdit->text().startsWith("SN-"))
            {
                QString t = serialEdit->text();
                t.remove(QRegularExpression("^[^0-9]*"));
                serialEdit->setText("SN-" + t);
                serialEdit->setCursorPosition(serialEdit->text().length());
            }
        }
    }

    //
    // ===== ALIAS =====
    //
    if (obj == aliasEdit && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *key = static_cast<QKeyEvent*>(event);

        if (key->key() == Qt::Key_Space)
        {
            aliasError->setText("Пробелы запрещены");
            aliasError->show();
            return true;
        }
    }

    return QDialog::eventFilter(obj, event);
}
