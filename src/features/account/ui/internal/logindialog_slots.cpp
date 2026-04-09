#include "../logindialog.h"

void LoginDialog::onLoginClicked()
{
    loginError->clear();
    QString error;
    UserInfo u;

    if (!loginRx.exactMatch(loginEdit->text())) {
        loginError->setText("Логин должен быть на английском и без спецсимволов");
        return;
    }

    if (!loginUser(loginEdit->text(), passEdit->text(), u, error)) {
        loginError->setText(error);
        return;
    }

    enableRememberMe(u.username);
    user_ = u;
    accept();
}

void LoginDialog::onRegisterClicked()
{
    stack->setCurrentIndex(1);
}

void LoginDialog::updatePasswordStrength(const QString &text)
{
    int score = 0;
    const int len = text.length();

    const bool hasLower = text.contains(QRegExp("[a-z]"));
    const bool hasUpper = text.contains(QRegExp("[A-Z]"));
    const bool hasDigit = text.contains(QRegExp("[0-9]"));
    const bool hasSpecial = text.contains(QRegExp("[^A-Za-z0-9]"));

    int classes = 0;
    if (hasLower)   ++classes;
    if (hasUpper)   ++classes;
    if (hasDigit)   ++classes;
    if (hasSpecial) ++classes;

    if (len >= 6)  score += 20;
    if (len >= 8)  score += 15;
    if (len >= 10) score += 15;
    if (len >= 12) score += 15;

    score += classes * 10;

    if (classes >= 3 && len >= 8)
        score += 10;

    if (classes <= 1 && len < 10)
        score -= 10;

    if (score < 0) score = 0;
    if (score > 100) score = 100;

    const int uiScore = (score >= 85) ? 100 : score;
    passStrength->setValue(uiScore);

    if (text.isEmpty()) {
        passStrengthLabel->setText("Надёжность: —");
        passStrengthLabel->setStyleSheet("color:#6B7280;font-weight:700;");
        passStrength->setStyleSheet(
            "QProgressBar{border:none;border-radius:3px;background:#E9EEF6;text-align:center;color:transparent;height:6px;}"
            "QProgressBar::chunk{background:#9CA3AF;border-radius:3px;}"
        );
    } else if (score < 35) {
        passStrengthLabel->setText("Надёжность: Слабый");
        passStrengthLabel->setStyleSheet("color:#DC2626;font-weight:700;");
        passStrength->setStyleSheet(
            "QProgressBar{border:none;border-radius:3px;background:#FEE2E2;text-align:center;color:transparent;height:6px;}"
            "QProgressBar::chunk{background:#EA4335;border-radius:3px;}"
        );
    } else if (score < 60) {
        passStrengthLabel->setText("Надёжность: Средний");
        passStrengthLabel->setStyleSheet("color:#D97706;font-weight:700;");
        passStrength->setStyleSheet(
            "QProgressBar{border:none;border-radius:3px;background:#FEF3C7;text-align:center;color:transparent;height:6px;}"
            "QProgressBar::chunk{background:#FBBC04;border-radius:3px;}"
        );
    } else if (score < 85) {
        passStrengthLabel->setText("Надёжность: Надёжный");
        passStrengthLabel->setStyleSheet("color:#15803D;font-weight:700;");
        passStrength->setStyleSheet(
            "QProgressBar{border:none;border-radius:3px;background:#DCFCE7;text-align:center;color:transparent;height:6px;}"
            "QProgressBar::chunk{background:#34A853;border-radius:3px;}"
        );
    } else {
        passStrengthLabel->setText("Надёжность: Отличный");
        passStrengthLabel->setStyleSheet("color:#166534;font-weight:800;");
        passStrength->setStyleSheet(
            "QProgressBar{border:none;border-radius:3px;background:#D1FAE5;text-align:center;color:transparent;height:6px;}"
            "QProgressBar::chunk{background:#0F9D58;border-radius:3px;}"
        );
    }
}

void LoginDialog::onRoleChanged(int index)
{
    QString role = regRoleCombo->itemData(index).toString();
    bool needAdminKey = (role == "admin" && hasAnyAdmin());
    bool needTechKey = (role == "tech" && hasAnyTech());
    adminKeyRow->setVisible(needAdminKey);
    techKeyRow->setVisible(needTechKey);
}
