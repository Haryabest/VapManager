#pragma once

#include <QString>

class QDialog;
class QAbstractButton;
class QWidget;

namespace LeftMenuStressUi {

QDialog *findVisibleDialogByTitle(const QString &titlePart);
QString normalizedUiText(QString text);
QString buttonDebugText(QAbstractButton *button);
QAbstractButton *findVisibleButtonByText(QWidget *root, const QString &textPart);
bool isUnsafeAutotestButton(QAbstractButton *button);
bool tryCloseDialog(QWidget *root);
void scheduleRejectDialog(const QString &titlePart, int delayMs = 140);
bool clickBackOn(QWidget *root);

} // namespace LeftMenuStressUi
