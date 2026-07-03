#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QRegularExpression>
#include <QTimer>
#include <QWidget>

namespace LeftMenuStressUi {

QDialog *findVisibleDialogByTitle(const QString &titlePart)
{
    const auto widgets = QApplication::topLevelWidgets();
    for (auto it = widgets.crbegin(); it != widgets.crend(); ++it) {
        QDialog *dlg = qobject_cast<QDialog *>(*it);
        if (!dlg || !dlg->isVisible())
            continue;
        if (titlePart.trimmed().isEmpty() ||
            dlg->windowTitle().contains(titlePart, Qt::CaseInsensitive)) {
            return dlg;
        }
    }
    return nullptr;
}

QString normalizedUiText(QString text)
{
    text = text.trimmed().toLower();
    text.remove(QRegularExpression(QStringLiteral("[\\s\\-_]+")));
    return text;
}

QString buttonDebugText(QAbstractButton *button)
{
    if (!button)
        return QString();
    const QString text = button->text().trimmed();
    if (!text.isEmpty())
        return text;
    const QString tip = button->toolTip().trimmed();
    if (!tip.isEmpty())
        return tip;
    return button->objectName().trimmed();
}

QAbstractButton *findVisibleButtonByText(QWidget *root, const QString &textPart)
{
    if (!root)
        return nullptr;
    const QString needle = normalizedUiText(textPart);
    const auto buttons = root->findChildren<QAbstractButton *>();
    for (QAbstractButton *btn : buttons) {
        if (!btn || !btn->isVisible() || !btn->isEnabled())
            continue;
        const QString hay = normalizedUiText(buttonDebugText(btn));
        if (!needle.isEmpty() && hay.contains(needle))
            return btn;
    }
    return nullptr;
}

bool isUnsafeAutotestButton(QAbstractButton *button)
{
    if (!button)
        return true;
    const QString label = buttonDebugText(button).toLower();
    if (label.isEmpty())
        return true;

    static const QStringList blocked = {
        QStringLiteral("выйти"),
        QStringLiteral("switch"),
        QStringLiteral("сменить аккаунт"),
        QStringLiteral("удалить"),
        QStringLiteral("очистить"),
        QStringLiteral("export"),
        QStringLiteral("экспорт"),
        QStringLiteral("печать"),
        QStringLiteral("print"),
        QStringLiteral("сохранить"),
        QStringLiteral("save"),
        QStringLiteral("open file"),
        QStringLiteral("файл"),
        QStringLiteral("загрузить"),
        QStringLiteral("выгруз"),
        QStringLiteral("стресс"),
        QStringLiteral("автотест")
    };
    for (const QString &token : blocked) {
        if (label.contains(token))
            return true;
    }
    return false;
}

bool tryCloseDialog(QWidget *root)
{
    QDialog *dlg = qobject_cast<QDialog *>(root);
    if (!dlg)
        return false;

    static const QStringList closeLabels = {
        QStringLiteral("отмена"),
        QStringLiteral("закрыть"),
        QStringLiteral("назад"),
        QStringLiteral("cancel"),
        QStringLiteral("close"),
        QStringLiteral("back")
    };

    for (const QString &label : closeLabels) {
        if (QAbstractButton *btn = findVisibleButtonByText(dlg, label)) {
            btn->click();
            QApplication::processEvents();
            return true;
        }
    }

    dlg->reject();
    QApplication::processEvents();
    if (!dlg->isVisible())
        return true;

    dlg->close();
    QApplication::processEvents();
    return !dlg->isVisible();
}

void scheduleRejectDialog(const QString &titlePart, int delayMs = 140)
{
    const QString wantedTitle = titlePart;
    for (int attempt = 0; attempt < 8; ++attempt) {
        QTimer::singleShot(qMax(0, delayMs + attempt * 120), qApp, [wantedTitle]() {
            if (QDialog *dlg = findVisibleDialogByTitle(wantedTitle))
                (void)tryCloseDialog(dlg);
        });
    }
}

bool clickBackOn(QWidget *root)
{
    if (!root)
        return false;
    static const QStringList backLabels = {
        QStringLiteral("назад"),
        QStringLiteral("back"),
        QStringLiteral("отмена"),
        QStringLiteral("cancel")
    };
    for (const QString &label : backLabels) {
        if (QAbstractButton *btn = findVisibleButtonByText(root, label)) {
            btn->click();
            QApplication::processEvents();
            return true;
        }
    }
    return false;
}

} // namespace LeftMenuStressUi
