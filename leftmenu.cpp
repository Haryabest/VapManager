#include "leftmenu.h"
#include "multisectionwidget.h"
#include "listagvinfo.h"
#include "agvdetailinfo.h"
#include "agvsettingspage.h"
//
// ======================= ДИАЛОГ НАСТРОЕК КАЛЕНДАРЯ =======================
//

class CalendarSettingsDialog : public QDialog {
public:
    CalendarSettingsDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Настройки календаря");
        setModal(true);
        setFixedSize(340, 260);

        QVBoxLayout *layout = new QVBoxLayout(this);

        yearBox  = new QComboBox(this);
        monthBox = new QComboBox(this);
        weekBox  = new QComboBox(this);
        dayBox   = new QComboBox(this);

        for (int y = 2020; y <= 2035; ++y)
            yearBox->addItem(QString::number(y), y);

        QStringList months = {
            "Январь","Февраль","Март","Апрель","Май","Июнь",
            "Июль","Август","Сентябрь","Октябрь","Ноябрь","Декабрь"
        };
        for (int i = 0; i < months.size(); ++i)
            monthBox->addItem(months[i], i + 1);

        weekBox->addItem("—", 0);
        for (int w = 1; w <= 4; ++w)
            weekBox->addItem(QString("Неделя %1").arg(w), w);

        dayBox->addItem("—", 0);
        for (int d = 1; d <= 31; ++d)
            dayBox->addItem(QString::number(d), d);

        layout->addWidget(new QLabel("Год:"));
        layout->addWidget(yearBox);
        layout->addWidget(new QLabel("Месяц:"));
        layout->addWidget(monthBox);
        layout->addWidget(new QLabel("Неделя:"));
        layout->addWidget(weekBox);
        layout->addWidget(new QLabel("День:"));
        layout->addWidget(dayBox);

        connect(weekBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx){
            int w = weekBox->itemData(idx).toInt();
            dayBox->setEnabled(w == 0);
        });

        connect(dayBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx){
            int d = dayBox->itemData(idx).toInt();
            weekBox->setEnabled(d == 0);
        });

        QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

        connect(btns, &QDialogButtonBox::accepted, this, [this]() {
            if (year() == 0 || month() == 0) {
                reject();
                return;
            }
            if (week() == 0 && day() == 0) {
                reject();
                return;
            }
            accept();
        });

        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

        layout->addWidget(btns);
    }

    int year()  const { return yearBox->currentData().toInt(); }
    int month() const { return monthBox->currentData().toInt(); }
    int week()  const { return weekBox->currentData().toInt(); }
    int day()   const { return dayBox->currentData().toInt(); }

private:
    QComboBox *yearBox;
    QComboBox *monthBox;
    QComboBox *weekBox;
    QComboBox *dayBox;
};

//
// ======================= КОНСТРУКТОР =======================
//

leftMenu::leftMenu(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);

    selectedDay_ = QDate(selectedYear_, selectedMonth_, 1);
    selectedWeek_ = 0;
    highlightWeek_ = false;

    initUI();
}

//
// ======================= SCALE HELPERS =======================
//

int leftMenu::s(int v) const
{
    return int(v * scaleFactor_);
}

void leftMenu::setScaleFactor(qreal factor)
{
    if (factor <= 0)
        factor = 1.0;

    if (qFuzzyCompare(scaleFactor_, factor))
        return;

    scaleFactor_ = factor;

    if (QLayout *old = layout()) {
        QLayoutItem *item;
        while ((item = old->takeAt(0)) != nullptr) {
            if (QWidget *w = item->widget()) {
                w->setParent(nullptr);
                delete w;
            }
            delete item;
        }
        delete old;
    }

    rightCalendarFrame = nullptr;
    rightUpcomingMaintenanceFrame = nullptr;
    listAgvInfo = nullptr;
    agvDetailInfo = nullptr;
    agvSettingsPage = nullptr;
    backButton = nullptr;
    monthLabel = nullptr;

    initUI();
}

void leftMenu::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    qreal wFactor = width()  / 1920.0;
    qreal hFactor = height() / 1080.0;
    qreal factor  = qMin(wFactor, hFactor);

    setScaleFactor(factor);
}
//
// ======================= initUI() — НАЧАЛО =======================
//

