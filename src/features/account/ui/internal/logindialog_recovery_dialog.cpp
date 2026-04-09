#include "../logindialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>

void LoginDialog::showRecoveryKeyDialog(const QString &username, const QString &recoveryKey)
{
    QDialog dlg(this);
    dlg.setWindowTitle("Ключ восстановления");
    dlg.setModal(true);
    dlg.setFixedSize(460, 370);
    dlg.setStyleSheet(
        "QDialog { background: #F5F7FB; }"
        "QFrame#keyCard { background: white; border: 1px solid #E4E8F0; border-radius: 14px; }"
        "QLabel#title { font-family: Inter; font-size: 20px; font-weight: 900; color: #0F172A; }"
        "QLabel#subtitle { font-family: Inter; font-size: 13px; font-weight: 500; color: #5B6475; }"
        "QLabel#keyLabel { font-family: Consolas, monospace; font-size: 15px; font-weight: 700; color: #0F172A; background: #F1F5F9; padding: 14px 16px; border-radius: 8px; }"
        "QPushButton { font-family: Inter; font-size: 14px; font-weight: 700; border-radius: 8px; padding: 10px 16px; }"
        "QPushButton#copyBtn { background: #0F00DB; color: white; border: none; }"
        "QPushButton#copyBtn:hover { background: #1A4ACD; }"
        "QPushButton#saveBtn { background: #EDF1FF; color: #182B7A; border: 1px solid #CFD8F4; }"
        "QPushButton#saveBtn:hover { background: #E3E9FB; }"
        "QPushButton#okBtn { background: #10B981; color: white; border: none; }"
        "QPushButton#okBtn:hover { background: #059669; }"
    );

    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(0);

    QFrame *keyCard = new QFrame(&dlg);
    keyCard->setObjectName("keyCard");
    QVBoxLayout *cardLayout = new QVBoxLayout(keyCard);
    cardLayout->setContentsMargins(20, 16, 20, 16);
    cardLayout->setSpacing(10);

    QLabel *title = new QLabel("Сохраните ключ восстановления!", keyCard);
    title->setObjectName("title");
    cardLayout->addWidget(title);

    QLabel *subtitle = new QLabel(QString("Аккаунт: %1\nЭтот ключ потребуется для восстановления доступа.").arg(username), keyCard);
    subtitle->setObjectName("subtitle");
    subtitle->setWordWrap(true);
    cardLayout->addWidget(subtitle);

    cardLayout->addSpacing(6);

    QLabel *keyLabel = new QLabel(recoveryKey, keyCard);
    keyLabel->setObjectName("keyLabel");
    keyLabel->setAlignment(Qt::AlignCenter);
    keyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cardLayout->addWidget(keyLabel);

    cardLayout->addSpacing(10);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    QPushButton *copyBtn = new QPushButton("Копировать", keyCard);
    copyBtn->setObjectName("copyBtn");
    connect(copyBtn, &QPushButton::clicked, this, [recoveryKey, copyBtn]() {
        QApplication::clipboard()->setText(recoveryKey);
        copyBtn->setText("Скопировано!");
    });
    btnRow->addWidget(copyBtn);

    QPushButton *saveBtn = new QPushButton("Сохранить в файл", keyCard);
    saveBtn->setObjectName("saveBtn");
    connect(saveBtn, &QPushButton::clicked, this, [&dlg, username, recoveryKey]() {
        QString path = QFileDialog::getSaveFileName(&dlg, "Сохранить ключ", QString("recovery_key_%1.txt").arg(username), "Text files (*.txt)");
        if (!path.isEmpty()) {
            QFile f(path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&f);
                out << "=== Ключ восстановления ===" << "\n";
                out << "Аккаунт: " << username << "\n";
                out << "Ключ: " << recoveryKey << "\n";
                out << "Дата: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
                f.close();
            }
        }
    });
    btnRow->addWidget(saveBtn);

    cardLayout->addLayout(btnRow);

    QPushButton *okBtn = new QPushButton("Готово", keyCard);
    okBtn->setObjectName("okBtn");
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    cardLayout->addWidget(okBtn);

    root->addWidget(keyCard);
    dlg.exec();
}
