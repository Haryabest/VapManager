#include "listagvinfo_ui_modules.h"

#include "listagvinfo.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

class ToggleSwitch : public QCheckBox
{
public:
    explicit ToggleSwitch(QWidget *parent = nullptr)
        : QCheckBox(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFixedSize(50, 28);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const bool on = isChecked();
        const QColor bg = on ? QColor("#18CF00") : QColor("#CCCCCC");
        const QColor knob = QColor("#FFFFFF");

        p.setBrush(bg);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect(), 14, 14);

        const QRect circle = on ? QRect(width() - 26, 2, 24, 24) : QRect(2, 2, 24, 24);
        p.setBrush(knob);
        p.drawEllipse(circle);
    }
};

class ToggleRow : public QWidget
{
public:
    explicit ToggleRow(ToggleSwitch *sw, QWidget *parent = nullptr)
        : QWidget(parent), sw_(sw)
    {
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        QWidget::mousePressEvent(e);
        if (sw_)
            sw_->toggle();
    }

private:
    ToggleSwitch *sw_ = nullptr;
};

class FilterDialog5 : public QDialog
{
public:
    explicit FilterDialog5(const FilterSettings &cur, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Фильтры AGV");
        setMinimumSize(540, 520);
        resize(620, 600);
        setSizeGripEnabled(true);

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setSpacing(18);
        root->setContentsMargins(16, 16, 16, 16);

        auto makeLine = [&](const QString &color) {
            QFrame *line = new QFrame();
            line->setFrameShape(QFrame::HLine);
            line->setFixedHeight(3);
            line->setStyleSheet(QString("background:%1;border:none;").arg(color));
            return line;
        };

        auto makeToggleRow = [&](const QString &text) {
            ToggleSwitch *sw = new ToggleSwitch();
            ToggleRow *w = new ToggleRow(sw);
            QHBoxLayout *h = new QHBoxLayout(w);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(6);

            QLabel *lbl = new QLabel(text);
            lbl->setStyleSheet("font-size:16px;font-weight:500;color:#222;");
            h->addWidget(lbl);
            h->addStretch();
            h->addWidget(sw);

            return std::make_pair(static_cast<QWidget *>(w), sw);
        };

        QHBoxLayout *row1 = new QHBoxLayout();
        row1->setSpacing(16);

        QWidget *servBox = new QWidget();
        QVBoxLayout *servL = new QVBoxLayout(servBox);
        servL->setSpacing(8);
        QLabel *servTitle = new QLabel("Обслуженные");
        servTitle->setStyleSheet("font-size:20px;font-weight:900;color:#18CF00;");

        QWidget *servAscRow;
        QWidget *servDescRow;
        ToggleSwitch *swServAsc;
        ToggleSwitch *swServDesc;
        {
            auto p1 = makeToggleRow("от меньшего к большему");
            servAscRow = p1.first;
            swServAsc = p1.second;
            auto p2 = makeToggleRow("от большего к меньшему");
            servDescRow = p2.first;
            swServDesc = p2.second;
        }

        swServAsc->setChecked(cur.serv == FilterSettings::Asc);
        swServDesc->setChecked(cur.serv == FilterSettings::Desc);
        QObject::connect(swServAsc, &QCheckBox::toggled, this, [=](bool on) {
            if (on) swServDesc->setChecked(false);
        });
        QObject::connect(swServDesc, &QCheckBox::toggled, this, [=](bool on) {
            if (on) swServAsc->setChecked(false);
        });

        servL->addWidget(servTitle);
        servL->addWidget(servAscRow);
        servL->addWidget(servDescRow);
        servL->addWidget(makeLine("#18CF00"));

        QWidget *upBox = new QWidget();
        QVBoxLayout *upL = new QVBoxLayout(upBox);
        upL->setSpacing(8);
        QLabel *upTitle = new QLabel("Ближайшие");
        upTitle->setStyleSheet("font-size:20px;font-weight:900;color:#FF8800;");

        QWidget *upAscRow;
        QWidget *upDescRow;
        ToggleSwitch *swUpAsc;
        ToggleSwitch *swUpDesc;
        {
            auto p1 = makeToggleRow("от меньшего к большему");
            upAscRow = p1.first;
            swUpAsc = p1.second;
            auto p2 = makeToggleRow("от большего к меньшему");
            upDescRow = p2.first;
            swUpDesc = p2.second;
        }

        swUpAsc->setChecked(cur.up == FilterSettings::UpAsc);
        swUpDesc->setChecked(cur.up == FilterSettings::UpDesc);
        QObject::connect(swUpAsc, &QCheckBox::toggled, this, [=](bool on) {
            if (on) swUpDesc->setChecked(false);
        });
        QObject::connect(swUpDesc, &QCheckBox::toggled, this, [=](bool on) {
            if (on) swUpAsc->setChecked(false);
        });

        upL->addWidget(upTitle);
        upL->addWidget(upAscRow);
        upL->addWidget(upDescRow);
        upL->addWidget(makeLine("#FF8800"));

        row1->addWidget(servBox);
        row1->addWidget(upBox);
        root->addLayout(row1);

        QHBoxLayout *row2 = new QHBoxLayout();
        row2->setSpacing(16);

        QWidget *overBox = new QWidget();
        QVBoxLayout *overL = new QVBoxLayout(overBox);
        overL->setSpacing(8);
        QLabel *overTitle = new QLabel("Просроченные");
        overTitle->setStyleSheet("font-size:20px;font-weight:900;color:#FF0000;");

        QWidget *overOldRow;
        QWidget *overNewRow;
        ToggleSwitch *swOverOld;
        ToggleSwitch *swOverNew;
        {
            auto p1 = makeToggleRow("от старых к новым");
            overOldRow = p1.first;
            swOverOld = p1.second;
            auto p2 = makeToggleRow("от новых к старым");
            overNewRow = p2.first;
            swOverNew = p2.second;
        }

        swOverOld->setChecked(cur.over == FilterSettings::OverOld);
        swOverNew->setChecked(cur.over == FilterSettings::OverNew);
        QObject::connect(swOverOld, &QCheckBox::toggled, this, [=](bool on) {
            if (on) swOverNew->setChecked(false);
        });
        QObject::connect(swOverNew, &QCheckBox::toggled, this, [=](bool on) {
            if (on) swOverOld->setChecked(false);
        });

        overL->addWidget(overTitle);
        overL->addWidget(overOldRow);
        overL->addWidget(overNewRow);
        overL->addWidget(makeLine("#FF0000"));

        QWidget *sortBox = new QWidget();
        QVBoxLayout *sortL = new QVBoxLayout(sortBox);
        sortL->setSpacing(8);
        QLabel *sortTitle = new QLabel("Сортировка");
        sortTitle->setStyleSheet("font-size:20px;font-weight:900;color:#444;");

        QWidget *modelAZRow;
        QWidget *modelZARow;
        QWidget *kmAscRow;
        QWidget *kmDescRow;
        ToggleSwitch *swModelAZ;
        ToggleSwitch *swModelZA;
        ToggleSwitch *swKmAsc;
        ToggleSwitch *swKmDesc;
        {
            auto p1 = makeToggleRow("Модель A → Z");
            modelAZRow = p1.first;
            swModelAZ = p1.second;
            auto p2 = makeToggleRow("Модель Z → A");
            modelZARow = p2.first;
            swModelZA = p2.second;
            auto p3 = makeToggleRow("Пробег ↑");
            kmAscRow = p3.first;
            swKmAsc = p3.second;
            auto p4 = makeToggleRow("Пробег ↓");
            kmDescRow = p4.first;
            swKmDesc = p4.second;
        }

        swModelAZ->setChecked(cur.modelSort == FilterSettings::ModelAZ);
        swModelZA->setChecked(cur.modelSort == FilterSettings::ModelZA);
        swKmAsc->setChecked(cur.km == FilterSettings::KmAsc);
        swKmDesc->setChecked(cur.km == FilterSettings::KmDesc);

        QObject::connect(swModelAZ, &QCheckBox::toggled, this, [=](bool on) { if (on) swModelZA->setChecked(false); });
        QObject::connect(swModelZA, &QCheckBox::toggled, this, [=](bool on) { if (on) swModelAZ->setChecked(false); });
        QObject::connect(swKmAsc, &QCheckBox::toggled, this, [=](bool on) { if (on) swKmDesc->setChecked(false); });
        QObject::connect(swKmDesc, &QCheckBox::toggled, this, [=](bool on) { if (on) swKmAsc->setChecked(false); });

        sortL->addWidget(sortTitle);
        sortL->addWidget(modelAZRow);
        sortL->addWidget(modelZARow);
        sortL->addWidget(kmAscRow);
        sortL->addWidget(kmDescRow);
        sortL->addWidget(makeLine("#888888"));

        row2->addWidget(overBox);
        row2->addWidget(sortBox);
        root->addLayout(row2);

        QLabel *nameTitle = new QLabel("По названию");
        nameTitle->setStyleSheet("font-size:20px;font-weight:900;color:#1A1A1A;");
        QLineEdit *nameEdit = new QLineEdit();
        nameEdit->setPlaceholderText("Введите часть названия...");
        nameEdit->setStyleSheet("padding:8px;font-size:16px;border-radius:8px;border:1px solid #CCC;");
        nameEdit->setText(cur.nameFilter);
        root->addWidget(nameTitle);
        root->addWidget(nameEdit);
        root->addWidget(makeLine("#1A1A1A"));

        QHBoxLayout *btns = new QHBoxLayout();
        QPushButton *reset = new QPushButton("Сбросить");
        QPushButton *apply = new QPushButton("Применить");
        btns->addWidget(reset);
        btns->addStretch();
        btns->addWidget(apply);

        root->addStretch();
        root->addLayout(btns);

        connect(apply, &QPushButton::clicked, this, [=]() {
            if (swServAsc->isChecked()) result.serv = FilterSettings::Asc;
            else if (swServDesc->isChecked()) result.serv = FilterSettings::Desc;
            else result.serv = FilterSettings::None;

            if (swUpAsc->isChecked()) result.up = FilterSettings::UpAsc;
            else if (swUpDesc->isChecked()) result.up = FilterSettings::UpDesc;
            else result.up = FilterSettings::UpNone;

            if (swOverOld->isChecked()) result.over = FilterSettings::OverOld;
            else if (swOverNew->isChecked()) result.over = FilterSettings::OverNew;
            else result.over = FilterSettings::OverNone;

            if (swModelAZ->isChecked()) result.modelSort = FilterSettings::ModelAZ;
            else if (swModelZA->isChecked()) result.modelSort = FilterSettings::ModelZA;
            else result.modelSort = FilterSettings::ModelNone;

            if (swKmAsc->isChecked()) result.km = FilterSettings::KmAsc;
            else if (swKmDesc->isChecked()) result.km = FilterSettings::KmDesc;
            else result.km = FilterSettings::KmNone;

            result.nameFilter = nameEdit->text();
            accept();
        });

        connect(reset, &QPushButton::clicked, this, [=]() {
            result = FilterSettings();
            accept();
        });
    }