void leftMenu::initUI()
{
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(s(5));
    rootLayout->setContentsMargins(s(10), s(10), s(10), s(10));

    //
    // ======================= ВЕРХНЯЯ ШАПКА =======================
    //
    QWidget *topRow = new QWidget(this);
    QHBoxLayout *topLayout = new QHBoxLayout(topRow);
    topLayout->setContentsMargins(0,0,0,0);
    topLayout->setSpacing(s(5));

    //
    // ЛЕВАЯ ЧАСТЬ ШАПКИ (ЛОГО)
    //
    QFrame *leftTopHeaderFrame = new QFrame(topRow);
    leftTopHeaderFrame->setStyleSheet("background-color:#F2F3F5;");
    leftTopHeaderFrame->setFixedSize(s(370), s(115));

    QVBoxLayout *leftTopHeaderLayout = new QVBoxLayout(leftTopHeaderFrame);
    leftTopHeaderLayout->setContentsMargins(s(3), s(0), 0, 0);
    leftTopHeaderLayout->setSpacing(0);

    QLabel *iconLabel = new QLabel(leftTopHeaderFrame);
    iconLabel->setPixmap(
        QPixmap(":/new/mainWindowIcons/noback/VAPManagerLogo.png")
            .scaled(s(288), s(92), Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
    iconLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    leftTopHeaderLayout->addStretch();
    leftTopHeaderLayout->addWidget(iconLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    leftTopHeaderLayout->addStretch();

    topLayout->addWidget(leftTopHeaderFrame);

    //
    // ПРАВАЯ ЧАСТЬ ШАПКИ
    //
    QFrame *rightTopHeaderFrame = new QFrame(topRow);
    rightTopHeaderFrame->setStyleSheet("background-color:#F1F2F4;");
    rightTopHeaderFrame->setFixedHeight(s(115));

    QHBoxLayout *rightTopHeaderLayout = new QHBoxLayout(rightTopHeaderFrame);
    rightTopHeaderLayout->setContentsMargins(0,0,0,0);

    QWidget *headerContent = new QWidget(rightTopHeaderFrame);
    QHBoxLayout *headerRow = new QHBoxLayout(headerContent);
    headerRow->setContentsMargins(s(5), s(5), s(5), s(5));

    //
    // Левый текстовый блок
    //
    QWidget *leftTextWidget = new QWidget(headerContent);
    QVBoxLayout *leftTextLayout = new QVBoxLayout(leftTextWidget);
    leftTextLayout->setContentsMargins(0,0,0,0);
    leftTextLayout->setSpacing(0);

    QLabel *titleLabel = new QLabel("Календарь технического обслуживания", leftTextWidget);
    titleLabel->setStyleSheet(QString(
        "font-family:Inter;font-weight:900;font-size:%1px;color:black;padding-left:%2px;"
    ).arg(s(22)).arg(s(9)));

    QLabel *subtitleLabel = new QLabel(
        "Отслеживание графиков обслуживания AGV\nи истории технического обслуживания",
        leftTextWidget
    );
    subtitleLabel->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:bold;color:#B8B8B8;padding-left:%2px;"
    ).arg(s(18)).arg(s(7)));

    leftTextLayout->addStretch();
    leftTextLayout->addWidget(titleLabel);
    leftTextLayout->addWidget(subtitleLabel);
    leftTextLayout->addStretch();

    headerRow->addWidget(leftTextWidget, 1);

    //
    // Правая колонка (поиск, уведомления, юзер)
    //
    QWidget *controls = new QWidget(headerContent);
    QHBoxLayout *controlsRow = new QHBoxLayout(controls);
    controlsRow->setContentsMargins(0,0,0,0);
    controlsRow->setSpacing(s(5));

    //
    // Поиск
    //
    QFrame *searchFrame = new QFrame(controls);
    searchFrame->setFixedSize(s(346), s(64));
    searchFrame->setStyleSheet("background-color:#DFDFDF;border-radius:8px;");

    QHBoxLayout *searchLayout = new QHBoxLayout(searchFrame);
    searchLayout->setContentsMargins(s(12), 0, s(12), 0);

    QLabel *searchIcon = new QLabel(searchFrame);
    searchIcon->setPixmap(
        QPixmap(":/new/mainWindowIcons/noback/lupa.png")
            .scaled(s(31), s(30), Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
    searchIcon->setAlignment(Qt::AlignCenter);

    QLineEdit *searchEdit = new QLineEdit(searchFrame);
    searchEdit->setPlaceholderText("Search AGV…");
    searchEdit->setStyleSheet(QString(
        "QLineEdit{background:transparent;border:none;font-family:Inter;font-size:%1px;color:#747474;}"
    ).arg(s(16)));

    searchLayout->addWidget(searchIcon);
    searchLayout->addWidget(searchEdit);

    //
    // Кнопка уведомлений
    //
    QPushButton *notifBtn = new QPushButton(controls);
    notifBtn->setFixedSize(s(58), s(65));
    notifBtn->setAttribute(Qt::WA_TranslucentBackground);
    notifBtn->setStyleSheet(
        "QPushButton{background-color:transparent;border-radius:8px;border:none;}"
        "QPushButton:hover{background-color:rgba(0,0,0,0.05);} "
        "QPushButton:pressed{border:3px solid #FFD700;}"
    );

    QLabel *bell = new QLabel(notifBtn);
    bell->setAttribute(Qt::WA_TranslucentBackground);
    bell->setStyleSheet("background: transparent;");
    bell->setPixmap(
        QPixmap(":/new/mainWindowIcons/noback/bell.png")
            .scaled(s(37), s(37), Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
    bell->setAlignment(Qt::AlignCenter);

    QHBoxLayout *notifLayout = new QHBoxLayout(notifBtn);
    notifLayout->setContentsMargins(0,0,0,0);
    notifLayout->setSpacing(0);
    notifLayout->addStretch();
    notifLayout->addWidget(bell);
    notifLayout->addStretch();

    //
    // Юзер
    //
    QFrame *userFrame = new QFrame(controls);
    userFrame->setFixedSize(s(65), s(65));
    QHBoxLayout *userLayout = new QHBoxLayout(userFrame);
    userLayout->setContentsMargins(s(5), s(5), s(5), s(5));

    QToolButton *userButton = new QToolButton(userFrame);
    userButton->setFixedSize(s(55), s(55));

    QPixmap avatarPm = loadUserAvatarFromDb("current_user_id");
    if (!avatarPm.isNull()) {
        QPixmap round = makeRoundPixmap(avatarPm, s(55));
        userButton->setIcon(QIcon(round));
        userButton->setIconSize(QSize(s(55), s(55)));
    } else {
        userButton->setText("USER");
    }

    userButton->setStyleSheet(QString(
        "QToolButton{background-color:#D9D9D9;border-radius:%1px;font-family:Inter;"
        "font-weight:900;font-size:%2px;color:black;border:none;padding:%3px;} "
        "QToolButton:hover{background-color:rgba(173,216,230,76);border:1px solid #ADD8E6;}"
    ).arg(s(27)).arg(s(14)).arg(s(4)));

    userLayout->addWidget(userButton);

    QMenu *userMenu = new QMenu(userButton);
    userMenu->setStyleSheet(QString(
        "QMenu { background-color: white; font-family: Inter; font-size:%1px; }"
        "QMenu::item { padding: %2px %3px; }"
        "QMenu::item:selected { background-color: #E6E6E6; }"
    ).arg(s(14)).arg(s(6)).arg(s(12)));

    userMenu->addAction("Редактировать профиль");
    userMenu->addAction("Сменить аватар");
    userMenu->addAction("Настройки профиля");
    userMenu->addAction("Сменить язык");
    userMenu->addSeparator();
    userMenu->addAction("О программе");
    userMenu->addSeparator();

    QAction *exitAppAction = userMenu->addAction("Выйти из приложения");

    connect(exitAppAction, &QAction::triggered, this, [](){
        qApp->quit();
    });

    connect(userButton, &QToolButton::clicked, this, [this, userButton, userMenu](){
        QPoint pos = userButton->mapToGlobal(QPoint(0, userButton->height() + s(5)));
        userMenu->popup(pos);
    });

    controlsRow->addWidget(searchFrame);
    controlsRow->addWidget(notifBtn);
    controlsRow->addWidget(userFrame);

    headerRow->addWidget(controls, 0, Qt::AlignRight);
    rightTopHeaderLayout->addWidget(headerContent);

    topLayout->addWidget(rightTopHeaderFrame);

    rootLayout->addWidget(topRow);

    //
    // ======================= НИЖНЯЯ ПАНЕЛЬ =======================
    //
    QWidget *bottomRow = new QWidget(this);
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomRow);
    bottomLayout->setContentsMargins(0, s(5), 0, 0);
    bottomLayout->setSpacing(s(5));

    //
    // ======================= ЛЕВАЯ ПАНЕЛЬ =======================
    //
    QWidget *leftPanel = new QWidget(bottomRow);
    leftPanel->setFixedWidth(s(370));

    QVBoxLayout *leftPanelLayout = new QVBoxLayout(leftPanel);
    leftPanelLayout->setContentsMargins(0,0,0,0);
    leftPanelLayout->setSpacing(s(8));

    //
    // МЕНЮ
    //
    QFrame *leftNavFrame = new QFrame(leftPanel);
    leftNavFrame->setStyleSheet("background-color:#F1F2F4;");
    QVBoxLayout *leftNavLayout = new QVBoxLayout(leftNavFrame);
    leftNavLayout->setContentsMargins(s(10), s(5), s(10), s(5));
    leftNavLayout->setSpacing(0);

    auto makeNavButton = [&](QString text, QString iconPath){
        QPushButton *btn = new QPushButton(text, leftNavFrame);
        QPixmap pix(iconPath);
        if (!pix.isNull())
            btn->setIcon(QIcon(pix.scaled(s(24), s(24), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        btn->setIconSize(QSize(s(24), s(24)));
        btn->setFixedHeight(s(45));
        btn->setStyleSheet(QString(
            "QPushButton{text-align:left;background-color:transparent;padding-left:%1px;font-family:Inter;"
            "font-size:%2px;font-weight:700;border-radius:8px;border:1px solid transparent;} "
            "QPushButton:hover{background-color:rgba(173,216,230,76);border:1px solid #ADD8E6;}"
        ).arg(s(15)).arg(s(14)));
        return btn;
    };

    QPushButton *btnUsers = makeNavButton("Пользователи", ":/new/mainWindowIcons/noback/user.png");
    QPushButton *btnAgv   = makeNavButton("AGV", ":/new/mainWindowIcons/noback/agvIcon.png");
    QPushButton *btnEdit  = makeNavButton("Модель AGV", ":/new/mainWindowIcons/noback/edit.png");
    QPushButton *btnYear  = makeNavButton("Сформировать годовой отчет", ":/new/mainWindowIcons/noback/YearListPrint.png");
    QPushButton *btnSet   = makeNavButton("Настройки", ":/new/mainWindowIcons/noback/agvSetting.png");

    leftNavLayout->addWidget(btnUsers);
    leftNavLayout->addWidget(btnAgv);
    leftNavLayout->addWidget(btnEdit);
    leftNavLayout->addWidget(btnYear);
    leftNavLayout->addWidget(btnSet);
    leftNavLayout->addSpacing(s(10));

    //
    // КНОПКА ДОБАВИТЬ AGV
    //
    QPushButton *addAgvButton = new QPushButton("+ Добавить AGV", leftNavFrame);
    connect(addAgvButton, &QPushButton::clicked, this, [this](){
        emit addAgvRequested();
    });

    addAgvButton->setFixedHeight(s(40));
    addAgvButton->setStyleSheet(QString(
        "QPushButton{background-color:#0F00DB;color:white;font-family:Inter;font-size:%1px;font-weight:800;"
        "border-radius:10px;border:none;} "
        "QPushButton:hover{background-color:#1A4ACD;}"
    ).arg(s(16)));

    QHBoxLayout *addAgvButtonRow = new QHBoxLayout();
    addAgvButtonRow->addSpacing(s(28));
    addAgvButtonRow->addWidget(addAgvButton);
    addAgvButtonRow->addSpacing(s(28));

    leftNavLayout->addLayout(addAgvButtonRow);
    leftNavLayout->addSpacing(s(5));

    leftPanelLayout->addWidget(leftNavFrame);

    //
    // ======================= СТАТУС СИСТЕМЫ =======================
    //
    QFrame *leftStatusFrame = new QFrame(leftPanel);
    leftStatusFrame->setStyleSheet("background-color:#F1F2F4;");
    leftStatusFrame->setMinimumHeight(s(160));

    QVBoxLayout *leftStatusLayout = new QVBoxLayout(leftStatusFrame);
    leftStatusLayout->setContentsMargins(s(10), s(20), s(10), s(5));
    leftStatusLayout->setSpacing(s(5));

    SystemStatus st = loadSystemStatus();

    MultiSectionWidget *statusWidget = new MultiSectionWidget(leftStatusFrame, scaleFactor_);
    statusWidget->setActiveAGVCurrentCount(st.active);
    statusWidget->setActiveAGVTotalCount(st.active + st.maintenance + st.error + st.disabled);
    statusWidget->setMaintenanceCurrentCount(st.maintenance);
    statusWidget->setMaintenanceTotalCount(st.maintenance);
    statusWidget->setErrorCurrentCount(st.error);
    statusWidget->setErrorTotalCount(st.error);
    statusWidget->setDisabledCurrentCount(st.disabled);
    statusWidget->setDisabledTotalCount(st.disabled);

    leftStatusLayout->addWidget(statusWidget);

    QPushButton *logsButton = new QPushButton("Logs", leftStatusFrame);
    logsButton->setFixedSize(s(120), s(25));
    logsButton->setStyleSheet(QString(
        "QPushButton{background-color:transparent;border:none;font-family:Inter;font-size:%1px;font-weight:800;color:black;} "
        "QPushButton:hover{background-color:rgba(173,216,230,76);border-radius:5px;}"
    ).arg(s(12)));

    QHBoxLayout *logsRow = new QHBoxLayout();
    logsRow->addStretch();
    logsRow->addWidget(logsButton);
    logsRow->addStretch();
    leftStatusLayout->addLayout(logsRow);

    QLabel *versionLabel = new QLabel("Версия: 1.0.0 от 30.11.2025", leftStatusFrame);
    versionLabel->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:rgb(120,120,120);").arg(s(10)));
    versionLabel->setAlignment(Qt::AlignCenter);
    leftStatusLayout->addWidget(versionLabel);

    leftPanelLayout->addWidget(leftStatusFrame);
    leftPanelLayout->setStretch(0,0);
    leftPanelLayout->setStretch(1,1);

    bottomLayout->addWidget(leftPanel);
    //
    // ======================= ПРАВАЯ ПАНЕЛЬ =======================
    //
    QWidget *rightPanel = new QWidget(bottomRow);
    QVBoxLayout *rightPanelLayout = new QVBoxLayout(rightPanel);
    rightPanelLayout->setContentsMargins(0, s(5), 0, 0);
    rightPanelLayout->setSpacing(s(8));

    QFrame *rightBodyFrame = new QFrame(rightPanel);
    QVBoxLayout *rightBodyLayout = new QVBoxLayout(rightBodyFrame);
    rightBodyLayout->setContentsMargins(0,0,0,0);
    rightBodyLayout->setSpacing(s(5));

    rightPanelLayout->addWidget(rightBodyFrame,1);
    bottomLayout->addWidget(rightPanel,1);

    rootLayout->addWidget(bottomRow,1);

    //
    // ======================= КАЛЕНДАРЬ =======================
    //
    rightCalendarFrame = new QFrame(rightBodyFrame);
    rightCalendarFrame->setStyleSheet("background-color:#F1F2F4;border-radius:12px;");
    rightCalendarFrame->setMinimumHeight(s(300));

    QVBoxLayout *rightCalendarLayout = new QVBoxLayout(rightCalendarFrame);
    rightCalendarLayout->setContentsMargins(s(8),s(8),s(8),s(8));
    rightCalendarLayout->setSpacing(s(8));

    //
    // ======== БЛОК ДЕЙСТВИЙ ДНЯ ========
    //
    calendarActionsFrame = new QFrame(rightCalendarFrame);
    calendarActionsFrame->setStyleSheet("background-color:#F1F2F4;border-radius:8px;");
    calendarActionsFrame->setVisible(false);

    QHBoxLayout *actionsLayout = new QHBoxLayout(calendarActionsFrame);
    actionsLayout->setContentsMargins(s(10), s(10), s(10), s(10));
    actionsLayout->setSpacing(s(10));

    QLabel *actionsLabel = new QLabel("Выберите день или неделю", calendarActionsFrame);
    actionsLabel->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:700;color:black;"
    ).arg(s(16)));

    QPushButton *btnAdd = new QPushButton("Добавить событие", calendarActionsFrame);
    btnAdd->setStyleSheet(QString(
        "QPushButton{background-color:#0F00DB;color:white;font-family:Inter;font-size:%1px;"
        "font-weight:700;border-radius:8px;padding:%2px %3px;} "
        "QPushButton:hover{background-color:#1A4ACD;}"
    ).arg(s(14)).arg(s(6)).arg(s(12)));

    actionsLayout->addWidget(actionsLabel);
    actionsLayout->addStretch();
    actionsLayout->addWidget(btnAdd);

    rightCalendarLayout->addWidget(calendarActionsFrame);

    //
    // ======== HEADER КАЛЕНДАРЯ ========
    //
    QWidget *calendarHeader = new QWidget(rightCalendarFrame);
    QVBoxLayout *calendarHeaderMainLayout = new QVBoxLayout(calendarHeader);
    calendarHeaderMainLayout->setContentsMargins(s(10),s(14),s(10),0);
    calendarHeaderMainLayout->setSpacing(s(16));

    QHBoxLayout *topRowLayoutCal = new QHBoxLayout();
    topRowLayoutCal->setContentsMargins(0,0,0,0);
    topRowLayoutCal->setSpacing(s(35));

    monthLabel = new QLabel(monthYearLabelText(selectedMonth_, selectedYear_), calendarHeader);
    monthLabel->setStyleSheet(QString(
        "font-family:Inter;font-weight:900;font-size:%1px;color:black;"
    ).arg(s(26)));
    monthLabel->setAlignment(Qt::AlignVCenter|Qt::AlignLeft);

    topRowLayoutCal->addWidget(monthLabel);
    topRowLayoutCal->addStretch();

    QPushButton *prevMonthBtn = new QPushButton("<");
    prevMonthBtn->setFixedSize(s(32),s(32));
    prevMonthBtn->setStyleSheet(QString(
        "QPushButton{background-color:#DFDFDF;border-radius:6px;font-size:%1px;} "
        "QPushButton:hover{background-color:#CFCFCF;}"
    ).arg(s(18)));

    QPushButton *nextMonthBtn = new QPushButton(">");
    nextMonthBtn->setFixedSize(s(32),s(32));
    nextMonthBtn->setStyleSheet(prevMonthBtn->styleSheet());

    connect(prevMonthBtn,&QPushButton::clicked,this,[this](){ changeMonth(-1); });
    connect(nextMonthBtn,&QPushButton::clicked,this,[this](){ changeMonth(1); });

    topRowLayoutCal->addWidget(prevMonthBtn);
    topRowLayoutCal->addWidget(nextMonthBtn);

    QPushButton *settingsBtn = new QPushButton("Настройки");
    settingsBtn->setFixedSize(s(120),s(38));
    settingsBtn->setStyleSheet(QString(
        "QPushButton{background-color:#DFDFDF;border-radius:8px;font-family:Inter;font-size:%1px;font-weight:700;} "
        "QPushButton:hover{background-color:#CFCFCF;}"
    ).arg(s(16)));

    connect(settingsBtn,&QPushButton::clicked,this,[this](){
        CalendarSettingsDialog dlg(this);
        if(dlg.exec()!=QDialog::Accepted) return;

        int y=dlg.year(), m=dlg.month(), w=dlg.week(), d=dlg.day();
        selectedWeek_=w;
        highlightWeek_=(w!=0);

        if(w!=0){
            int startDay=1+(w-1)*7;
            int endDay=(w==4? QDate(y,m,1).daysInMonth() : startDay+6);
            setSelectedMonthYear(m,y);
            selectDay(y,m,startDay);
        } else {
            highlightWeek_=false;
            setSelectedMonthYear(m,y);
            selectDay(y,m,d);
        }
    });

    topRowLayoutCal->addWidget(settingsBtn);
    calendarHeaderMainLayout->addLayout(topRowLayoutCal);

    //
    // ======== ЛЕГЕНДА ========
    //
    QHBoxLayout *legendLayout = new QHBoxLayout();
    legendLayout->setSpacing(s(15));
    legendLayout->setAlignment(Qt::AlignLeft);

    auto makeDot=[&](QString color){
        QLabel *dot=new QLabel();
        dot->setFixedSize(s(12),s(12));
        dot->setStyleSheet(QString("background-color:%1;border-radius:%2px;").arg(color).arg(s(6)));
        return dot;
    };
    auto makeLegendText=[&](QString text){
        QLabel *lbl=new QLabel(text);
        lbl->setStyleSheet(QString("font-family:Inter;font-size:%1px;color:#A39E9E;").arg(s(17)));
        return lbl;
    };

    legendLayout->addWidget(makeDot("#FF0000"));
    legendLayout->addWidget(makeLegendText("Просрочен"));
    legendLayout->addSpacing(s(8));
    legendLayout->addWidget(makeDot("#FF8800"));
    legendLayout->addWidget(makeLegendText("Ближайшие события"));
    legendLayout->addSpacing(s(8));
    legendLayout->addWidget(makeDot("#18CF00"));
    legendLayout->addWidget(makeLegendText("Запланировано"));
    legendLayout->addSpacing(s(8));
    legendLayout->addWidget(makeDot("#00E5FF"));
    legendLayout->addWidget(makeLegendText("Обслужено"));

    calendarHeaderMainLayout->addLayout(legendLayout);

    //
    // ======== ЛИНИЯ ========
    //
    QFrame *calendarDivider = new QFrame(calendarHeader);
    calendarDivider->setFrameShape(QFrame::HLine);
    calendarDivider->setFixedHeight(s(2));
    calendarDivider->setStyleSheet("background-color:rgba(211,211,211,0.8); border:none;");
    calendarHeaderMainLayout->addWidget(calendarDivider);

    rightCalendarLayout->addWidget(calendarHeader);


    //
    // ======================= СЕТКА КАЛЕНДАРЯ (ФИНАЛ + ТЁМНАЯ ШАПКА + ТЕМНЕЕ СТАРЫЕ/НОВЫЕ ДНИ) =======================
    //

    QDate firstDay(selectedYear_, selectedMonth_, 1);
    int daysInMonth = firstDay.daysInMonth();
    int startCol = (firstDay.dayOfWeek() + 5) % 7;

    int totalCells = startCol + daysInMonth;
    int calendarRows = (totalCells + 6) / 7;
    int totalRows = calendarRows + 1;

    QTableWidget *calendarTable = new QTableWidget(totalRows, 7, rightCalendarFrame);

    // таблица не растягивается сверх нужного
    calendarTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

    calendarTable->setStyleSheet(
        "QTableWidget { border:none; }"
        "QTableWidget::item { border:none; padding:3px; }"
    );

    calendarTable->horizontalHeader()->setVisible(false);
    calendarTable->verticalHeader()->setVisible(false);
    calendarTable->setShowGrid(false);
    calendarTable->setSelectionMode(QAbstractItemView::NoSelection);
    calendarTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    calendarTable->setFocusPolicy(Qt::NoFocus);
    calendarTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    calendarTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    calendarTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // ===== ВСЕ ЯЧЕЙКИ ОДИНАКОВОГО РАЗМЕРА =====
    int cellH = s(71);
    int headerH = s(52);

    calendarTable->verticalHeader()->setDefaultSectionSize(cellH);
    calendarTable->verticalHeader()->setMinimumSectionSize(cellH);

    calendarTable->setRowHeight(0, headerH);

    for (int r = 1; r < totalRows; r++)
        calendarTable->setRowHeight(r, cellH);

    //
    // ===== ДНИ НЕДЕЛИ (фон 555555, текст 222222) =====
    //
    QStringList weekdaysList = {
        "Понедельник",
        "Вторник",
        "Среда",
        "Четверг",
        "Пятница",
        "Суббота",
        "Воскресенье"
    };

    for (int c = 0; c < 7; c++) {
        QTableWidgetItem *item = new QTableWidgetItem(weekdaysList[c]);
        item->setTextAlignment(Qt::AlignCenter);
        item->setFont(QFont("Inter", s(17), QFont::Bold));

        item->setForeground(QBrush(QColor("#222222")));   // очень тёмно‑серый текст
        item->setBackground(QBrush(QColor("#555555")));   // тёмно‑серый фон

        calendarTable->setItem(0, c, item);
    }

    //
    // ===== ЗАПОЛНЕНИЕ КАЛЕНДАРЯ =====
    //

    // строка 1 — первая строка с числами
    int row = 1;
    int col = 0;

    // ===== ХВОСТ ПРЕДЫДУЩЕГО МЕСЯЦА (минимум 2 дня, но не меняем startCol) =====
    QDate prevMonth = firstDay.addMonths(-1);
    int prevDays = prevMonth.daysInMonth();

    int prevCount = startCol;
    if (prevCount < 2)
        prevCount = 2;

    int tailStart = prevDays - prevCount + 1;

    // старые дни — на 10% темнее: #A5A5A5
    QColor oldMonthColor("#A5A5A5");

    for (int d = tailStart; d <= prevDays; d++) {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(d));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        item->setFont(QFont("Inter", s(14), QFont::Bold));
        item->setForeground(QBrush(oldMonthColor));
        calendarTable->setItem(row, col, item);
        col++;
    }

    // ===== ТЕКУЩИЙ МЕСЯЦ =====
    row = 1;
    col = startCol;

    for (int d = 1; d <= daysInMonth; d++) {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(d));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        item->setFont(QFont("Inter", s(14), QFont::Bold));
        item->setForeground(QBrush(QColor("#000000")));
        calendarTable->setItem(row, col, item);

        col++;
        if (col > 6) { col = 0; row++; }
    }

    // ===== ХВОСТ СЛЕДУЮЩЕГО МЕСЯЦА =====
    int used = (row - 1) * 7 + col;
    int tail = 7 - (used % 7);
    if (tail == 7) tail = 0;

    int nextDay = 1;

    // новые дни — тоже на 10% темнее: #A5A5A5
    QColor nextMonthColor("#A5A5A5");

    for (int i = 0; i < tail; i++) {
        QTableWidgetItem *item = new QTableWidgetItem(QString::number(nextDay));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        item->setFont(QFont("Inter", s(14), QFont::Bold));
        item->setForeground(QBrush(nextMonthColor));
        calendarTable->setItem(row, col, item);

        col++;
        nextDay++;
    }

    //
    // ===== ДЕЛЕГАТ (ТОЛЬКО ВНУТРЕННИЕ ЛИНИИ) =====
    //
    class CalendarDelegate : public QStyledItemDelegate {
    public:
        void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {

            QStyledItemDelegate::paint(p, opt, idx);

            if (idx.row() == 0) return;

            p->setPen(QColor("#D3D3D3"));
            QRect r = opt.rect;

            if (idx.column() < idx.model()->columnCount() - 1)
                p->drawLine(r.right(), r.top(), r.right(), r.bottom());

            if (idx.row() < idx.model()->rowCount() - 1)
                p->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
        }
    };

    calendarTable->setItemDelegate(new CalendarDelegate());

    rightCalendarLayout->addWidget(calendarTable);

    //
    // ======================= ПРЕДСТОЯЩЕЕ ТО (с иконками) =======================
    //
    rightUpcomingMaintenanceFrame = new QFrame(rightBodyFrame);
    rightUpcomingMaintenanceFrame->setStyleSheet("background-color:#F1F2F4;border-radius:12px;");
    rightUpcomingMaintenanceFrame->setMinimumHeight(s(130));

    QVBoxLayout *rightUpcomingLayout = new QVBoxLayout(rightUpcomingMaintenanceFrame);
    rightUpcomingLayout->setContentsMargins(s(8), s(8), s(8), s(8));
    rightUpcomingLayout->setSpacing(s(8));

    QLabel *maintenanceTitle = new QLabel("Предстоящее Техническое обслуживание", rightUpcomingMaintenanceFrame);
    maintenanceTitle->setStyleSheet(QString(
        "background:transparent;font-family:Inter;font-size:%1px;font-weight:800;color:black;padding:%2px %3px;"
    ).arg(s(18)).arg(s(10)).arg(s(15)));
    maintenanceTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    rightUpcomingLayout->addWidget(maintenanceTitle);

    QFrame *thickDivider = new QFrame(rightUpcomingMaintenanceFrame);
    thickDivider->setFrameShape(QFrame::HLine);
    thickDivider->setFixedHeight(s(2));
    thickDivider->setStyleSheet("background-color:rgba(211,211,211,0.8); border:none;");
    rightUpcomingLayout->addWidget(thickDivider);

    QWidget *contentContainer = new QWidget(rightUpcomingMaintenanceFrame);
    QVBoxLayout *contentLayout = new QVBoxLayout(contentContainer);
    contentLayout->setContentsMargins(s(10), s(10), s(10), s(10));
    contentLayout->setSpacing(s(5));

    //
    // ======== НОВЫЙ addMaintenanceItem() С ИКОНКАМИ ========
    //
    auto addMaintenanceItem = [&](const MaintenanceItemData &item){
        QColor bgColor, btnColor;
        QString iconPath;

        if (item.severity == "red") {
            bgColor = QColor(255,0,0,33);
            btnColor = QColor(235,61,61,204);
            iconPath = ":/new/mainWindowIcons/noback/alert.png";
        }
        else if (item.severity == "orange") {
            bgColor = QColor(255,136,0,33);
            btnColor = QColor(255,196,0,204);
            iconPath = ":/new/mainWindowIcons/noback/warning.png";
        }
        else if (item.severity == "green") {
            bgColor = QColor(24,207,0,87);
            btnColor = QColor(0,154,10,255);
            iconPath = ":/new/mainWindowIcons/noback/galka.png";
        }
        else {
            bgColor = QColor(0,229,255,33);
            btnColor = QColor(0,180,220,204);
            iconPath = ":/new/mainWindowIcons/noback/info.png";
        }

        QFrame *itemFrame = new QFrame(contentContainer);
        itemFrame->setStyleSheet(QString(
            "QFrame{background-color:rgba(%1,%2,%3,%4);border-radius:10px;}"
        ).arg(bgColor.red()).arg(bgColor.green()).arg(bgColor.blue()).arg(bgColor.alpha()));

        QHBoxLayout *itemLayout = new QHBoxLayout(itemFrame);
        itemLayout->setContentsMargins(s(10), s(8), s(10), s(8));
        itemLayout->setSpacing(s(12));

        //
        // ===== ИКОНКА =====
        //
        QLabel *iconLabel = new QLabel(itemFrame);
        iconLabel->setFixedSize(s(32), s(32));
        iconLabel->setPixmap(
            QPixmap(iconPath).scaled(s(32), s(32), Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );
        iconLabel->setStyleSheet(
            "QLabel { background-color: transparent; border: none; padding: 0px; margin: 0px; }"
        );
        iconLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        itemLayout->addWidget(iconLabel);

        //
        // ===== ТЕКСТ =====
        //
        QLabel *textLabel = new QLabel(itemFrame);
        // ===== ПЕРВАЯ СТРОКА — ВСЯ ЧЁРНАЯ И ЖИРНАЯ =====
        QString topLine = QString(
            "<span style='font-weight:800; color:#000000;'>%1 — %2</span>"
        ).arg(item.agvId)
         .arg(item.type);

        // ===== ВТОРАЯ СТРОКА — СЕРАЯ, КАК БЫЛО, НО С ОТСТУПОМ =====
        QString bottomLine = QString(
            "<span style='color:#777777;'>%1 — %2</span>"
        ).arg(item.date.toString("dd.MM.yyyy"))
         .arg(item.details);

        // ===== ОБЪЕДИНЯЕМ С НЕБОЛЬШИМ ОТСТУПОМ =====
        textLabel->setText(
            topLine +
            "<div style='height:3px;'></div>" +   // ← отступ 3px
            bottomLine
        );

        textLabel->setStyleSheet(QString(
            "background:transparent;font-family:Inter;font-size:%1px;"
        ).arg(s(14)));


        textLabel->setWordWrap(true);

        itemLayout->addWidget(textLabel, 1);

        //
        // ===== КНОПКА =====
        //
        QPushButton *showBtn = new QPushButton("Показать", itemFrame);
        showBtn->setStyleSheet(QString(
            "QPushButton{background-color:rgba(%1,%2,%3,%4);color:white;font-family:Inter;font-size:%5px;"
            "font-weight:700;border-radius:8px;padding:%6px %7px;border:none;} "
            "QPushButton:hover{background-color:rgba(%8,%9,%10,%11);} "
            "QPushButton:pressed{background-color:rgba(%12,%13,%14,%15);}"
        )
        .arg(btnColor.red()).arg(btnColor.green()).arg(btnColor.blue()).arg(btnColor.alpha())
        .arg(s(13)).arg(s(4)).arg(s(10))
        .arg(btnColor.lighter().red()).arg(btnColor.lighter().green()).arg(btnColor.lighter().blue()).arg(btnColor.lighter().alpha())
        .arg(btnColor.darker().red()).arg(btnColor.darker().green()).arg(btnColor.darker().blue()).arg(btnColor.darker().alpha()));

        connect(showBtn, &QPushButton::clicked, this, [this, item](){
            emit openAgvTaskRequested(item.agvId, item.type);
            showAgvList();
        });

        itemLayout->addWidget(showBtn, 0, Qt::AlignVCenter | Qt::AlignRight);

        contentLayout->addWidget(itemFrame);
    };


    QVector<MaintenanceItemData> upcoming = loadUpcomingMaintenance(selectedMonth_, selectedYear_);
    for (const auto &it : upcoming)
        addMaintenanceItem(it);

    rightUpcomingLayout->addWidget(contentContainer, 1);

    //
    // ======================= НОВЫЙ AGV LIST =======================
    //
    listAgvInfo = new ListAgvInfo([this](int v){ return s(v); }, rightBodyFrame);
    listAgvInfo->setVisible(false);
    connect(listAgvInfo, &ListAgvInfo::openAgvDetails, this, [this](const QString &id){
        showAgvDetailInfo(id);
    });

    // Когда нажали "Назад" в списке AGV → возвращаемся в календарь
    connect(listAgvInfo, &ListAgvInfo::backRequested, this, [this](){
        showCalendar();
    });



    //
    // ======================= ДЕТАЛЬНАЯ СТРАНИЦА AGV =======================
    //
    agvDetailInfo = new AgvDetailInfo([this](int v){ return s(v); }, rightBodyFrame);
    agvDetailInfo->setVisible(false);

    // Назад → возвращаемся в список AGV
    connect(agvDetailInfo, &AgvDetailInfo::backRequested, this, [this](){
        agvDetailInfo->setVisible(false);
        listAgvInfo->setVisible(true);
    });


    //
    // ======================= СУПЕР‑НАСТРОЙКИ AGV =======================
    //
    agvSettingsPage = new AgvSettingsPage([this](int v){ return s(v); }, rightBodyFrame);
    agvSettingsPage->setVisible(false);
    //
    // ======================= МОДЕЛИ AGV =======================
    //
    modelListPage = new ModelListPage([this](int v){ return s(v); }, rightBodyFrame);
    modelListPage->setVisible(false);

    connect(modelListPage, &ModelListPage::backRequested, this, [this](){
        showCalendar();
    });

    //
    // ======================= ДОБАВЛЯЕМ В ПРАВУЮ ПАНЕЛЬ =======================
    //
    rightBodyLayout->addWidget(rightCalendarFrame, 3);
    rightBodyLayout->addWidget(rightUpcomingMaintenanceFrame, 2);
    rightBodyLayout->addWidget(listAgvInfo, 3);
    rightBodyLayout->addWidget(agvDetailInfo, 3);
    rightBodyLayout->addWidget(agvSettingsPage, 3);
    rightBodyLayout->addWidget(modelListPage, 3);

    //
    // ======================= ПЕРЕКЛЮЧЕНИЕ РЕЖИМОВ =======================
    //
    connect(btnAgv, &QPushButton::clicked, this, [this](){
        showAgvList();
    });
    connect(btnEdit, &QPushButton::clicked, this, [this](){
        showModelList();
    });

}
//
// ======================= РЕЖИМЫ ПРАВОЙ ПАНЕЛИ =======================
//

void leftMenu::showAgvList()
{
    if (rightCalendarFrame)              rightCalendarFrame->setVisible(false);
    if (rightUpcomingMaintenanceFrame)   rightUpcomingMaintenanceFrame->setVisible(false);
    if (agvDetailInfo)                   agvDetailInfo->setVisible(false);
    if (agvSettingsPage)                 agvSettingsPage->setVisible(false);
    if (modelListPage)                   modelListPage->setVisible(false);   // ← ВОТ ЭТО ДОБАВИТЬ

    if (listAgvInfo)                     listAgvInfo->setVisible(true);
}


void leftMenu::showCalendar()
{
    if (listAgvInfo)                     listAgvInfo->setVisible(false);
    if (agvDetailInfo)                   agvDetailInfo->setVisible(false);
    if (agvSettingsPage)                 agvSettingsPage->setVisible(false);
    if (modelListPage)                   modelListPage->setVisible(false);   // ← ДОБАВЬ ЭТУ СТРОКУ

    if (rightCalendarFrame)              rightCalendarFrame->setVisible(true);
    if (rightUpcomingMaintenanceFrame)   rightUpcomingMaintenanceFrame->setVisible(true);
}


//
// ======================= ДЕТАЛЬНАЯ СТРАНИЦА AGV =======================
//

void leftMenu::showAgvDetailInfo(const QString &agvId)
{
    if (rightCalendarFrame)              rightCalendarFrame->setVisible(false);
    if (rightUpcomingMaintenanceFrame)   rightUpcomingMaintenanceFrame->setVisible(false);
    if (listAgvInfo)                     listAgvInfo->setVisible(false);
    if (agvSettingsPage)                 agvSettingsPage->setVisible(false);

    if (agvDetailInfo) {
        agvDetailInfo->setAgv(agvId);
        agvDetailInfo->setVisible(true);
    }
}

//
// ======================= МЕСЯЦ / ДЕНЬ =======================
//

void leftMenu::changeMonth(int delta)
{
    setSelectedMonthYear(selectedMonth_ + delta, selectedYear_);
}

void leftMenu::setSelectedMonthYear(int month, int year)
{
    if (month < 1) { month = 12; year--; }
    if (month > 12) { month = 1; year++; }

    selectedMonth_ = month;
    selectedYear_  = year;

    if (!selectedDay_.isValid() ||
        selectedDay_.year()  != selectedYear_ ||
        selectedDay_.month() != selectedMonth_)
    {
        selectedDay_ = QDate(selectedYear_, selectedMonth_, 1);
    }

    if (monthLabel)
        monthLabel->setText(monthYearLabelText(selectedMonth_, selectedYear_));

    //
    // Полный пересоздание UI под новый месяц
    //
    setScaleFactor(scaleFactor_);
}

void leftMenu::selectDay(int year, int month, int day)
{
    selectedDay_ = QDate(year, month, day);
    setSelectedMonthYear(month, year);
}

//
// ======================= ТЕКСТ МЕСЯЦА =======================
//

QString leftMenu::monthYearLabelText(int month, int year) const
{
    static QStringList months = {
        "Январь","Февраль","Март","Апрель","Май","Июнь",
        "Июль","Август","Сентябрь","Октябрь","Ноябрь","Декабрь"
    };

    return QString("%1 %2").arg(months[month - 1]).arg(year);
}
//
// ======================= АВАТАРЫ =======================
//

QPixmap leftMenu::makeRoundPixmap(const QPixmap &src, int size)
{
    if (src.isNull())
        return QPixmap();

    QPixmap out(size, size);
    out.fill(Qt::transparent);

    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    p.setClipPath(path);

    p.drawPixmap(0, 0, size, size, src);
    return out;
}

QPixmap leftMenu::loadUserAvatarFromDb(const QString &userId)
{
    // Заглушка — у тебя уже есть реализация
    return QPixmap(":/new/mainWindowIcons/noback/defaultUser.png");
}

//
// ======================= ЗАГРУЗКА ДАННЫХ =======================
//

QVector<CalendarEvent> leftMenu::loadCalendarEvents(int month, int year)
{
    // Заглушка — у тебя уже есть реализация
    return {};
}

QVector<MaintenanceItemData> leftMenu::loadUpcomingMaintenance(int month, int year)
{
    // Заглушка — у тебя уже есть реализация
    return {
        {"AGV-01", "ТО-1", QDate(2025,12,10), "Замена масла", "orange"},
        {"AGV-02", "ТО-2", QDate(2025,12,12), "Проверка датчиков", "green"},
        {"AGV-03", "ТО-1", QDate(2025,12,05), "Просрочено", "red"}
    };
}

SystemStatus leftMenu::loadSystemStatus()
{
    // Заглушка — у тебя уже есть реализация
    return {5, 2, 1, 1};
}
void leftMenu::showModelList()
{
    if (rightCalendarFrame)              rightCalendarFrame->setVisible(false);
    if (rightUpcomingMaintenanceFrame)   rightUpcomingMaintenanceFrame->setVisible(false);
    if (listAgvInfo)                     listAgvInfo->setVisible(false);
    if (agvDetailInfo)                   agvDetailInfo->setVisible(false);
    if (agvSettingsPage)                 agvSettingsPage->setVisible(false);

    if (modelListPage)                   modelListPage->setVisible(true);
}
bool leftMenu::eventFilter(QObject *obj, QEvent *event)
{
    if (calendarTablePtr && obj == calendarTablePtr->viewport() && event->type() == QEvent::Paint) {

        QPainter p(calendarTablePtr->viewport());
        p.setPen(QColor("#D3D3D3"));

        for (int r = 1; r < calendarTablePtr->rowCount(); r++) {
            for (int c = 0; c < calendarTablePtr->columnCount(); c++) {
                QRect rect = calendarTablePtr->visualRect(calendarTablePtr->model()->index(r, c));
                p.drawRect(rect.adjusted(0,0,-1,-1));
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

//
// ======================= КОНЕЦ ФАЙЛА =======================
//

