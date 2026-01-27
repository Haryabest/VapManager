    #include "listagvinfo.h"
    #include "AddAgvDialog.h"

    #include <QRegularExpression>
    #include <QRegularExpressionMatch>
    #include <QRegularExpressionMatchIterator>

    #include <QHBoxLayout>
    #include <QVBoxLayout>
    #include <QScrollArea>
    #include <QPushButton>
    #include <QLabel>
    #include <QEvent>
    #include <QPixmap>
    #include <QIcon>
    #include <QLayoutItem>
    #include <QDialog>
    #include <QFrame>
    #include <QLineEdit>
    #include <QPainter>
    #include <QCheckBox>
    #include <QMouseEvent>
    #include <QComboBox>
    //
    // ======================= ToggleSwitch =======================
    //

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

            QRect circle = on ? QRect(width()-26, 2, 24, 24)
                              : QRect(2, 2, 24, 24);

            p.setBrush(knob);
            p.drawEllipse(circle);
        }
    };

    //
    // ======================= FilterDialog5 =======================
    //

    class FilterDialog5 : public QDialog
    {
    public:
        FilterDialog5(const FilterSettings &cur, QWidget *parent = nullptr)
            : QDialog(parent)
        {
            setWindowTitle("Фильтры AGV");
            setFixedSize(620, 600);

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
                QWidget *w = new QWidget();
                QHBoxLayout *h = new QHBoxLayout(w);
                h->setContentsMargins(0,0,0,0);
                h->setSpacing(6);

                QLabel *lbl = new QLabel(text);
                lbl->setStyleSheet("font-size:16px;font-weight:500;color:#222;");

                ToggleSwitch *sw = new ToggleSwitch();

                h->addWidget(lbl);
                h->addStretch();
                h->addWidget(sw);

                return std::make_pair(w, sw);
            };

            //
            // ===== 1-я строка =====
            //
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

            swServAsc->setChecked(cur.servAsc);
            swServDesc->setChecked(cur.servDesc);

            servL->addWidget(servTitle);
            servL->addWidget(servAscRow);
            servL->addWidget(servDescRow);
            servL->addWidget(makeLine("#18CF00"));

            //
            // ===== Ближайшие =====
            //
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

            swUpAsc->setChecked(cur.upAsc);
            swUpDesc->setChecked(cur.upDesc);

            upL->addWidget(upTitle);
            upL->addWidget(upAscRow);
            upL->addWidget(upDescRow);
            upL->addWidget(makeLine("#FF8800"));

            row1->addWidget(servBox);
            row1->addWidget(upBox);

            root->addLayout(row1);

            //
            // ===== 2-я строка =====
            //
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

            swOverOld->setChecked(cur.overOld);
            swOverNew->setChecked(cur.overNew);

            overL->addWidget(overTitle);
            overL->addWidget(overOldRow);
            overL->addWidget(overNewRow);
            overL->addWidget(makeLine("#FF0000"));

            //
            // ===== Сортировки =====
            //
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

            swModelAZ->setChecked(cur.modelAZ);
            swModelZA->setChecked(cur.modelZA);
            swKmAsc->setChecked(cur.kmAsc);
            swKmDesc->setChecked(cur.kmDesc);

            sortL->addWidget(sortTitle);
            sortL->addWidget(modelAZRow);
            sortL->addWidget(modelZARow);
            sortL->addWidget(kmAscRow);
            sortL->addWidget(kmDescRow);
            sortL->addWidget(makeLine("#888888"));

            row2->addWidget(overBox);
            row2->addWidget(sortBox);

            root->addLayout(row2);

            //
            // ===== По названию =====
            //
            QLabel *nameTitle = new QLabel("По названию");
            nameTitle->setStyleSheet("font-size:20px;font-weight:900;color:#1A1A1A;");

            QLineEdit *nameEdit = new QLineEdit();
            nameEdit->setPlaceholderText("Введите часть названия...");
            nameEdit->setStyleSheet("padding:8px;font-size:16px;border-radius:8px;border:1px solid #CCC;");

            root->addWidget(nameTitle);
            root->addWidget(nameEdit);
            root->addWidget(makeLine("#1A1A1A"));

            //
            // ===== Кнопки =====
            //
            QHBoxLayout *btns = new QHBoxLayout();
            QPushButton *reset = new QPushButton("Сбросить");
            QPushButton *apply = new QPushButton("Применить");

            btns->addWidget(reset);
            btns->addStretch();
            btns->addWidget(apply);

            root->addStretch();
            root->addLayout(btns);

            connect(apply, &QPushButton::clicked, this, [=](){
                result.servAsc = swServAsc->isChecked();
                result.servDesc = swServDesc->isChecked();

                result.upAsc = swUpAsc->isChecked();
                result.upDesc = swUpDesc->isChecked();

                result.overOld = swOverOld->isChecked();
                result.overNew = swOverNew->isChecked();

                result.modelAZ = swModelAZ->isChecked();
                result.modelZA = swModelZA->isChecked();
                result.kmAsc = swKmAsc->isChecked();
                result.kmDesc = swKmDesc->isChecked();

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

    //
    // ======================= AgvItem =======================
    //

    AgvItem::AgvItem(const AgvInfo &info, std::function<int(int)> scale, QWidget *parent)
        : QFrame(parent), agv(info), s(scale)
    {
        setObjectName("agvItem");
        setStyleSheet(
            "#agvItem{background:white;border-radius:10px;border:1px solid #E0E0E0;}"
            "#agvItem:hover{background:#F7F7F7;}"
        );

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(s(12), s(10), s(12), s(10));
        root->setSpacing(s(6));

        header = new QWidget(this);
        QHBoxLayout *h = new QHBoxLayout(header);
        h->setContentsMargins(0,0,0,0);
        h->setSpacing(s(10));

        QLabel *icon = new QLabel(header);
        icon->setPixmap(QPixmap(":/new/mainWindowIcons/noback/agvIcon.png")
                        .scaled(s(32), s(32), Qt::KeepAspectRatio, Qt::SmoothTransformation));

        QLabel *title = new QLabel(agv.id + " — " + agv.model, header);
        title->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:800;color:#1A1A1A;"
        ).arg(s(16)));

        arrowLabel = new QLabel(header);
        arrowLabel->setPixmap(QPixmap(":/new/mainWindowIcons/noback/arrow_down.png")
                              .scaled(s(18), s(18)));

        detailsButton = new QPushButton("Подробнее", header);
        detailsButton->setStyleSheet(QString(
            "QPushButton{background:#0F00DB;color:white;font-family:Inter;"
            "font-size:%1px;font-weight:700;border-radius:%2px;padding:%3px %4px;}"
            "QPushButton:hover{background:#1A4ACD;}"
        ).arg(s(14)).arg(s(6)).arg(s(4)).arg(s(10)));

        connect(detailsButton, &QPushButton::clicked, this, [this](){
            emit openDetailsRequested(agv.id);
        });

        h->addWidget(icon);
        h->addWidget(title);
        h->addStretch();

        QWidget *btnWrap = new QWidget(header);
        QVBoxLayout *btnLay = new QVBoxLayout(btnWrap);
        btnLay->setContentsMargins(0,0,0,0);
        btnLay->setSpacing(0);
        btnLay->addStretch();
        btnLay->addWidget(detailsButton, 0, Qt::AlignCenter);
        btnLay->addStretch();

        h->addWidget(btnWrap, 0, Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(arrowLabel, 0, Qt::AlignVCenter);

        root->addWidget(header);

        QWidget *sub = new QWidget(this);
        QHBoxLayout *subL = new QHBoxLayout(sub);
        subL->setContentsMargins(0,0,0,0);
        subL->setSpacing(s(15));

        QLabel *serial = new QLabel("SN: " + agv.serial, sub);
        QLabel *km = new QLabel(QString("Пробег: %1 км").arg(agv.kilometers), sub);

        QLabel *status = new QLabel(sub);
        status->setFixedSize(s(12), s(12));
        status->setStyleSheet(QString(
            "background:%1;border-radius:%2px;"
        ).arg(statusColor(agv.status)).arg(s(6)));

        subL->addWidget(serial);
        subL->addWidget(km);
        subL->addWidget(status);
        subL->addStretch();

        root->addWidget(sub);

        details = new QWidget(this);
        details->setVisible(false);

        QVBoxLayout *d = new QVBoxLayout(details);
        d->setContentsMargins(s(5), s(5), s(5), s(5));
        d->setSpacing(s(6));

        QLabel *task = new QLabel("Текущая задача: " + agv.task, details);
        QLabel *last = new QLabel("Последняя активность: " + agv.lastActive.toString("dd.MM.yyyy"), details);

        QLabel *bp = new QLabel(details);
        bp->setPixmap(QPixmap(agv.blueprintPath)
                      .scaled(s(180), s(120), Qt::KeepAspectRatio, Qt::SmoothTransformation));

        d->addWidget(task);
        d->addWidget(last);
        d->addWidget(bp);


        root->addWidget(details);
    }

    QString AgvItem::statusColor(const QString &st)
    {
        if (st == "online")        return "#18CF00";
        if (st == "maintenance")   return "#FF8800";
        if (st == "error")         return "#FF0000";
        if (st == "working")       return "#00C8FF";
        return "#999999";
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
    //
    // ======================= ListAgvInfo =======================
    //

    ListAgvInfo::ListAgvInfo(std::function<int(int)> scale, QWidget *parent)
        : QFrame(parent), s(scale)
    {
        currentFilter = FilterSettings();

        setAttribute(Qt::WA_StyledBackground, true);
        setStyleSheet("background-color:#F1F2F4;border-radius:12px;");

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(s(10), s(10), s(10), s(10));
        root->setSpacing(s(12));

        //
        // ===== ШАПКА =====
        //
        QWidget *header = new QWidget(this);
        QHBoxLayout *hdr = new QHBoxLayout(header);
        hdr->setContentsMargins(0,0,0,0);
        hdr->setSpacing(s(10));

        // Кнопка назад
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

        //
        // ===== КНОПКА ДОБАВИТЬ =====
        //
        QPushButton *addBtn = new QPushButton("+ Добавить", header);
        addBtn->setFixedSize(s(150), s(50));
        addBtn->setStyleSheet(QString(
            "QPushButton {"
            "   background-color:#0F00DB;"
            "   border-radius:%1px;"
            "   font-family:Inter;"
            "   font-size:%2px;"
            "   font-weight:800;"
            "   color:white;"
            "}"
            "QPushButton:hover { background-color:#1A4ACD; }"
        ).arg(s(10)).arg(s(16)));

        hdr->addWidget(addBtn, 0, Qt::AlignLeft);

        //
        // ===== CONNECT ДОБАВИТЬ =====
        //
        connect(addBtn, &QPushButton::clicked, this, [this](){
            AddAgvDialog dlg(s, this);
            if (dlg.exec() == QDialog::Accepted)
            {
                AgvInfo info;
                QString baseName = dlg.result.name.trimmed();

                // последние 4 цифры SN
                QString digits;
                QRegularExpression re("\\d+");
                auto it = re.globalMatch(dlg.result.serial);
                while (it.hasNext())
                    digits += it.next().captured();

                QString last4 = digits.right(4);
                if (last4.isEmpty()) last4 = "0000";

                QString modelLower = dlg.result.model.toLower();

                QString finalId = QString("%1_%2_%3")
                                    .arg(baseName)
                                    .arg(last4)
                                    .arg(modelLower);

                info.id = finalId;

                info.model = dlg.result.model.toUpper();
                info.serial = dlg.result.serial;
                info.status = dlg.result.status;
                info.task = dlg.result.alias.isEmpty() ? "—" : dlg.result.alias;
                info.kilometers = 0;
                info.blueprintPath = ":/new/mainWindowIcons/noback/blueprint.png";
                info.lastActive = QDate::currentDate();

                addAgv(info);
            }
        });

        //
        // ===== ЗАГОЛОВОК =====
        //
        // ===== ЗАГОЛОВОК =====

        // Левый блок уже добавлен выше (Назад + Добавить)

        // Центрируем заголовок
        hdr->addStretch();

        QLabel *title = new QLabel("Список AGV", header);
        title->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
        ).arg(s(26)));
        title->setAlignment(Qt::AlignCenter);


        hdr->addWidget(title, 0, Qt::AlignCenter);

        // Правый блок
        hdr->addStretch();

        // ===== КНОПКА УДАЛИТЬ =====
        QPushButton *deleteBtn = new QPushButton("Удалить", header);
        deleteBtn->setFixedSize(s(130), s(40));
        deleteBtn->setStyleSheet(QString(
            "QPushButton{"
            "   background-color:#FF3B30;"
            "   border-radius:%1px;"
            "   font-family:Inter;"
            "   font-size:%2px;"
            "   font-weight:800;"
            "   color:white;"
            "}"
            "QPushButton:hover{background-color:#E13228;}"
        ).arg(s(10)).arg(s(16)));

        hdr->addWidget(deleteBtn, 0, Qt::AlignRight);

        // Прижимаем к правому краю + отступ 5px
        hdr->setAlignment(deleteBtn, Qt::AlignRight);
        deleteBtn->setContentsMargins(0, 0, s(5), 0);


        // ===== CONNECT =====
        connect(deleteBtn, &QPushButton::clicked, this, [this](){

            QDialog dlg(this);
            dlg.setWindowTitle("Удалить AGV");
            dlg.setFixedSize(s(360), s(420));
            dlg.setStyleSheet(
                "QDialog {"
                "    background: white;"
                "    border-radius: 12px;"
                "}"

                "QLabel {"
                "    background: transparent;"
                "    font-family: Inter;"
                "    font-size: 18px;"
                "    font-weight: 800;"
                "    color: #1A1A1A;"
                "}"

                "QCheckBox {"
                "    background: transparent;"
                "    font-family: Inter;"
                "    font-size: 16px;"
                "    color: #1A1A1A;"
                "}"

                "QPushButton {"
                "    font-family: Inter;"
                "    font-size: 16px;"
                "    font-weight: 800;"
                "    border-radius: 8px;"
                "    padding: 6px 14px;"
                "}"

                "QPushButton#ok {"
                "    background: #FF3B30;"
                "    color: white;"
                "}"

                "QPushButton#ok:hover {"
                "    background: #E13228;"
                "}"

                "QPushButton#cancel {"
                "    background: #E6E6E6;"
                "    color: black;"
                "}"

                "QPushButton#cancel:hover {"
                "    background: #D5D5D5;"
                "}"
            );

            QVBoxLayout *v = new QVBoxLayout(&dlg);

            QLabel *lbl = new QLabel("Выберите AGV для удаления:", &dlg);
            v->addWidget(lbl);

            QVector<QCheckBox*> boxes;

            for (int i = 0; i < layout->count(); ++i) {
                QLayoutItem *it = layout->itemAt(i);
                QWidget *w = it ? it->widget() : nullptr;
                AgvItem *agvItem = qobject_cast<AgvItem*>(w);
                if (agvItem) {
                    QCheckBox *cb = new QCheckBox(agvItem->agvId(), &dlg);
                    boxes.push_back(cb);
                    v->addWidget(cb);
                }
            }

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
                for (auto cb : boxes) {
                    if (cb->isChecked())
                        removeAgvById(cb->text());
                }
            }
        });



        root->addWidget(header);

        //
        // ===== ЛЕГЕНДА + ФИЛЬТР =====
        //
        filterBtn = new QPushButton(this);
        filterBtn->setIcon(QIcon(":/new/mainWindowIcons/noback/filter.png"));
        filterBtn->setIconSize(QSize(s(28), s(28)));
        filterBtn->setFixedSize(s(60), s(50));
        filterBtn->setStyleSheet(
            "QPushButton{background-color:#E6E6E6;border-radius:10px;border:1px solid #C8C8C8;}"
            "QPushButton:hover{background-color:#D5D5D5;}"
        );

        filterCount = new QLabel("0", this);
        filterCount->setFixedSize(s(22), s(22));
        filterCount->setAlignment(Qt::AlignCenter);
        filterCount->setStyleSheet(
            "background:#18CF00;color:white;font-weight:900;border-radius:11px;"
        );
        filterCount->hide();

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
        lg->addWidget(filterBtn);
        lg->addWidget(filterCount);

        root->addWidget(legend);

        //
        // ===== СКРОЛЛ =====
        //
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

        //
        // ===== ЗАГРУЗКА СПИСКА =====
        //
        QVector<AgvInfo> agvs = loadAgvList();
        rebuildList(agvs);

        //
        // ===== ФИЛЬТР =====
        //
        connect(filterBtn, &QPushButton::clicked, this, [this](){
            FilterDialog5 dlg(currentFilter, this);
            if (dlg.exec() == QDialog::Accepted)
            {
                currentFilter = dlg.result;
                applyFilter(currentFilter);

                int active = currentFilter.isActive() ? 1 : 0;

                if (active > 0) {
                    filterCount->setText(QString::number(active));
                    filterCount->show();
                    filterBtn->setStyleSheet(
                        "QPushButton{background-color:#18CF00;border-radius:10px;border:1px solid #0F8F00;}"
                        "QPushButton:hover{background-color:#15B800;}"
                    );
                } else {
                    filterCount->hide();
                    filterBtn->setStyleSheet(
                        "QPushButton{background-color:#E6E6E6;border-radius:10px;border:1px solid #C8C8C8;}"
                        "QPushButton:hover{background-color:#D5D5D5;}"
                    );
                }
            }
        });
    }

    //
    // ======================= applyFilter =======================
    //

    void ListAgvInfo::applyFilter(const FilterSettings &fs)
    {
        QVector<AgvInfo> list = loadAgvList();

        if (!fs.nameFilter.isEmpty())
        {
            list.erase(
                std::remove_if(list.begin(), list.end(),
                    [&](const AgvInfo &a){
                        return !a.id.contains(fs.nameFilter, Qt::CaseInsensitive)
                            && !a.model.contains(fs.nameFilter, Qt::CaseInsensitive);
                    }),
                list.end()
            );
        }

        if (fs.servAsc)
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.kilometers < b.kilometers;
                }
            );

        if (fs.servDesc)
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.kilometers > b.kilometers;
                }
            );

        if (fs.upAsc)
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.lastActive < b.lastActive;
                }
            );

        if (fs.upDesc)
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.lastActive > b.lastActive;
                }
            );

        if (fs.overOld)
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.lastActive < b.lastActive;
                }
            );

        if (fs.overNew)
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.lastActive > b.lastActive;
                }
            );

        if (fs.modelAZ)
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.model < b.model;
                }
            );

        if (fs.modelZA)
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.model > b.model;
                }
            );

        if (fs.kmAsc)
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.kilometers < b.kilometers;
                }
            );

        if (fs.kmDesc)
            std::sort(list.begin(), list.end(),
                [](const AgvInfo &a, const AgvInfo &b){
                    return a.kilometers > b.kilometers;
                }
            );

        rebuildList(list);
    }

    //
    // ======================= rebuildList =======================
    //

    void ListAgvInfo::rebuildList(const QVector<AgvInfo> &list)
    {
        QLayoutItem *child;
        while ((child = layout->takeAt(0)) != nullptr) {
            if (child->widget())
                child->widget()->deleteLater();
            delete child;
        }

        for (int i = 0; i < list.size(); ++i)
        {
            AgvItem *item = new AgvItem(list[i], s, content);

            connect(item, &AgvItem::openDetailsRequested, this, [this](const QString &id){
                emit openAgvDetails(id);
            });

            layout->addWidget(item);
        }

        layout->addStretch();
    }

    //
    // ======================= loadAgvList =======================
    //

    QVector<AgvInfo> ListAgvInfo::loadAgvList()
    {
        QVector<AgvInfo> list;

        list.push_back({
            "AGV_101", "AGV Model X1", "SN-101-0001", 12450,
            ":/new/mainWindowIcons/noback/blueprint.png",
            "online", "Перевозка паллеты", QDate::currentDate()
        });

        list.push_back({
            "AGV_202", "AGV Model X2", "SN-202-0002", 9800,
            ":/new/mainWindowIcons/noback/blueprint.png",
            "maintenance", "Диагностика", QDate::currentDate().addDays(-1)
        });

        list.push_back({
            "AGV_303", "AGV Model X3", "SN-303-0003", 4500,
            ":/new/mainWindowIcons/noback/blueprint.png",
            "error", "Ошибка датчика", QDate::currentDate().addDays(-3)
        });

        return list;
    }

    //
    // ======================= addAgv =======================
    //

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
    void ListAgvInfo::removeAgvById(const QString &id)
    {
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
    }
