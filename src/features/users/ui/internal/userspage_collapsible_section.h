#pragma once

#include <QFrame>
#include <functional>

class QVBoxLayout;
class QPushButton;
class QLabel;
class QWidget;

namespace UsersPageInternal {

class CollapsibleSection : public QFrame
{
public:
    enum SectionStyle { StyleDefault, StyleTech, StyleAdmin, StyleViewer };

    CollapsibleSection(const QString &title,
                       bool expandedByDefault,
                       std::function<int(int)> scale,
                       QWidget *parent = nullptr,
                       SectionStyle style = StyleDefault);

    QVBoxLayout *contentLayout();
    void setTitle(const QString &title);

private:
    void updateArrow();

    QPushButton *headerBtn_ = nullptr;
    QLabel *arrowLbl_ = nullptr;
    QLabel *titleLbl_ = nullptr;
    QWidget *content_ = nullptr;
    QVBoxLayout *contentLayout_ = nullptr;
    std::function<int(int)> s_;
    bool expanded_ = false;
};

} // namespace UsersPageInternal
