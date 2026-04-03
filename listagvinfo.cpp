    #include "listagvinfo.h"
    #include "addagvdialog.h"
    #include "db_agv_tasks.h"
    #include "databus.h"
    #include "db_users.h"
    #include "app_session.h"

    #include <QPainter>
    #include <QStyleOption>
    #include <QScrollArea>
    #include <QLineEdit>
    #include <QFrame>
    #include <QCheckBox>
    #include <QIcon>
    #include <QDate>
    #include <QMessageBox>
    #include <QMap>
    #include <QSet>
    #include <QTimer>
    #include <QSqlDatabase>
    #include <QSqlDriver>
    #include <QSqlQuery>
    #include <QHash>
    #include <algorithm>

    // ======================= ToggleSwitch =======================

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

            bool on = isChecked();

            QColor bg = on ? QColor("#18CF00") : QColor("#CCCCCC");
            QColor knob = QColor("#FFFFFF");

            p.setBrush(bg);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(rect(), 14, 14);

            QRect circle = on ? QRect(width() - 26, 2, 24, 24)
                              : QRect(2, 2, 24, 24);

            p.setBrush(knob);
            p.drawEllipse(circle);
        }
    };

    // ======================= CollapsibleSection =======================

    class CollapsibleSection : public QFrame
    {
    public:
        enum SectionStyle { StyleDefault, StyleMine, StyleCommon, StyleOverdue, StyleSoon, StyleDone, StyleDelegated };
        CollapsibleSection(const QString &title, bool expandedByDefault,
                           std::function<int(int)> scale, QWidget *parent = nullptr,
                           SectionStyle style = StyleDefault)
            : QFrame(parent), s_(scale), expanded_(expandedByDefault)
        {
            setStyleSheet("QFrame{background:transparent;}");
            QVBoxLayout *root = new QVBoxLayout(this);
            root->setContentsMargins(0, 0, 0, 0);
            root->setSpacing(0);

            QString bg, bgHover, textColor;
            switch (style) {
            case StyleMine:    bg = "#C7D2FE"; bgHover = "#A5B4FC"; textColor = "#1E3A8A"; break;
            case StyleCommon:  bg = "#94A3B8"; bgHover = "#64748B"; textColor = "#0F172A"; break;
            case StyleOverdue: bg = "#FECACA"; bgHover = "#FCA5A5"; textColor = "#991B1B"; break;
            case StyleSoon:    bg = "#FED7AA"; bgHover = "#FDBA74"; textColor = "#9A3412"; break;
            case StyleDone:    bg = "#BBF7D0"; bgHover = "#86EFAC"; textColor = "#166534"; break;
            case StyleDelegated: bg = "#FBCFE8"; bgHover = "#F9A8D4"; textColor = "#9D174D"; break;
            default:           bg = "#E8EAED"; bgHover = "#D8DAE0"; textColor = "#1A1A1A"; break;
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

            connect(headerBtn_, &QPushButton::clicked, this, [this](){
                expanded_ = !expanded_;
                updateArrow();
                content_->setVisible(expanded_);
            });
            updateArrow();
            content_->setVisible(expanded_);
        }

        QVBoxLayout *contentLayout() { return contentLayout_; }
        void setTitle(const QString &t) { titleLbl_->setText(t); }
        void setExpanded(bool e) { if (e != expanded_) { expanded_ = e; updateArrow(); content_->setVisible(expanded_); } }
        bool isExpanded() const { return expanded_; }

    private:
        void updateArrow() {
            arrowLbl_->setText(expanded_ ? "▼" : "▶");
            arrowLbl_->setStyleSheet("background:transparent;font-size:14px;color:#555;");
        }
        QPushButton *headerBtn_ = nullptr;
        QLabel *arrowLbl_ = nullptr;
        QLabel *titleLbl_ = nullptr;
        QWidget *content_ = nullptr;
        QVBoxLayout *contentLayout_ = nullptr;
        std::function<int(int)> s_;
        bool expanded_;
    };

    // ======================= ToggleRow (кликабельная строка) =======================

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

    /// Один «слот» карточки в порядке показа: Ваши (просроченные→скоро→обслужены) → Общие → делегированные по пользователям.
    struct AgvDispSlot {
        AgvInfo info;
        enum Kind {
            MineOver, MineSoon, MineDone,
            ComOver, ComSoon, ComDone,
            DelOver, DelSoon, DelDone
        } kind;
        QString delegUser;

        AgvDispSlot(const AgvInfo &i, Kind k, const QString &du = QString())
            : info(i), kind(k), delegUser(du) {}
    };

    // ======================= FilterDialog (умные фильтры) =======================

    class FilterDialog5 : public QDialog
    {
    public:
        FilterDialog5(const FilterSettings &cur, QWidget *parent = nullptr)
            : QDialog(parent)
        {
            setWindowTitle("Фильтры AGV");
            setMinimumSize(540, 520);
            resize(620, 600);
            setSizeGripEnabled(true);

            QVBoxLayout *root = new QVBoxLayout(this);
            root->setSpacing(18);
            root->setContentsMargins(16, 16, 16, 16);

            auto makeLine = [&](QString color){
                QFrame *line = new QFrame();
                line->setFrameShape(QFrame::HLine);
                line->setFixedHeight(3);
                line->setStyleSheet(QString("background:%1;border:none;").arg(color));
                return line;
            };

            auto makeToggleRow = [&](QString text){
                ToggleSwitch *sw = new ToggleSwitch();

                ToggleRow *w = new ToggleRow(sw);
                QHBoxLayout *h = new QHBoxLayout(w);
                h->setContentsMargins(0,0,0,0);
                h->setSpacing(6);

                QLabel *lbl = new QLabel(text);
                lbl->setStyleSheet("font-size:16px;font-weight:500;color:#222;");

                h->addWidget(lbl);
                h->addStretch();
                h->addWidget(sw);

                return std::make_pair(static_cast<QWidget*>(w), sw);
            };

            // ===== 1-я строка =====
            QHBoxLayout *row1 = new QHBoxLayout();
            row1->setSpacing(16);

            // Обслуженные
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

            // Инициализация по текущим настройкам
            swServAsc->setChecked(cur.serv == FilterSettings::Asc);
            swServDesc->setChecked(cur.serv == FilterSettings::Desc);

            // Взаимоисключающие
            QObject::connect(swServAsc, &QCheckBox::toggled, this, [=](bool on){
                if (on) swServDesc->setChecked(false);
            });
            QObject::connect(swServDesc, &QCheckBox::toggled, this, [=](bool on){
                if (on) swServAsc->setChecked(false);
            });

            servL->addWidget(servTitle);
            servL->addWidget(servAscRow);
            servL->addWidget(servDescRow);
            servL->addWidget(makeLine("#18CF00"));

            // Ближайшие
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

            QObject::connect(swUpAsc, &QCheckBox::toggled, this, [=](bool on){
                if (on) swUpDesc->setChecked(false);
            });
            QObject::connect(swUpDesc, &QCheckBox::toggled, this, [=](bool on){
                if (on) swUpAsc->setChecked(false);
            });

            upL->addWidget(upTitle);
            upL->addWidget(upAscRow);
            upL->addWidget(upDescRow);
            upL->addWidget(makeLine("#FF8800"));

            row1->addWidget(servBox);
            row1->addWidget(upBox);

            root->addLayout(row1);

            // ===== 2-я строка =====
            QHBoxLayout *row2 = new QHBoxLayout();
            row2->setSpacing(16);

            // Просроченные
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

            QObject::connect(swOverOld, &QCheckBox::toggled, this, [=](bool on){
                if (on) swOverNew->setChecked(false);
            });
            QObject::connect(swOverNew, &QCheckBox::toggled, this, [=](bool on){
                if (on) swOverOld->setChecked(false);
            });

            overL->addWidget(overTitle);
            overL->addWidget(overOldRow);
            overL->addWidget(overNewRow);
            overL->addWidget(makeLine("#FF0000"));

            // Сортировки
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

            QObject::connect(swModelAZ, &QCheckBox::toggled, this, [=](bool on){
                if (on) swModelZA->setChecked(false);
            });
            QObject::connect(swModelZA, &QCheckBox::toggled, this, [=](bool on){
                if (on) swModelAZ->setChecked(false);
            });

            QObject::connect(swKmAsc, &QCheckBox::toggled, this, [=](bool on){
                if (on) swKmDesc->setChecked(false);
            });
            QObject::connect(swKmDesc, &QCheckBox::toggled, this, [=](bool on){
                if (on) swKmAsc->setChecked(false);
            });

            sortL->addWidget(sortTitle);
            sortL->addWidget(modelAZRow);
            sortL->addWidget(modelZARow);
            sortL->addWidget(kmAscRow);
            sortL->addWidget(kmDescRow);
            sortL->addWidget(makeLine("#888888"));

            row2->addWidget(overBox);
            row2->addWidget(sortBox);

            root->addLayout(row2);

            // ===== По названию =====
            QLabel *nameTitle = new QLabel("По названию");
            nameTitle->setStyleSheet("font-size:20px;font-weight:900;color:#1A1A1A;");

            QLineEdit *nameEdit = new QLineEdit();
            nameEdit->setPlaceholderText("Введите часть названия...");
            nameEdit->setStyleSheet("padding:8px;font-size:16px;border-radius:8px;border:1px solid #CCC;");
            nameEdit->setText(cur.nameFilter);

            root->addWidget(nameTitle);
            root->addWidget(nameEdit);
            root->addWidget(makeLine("#1A1A1A"));

            // ===== Кнопки =====
            QHBoxLayout *btns = new QHBoxLayout();
            QPushButton *reset = new QPushButton("Сбросить");
            QPushButton *apply = new QPushButton("Применить");

            btns->addWidget(reset);
            btns->addStretch();
            btns->addWidget(apply);

            root->addStretch();
            root->addLayout(btns);

            connect(apply, &QPushButton::clicked, this, [=](){
                // Группа 1
                if (swServAsc->isChecked())
                    result.serv = FilterSettings::Asc;
                else if (swServDesc->isChecked())
                    result.serv = FilterSettings::Desc;
                else
                    result.serv = FilterSettings::None;

                // Группа 2
                if (swUpAsc->isChecked())
                    result.up = FilterSettings::UpAsc;
                else if (swUpDesc->isChecked())
                    result.up = FilterSettings::UpDesc;
                else
                    result.up = FilterSettings::UpNone;

                // Группа 3
                if (swOverOld->isChecked())
                    result.over = FilterSettings::OverOld;
                else if (swOverNew->isChecked())
                    result.over = FilterSettings::OverNew;
                else
                    result.over = FilterSettings::OverNone;

                // Группа 4
                if (swModelAZ->isChecked())
                    result.modelSort = FilterSettings::ModelAZ;
                else if (swModelZA->isChecked())
                    result.modelSort = FilterSettings::ModelZA;
                else
                    result.modelSort = FilterSettings::ModelNone;

                // Группа 5
                if (swKmAsc->isChecked())
                    result.km = FilterSettings::KmAsc;
                else if (swKmDesc->isChecked())
                    result.km = FilterSettings::KmDesc;
                else
                    result.km = FilterSettings::KmNone;

                result.nameFilter = nameEdit->text();

                accept();
            });

            connect(reset, &QPushButton::clicked, this, [=](){
                result = FilterSettings();
                accept();
            });
        }

        FilterSettings result;
    };

    // ======================= AgvItem =======================

    AgvItem::AgvItem(const AgvInfo &info, std::function<int(int)> scale, QWidget *parent)
        : QFrame(parent), agv(info), s(scale)
    {
        setObjectName("agvItem");
        // Карточка не должна растягиваться по высоте от свободного места.
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        setStyleSheet(
            "#agvItem{background:white;border-radius:10px;border:1px solid #E0E0E0;}"
            "#agvItem:hover{background:#F7F7F7;}"
        );

        QHBoxLayout *root = new QHBoxLayout(this);
        root->setContentsMargins(s(12), s(10), s(12), s(10));
        root->setSpacing(s(12));

        QWidget *leftCol = new QWidget(this);
        QVBoxLayout *left = new QVBoxLayout(leftCol);
        left->setContentsMargins(0, 0, 0, 0);
        left->setSpacing(s(6));

        // HEADER
        header = new QWidget(leftCol);
        QHBoxLayout *h = new QHBoxLayout(header);
        h->setContentsMargins(0,0,0,0);
        h->setSpacing(s(10));

        QLabel *icon = new QLabel(header);
        icon->setPixmap(QPixmap(":/new/mainWindowIcons/noback/agvIcon.png")
                        .scaled(s(32), s(32), Qt::KeepAspectRatio, Qt::SmoothTransformation));

        QLabel *title = new QLabel(agv.id, header);
        title->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
        ).arg(s(16)));

        arrowLabel = new QLabel(header);
        arrowLabel->setPixmap(QPixmap(":/new/mainWindowIcons/noback/arrow_down.png")
                              .scaled(s(18), s(18)));

        h->addWidget(icon);
        h->addWidget(title);
        h->addStretch();
        h->addWidget(arrowLabel, 0, Qt::AlignVCenter);

        left->addWidget(header);

        // SUB INFO
        QWidget *sub = new QWidget(leftCol);
        QHBoxLayout *subL = new QHBoxLayout(sub);
        subL->setContentsMargins(0,0,0,0);
        subL->setSpacing(s(15));

        QString subStyle = QString(
            "font-family:Inter;font-size:%1px;font-weight:600;color:#333;"
        ).arg(s(14));

        QLabel *serial = new QLabel("SN: " + agv.serial, sub);
        serial->setStyleSheet(subStyle);

        QLabel *km = new QLabel(QString("Пробег: %1 км").arg(agv.kilometers), sub);
        km->setStyleSheet(subStyle);

        auto makeMaintenanceDot = [&](const QString &state){
            QLabel *dot = new QLabel(sub);
            dot->setFixedSize(s(12), s(12));
            dot->setStyleSheet(QString(
                "background:%1;border-radius:%2px;"
            ).arg(maintenanceColor(state)).arg(s(6)));
            return dot;
        };

        subL->addWidget(serial);
        subL->addWidget(km);
        subL->addSpacing(s(2)); // вернуть индикатор ТО вплотную к тексту
        if (agv.hasOverdueMaintenance) {
            subL->addWidget(makeMaintenanceDot("red")); // сначала красный
            if (agv.hasSoonMaintenance)
                subL->addWidget(makeMaintenanceDot("orange")); // рядом оранжевый
        } else if (agv.hasSoonMaintenance) {
            subL->addWidget(makeMaintenanceDot("orange"));
        } else {
            subL->addWidget(makeMaintenanceDot("green"));
        }
        subL->addStretch();

        left->addWidget(sub);

        // DETAILS
        details = new QWidget(leftCol);
        details->setVisible(false);

        QVBoxLayout *d = new QVBoxLayout(details);
        d->setContentsMargins(s(5), s(5), s(5), s(5));
        d->setSpacing(s(6));

        QString currentTaskText = agv.task.trimmed();
        if (currentTaskText.isEmpty() || currentTaskText == "—")
            currentTaskText = "—";
        QLabel *task = new QLabel("Текущая задача: " + currentTaskText, details);
        task->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:600;color:#1A1A1A;"
        ).arg(s(14)));

        QLabel *bp = new QLabel(details);
        QPixmap bpPix(agv.blueprintPath);
        if (!bpPix.isNull())
            bp->setPixmap(bpPix.scaled(s(200), s(150), Qt::KeepAspectRatio, Qt::SmoothTransformation));

        d->addWidget(task);
        d->addWidget(bp);

        left->addWidget(details);

        QWidget *rightCol = new QWidget(this);
        rightCol->setMinimumWidth(s(180));
        rightCol->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        QVBoxLayout *right = new QVBoxLayout(rightCol);
        right->setContentsMargins(s(8), s(4), s(2), s(4));
        right->setSpacing(s(10));

        QLabel *onlineStatus = new QLabel(rightCol);
        onlineStatus->setFixedSize(s(28), s(28));
        onlineStatus->setStyleSheet(QString(
            "background:%1;border-radius:%2px;"
        ).arg(statusColor(agv.status)).arg(s(14)));

        detailsButton = new QPushButton("Подробнее", rightCol);
        detailsButton->setStyleSheet(QString(
            "QPushButton{background:#0F00DB;color:white;font-family:Inter;"
            "font-size:%1px;font-weight:700;border-radius:%2px;padding:%3px %4px;}"
            "QPushButton:hover{background:#1A4ACD;}"
        ).arg(s(14)).arg(s(8)).arg(s(5)).arg(s(12)));
        detailsButton->setMinimumHeight(s(38));
        detailsButton->setCursor(Qt::PointingHandCursor);

        connect(detailsButton, &QPushButton::clicked, this, [this](){
            emit openDetailsRequested(agv.id);
        });

        QWidget *controlRow = new QWidget(rightCol);
        QHBoxLayout *controlLay = new QHBoxLayout(controlRow);
        controlLay->setContentsMargins(0, 0, 0, 0);
        controlLay->setSpacing(s(22)); // заметно больший зазор между кружком и кнопкой

        QWidget *statusWrap = new QWidget(controlRow);
        QHBoxLayout *statusLay = new QHBoxLayout(statusWrap);
        statusLay->setContentsMargins(0, 0, 0, 0);
        statusLay->setSpacing(0);
        statusLay->addWidget(onlineStatus);

        controlLay->addWidget(statusWrap, 0, Qt::AlignVCenter);
        controlLay->addWidget(detailsButton, 0, Qt::AlignVCenter);

        right->addStretch();
        right->addWidget(controlRow, 0, Qt::AlignRight | Qt::AlignVCenter);
        right->addStretch();

        root->addWidget(leftCol, 1);
        root->addWidget(rightCol, 0, Qt::AlignRight);
    }

    QString AgvItem::statusColor(const QString &st)
    {
        const QString state = st.trimmed().toLower();
        if (state == "online" || state == "working")
            return "#00C8FF"; // голубой — AGV в сети
        if (state == "offline" || state == "disabled" || state == "off")
            return "#999999"; // серый — отключен
        return "#999999";
    }

    QString AgvItem::maintenanceColor(const QString &state)
    {
        const QString st = state.trimmed().toLower();
        if (st == "red")
            return "#FF0000"; // просрочено
        if (st == "orange")
            return "#FF8800"; // скоро обслуживание
        return "#18CF00";     // обслужен / в норме
    }

    void AgvItem::mouseReleaseEvent(QMouseEvent *event)
    {
        QFrame::mouseReleaseEvent(event);

        bool vis = details->isVisible();
        details->setVisible(!vis);

        arrowLabel->setPixmap(QPixmap(
            vis ?
            ":/new/mainWindowIcons/noback/arrow_down.png" :
            ":/new/mainWindowIcons/noback/arrow_up.png"
        ).scaled(s(18), s(18)));
    }

    // ======================= ListAgvInfo =======================

    ListAgvInfo::ListAgvInfo(std::function<int(int)> scale, QWidget *parent)
        : QFrame(parent), s(scale)
    {
        currentFilter = FilterSettings();

        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet("background-color:#F1F2F4;border-radius:12px;");

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(s(10), s(10), s(10), s(10));
        root->setSpacing(s(12));

        // ШАПКА
        QWidget *header = new QWidget(this);
        QHBoxLayout *hdr = new QHBoxLayout(header);
        hdr->setContentsMargins(0,0,0,0);
        hdr->setSpacing(s(10));

        // Назад
        QPushButton *back = new QPushButton("   Назад", header);
        back->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
        back->setIconSize(QSize(s(24), s(24)));
        back->setFixedSize(s(150), s(50));
        back->setStyleSheet(QString(
            "QPushButton {"
            "   background-color:#E6E6E6;"
            "   border-radius:%1px;"
            "   border:1px solid #C8C8C8;"
            "   font-family:Inter;"
            "   font-size:%2px;"
            "   font-weight:800;"
            "   color:black;"
            "   text-align:left;"
            "   padding-left:%3px;"
            "}"
            "QPushButton:hover { background-color:#D5D5D5; }"
        ).arg(s(10)).arg(s(16)).arg(s(10)));

        connect(back, &QPushButton::clicked, this, [this](){
            emit backRequested();
        });

        hdr->addWidget(back, 0, Qt::AlignLeft);

        // Заголовок
        hdr->addStretch();

        QLabel *title = new QLabel("Список AGV", header);
        title->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
        ).arg(s(26)));
        title->setAlignment(Qt::AlignCenter);

        hdr->addWidget(title, 0, Qt::AlignCenter);

        hdr->addStretch();

        // Кнопка удалить
        QPushButton *deleteBtn = new QPushButton("Удалить", header);
        deleteBtn->setCursor(Qt::PointingHandCursor);
        deleteBtn->setFixedSize(s(165), s(50));
        deleteBtn->setStyleSheet(QString(
            "QPushButton{"
            "   background-color:#FF3B30;"
            "   border:1px solid #C72B22;"
            "   border-radius:%1px;"
            "   font-family:Inter;"
            "   font-size:%2px;"
            "   font-weight:800;"
            "   color:white;"
            "   text-align:center;"
            "   padding:0 14px;"
            "}"
            "QPushButton:hover{background-color:#E4372D;}"
            "QPushButton:pressed{background-color:#C92D24;}"
        ).arg(s(10)).arg(s(16)));

        {
            QString role = getUserRole(AppSession::currentUsername());
            if (role == "viewer")
                deleteBtn->hide();
        }

        hdr->addWidget(deleteBtn, 0, Qt::AlignRight);
        hdr->setAlignment(deleteBtn, Qt::AlignRight);
        deleteBtn->setContentsMargins(0, 0, s(5), 0);

        root->addWidget(header);

        // ЛЕГЕНДА
        QWidget *legend = new QWidget(this);
        QHBoxLayout *lg = new QHBoxLayout(legend);
        lg->setContentsMargins(0, s(7), 0, 0);
        lg->setSpacing(s(12));

        auto makeDot = [&](QString color){
            QLabel *dot = new QLabel(legend);
            dot->setFixedSize(s(18), s(18));
            dot->setStyleSheet(QString("background:%1;border-radius:%2px;")
                               .arg(color).arg(s(9)));
            return dot;
        };

        auto makeLabel = [&](QString text){
            QLabel *l = new QLabel(text, legend);
            l->setStyleSheet(QString(
                "font-family:Inter;font-size:%1px;font-weight:800;color:#222;"
            ).arg(s(18)));
            return l;
        };

        lg->addWidget(makeDot("#18CF00"));
        lg->addWidget(makeLabel("Обслужено"));

        lg->addSpacing(s(18));
        lg->addWidget(makeDot("#FF8800"));
        lg->addWidget(makeLabel("Скоро обслуживание"));

        lg->addSpacing(s(18));
        lg->addWidget(makeDot("#FF0000"));
        lg->addWidget(makeLabel("Просрочено"));

        lg->addSpacing(s(18));
        lg->addWidget(makeDot("#00C8FF"));
        lg->addWidget(makeLabel("В работе"));

        lg->addStretch();

        root->addWidget(legend);

        // СКРОЛЛ
        QScrollArea *scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");

        content = new QWidget();
        content->setStyleSheet("background:transparent;");
        layout = new QVBoxLayout(content);
        layout->setSpacing(s(8));
        layout->setContentsMargins(0,0,0,0);

        scroll->setWidget(content);
        root->addWidget(scroll);

        addBtn_ = new QPushButton("+ Добавить AGV", this);
        addBtn_->setFixedSize(s(320), s(50));
        addBtn_->setStyleSheet(QString(
            "QPushButton { background-color:#0F00DB; border-radius:%1px; font-family:Inter; font-size:%2px; font-weight:800; color:white; }"
            "QPushButton:hover { background-color:#1A4ACD; }"
        ).arg(s(10)).arg(s(16)));
        addBtn_->raise();
        {
            QString role = getUserRole(AppSession::currentUsername());
            if (role == "viewer")
                addBtn_->hide();
        }
        root->addWidget(addBtn_, 0, Qt::AlignHCenter);

        undoToast_ = new QFrame(this);
        undoToast_->setStyleSheet(
            "QFrame{background:#111827;border-radius:12px;}"
            "QLabel{color:white;font-family:Inter;font-size:14px;font-weight:700;background:transparent;}"
            "QPushButton{background:#2563EB;color:white;border:none;border-radius:8px;padding:6px 14px;"
            "font-family:Inter;font-size:13px;font-weight:800;}"
            "QPushButton:hover{background:#1D4ED8;}"
        );
        undoToast_->setFixedHeight(s(50));
        undoToast_->hide();
        QHBoxLayout *undoLay = new QHBoxLayout(undoToast_);
        undoLay->setContentsMargins(s(12), s(8), s(12), s(8));
        undoLay->setSpacing(s(10));
        QLabel *undoText = new QLabel("AGV удалены", undoToast_);
        undoBtn_ = new QPushButton("Вернуть", undoToast_);
        undoLay->addWidget(undoText);
        undoLay->addStretch();
        undoLay->addWidget(undoBtn_);

        undoTimer_ = new QTimer(this);
        undoTimer_->setSingleShot(true);
        connect(undoTimer_, &QTimer::timeout, this, [this](){
            undoToast_->hide();
            clearUndoSnapshot();
        });
        connect(undoBtn_, &QPushButton::clicked, this, [this](){
            restoreDeletedAgvs();
        });

        loadMoreBtn_ = new QPushButton(QStringLiteral("Показать ещё 50"), this);
        loadMoreBtn_->setFixedSize(s(260), s(44));
        loadMoreBtn_->setStyleSheet(QString(
            "QPushButton { background-color:#E6E6E6; border:1px solid #C8C8C8; border-radius:%1px;"
            "font-family:Inter; font-size:%2px; font-weight:800; color:#1A1A1A; }"
            "QPushButton:hover { background-color:#D5D5D5; }"
        ).arg(s(10)).arg(s(15)));
        loadMoreBtn_->hide();
        connect(loadMoreBtn_, &QPushButton::clicked, this, [this](){
            shownCount_ = qMin(shownCount_ + batchSize_, displayQueueTotal_);
            rebuildShownChunk();
        });

        // Важно для производительности:
        // не загружаем/не строим все карточки AGV в конструкторе.
        // Список будет загружен лениво при открытии страницы AGV.


        // CONNECT: Добавить
        connect(addBtn_, &QPushButton::clicked, this, [this](){
            AddAgvDialog dlg(s, this);
            if (dlg.exec() == QDialog::Accepted)
            {
                AgvInfo info;

                QString baseName = dlg.result.name.trimmed();

                QString digits;
                QRegularExpression re("\\d+");
                auto it = re.globalMatch(dlg.result.serial);
                while (it.hasNext())
                    digits += it.next().captured();

                QString last4 = digits.right(4);
                if (last4.isEmpty())
                    last4 = "0000";

                QString modelUpper = dlg.result.model.toUpper();

                QString finalId = QString("%1_%2_%3")
                                    .arg(baseName)
                                    .arg(last4)
                                    .arg(modelUpper);

                info.id = finalId;
                info.model = modelUpper;
                info.serial = dlg.result.serial;
                info.status = dlg.result.status;
                info.task = dlg.result.alias.trimmed();
                info.kilometers = 0;
                info.blueprintPath = ":/new/mainWindowIcons/noback/blueprint.png";
                info.lastActive = QDate::currentDate();

                if (!insertAgvToDb(info)) {
                    qDebug() << "insertAgvToDb: не удалось записать AGV";
                    return;
                }

                if (!copyModelTasksToAgv(info.id, info.model)) {
                    qDebug() << "copyModelTasksToAgv: ошибка копирования задач для" << info.id;
                }

                QVector<AgvInfo> agvs = loadAgvList();
                rebuildList(agvs);
                emit agvListChanged();

            }
        });

        // CONNECT: Удалить
        connect(deleteBtn, &QPushButton::clicked, this, [this](){

            QDialog dlg(this);
            dlg.setWindowTitle("Удалить AGV");
            dlg.setFixedSize(s(460), s(520));
            dlg.setStyleSheet(
                "QDialog { background: #FFFFFF; border: 1px solid #E7E9ED; border-radius: 12px; }"
                "QLabel { background: transparent; font-family: Inter; color: #1A1A1A; }"
                "QLabel#title { font-size: 20px; font-weight: 900; }"
                "QLabel#subtitle { font-size: 14px; font-weight: 600; color: #687083; }"
                "QFrame#listBox { background: #F7F8FA; border: 1px solid #E1E4EA; border-radius: 10px; }"
                "QCheckBox { background: transparent; font-family: Inter; font-size: 15px; color: #1A1A1A; spacing: 8px; }"
                "QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #B7BECC; border-radius: 5px; background: #FFFFFF; }"
                "QCheckBox::indicator:checked { background: #0F00DB; border: 1px solid #0F00DB; }"
                "QPushButton { font-family: Inter; font-size: 15px; font-weight: 800; border-radius: 8px; padding: 7px 14px; border: 1px solid transparent; }"
                "QPushButton#ok { background: #FF3B30; color: white; }"
                "QPushButton#ok:hover { background: #E13228; }"
                "QPushButton#ok:pressed { background: #C92D24; }"
                "QPushButton#cancel { background: #EFF1F5; border-color: #D3D9E4; color: #1A1A1A; }"
                "QPushButton#cancel:hover { background: #E2E6EE; }"
                "QPushButton#selectAll { background: #0F00DB; color: white; border-color: #0B00A6; }"
                "QPushButton#selectAll:hover { background: #1A4ACD; }"
            );

            QVBoxLayout *v = new QVBoxLayout(&dlg);
            v->setContentsMargins(s(18), s(16), s(18), s(16));
            v->setSpacing(s(10));

            QLabel *titleLbl = new QLabel("Удаление AGV", &dlg);
            titleLbl->setObjectName("title");
            v->addWidget(titleLbl);

            QLabel *selectedLbl = new QLabel("Выбрано: 0", &dlg);
            selectedLbl->setObjectName("subtitle");
            v->addWidget(selectedLbl);

            QFrame *listBox = new QFrame(&dlg);
            listBox->setObjectName("listBox");
            QVBoxLayout *listBoxLay = new QVBoxLayout(listBox);
            listBoxLay->setContentsMargins(s(10), s(10), s(10), s(10));
            listBoxLay->setSpacing(s(8));

            QScrollArea *listScroll = new QScrollArea(listBox);
            listScroll->setWidgetResizable(true);
            listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            listScroll->setStyleSheet(
                "QScrollArea { border: none; background: transparent; }"
                "QScrollBar:vertical { width: 8px; background: transparent; margin: 2px; }"
                "QScrollBar::handle:vertical { background: #C5CAD5; border-radius: 4px; min-height: 26px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
            );

            QWidget *listHost = new QWidget(listScroll);
            listHost->setObjectName("listHost");
            listHost->setStyleSheet("background: transparent;");
            QVBoxLayout *listHostLay = new QVBoxLayout(listHost);
            listHostLay->setContentsMargins(0, 0, 0, 0);
            listHostLay->setSpacing(s(6));

            QVector<QCheckBox*> boxes;
            QSqlQuery idsQ(QSqlDatabase::database("main_connection"));
            idsQ.prepare("SELECT agv_id FROM agv_list ORDER BY created_at DESC");
            if (idsQ.exec()) {
                while (idsQ.next()) {
                    const QString agvId = idsQ.value(0).toString();
                    QCheckBox *cb = new QCheckBox(agvId, &dlg);
                    cb->setProperty("db_id", agvId);
                    boxes.push_back(cb);
                    listHostLay->addWidget(cb);
                }
            } else {
                qDebug() << "delete dialog: failed to load AGV ids:" << idsQ.lastError().text();
            }

            if (boxes.isEmpty()) {
                QLabel *emptyLbl = new QLabel("Список AGV пуст.", listHost);
                emptyLbl->setObjectName("subtitle");
                listHostLay->addWidget(emptyLbl);
            }

            listHostLay->addStretch();
            listScroll->setWidget(listHost);
            listBoxLay->addWidget(listScroll);
            v->addWidget(listBox, 1);

            QPushButton *selectAllBtn = new QPushButton("Выбрать все", &dlg);
            selectAllBtn->setObjectName("selectAll");
            selectAllBtn->setCursor(Qt::PointingHandCursor);
            v->addWidget(selectAllBtn);

            auto updateSelectionUi = [selectedLbl, selectAllBtn, boxes]() {
                int selectedCount = 0;
                for (int i = 0; i < boxes.size(); ++i) {
                    if (boxes[i]->isChecked()) {
                        ++selectedCount;
                    }
                }

                selectedLbl->setText(QString("Выбрано: %1").arg(selectedCount));

                const bool allChecked = (!boxes.isEmpty() && selectedCount == boxes.size());
                selectAllBtn->setText(allChecked ? "Снять выбор" : "Выбрать все");
            };

            connect(selectAllBtn, &QPushButton::clicked, &dlg, [boxes, updateSelectionUi]() {
                bool allChecked = true;
                for (int i = 0; i < boxes.size(); ++i) {
                    if (!boxes[i]->isChecked()) {
                        allChecked = false;
                        break;
                    }
                }

                const bool targetState = !allChecked;
                for (int i = 0; i < boxes.size(); ++i) {
                    boxes[i]->setChecked(targetState);
                }

                updateSelectionUi();
            });

            for (int i = 0; i < boxes.size(); ++i) {
                connect(boxes[i], &QCheckBox::toggled, &dlg, [updateSelectionUi](bool checked){
                    Q_UNUSED(checked)
                    updateSelectionUi();
                });
            }

            updateSelectionUi();

            v->addStretch();

            QHBoxLayout *btns = new QHBoxLayout();
            QPushButton *cancel = new QPushButton("Отмена", &dlg);
            cancel->setObjectName("cancel");
            QPushButton *ok = new QPushButton("Удалить", &dlg);
            ok->setObjectName("ok");

            btns->addWidget(cancel);
            btns->addStretch();
            btns->addWidget(ok);
            v->addLayout(btns);

            connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
            connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);

            if (dlg.exec() == QDialog::Accepted) {
                QStringList selectedIds;
                for (QCheckBox *cb : boxes) {
                    if (cb->isChecked()) {
                        const QString id = cb->property("db_id").toString().trimmed();
                        if (!id.isEmpty())
                            selectedIds << id;
                    }
                }

                if (selectedIds.isEmpty()) {
                    QMessageBox::information(this, "Удаление AGV", "Выберите хотя бы один AGV.");
                    return;
                }

                QSqlDatabase db = QSqlDatabase::database("main_connection");
                if (!db.isOpen()) {
                    QMessageBox::warning(this, "Удаление AGV", "База данных не открыта.");
                    return;
                }

                // Снимок данных для "Вернуть"
                lastDeletedAgvs_.clear();
                lastDeletedTasks_.clear();
                lastDeletedHistory_.clear();

                auto loadAgvSnapshot = [&](const QStringList &ids) {
                    const int chunkSize = 300;
                    for (int offset = 0; offset < ids.size(); offset += chunkSize) {
                        const int count = qMin(chunkSize, ids.size() - offset);
                        QStringList placeholders;
                        placeholders.reserve(count);
                        for (int i = 0; i < count; ++i)
                            placeholders << QString(":id%1").arg(i);

                        QSqlQuery q(db);
                        q.prepare(QString(
                            "SELECT agv_id, model, serial, status, alias, kilometers, blueprintPath, lastActive "
                            "FROM agv_list WHERE agv_id IN (%1)").arg(placeholders.join(",")));
                        for (int i = 0; i < count; ++i)
                            q.bindValue(QString(":id%1").arg(i), ids[offset + i]);

                        if (q.exec()) {
                            while (q.next()) {
                                AgvInfo info;
                                info.id = q.value(0).toString();
                                info.model = q.value(1).toString();
                                info.serial = q.value(2).toString();
                                info.status = q.value(3).toString();
                                info.task = q.value(4).toString().trimmed();
                                if (info.task == "—")
                                    info.task.clear();
                                info.kilometers = q.value(5).toInt();
                                info.blueprintPath = q.value(6).toString();
                                info.lastActive = q.value(7).toDate();
                                lastDeletedAgvs_.push_back(info);
                            }
                        }
                    }
                };

                auto loadTaskSnapshot = [&](const QStringList &ids) {
                    const int chunkSize = 300;
                    for (int offset = 0; offset < ids.size(); offset += chunkSize) {
                        const int count = qMin(chunkSize, ids.size() - offset);
                        QStringList placeholders;
                        placeholders.reserve(count);
                        for (int i = 0; i < count; ++i)
                            placeholders << QString(":id%1").arg(i);

                        QSqlQuery q(db);
                        q.prepare(QString(
                            "SELECT agv_id, task_name, task_description, interval_days, duration_minutes, is_default, next_date "
                            "FROM agv_tasks WHERE agv_id IN (%1)").arg(placeholders.join(",")));
                        for (int i = 0; i < count; ++i)
                            q.bindValue(QString(":id%1").arg(i), ids[offset + i]);

                        if (q.exec()) {
                            while (q.next()) {
                                AgvTask t;
                                t.agvId = q.value(0).toString();
                                t.taskName = q.value(1).toString();
                                t.taskDescription = q.value(2).toString();
                                t.intervalDays = q.value(3).toInt();
                                t.durationMinutes = q.value(4).toInt();
                                t.isDefault = q.value(5).toBool();
                                t.nextDate = q.value(6).toDate();
                                lastDeletedTasks_.push_back(t);
                            }
                        }
                    }
                };

                auto loadHistorySnapshot = [&](const QStringList &ids) {
                    const int chunkSize = 300;
                    for (int offset = 0; offset < ids.size(); offset += chunkSize) {
                        const int count = qMin(chunkSize, ids.size() - offset);
                        QStringList placeholders;
                        placeholders.reserve(count);
                        for (int i = 0; i < count; ++i)
                            placeholders << QString(":id%1").arg(i);

                        QSqlQuery q(db);
                        q.prepare(QString(
                            "SELECT agv_id, task_id, task_name, interval_days, completed_at, next_date_after, performed_by "
                            "FROM agv_task_history WHERE agv_id IN (%1)").arg(placeholders.join(",")));
                        for (int i = 0; i < count; ++i)
                            q.bindValue(QString(":id%1").arg(i), ids[offset + i]);

                        if (q.exec()) {
                            while (q.next()) {
                                DeletedHistoryRow h;
                                h.agvId = q.value(0).toString();
                                h.taskId = q.value(1).toInt();
                                h.taskName = q.value(2).toString();
                                h.intervalDays = q.value(3).toInt();
                                h.completedAt = q.value(4).toDate();
                                h.nextDateAfter = q.value(5).toDate();
                                h.performedBy = q.value(6).toString();
                                lastDeletedHistory_.push_back(h);
                            }
                        }
                    }
                };

                loadAgvSnapshot(selectedIds);
                loadTaskSnapshot(selectedIds);
                loadHistorySnapshot(selectedIds);

                auto deleteByIds = [&](const QString &table, const QString &column) -> bool {
                    const int chunkSize = 300;
                    for (int offset = 0; offset < selectedIds.size(); offset += chunkSize) {
                        const int count = qMin(chunkSize, selectedIds.size() - offset);
                        QStringList placeholders;
                        placeholders.reserve(count);

                        QSqlQuery q(db);
                        for (int i = 0; i < count; ++i)
                            placeholders << QString(":id%1").arg(i);

                        q.prepare(QString("DELETE FROM %1 WHERE %2 IN (%3)")
                                  .arg(table, column, placeholders.join(",")));
                        for (int i = 0; i < count; ++i)
                            q.bindValue(QString(":id%1").arg(i), selectedIds[offset + i]);

                        if (!q.exec()) {
                            qDebug() << "Batch delete failed for" << table << ":" << q.lastError().text();
                            return false;
                        }
                    }
                    return true;
                };

                const bool txSupported = db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions);
                bool txStarted = false;
                if (txSupported)
                    txStarted = db.transaction();

                // Порядок важен: сначала зависимые таблицы, потом agv_list.
                bool okDelete = deleteByIds("agv_task_history", "agv_id")
                             && deleteByIds("agv_tasks", "agv_id")
                             && deleteByIds("agv_list", "agv_id");

                if (!okDelete) {
                    if (txStarted)
                        db.rollback();
                    QMessageBox::warning(this, "Удаление AGV", "Не удалось удалить выбранные AGV.");
                    return;
                }

                if (txStarted && !db.commit()) {
                    db.rollback();
                    QMessageBox::warning(this, "Удаление AGV", "Ошибка сохранения удаления.");
                    return;
                }

                logAction(AppSession::currentUsername(),
                          "agv_deleted_batch",
                          QString("Пакетное удаление AGV: %1 шт.").arg(selectedIds.size()));

                QVector<AgvInfo> agvs = loadAgvList();
                rebuildList(agvs);
                emit agvListChanged();
                emit DataBus::instance().agvListChanged();
                emit DataBus::instance().calendarChanged();
                showUndoToast();
            }
        });

    }

    void ListAgvInfo::resizeEvent(QResizeEvent *event)
    {
        QFrame::resizeEvent(event);
        if (addBtn_) {
            int x = (width() - addBtn_->width()) / 2;
            int y = height() - addBtn_->height() - s(20);
            addBtn_->move(x, y);
            addBtn_->raise();
        }
        if (undoToast_) {
            int x = width() - undoToast_->width() - s(20);
            int y = height() - undoToast_->height() - s(20);
            if (x < s(10)) x = s(10);
            if (y < s(10)) y = s(10);
            undoToast_->move(x, y);
            undoToast_->raise();
        }
    }

    // ======================= applyFilter =======================

    void ListAgvInfo::applyFilter(const FilterSettings &fs)
    {
        QVector<AgvInfo> list = loadAgvList();

        // Фильтрация по названию / модели
        if (!fs.nameFilter.isEmpty()) {
            list.erase(
                std::remove_if(list.begin(), list.end(),
                    [&](const AgvInfo &a){
                        return !a.id.contains(fs.nameFilter, Qt::CaseInsensitive)
                            && !a.model.contains(fs.nameFilter, Qt::CaseInsensitive);
                    }),
                list.end()
            );
        }

        // Приоритет групп: serv → up → over → modelSort → km

        // Группа 1 — обслуженные (километры)
        if (fs.serv == FilterSettings::Asc) {
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.kilometers < b.kilometers;
                });
        }
        else if (fs.serv == FilterSettings::Desc) {
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.kilometers > b.kilometers;
                });
        }
        // Группа 2 — ближайшие (дата)
        else if (fs.up == FilterSettings::UpAsc) {
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.lastActive < b.lastActive;
                });
        }
        else if (fs.up == FilterSettings::UpDesc) {
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.lastActive > b.lastActive;
                });
        }
        // Группа 3 — просроченные (дата)
        else if (fs.over == FilterSettings::OverOld) {
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.lastActive < b.lastActive;
                });
        }
        else if (fs.over == FilterSettings::OverNew) {
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.lastActive > b.lastActive;
                });
        }
        // Группа 4 — модель
        else if (fs.modelSort == FilterSettings::ModelAZ) {
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.model < b.model;
                });
        }
        else if (fs.modelSort == FilterSettings::ModelZA) {
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.model > b.model;
                });
        }
        // Группа 5 — пробег
        else if (fs.km == FilterSettings::KmAsc) {
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.kilometers < b.kilometers;
                });
        }
        else if (fs.km == FilterSettings::KmDesc) {
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.kilometers > b.kilometers;
                });
        }

        rebuildList(list);
    }

    // ======================= rebuildList =======================

    void ListAgvInfo::rebuildList(const QVector<AgvInfo> &list)
    {
        currentDisplayList_ = list;
        shownCount_ = batchSize_;
        appearRetryLeft_ = 2;
        hasRenderedState_ = false;
        rebuildShownChunk();
    }

    void ListAgvInfo::rebuildShownChunk()
    {
        if (content)
            content->setUpdatesEnabled(false);

        QLayoutItem *child;
        while ((child = layout->takeAt(0)) != nullptr) {
            if (child->widget()) {
                if (child->widget() == loadMoreBtn_) {
                    loadMoreBtn_->setParent(this);
                } else {
                    child->widget()->deleteLater();
                }
            }
            delete child;
        }

        if (currentDisplayList_.isEmpty()) {
            displayQueueTotal_ = 0;
            if (loadMoreBtn_)
                loadMoreBtn_->hide();
            QLabel *emptyLabel = new QLabel("Здесь ничего нет", content);
            emptyLabel->setAlignment(Qt::AlignCenter);
            emptyLabel->setStyleSheet(QString(
                "font-family:Inter;"
                "font-size:%1px;"
                "font-weight:900;"
                "color:#555;"
            ).arg(s(28)));

            layout->addStretch();
            layout->addWidget(emptyLabel, 0, Qt::AlignCenter);
            layout->addStretch();
            hasRenderedState_ = true;
            if (content) {
                content->setUpdatesEnabled(true);
                content->update();
            }
            return;
        }

        QString currentUser = AppSession::currentUsername();
        const QString curRole = getUserRole(currentUser);

        const bool isStaff = (curRole == QStringLiteral("admin") || curRole == QStringLiteral("tech"));

        QVector<AgvInfo> mO, mS, mD, cO, cS, cD;
        for (const AgvInfo &a : currentDisplayList_) {
            const QString assignee = a.assignedUser.trimmed();
            const bool isMine = (assignee == currentUser);
            const bool isUnassigned = assignee.isEmpty();
            const bool overdue = a.hasOverdueMaintenance;
            const bool soon = a.hasSoonMaintenance && !overdue;
            auto push = [&](QVector<AgvInfo> &ov, QVector<AgvInfo> &sn, QVector<AgvInfo> &dn) {
                if (overdue) ov.append(a);
                else if (soon) sn.append(a);
                else dn.append(a);
            };
            if (isMine)
                push(mO, mS, mD);
            // viewer: в «Общие» только свободные; admin/tech: в «Общие» абсолютно все AGV (и дублируются в блоке владельца)
            if (isStaff)
                push(cO, cS, cD);
            else if (isUnassigned)
                push(cO, cS, cD);
        }

        QHash<QString, AgvInfo> agvFull;
        agvFull.reserve(currentDisplayList_.size());
        for (const AgvInfo &a : currentDisplayList_)
            agvFull.insert(a.id, a);

        QVector<AgvDispSlot> queue;
        auto appendVec = [&queue](const QVector<AgvInfo> &v, AgvDispSlot::Kind k) {
            for (const AgvInfo &x : v)
                queue.push_back(AgvDispSlot(x, k));
        };

        appendVec(mO, AgvDispSlot::MineOver);
        appendVec(mS, AgvDispSlot::MineSoon);
        appendVec(mD, AgvDispSlot::MineDone);
        appendVec(cO, AgvDispSlot::ComOver);
        appendVec(cS, AgvDispSlot::ComSoon);
        appendVec(cD, AgvDispSlot::ComDone);

        QMap<QString, QVector<AgvInfo>> delFull;
        if (curRole == QStringLiteral("admin") || curRole == QStringLiteral("tech")) {
            QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
            if (db.isOpen()) {
                QSqlQuery q(db);
                q.prepare(QStringLiteral(
                    "SELECT DISTINCT assigned_to, agv_id "
                    "FROM agv_tasks "
                    "WHERE assigned_to != '' AND assigned_to != :me "
                    "ORDER BY assigned_to, agv_id"));
                q.bindValue(QStringLiteral(":me"), currentUser);
                if (q.exec()) {
                    QHash<QString, QSet<QString>> seen;
                    while (q.next()) {
                        const QString delegatedUser = q.value(0).toString().trimmed();
                        const QString agvId = q.value(1).toString().trimmed();
                        if (delegatedUser.isEmpty() || agvId.isEmpty())
                            continue;
                        if (!agvFull.contains(agvId))
                            continue;
                        if (seen[delegatedUser].contains(agvId))
                            continue;
                        seen[delegatedUser].insert(agvId);
                        delFull[delegatedUser].append(agvFull.value(agvId));
                    }
                }
            }
            // AGV закреплены в agv_list за другим пользователем (без задач в agv_tasks) — тот же блок по имени владельца
            QHash<QString, QSet<QString>> idsInDel;
            for (auto it = delFull.constBegin(); it != delFull.constEnd(); ++it) {
                for (const AgvInfo &x : it.value())
                    idsInDel[it.key()].insert(x.id);
            }
            for (const AgvInfo &a : currentDisplayList_) {
                const QString owner = a.assignedUser.trimmed();
                if (owner.isEmpty() || owner == currentUser)
                    continue;
                if (!agvFull.contains(a.id))
                    continue;
                if (idsInDel[owner].contains(a.id))
                    continue;
                idsInDel[owner].insert(a.id);
                delFull[owner].append(a);
            }
        }

        QStringList delUsers = delFull.keys();
        std::sort(delUsers.begin(), delUsers.end());

        QMap<QString, QVector<AgvInfo>> delOFull, delSFull, delDFull;
        for (const QString &du : delUsers) {
            QVector<AgvInfo> dO, dS, dD;
            for (const AgvInfo &a : delFull.value(du)) {
                if (a.hasOverdueMaintenance)
                    dO.append(a);
                else if (a.hasSoonMaintenance)
                    dS.append(a);
                else
                    dD.append(a);
            }
            delOFull.insert(du, dO);
            delSFull.insert(du, dS);
            delDFull.insert(du, dD);
            for (const AgvInfo &x : dO)
                queue.push_back(AgvDispSlot(x, AgvDispSlot::DelOver, du));
            for (const AgvInfo &x : dS)
                queue.push_back(AgvDispSlot(x, AgvDispSlot::DelSoon, du));
            for (const AgvInfo &x : dD)
                queue.push_back(AgvDispSlot(x, AgvDispSlot::DelDone, du));
        }

        const int totalSlots = queue.size();
        displayQueueTotal_ = totalSlots;
        shownCount_ = qBound(0, shownCount_, totalSlots);
        const QVector<AgvDispSlot> visibleSlots = queue.mid(0, shownCount_);

        QVector<AgvInfo> mineOverdue, mineSoon, mineDone;
        QVector<AgvInfo> commonOverdue, commonSoon, commonDone;
        QMap<QString, QVector<AgvInfo>> vDelO, vDelS, vDelD;
        for (const AgvDispSlot &sl : visibleSlots) {
            switch (sl.kind) {
            case AgvDispSlot::MineOver: mineOverdue.append(sl.info); break;
            case AgvDispSlot::MineSoon: mineSoon.append(sl.info); break;
            case AgvDispSlot::MineDone: mineDone.append(sl.info); break;
            case AgvDispSlot::ComOver: commonOverdue.append(sl.info); break;
            case AgvDispSlot::ComSoon: commonSoon.append(sl.info); break;
            case AgvDispSlot::ComDone: commonDone.append(sl.info); break;
            case AgvDispSlot::DelOver: vDelO[sl.delegUser].append(sl.info); break;
            case AgvDispSlot::DelSoon: vDelS[sl.delegUser].append(sl.info); break;
            case AgvDispSlot::DelDone: vDelD[sl.delegUser].append(sl.info); break;
            }
        }

        const bool hasMineFull = !mO.isEmpty() || !mS.isEmpty() || !mD.isEmpty();
        const bool hasCommonFull = !cO.isEmpty() || !cS.isEmpty() || !cD.isEmpty();

        auto addSubSectionStyled = [&](QVBoxLayout *parentLayout, const QString &title,
                                       const QVector<AgvInfo> &items, int fullCount, bool defaultExpanded,
                                       CollapsibleSection::SectionStyle subStyle) {
            if (fullCount <= 0)
                return;
            CollapsibleSection *sec = new CollapsibleSection(
                QStringLiteral("%1 (%2)").arg(title).arg(fullCount),
                defaultExpanded, s, content, subStyle);
            for (const AgvInfo &info : items) {
                AgvItem *item = new AgvItem(info, s, sec);
                connect(item, &AgvItem::openDetailsRequested, this, [this](const QString &id) {
                    emit openAgvDetails(id);
                });
                sec->contentLayout()->addWidget(item);
            }
            parentLayout->addWidget(sec);
        };

        if (hasMineFull) {
            const int mineFullCount = mO.size() + mS.size() + mD.size();
            CollapsibleSection *mineParent = new CollapsibleSection(
                QStringLiteral("Ваши (%1)").arg(mineFullCount), true, s, content, CollapsibleSection::StyleMine);
            addSubSectionStyled(mineParent->contentLayout(), QStringLiteral("Просроченные"),
                                mineOverdue, mO.size(), true, CollapsibleSection::StyleOverdue);
            addSubSectionStyled(mineParent->contentLayout(), QStringLiteral("Скоро обслуживание"),
                                mineSoon, mS.size(), true, CollapsibleSection::StyleSoon);
            addSubSectionStyled(mineParent->contentLayout(), QStringLiteral("Обслужены"),
                                mineDone, mD.size(), true, CollapsibleSection::StyleDone);
            layout->addWidget(mineParent);
        }
        if (hasCommonFull) {
            const int commonFullCount = cO.size() + cS.size() + cD.size();
            const bool expandCommon = !hasMineFull;
            CollapsibleSection *commonParent = new CollapsibleSection(
                QStringLiteral("Общие (%1)").arg(commonFullCount), expandCommon, s, content, CollapsibleSection::StyleCommon);
            addSubSectionStyled(commonParent->contentLayout(), QStringLiteral("Просроченные"),
                                commonOverdue, cO.size(), expandCommon, CollapsibleSection::StyleOverdue);
            addSubSectionStyled(commonParent->contentLayout(), QStringLiteral("Скоро обслуживание"),
                                commonSoon, cS.size(), expandCommon, CollapsibleSection::StyleSoon);
            addSubSectionStyled(commonParent->contentLayout(), QStringLiteral("Обслужены"),
                                commonDone, cD.size(), expandCommon, CollapsibleSection::StyleDone);
            layout->addWidget(commonParent);
        }

        if (curRole == QStringLiteral("admin") || curRole == QStringLiteral("tech")) {
            for (const QString &delegatedUser : delUsers) {
                const QVector<AgvInfo> &fullList = delFull.value(delegatedUser);
                if (fullList.isEmpty())
                    continue;
                CollapsibleSection *delegatedParent = new CollapsibleSection(
                    QStringLiteral("%1 (%2)").arg(delegatedUser).arg(fullList.size()),
                    false, s, content, CollapsibleSection::StyleDelegated);
                addSubSectionStyled(delegatedParent->contentLayout(), QStringLiteral("Просроченные"),
                                    vDelO.value(delegatedUser), delOFull.value(delegatedUser).size(), false,
                                    CollapsibleSection::StyleOverdue);
                addSubSectionStyled(delegatedParent->contentLayout(), QStringLiteral("Скоро обслуживание"),
                                    vDelS.value(delegatedUser), delSFull.value(delegatedUser).size(), false,
                                    CollapsibleSection::StyleSoon);
                addSubSectionStyled(delegatedParent->contentLayout(), QStringLiteral("Обслужены"),
                                    vDelD.value(delegatedUser), delDFull.value(delegatedUser).size(), false,
                                    CollapsibleSection::StyleDone);
                layout->addWidget(delegatedParent);
            }
        }

        if (shownCount_ < totalSlots) {
            const int rem = totalSlots - shownCount_;
            const int nextN = qMin(batchSize_, rem);
            loadMoreBtn_->setText(QStringLiteral("Показать ещё %1").arg(nextN));
            loadMoreBtn_->show();
            layout->addWidget(loadMoreBtn_, 0, Qt::AlignHCenter);
        } else {
            loadMoreBtn_->hide();
        }

        layout->addStretch();
        hasRenderedState_ = true;
        if (content) {
            content->setUpdatesEnabled(true);
            content->update();
        }
        runListAppearSmokeTest(1);
    }

    bool ListAgvInfo::hasRenderedState() const
    {
        return hasRenderedState_;
    }

    void ListAgvInfo::runListAppearSmokeTest(int expectedVisible)
    {
        if (expectedVisible <= 0 || appearRetryLeft_ <= 0)
            return;

        QTimer::singleShot(80, this, [this, expectedVisible]() {
            if (!content)
                return;
            const int rendered = content->findChildren<AgvItem*>().size();
            if (rendered > 0)
                return;

            --appearRetryLeft_;
            qDebug() << "ListAgvInfo smoke-test: список пуст после рендера, повторная отрисовка. retry="
                     << appearRetryLeft_ << "expectedVisible=" << expectedVisible;
            rebuildShownChunk();
        });
    }

    void ListAgvInfo::showUndoToast()
    {
        if (!undoToast_)
            return;
        undoToast_->show();
        undoToast_->raise();
        undoTimer_->start(15000);
    }

    void ListAgvInfo::clearUndoSnapshot()
    {
        lastDeletedAgvs_.clear();
        lastDeletedTasks_.clear();
        lastDeletedHistory_.clear();
    }

    void ListAgvInfo::restoreDeletedAgvs()
    {
        if (lastDeletedAgvs_.isEmpty())
            return;

        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (!db.isOpen()) {
            QMessageBox::warning(this, "Восстановление AGV", "База данных не открыта.");
            return;
        }

        const bool txSupported = db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions);
        bool txStarted = false;
        if (txSupported)
            txStarted = db.transaction();

        for (const AgvInfo &info : lastDeletedAgvs_) {
            QSqlQuery q(db);
            q.prepare(R"(
                INSERT INTO agv_list
                (agv_id, model, serial, status, alias, kilometers, blueprintPath, lastActive)
                VALUES (:agv_id, :model, :serial, :status, :alias, :kilometers, :blueprintPath, :lastActive)
            )");
            q.bindValue(":agv_id", info.id);
            q.bindValue(":model", info.model);
            q.bindValue(":serial", info.serial);
            q.bindValue(":status", info.status);
            q.bindValue(":alias", info.task);
            q.bindValue(":kilometers", info.kilometers);
            q.bindValue(":blueprintPath", info.blueprintPath);
            q.bindValue(":lastActive", info.lastActive);
            if (!q.exec()) {
                if (txStarted) db.rollback();
                QMessageBox::warning(this, "Восстановление AGV", "Ошибка восстановления AGV: " + q.lastError().text());
                return;
            }
        }

        for (const AgvTask &t : lastDeletedTasks_) {
            QSqlQuery q(db);
            q.prepare(R"(
                INSERT INTO agv_tasks
                (agv_id, task_name, task_description, interval_days, duration_minutes, is_default, next_date)
                VALUES (:agv_id, :name, :dsc, :days, :mins, :def, :next)
            )");
            q.bindValue(":agv_id", t.agvId);
            q.bindValue(":name", t.taskName);
            q.bindValue(":dsc", t.taskDescription);
            q.bindValue(":days", t.intervalDays);
            q.bindValue(":mins", t.durationMinutes);
            q.bindValue(":def", t.isDefault ? 1 : 0);
            q.bindValue(":next", t.nextDate);
            if (!q.exec()) {
                if (txStarted) db.rollback();
                QMessageBox::warning(this, "Восстановление AGV", "Ошибка восстановления задач: " + q.lastError().text());
                return;
            }
        }

        for (const DeletedHistoryRow &h : lastDeletedHistory_) {
            QSqlQuery q(db);
            q.prepare(R"(
                INSERT INTO agv_task_history
                (agv_id, task_id, task_name, interval_days, completed_at, next_date_after, performed_by)
                VALUES (:agv, :tid, :name, :intv, :done, :next, :by)
            )");
            q.bindValue(":agv", h.agvId);
            q.bindValue(":tid", h.taskId);
            q.bindValue(":name", h.taskName);
            q.bindValue(":intv", h.intervalDays);
            q.bindValue(":done", h.completedAt);
            q.bindValue(":next", h.nextDateAfter);
            q.bindValue(":by", h.performedBy);
            q.exec();
        }

        if (txStarted && !db.commit()) {
            db.rollback();
            QMessageBox::warning(this, "Восстановление AGV", "Ошибка сохранения восстановления.");
            return;
        }

        logAction(AppSession::currentUsername(), "agv_restore_batch",
                  QString("Восстановлено AGV: %1 шт.").arg(lastDeletedAgvs_.size()));

        clearUndoSnapshot();
        if (undoToast_) undoToast_->hide();

        QVector<AgvInfo> agvs = loadAgvList();
        rebuildList(agvs);
        emit agvListChanged();
        emit DataBus::instance().agvListChanged();
        emit DataBus::instance().calendarChanged();
    }

    // ======================= loadAgvList =======================

    QVector<AgvInfo> ListAgvInfo::loadAgvList()
    {
        QVector<AgvInfo> list;

        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (!db.isOpen()) {
            qDebug() << "loadAgvList: main_connection НЕ ОТКРЫТА!";
            return list;
        }

        QSqlQuery q(db);
        q.prepare("SELECT agv_id, model, serial, status, alias, kilometers, blueprintPath, lastActive, "
                  "COALESCE(assigned_user, '') FROM agv_list ORDER BY created_at DESC");

        if (!q.exec()) {
            qDebug() << "loadAgvList: запрос не выполнился:" << q.lastError().text();
            return list;
        }

        while (q.next()) {
            AgvInfo info;
            info.id           = q.value(0).toString();
            info.model        = q.value(1).toString();
            info.serial       = q.value(2).toString();
            info.status       = q.value(3).toString();
            info.maintenanceState = "green";
            info.hasOverdueMaintenance = false;
            info.hasSoonMaintenance = false;
            info.task         = q.value(4).toString().trimmed();
            if (info.task == "—")
                info.task.clear();
            info.kilometers   = q.value(5).toInt();
            info.blueprintPath= q.value(6).toString();
            info.lastActive   = q.value(7).toDate();
            info.assignedUser = q.value(8).toString().trimmed();

            if (info.blueprintPath.isEmpty())
                info.blueprintPath = ":/new/mainWindowIcons/noback/blueprint.png";

            list.push_back(info);
        }

        QMap<QString, QString> maintenanceByAgv;
        QMap<QString, bool> hasOverdueByAgv;
        QMap<QString, bool> hasSoonByAgv;
        for (int i = 0; i < list.size(); ++i)
        {
            maintenanceByAgv[list[i].id] = "green";
            hasOverdueByAgv[list[i].id] = false;
            hasSoonByAgv[list[i].id] = false;
        }

        QSqlQuery tasksQ(db);
        tasksQ.prepare(R"(
            SELECT
                agv_id,
                MAX(CASE WHEN next_date <= DATE_ADD(CURDATE(), INTERVAL 3 DAY) THEN 1 ELSE 0 END) AS has_overdue,
                MAX(CASE WHEN next_date > DATE_ADD(CURDATE(), INTERVAL 3 DAY)
                          AND next_date <= DATE_ADD(CURDATE(), INTERVAL 6 DAY)
                         THEN 1 ELSE 0 END) AS has_soon
            FROM agv_tasks
            GROUP BY agv_id
        )");
        if (tasksQ.exec()) {
            while (tasksQ.next()) {
                const QString agvId = tasksQ.value(0).toString();
                if (!maintenanceByAgv.contains(agvId))
                    continue;

                const bool hasOverdue = tasksQ.value(1).toInt() > 0;
                const bool hasSoon = tasksQ.value(2).toInt() > 0;
                if (hasOverdue) {
                    hasOverdueByAgv[agvId] = true;
                    maintenanceByAgv[agvId] = "red";
                }
                if (hasSoon) {
                    hasSoonByAgv[agvId] = true;
                    if (!hasOverdue)
                        maintenanceByAgv[agvId] = "orange";
                }
            }
        } else {
            qDebug() << "loadAgvList: agv_tasks query failed:" << tasksQ.lastError().text();
        }

        for (int i = 0; i < list.size(); ++i) {
            list[i].maintenanceState = maintenanceByAgv.value(list[i].id, "green");
            list[i].hasOverdueMaintenance = hasOverdueByAgv.value(list[i].id, false);
            list[i].hasSoonMaintenance = hasSoonByAgv.value(list[i].id, false);
        }

        const QString me = AppSession::currentUsername();
        if (getUserRole(me) == QStringLiteral("viewer")) {
            list.erase(std::remove_if(list.begin(), list.end(),
                [&me](const AgvInfo &a) {
                    const QString u = a.assignedUser.trimmed();
                    return !u.isEmpty() && u != me;
                }), list.end());
        }

        qDebug() << "loadAgvList: загружено записей:" << list.size();
        return list;
    }

    // ======================= addAgv =======================

    void ListAgvInfo::addAgv(const AgvInfo &info)
    {
        AgvItem *item = new AgvItem(info, s, content);

        connect(item, &AgvItem::openDetailsRequested, this, [this](const QString &id){
            emit openAgvDetails(id);
        });

        int count = layout->count();
        if (count > 0) {
            QLayoutItem *last = layout->itemAt(count - 1);
            if (last->spacerItem()) {
                layout->insertWidget(count - 1, item);
            } else {
                layout->addWidget(item);
            }
        } else {
            layout->addWidget(item);
            layout->addStretch();
        }
    }

    // ======================= deleteAgvFromDb =======================

    static bool deleteAgvFromDb(const QString &id)
    {
        QSqlQuery q(QSqlDatabase::database("main_connection"));
        q.prepare("DELETE FROM agv_list WHERE agv_id = :id");
        q.bindValue(":id", id);

        if (!q.exec()) {
            qDebug() << "Ошибка удаления AGV из БД:" << q.lastError().text();
            return false;
        }

        logAction(AppSession::currentUsername(),
                  "agv_deleted",
                  QString("Удален AGV: %1").arg(id));

        return true;
    }

    // ======================= removeAgvById =======================

    void ListAgvInfo::removeAgvById(const QString &id)
    {
        if (!deleteAgvFromDb(id)) {
            qDebug() << "removeAgvById: не удалось удалить AGV из базы";
            return;
        }

        for (int i = 0; i < layout->count(); ++i) {
            QLayoutItem *it = layout->itemAt(i);
            QWidget *w = it ? it->widget() : nullptr;
            AgvItem *agvItem = qobject_cast<AgvItem*>(w);

            if (agvItem && agvItem->agvId() == id) {
                layout->removeItem(it);
                agvItem->deleteLater();
                delete it;
                break;
            }
        }

        QVector<AgvInfo> agvs = loadAgvList();
        rebuildList(agvs);


        qDebug() << "AGV" << id << "успешно удалён из БД и UI";
    }

    // ======================= insertAgvToDb =======================

    bool insertAgvToDb(const AgvInfo &info)
    {
        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (!db.isOpen()) {
            qDebug() << "insertAgvToDb: main_connection НЕ ОТКРЫТА!";
            return false;
        }

        QSqlQuery q(db);
        q.prepare(R"(
            INSERT INTO agv_list
            (agv_id, model, serial, status, alias, kilometers, blueprintPath, lastActive)
            VALUES (:agv_id, :model, :serial, :status, :alias, :kilometers, :blueprintPath, :lastActive)
        )");

        q.bindValue(":agv_id", info.id);
        q.bindValue(":model", info.model);
        q.bindValue(":serial", info.serial);
        q.bindValue(":status", info.status);
        q.bindValue(":alias", info.task);
        q.bindValue(":kilometers", info.kilometers);
        q.bindValue(":blueprintPath", info.blueprintPath);
        q.bindValue(":lastActive", info.lastActive);

        if (!q.exec()) {
            qDebug() << "Ошибка вставки AGV:" << q.lastError().text();
            return false;
        }
        db.commit(); // явная фиксация в БД (phpMyAdmin покажет после обновления таблицы)
        qDebug() << "AGV добавлен в БД:" << db.hostName() << db.databaseName() << "agv_id=" << info.id;

        logAction(AppSession::currentUsername(),
                  "agv_created",
                  QString("Создан AGV: %1; модель=%2; serial=%3")
                      .arg(info.id, info.model, info.serial));

        emit DataBus::instance().agvListChanged();
        return true;
    }

