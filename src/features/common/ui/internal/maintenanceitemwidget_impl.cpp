#include "../maintenanceitemwidget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>

static QString rgbaCss(const QColor &c)
{
    return QString("rgba(%1,%2,%3,%4)")
           .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}

MaintenanceItemWidget::MaintenanceItemWidget(
    const QString &iconResPath,
    const QSize &iconSize,
    const QString &titleText,
    const QString &subtitleText,
    const QColor &rowBgColor,
    const QColor &buttonBgColor,
    const QColor &buttonTextColor,
    QWidget *parent
) : QWidget(parent),
    root_(new QFrame(this)),
    iconLabel_(new QLabel(root_)),
    titleLabel_(new QLabel(root_)),
    subtitleLabel_(new QLabel(root_)),
    showButton_(new QPushButton(QStringLiteral("Показать"), root_))
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(root_);

    root_->setObjectName("mi_root");
    root_->setStyleSheet(QString(
        "QFrame#mi_root {"
        "  background-color: %1;"
        "  border: none;"
        "  border-radius: 12px;"
        "}"
    ).arg(rgbaCss(rowBgColor)));

    auto *row = new QHBoxLayout(root_);
    row->setContentsMargins(12, 10, 12, 10);
    row->setSpacing(10);

    // Icon (left)
    iconLabel_->setFixedSize(iconSize);
    QPixmap ico(iconResPath);
    if (!ico.isNull())
        iconLabel_->setPixmap(ico.scaled(iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Texts
    auto *textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(2);

    titleLabel_->setText(titleText);
    titleLabel_->setWordWrap(true);
    titleLabel_->setStyleSheet(
        "font-family: 'Inter';"
        "font-size: 12px;"
        "font-weight: 800;"
        "color: #000000;"
    );

    subtitleLabel_->setText(subtitleText);
    subtitleLabel_->setWordWrap(true);
    subtitleLabel_->setStyleSheet(
        "font-family: 'Inter';"
        "font-size: 12px;"
        "font-weight: 800;"
        "color: #8B8B8B;"
    );

    textCol->addWidget(titleLabel_);
    textCol->addWidget(subtitleLabel_);

    // Button
    showButton_->setFixedSize(80, 30);
    showButton_->setCursor(Qt::PointingHandCursor);

    // Стили для hover и pressed состояния
    showButton_->setStyleSheet(QString(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  font-family: 'Inter';"
        "  font-size: 12px;"
        "  font-weight: 800;"
        "  border: none;"
        "  border-radius: 6px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %3;"
        "}"
        "QPushButton:pressed {"
        "  background-color: %4;"
        "}"
    )
    .arg(rgbaCss(buttonBgColor))              // Исходный фон
    .arg(buttonTextColor.name(QColor::HexRgb)) // Цвет текста
    .arg(rgbaCss(buttonBgColor.lighter()))    // Фон при наведении (светлее исходного цвета)
    .arg(rgbaCss(buttonBgColor.darker()))) ;  // Фон при нажатии (темнее исходного цвета)

    connect(showButton_, &QPushButton::clicked, this, &MaintenanceItemWidget::showClicked);

    row->addWidget(iconLabel_, 0, Qt::AlignLeft | Qt::AlignVCenter);
    row->addLayout(textCol, 1);
    row->addWidget(showButton_, 0, Qt::AlignRight | Qt::AlignVCenter);
}
