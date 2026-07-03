#pragma once

#include <QFrame>
#include <functional>

struct FilterSettings;
class QWidget;
class QVBoxLayout;
class QPushButton;
class QLabel;

namespace ListAgvInfoUi {

class CollapsibleSection : public QFrame
{
public:
    enum SectionStyle { StyleDefault, StyleMine, StyleCommon, StyleOverdue, StyleSoon, StyleDone, StyleDelegated };

    CollapsibleSection(const QString &title,
                       bool expandedByDefault,
                       std::function<int(int)> scale,
                       QWidget *parent = nullptr,
                       SectionStyle style = StyleDefault);

    QVBoxLayout *contentLayout();
    void setTitle(const QString &t);
    void setExpanded(bool e);
    bool isExpanded() const;

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

bool showFilterDialog(const FilterSettings &current, FilterSettings &result, QWidget *parent = nullptr);

} // namespace ListAgvInfoUi
