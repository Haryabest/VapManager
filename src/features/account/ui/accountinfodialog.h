#pragma once
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QClipboard>
#include <QApplication>
#include <QTimer>
#include <QPixmap>

class AccountInfoDialog : public QDialog
{
    Q_OBJECT
public:
    AccountInfoDialog(const QString &username,
                      const QString &role,
                      const QString &inviteKey,
                      const QPixmap &avatarFromDb,
                      QWidget *parent = nullptr);
};
