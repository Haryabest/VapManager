#include "../logindialog.h"
#include "db_users.h"

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>

void LoginDialog::wireSignals()
{
    connect(btnLogin_, SIGNAL(clicked()), this, SLOT(onLoginClicked()));
    connect(btnReg_, SIGNAL(clicked()), this, SLOT(onRegisterClicked()));

    connect(btnRecovery_, &QPushButton::clicked, this, [this]() {
        stack->setCurrentIndex(2);
    });

    connect(btnRecoveryBack_, &QPushButton::clicked, this, [this]() {
        recoveryError->clear();
        recoveryKeyEdit->clear();
        stack->setCurrentIndex(0);
    });

    connect(recoveryKeyEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        recoveryError->clear();
    });

    auto loginByRecoveryKey = [this](const QString &rawKey) {
        recoveryError->clear();
        const QString key = rawKey.trimmed().toUpper();

        if (key.isEmpty()) {
            recoveryError->setText("Введите ключ восстановления");
            return;
        }

        QString username;
        QString error;
        if (!verifyPermanentRecoveryKey(key, username, error)) {
            recoveryError->setText(error);
            return;
        }

        UserInfo u;
        u.username = username;
        u.isActive = true;

        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare("SELECT id, role, is_active FROM users WHERE username = :u");
            q.bindValue(":u", username);
            if (q.exec() && q.next()) {
                u.id = q.value(0).toInt();
                u.role = q.value(1).toString();
                u.isActive = q.value(2).toInt() == 1;
            }
        }

        if (!u.isActive) {
            recoveryError->setText("Аккаунт заблокирован");
            return;
        }

        enableRememberMe(username);
        user_ = u;
        logAction(username, "login_by_recovery_key", "Вход выполнен по ключу восстановления");
        recoveryKeyEdit->clear();
        accept();
    };

    connect(btnRecoveryOk_, &QPushButton::clicked, this, [this, loginByRecoveryKey]() {
        loginByRecoveryKey(recoveryKeyEdit->text());
    });

    connect(btnRecoveryFromFile_, &QPushButton::clicked, this, [this, loginByRecoveryKey]() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            "Выберите файл с ключом",
            QString(),
            "Text files (*.txt);;All files (*.*)"
        );
        if (path.isEmpty())
            return;

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            recoveryError->setText("Не удалось открыть файл");
            return;
        }

        const QString content = QString::fromUtf8(f.readAll());
        f.close();

        QString key;
        const QRegularExpression rkRe("RK-[A-Za-z0-9]{4}-[A-Za-z0-9]{4}-[A-Za-z0-9]{4}");
        const QRegularExpressionMatch m = rkRe.match(content);
        if (m.hasMatch())
            key = m.captured(0);

        if (key.isEmpty()) {
            const QStringList lines = content.split('\n');
            for (const QString &lineRaw : lines) {
                const QString line = lineRaw.trimmed();
                if (line.isEmpty())
                    continue;
                if (line.startsWith("Ключ:", Qt::CaseInsensitive)) {
                    key = line.section(':', 1).trimmed();
                    break;
                }
                if (line.startsWith("Recovery key:", Qt::CaseInsensitive)) {
                    key = line.section(':', 1).trimmed();
                    break;
                }
            }
        }

        if (key.isEmpty()) {
            recoveryError->setText("В файле не найден корректный ключ");
            return;
        }

        recoveryKeyEdit->setText(key.toUpper());
        loginByRecoveryKey(key);
    });

    connect(newPass1Edit, &QLineEdit::textChanged, this, [this](const QString &text) {
        newPassError->clear();

        int score = 0;
        const int len = text.length();
        const bool hasLower = text.contains(QRegExp("[a-z]"));
        const bool hasUpper = text.contains(QRegExp("[A-Z]"));
        const bool hasDigit = text.contains(QRegExp("[0-9]"));
        const bool hasSpecial = text.contains(QRegExp("[^A-Za-z0-9]"));

        int classes = 0;
        if (hasLower) ++classes;
        if (hasUpper) ++classes;
        if (hasDigit) ++classes;
        if (hasSpecial) ++classes;

        if (len >= 6) score += 20;
        if (len >= 8) score += 15;
        if (len >= 10) score += 15;
        if (len >= 12) score += 15;
        score += classes * 10;
        if (classes >= 3 && len >= 8) score += 10;
        if (classes <= 1 && len < 10) score -= 10;
        if (score < 0) score = 0;
        if (score > 100) score = 100;

        newPassStrength->setValue(score >= 85 ? 100 : score);

        if (text.isEmpty()) {
            newPassStrengthLabel->setText("Надёжность: —");
            newPassStrengthLabel->setStyleSheet("color:#6B7280;font-weight:700;");
            newPassStrength->setStyleSheet("QProgressBar{border:none;border-radius:3px;background:#E9EEF6;height:6px;}QProgressBar::chunk{background:#9CA3AF;border-radius:3px;}");
        } else if (score < 35) {
            newPassStrengthLabel->setText("Надёжность: Слабый");
            newPassStrengthLabel->setStyleSheet("color:#DC2626;font-weight:700;");
            newPassStrength->setStyleSheet("QProgressBar{border:none;border-radius:3px;background:#FEE2E2;height:6px;}QProgressBar::chunk{background:#EA4335;border-radius:3px;}");
        } else if (score < 60) {
            newPassStrengthLabel->setText("Надёжность: Средний");
            newPassStrengthLabel->setStyleSheet("color:#D97706;font-weight:700;");
            newPassStrength->setStyleSheet("QProgressBar{border:none;border-radius:3px;background:#FEF3C7;height:6px;}QProgressBar::chunk{background:#FBBC04;border-radius:3px;}");
        } else if (score < 85) {
            newPassStrengthLabel->setText("Надёжность: Надёжный");
            newPassStrengthLabel->setStyleSheet("color:#15803D;font-weight:700;");
            newPassStrength->setStyleSheet("QProgressBar{border:none;border-radius:3px;background:#DCFCE7;height:6px;}QProgressBar::chunk{background:#34A853;border-radius:3px;}");
        } else {
            newPassStrengthLabel->setText("Надёжность: Отличный");
            newPassStrengthLabel->setStyleSheet("color:#166534;font-weight:800;");
            newPassStrength->setStyleSheet("QProgressBar{border:none;border-radius:3px;background:#D1FAE5;height:6px;}QProgressBar::chunk{background:#0F9D58;border-radius:3px;}");
        }
    });

    connect(newPass2Edit, &QLineEdit::textChanged, this, [this](const QString &) {
        newPassError->clear();
    });

    connect(btnChangePass_, &QPushButton::clicked, this, [this]() {
        newPassError->clear();
        QString p1 = newPass1Edit->text();
        QString p2 = newPass2Edit->text();

        if (!passRx.exactMatch(p1)) {
            newPassError->setText("Пароль: только английские буквы, цифры и спецсимволы");
            return;
        }

        if (p1.length() < 8) {
            newPassError->setText("Пароль должен быть минимум 8 символов");
            return;
        }

        if (p1 != p2) {
            newPassError->setText("Пароли не совпадают");
            return;
        }

        QString error;
        if (!setNewPassword(recoveryUsername_, p1, error)) {
            newPassError->setText(error);
            return;
        }

        QString newRecoveryKey;
        if (!regenerateRecoveryKey(recoveryUsername_, newRecoveryKey, error)) {
            newPassError->setText("Пароль изменён, но не удалось создать новый ключ: " + error);
        }

        UserInfo u;
        u.username = recoveryUsername_;
        u.isActive = true;

        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare("SELECT id, role FROM users WHERE username = :u");
            q.bindValue(":u", recoveryUsername_);
            if (q.exec() && q.next()) {
                u.id = q.value(0).toInt();
                u.role = q.value(1).toString();
            }
        }

        enableRememberMe(recoveryUsername_);
        user_ = u;
        logAction(recoveryUsername_, "password_changed_via_recovery", "Пароль изменён через ключ восстановления");

        if (!newRecoveryKey.isEmpty())
            showRecoveryKeyDialog(recoveryUsername_, newRecoveryKey);

        accept();
    });

    connect(loginEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        loginError->clear();
    });
    connect(passEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        loginError->clear();
    });
    connect(regLoginEdit, &QLineEdit::textChanged, this, [this](const QString &) {
        regError->clear();
    });
    connect(regPass1Edit, &QLineEdit::textChanged, this, [this](const QString &) {
        regError->clear();
    });
    connect(regPass2Edit, &QLineEdit::textChanged, this, [this](const QString &) {
        regError->clear();
    });

    connect(btnBack_, &QPushButton::clicked, this, [this]() {
        regError->clear();
        stack->setCurrentIndex(0);
    });

    connect(btnRegOk_, &QPushButton::clicked, this, [this]() {
        regError->clear();
        QString login = regLoginEdit->text().trimmed();
        QString p1 = regPass1Edit->text();
        QString p2 = regPass2Edit->text();

        if (!loginRx.exactMatch(login)) {
            regError->setText("Логин: только латиница и цифры");
            return;
        }

        if (!passRx.exactMatch(p1)) {
            regError->setText("Пароль: только английские буквы, цифры и спецсимволы");
            return;
        }

        if (p1 != p2) {
            regError->setText("Пароли не совпадают");
            return;
        }

        QString role = regRoleCombo->currentData().toString();

        if (role == "admin" && hasAnyAdmin()) {
            QString adminKey = regAdminKeyEdit->text().trimmed();
            QString keyError;
            if (!verifyAdminInviteKey(adminKey, keyError)) {
                regError->setText(keyError);
                return;
            }
        }
        if (role == "tech" && hasAnyTech()) {
            QString techKey = regTechKeyEdit->text().trimmed();
            QString keyError;
            if (!verifyTechInviteKey(techKey, keyError)) {
                regError->setText(keyError);
                return;
            }
        }

        QString recoveryKey;
        QString error;
        if (!registerUser(login, p1, role, recoveryKey, error)) {
            regError->setText(error);
            return;
        }

        regLoginEdit->clear();
        regPass1Edit->clear();
        regPass2Edit->clear();
        if (regAdminKeyEdit)
            regAdminKeyEdit->clear();
        if (regTechKeyEdit)
            regTechKeyEdit->clear();
        regRoleCombo->setCurrentIndex(0);
        regError->clear();
        passStrength->setValue(0);
        passStrengthLabel->setText("Надёжность: —");
        passStrengthLabel->setStyleSheet("color:#6B7280;font-weight:700;");
        passStrength->setStyleSheet(
            "QProgressBar{border:none;border-radius:3px;background:#E9EEF6;text-align:center;color:transparent;height:6px;}"
            "QProgressBar::chunk{background:#9CA3AF;border-radius:3px;}"
        );

        showRecoveryKeyDialog(login, recoveryKey);
        stack->setCurrentIndex(0);
    });

    connect(regRoleCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onRoleChanged(int)));
}