    FilterSettings result;
};

} // namespace

namespace ListAgvInfoUi {

CollapsibleSection::CollapsibleSection(const QString &title, bool expandedByDefault,
                                       std::function<int(int)> scale, QWidget *parent,
                                       SectionStyle style)
    : QFrame(parent), s_(scale), expanded_(expandedByDefault)
{
    setStyleSheet("QFrame{background:transparent;}");
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QString bg, bgHover, textColor;
    switch (style) {
    case StyleMine: bg = "#C7D2FE"; bgHover = "#A5B4FC"; textColor = "#1E3A8A"; break;
    case StyleCommon: bg = "#94A3B8"; bgHover = "#64748B"; textColor = "#0F172A"; break;
    case StyleOverdue: bg = "#FECACA"; bgHover = "#FCA5A5"; textColor = "#991B1B"; break;
    case StyleSoon: bg = "#FED7AA"; bgHover = "#FDBA74"; textColor = "#9A3412"; break;
    case StyleDone: bg = "#BBF7D0"; bgHover = "#86EFAC"; textColor = "#166534"; break;
    case StyleDelegated: bg = "#FBCFE8"; bgHover = "#F9A8D4"; textColor = "#9D174D"; break;
    default: bg = "#E8EAED"; bgHover = "#D8DAE0"; textColor = "#1A1A1A"; break;
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

    QObject::connect(headerBtn_, &QPushButton::clicked, this, [this]() {
        expanded_ = !expanded_;
        updateArrow();
        content_->setVisible(expanded_);
    });
    updateArrow();
    content_->setVisible(expanded_);
}

QVBoxLayout *CollapsibleSection::contentLayout() { return contentLayout_; }
void CollapsibleSection::setTitle(const QString &t) { titleLbl_->setText(t); }
void CollapsibleSection::setExpanded(bool e) { if (e != expanded_) { expanded_ = e; updateArrow(); content_->setVisible(expanded_); } }
bool CollapsibleSection::isExpanded() const { return expanded_; }

void CollapsibleSection::updateArrow()
{
    arrowLbl_->setText(expanded_ ? "▼" : "▶");
    arrowLbl_->setStyleSheet("background:transparent;font-size:14px;color:#555;");
}

bool showFilterDialog(const FilterSettings &current, FilterSettings &result, QWidget *parent)
{
    FilterDialog5 dlg(current, parent);
    if (dlg.exec() != QDialog::Accepted)
        return false;
    result = dlg.result;
    return true;
}

} // namespace ListAgvInfoUi
