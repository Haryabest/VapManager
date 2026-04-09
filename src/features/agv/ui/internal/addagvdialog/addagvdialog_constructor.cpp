#include "addagvdialog.h"
#include "db_models.h"

#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

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

    root->addWidget(makeSectionTitle(this, "Название AGV", s(15), s(7), s(4)));
    nameEdit = new AgvMaskedEdit(this);
    nameEdit->setStyleSheet(lineEditStyle);
    root->addWidget(nameEdit);

    nameError = makeErrorLabel(this);
    root->addWidget(nameError);

    root->addWidget(makeSectionTitle(this, "Серийный номер (10 цифр)", s(15), s(7), s(4)));
    serialEdit = new SnMaskedEdit(this);
    serialEdit->setStyleSheet(lineEditStyle);
    root->addWidget(serialEdit);

    serialError = makeErrorLabel(this);
    root->addWidget(serialError);

    root->addWidget(makeSectionTitle(this, "Статус", s(15), s(7), s(4)));
    statusBox = new QComboBox(this);
    statusBox->addItem("online");
    statusBox->addItem("offline");
    statusBox->setStyleSheet(comboStyle);
    root->addWidget(statusBox);

    root->addWidget(makeSectionTitle(this, "Модель AGV", s(15), s(7), s(4)));
    modelBox = new QComboBox(this);
    modelBox->setStyleSheet(comboStyle);

    populateModelNames(modelBox);

    root->addWidget(modelBox);

    root->addWidget(makeSectionTitle(this, "Заметка (необязательно)", s(15), s(7), s(4)));
    aliasEdit = new QLineEdit(this);
    aliasEdit->setPlaceholderText("...");
    aliasEdit->setMaxLength(40);
    aliasEdit->setStyleSheet(lineEditStyle);
    root->addWidget(aliasEdit);

    aliasError = makeErrorLabel(this);
    root->addWidget(aliasError);

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
