#include "userspage_collapsible_section.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace UsersPageInternal {

CollapsibleSection::CollapsibleSection(const QString &title,
                                       bool expandedByDefault,
                                       std::function<int(int)> scale,
                                       QWidget *parent,
                                       SectionStyle style)
    : QFrame(parent), s_(scale), expanded_(expandedByDefault)
{
    setStyleSheet("QFrame{background:transparent;}");
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QString bg;
    QString bgHover;
    QString textColor;
    switch (style) {
    case StyleTech:
        bg = "#BBF7D0";
        bgHover = "#86EFAC";
        textColor = "#166534";
        break;
    case StyleAdmin:
        bg = "#FECACA";
        bgHover = "#FCA5A5";
        textColor = "#991B1B";
        break;
    case StyleViewer:
        bg = "#94A3B8";
        bgHover = "#64748B";
        textColor = "#0F172A";
        break;
    default:
        bg = "#E8EAED";
        bgHover = "#D8DAE0";
        textColor = "#1A1A1A";
        break;
    }

    headerBtn_ = new QPushButton(this);
    headerBtn_->setCursor(Qt::PointingHandCursor);
    headerBtn_->setStyleSheet(QString(
        "QPushButton{background:%1;border:none;border-radius:8px;text-align:left;"
        "font-family:Inter;font-size:%2px;font-weight:800;color:%3;padding:%4px %5px;}"
        "QPushButton:hover{background:%6;}"
    ).arg(bg).arg(s_(16)).arg(textColor).arg(s_(10)).arg(s_(14)).arg(bgHover));
    headerBtn_->setFixedHeight(s_(44));

    QHBoxLayout *h = new QHBoxLayout(headerBtn_);
    h->setContentsMargins(s_(12), 0, s_(12), 0);
    h->setSpacing(s_(8));

    arrowLbl_ = new QLabel(headerBtn_);
    arrowLbl_->setFixedWidth(s_(20));
    titleLbl_ = new QLabel(title, headerBtn_);
    titleLbl_->setStyleSheet("background:transparent;font:inherit;");
    h->addWidget(arrowLbl_);
    h->addWidget(titleLbl_);
    h->addStretch();

    content_ = new QWidget(this);
    content_->setStyleSheet("background:transparent;");
    contentLayout_ = new QVBoxLayout(content_);
    contentLayout_->setContentsMargins(s_(8), s_(6), 0, s_(8));
    contentLayout_->setSpacing(s_(6));

    root->addWidget(headerBtn_);
    root->addWidget(content_);

    connect(headerBtn_, &QPushButton::clicked, this, [this]() {
        expanded_ = !expanded_;
        updateArrow();
        content_->setVisible(expanded_);
    });

    updateArrow();
    content_->setVisible(expanded_);
}

QVBoxLayout *CollapsibleSection::contentLayout()
{
    return contentLayout_;
}

void CollapsibleSection::setTitle(const QString &title)
{
    titleLbl_->setText(title);
}

void CollapsibleSection::updateArrow()
{
    arrowLbl_->setText(expanded_ ? "▼" : "▶");
    arrowLbl_->setStyleSheet("background:transparent;font-size:14px;color:#555;");
}

} // namespace UsersPageInternal
