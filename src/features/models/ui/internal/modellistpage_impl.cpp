#include "../modellistpage.h"
#include "db_users.h"
#include "app_session.h"

// C++11: constexpr static member needs one out-of-line definition for the linker.
constexpr int ModelListPage::kModelsPageBatch;

// ====================== TaskRow ======================
// ====================== TaskRow (адаптивный) ======================

TaskRow::TaskRow(std::function<int(int)> s, bool checkboxVisible, QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(s(12));
    row->setAlignment(Qt::AlignVCenter);

    // Чекбокс
    check = new QCheckBox(this);
    check->setVisible(checkboxVisible);
    check->setFixedWidth(s(40));
    check->setStyleSheet(QString(
        "QCheckBox::indicator { width:%1px; height:%1px; }"
    ).arg(s(28)));
    row->addWidget(check);

    QString editStyle =
        "QLineEdit{font-size:18px;padding:4px 8px;border:1px solid #C8C8C8;border-radius:6px;}"
        "QLineEdit:focus{border:1px solid #0F00DB;}";

    // PROC — тянется ×3
    proc = new QLineEdit(this);
    proc->setPlaceholderText("Процедура");
    proc->setMinimumHeight(s(40));
    proc->setStyleSheet(editStyle);
    proc->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    proc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row->addWidget(proc, 3);

    // DAYS — тянется ×1
    days = new QLineEdit(this);
    days->setPlaceholderText("Дни");
    days->setValidator(new QIntValidator(0, 9999, this));
    days->setMinimumHeight(s(40));
    days->setStyleSheet(editStyle);
    days->setAlignment(Qt::AlignCenter);
    days->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row->addWidget(days, 1);

    // MINS — тянется ×1
    mins = new QLineEdit(this);
    mins->setPlaceholderText("Минуты");
    mins->setValidator(new QIntValidator(0, 9999, this));
    mins->setMinimumHeight(s(40));
    mins->setStyleSheet(editStyle);
    mins->setAlignment(Qt::AlignCenter);
    mins->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row->addWidget(mins, 1);

    row->addStretch();
}


// ====================== AddModelDialog ======================

AddModelDialog::AddModelDialog(std::function<int(int)> scale, QWidget *parent)
    : QDialog(parent)
    , s_(scale)
{
    setWindowTitle("Добавить модель AGV");
    setModal(true);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(s_(24), s_(24), s_(24), s_(24));
    root->setSpacing(s_(16));

    // === Стили ===
    QString roundedStyle = QString(
        "QLineEdit{border:1px solid #C8C8C8;border-radius:%1px;"
        "padding:8px 12px;font-family:Inter;font-size:15px;background:white;}"
        "QLineEdit:focus{border:2px solid #0F00DB;}"
    ).arg(s_(10));

    QString underlineStyle =
        "QLineEdit{background:transparent;border:none;border-bottom:2px solid #333;"
        "font-family:Inter;font-size:15px;font-weight:700;padding:4px 2px;}"
        "QLineEdit:focus{border-bottom:2px solid #0F00DB;}";

    QString errorStyle = "color:#D00000;font-size:12px;font-family:Inter;font-weight:600;";

    // === Название модели ===
    QLabel *lblName = new QLabel("Название модели:", this);
    lblName->setStyleSheet("font-family:Inter;font-size:14px;font-weight:600;color:#333;");
    root->addWidget(lblName);

    nameEdit_ = new QLineEdit(this);
    nameEdit_->setMaxLength(15);
    nameEdit_->setPlaceholderText("Только A-Z, a-z, 0-9");
    nameEdit_->setValidator(new QRegularExpressionValidator(QRegularExpression("^[A-Za-z0-9]*$"), nameEdit_));
    nameEdit_->setStyleSheet(roundedStyle);
    nameEdit_->setMinimumHeight(s_(40));
    root->addWidget(nameEdit_);

    errName_ = new QLabel(this);
    errName_->setStyleSheet(errorStyle);
    errName_->hide();
    root->addWidget(errName_);

    // === Версия ПО ===
    QLabel *lblVersionPo = new QLabel("Версия ПО:", this);
    lblVersionPo->setStyleSheet("font-family:Inter;font-size:14px;font-weight:600;color:#333;");
    root->addWidget(lblVersionPo);

    QWidget *versionPoContainer = new QWidget(this);
    QHBoxLayout *hVersionPo = new QHBoxLayout(versionPoContainer);
    hVersionPo->setContentsMargins(0, 0, 0, 0);
    hVersionPo->setSpacing(0);
    versionPoSegments_.clear();
    for (int i = 0; i < 4; i++) {
        if (i > 0) {
            QLabel *dot = new QLabel(".", versionPoContainer);
            dot->setStyleSheet("font-size:18px;font-weight:900;color:#333;");
            dot->setFixedWidth(s_(12));
            dot->setAlignment(Qt::AlignCenter);
            hVersionPo->addWidget(dot);
        }
        QLineEdit *seg = new QLineEdit(versionPoContainer);
        seg->setMaxLength(4);
        seg->setFixedWidth(s_(60));
        seg->setMinimumHeight(s_(36));
        seg->setAlignment(Qt::AlignCenter);
        seg->setValidator(new QRegularExpressionValidator(QRegularExpression("^[A-Za-z0-9]*$"), seg));
        seg->setStyleSheet(underlineStyle);
        versionPoSegments_.append(seg);
        hVersionPo->addWidget(seg);
    }
    hVersionPo->addStretch();
    root->addWidget(versionPoContainer);

    errVersionPo_ = new QLabel(this);
    errVersionPo_->setStyleSheet(errorStyle);
    errVersionPo_->hide();
    root->addWidget(errVersionPo_);

    // === Версия EPLAN ===
    QLabel *lblVersionEplan = new QLabel("Версия EPLAN:", this);
    lblVersionEplan->setStyleSheet("font-family:Inter;font-size:14px;font-weight:600;color:#333;");
    root->addWidget(lblVersionEplan);

    QWidget *versionEplanContainer = new QWidget(this);
    QHBoxLayout *hVersionEplan = new QHBoxLayout(versionEplanContainer);
    hVersionEplan->setContentsMargins(0, 0, 0, 0);
    hVersionEplan->setSpacing(0);
    versionEplanSegments_.clear();
    for (int i = 0; i < 4; i++) {
        if (i > 0) {
            QLabel *dot = new QLabel(".", versionEplanContainer);
            dot->setStyleSheet("font-size:18px;font-weight:900;color:#333;");
            dot->setFixedWidth(s_(12));
            dot->setAlignment(Qt::AlignCenter);
            hVersionEplan->addWidget(dot);
        }
        QLineEdit *seg = new QLineEdit(versionEplanContainer);
        seg->setMaxLength(4);
        seg->setFixedWidth(s_(60));
        seg->setMinimumHeight(s_(36));
        seg->setAlignment(Qt::AlignCenter);
        seg->setValidator(new QRegularExpressionValidator(QRegularExpression("^[A-Za-z0-9]*$"), seg));
        seg->setStyleSheet(underlineStyle);
        versionEplanSegments_.append(seg);
        hVersionEplan->addWidget(seg);
    }
    hVersionEplan->addStretch();
    root->addWidget(versionEplanContainer);

    errVersionEplan_ = new QLabel(this);
    errVersionEplan_->setStyleSheet(errorStyle);
    errVersionEplan_->hide();
    root->addWidget(errVersionEplan_);

    // === Грузоподъёмность ===
    QLabel *lblCap = new QLabel("Грузоподъёмность (кг, до 10000):", this);
    lblCap->setStyleSheet("font-family:Inter;font-size:14px;font-weight:600;color:#333;");
    root->addWidget(lblCap);

    capacityEdit_ = new QLineEdit(this);
    capacityEdit_->setMaxLength(5);
    capacityEdit_->setPlaceholderText("Только цифры");
    capacityEdit_->setValidator(new QIntValidator(0, 10000, this));
    capacityEdit_->setStyleSheet(roundedStyle);
    capacityEdit_->setMinimumHeight(s_(40));
    root->addWidget(capacityEdit_);

    errCap_ = new QLabel(this);
    errCap_->setStyleSheet(errorStyle);
    errCap_->hide();
    root->addWidget(errCap_);

    root->addStretch();

    // === Кнопки ===
    QHBoxLayout *btns = new QHBoxLayout();
    btns->setSpacing(s_(16));

    QPushButton *tplBtn = new QPushButton("Шаблон ТО", this);
    tplBtn->setMinimumHeight(s_(44));
    tplBtn->setStyleSheet(
        "QPushButton{background:#E6E6E6;border-radius:10px;border:1px solid #C8C8C8;"
        "font-family:Inter;font-size:15px;font-weight:700;padding:8px 16px;}"
        "QPushButton:hover{background:#D5D5D5;}"
    );

    QPushButton *cancel = new QPushButton("Отмена", this);
    cancel->setMinimumHeight(s_(44));
    cancel->setStyleSheet(
        "QPushButton{background:#F7F7F7;border-radius:10px;border:1px solid #C8C8C8;"
        "font-family:Inter;font-size:15px;font-weight:700;padding:8px 16px;}"
        "QPushButton:hover{background:#EDEDED;}"
    );

    QPushButton *ok = new QPushButton("Добавить", this);
    ok->setMinimumHeight(s_(44));
    ok->setStyleSheet(
        "QPushButton{background:#28A745;color:white;border-radius:10px;"
        "font-family:Inter;font-size:15px;font-weight:800;padding:8px 20px;}"
        "QPushButton:hover{background:#2EC24F;}"
    );

    btns->addWidget(tplBtn);
    btns->addWidget(cancel);
    btns->addStretch();
    btns->addWidget(ok);
    root->addLayout(btns);

    // === Автопереход между сегментами версий ===
    for (int i = 0; i < versionPoSegments_.size() - 1; i++) {
        QLineEdit *cur = versionPoSegments_[i];
        QLineEdit *next = versionPoSegments_[i + 1];
        connect(cur, &QLineEdit::textChanged, this, [cur, next](const QString &t){
            if (t.length() >= 4) next->setFocus();
        });
    }
    for (int i = 0; i < versionEplanSegments_.size() - 1; i++) {
        QLineEdit *cur = versionEplanSegments_[i];
        QLineEdit *next = versionEplanSegments_[i + 1];
        connect(cur, &QLineEdit::textChanged, this, [cur, next](const QString &t){
            if (t.length() >= 4) next->setFocus();
        });
    }

    // === Валидация в реальном времени ===
    connect(nameEdit_, &QLineEdit::textChanged, this, [this](){
        QString name = nameEdit_->text();
        if (name.isEmpty()) {
            errName_->setText("Введите название модели");
            errName_->show();
        } else {
            errName_->hide();
        }
    });
    connect(nameEdit_, &QLineEdit::inputRejected, this, [this](){
        errName_->setText("Только английские буквы и цифры!");
        errName_->show();
    });

    connect(capacityEdit_, &QLineEdit::textChanged, this, [this](){
        QString text = capacityEdit_->text();
        if (text.isEmpty()) {
            errCap_->setText("Введите грузоподъёмность");
            errCap_->show();
        } else {
            int val = text.toInt();
            if (val > 10000) {
                capacityEdit_->setText("10000");
                errCap_->setText("Максимум 10000 кг");
                errCap_->show();
            } else {
                errCap_->hide();
            }
        }
    });
    connect(capacityEdit_, &QLineEdit::inputRejected, this, [this](){
        errCap_->setText("Только цифры!");
        errCap_->show();
    });

    // === Кнопки действий ===
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, [this](){
        if (!validateAll()) return;
        fillResultFromForm();
        result_.useTemplate = false;
        accept();
    });
    connect(tplBtn, &QPushButton::clicked, this, [this](){
        if (!validateAll()) return;
        fillResultFromForm();
        result_.useTemplate = true;
        accept();
    });

    setFixedSize(s_(520), s_(520));
}

void AddModelDialog::fillResultFromForm()
{
    result_.model.name = nameEdit_->text().trimmed();
    QStringList vp, ve;
    for (auto *e : versionPoSegments_) vp << e->text().trimmed();
    for (auto *e : versionEplanSegments_) ve << e->text().trimmed();
    result_.model.versionPo = vp.join(".");
    result_.model.versionEplan = ve.join(".");
    result_.model.dimensions = QString();
    result_.model.capacityKg = capacityEdit_->text().toInt();
    result_.model.category = QString();
    result_.model.maxSpeed = 0;
    result_.model.couplingCount = 0;
    result_.model.direction = QString();
}

AddModelDialog::Result AddModelDialog::result() const
{
    return result_;
}

bool AddModelDialog::validateAll()
{
    bool ok = true;

    const QString name = nameEdit_->text().trimmed();
    const QRegularExpression nameRe("^[A-Za-z0-9]{1,15}$");
    if (!nameRe.match(name).hasMatch()) {
        errName_->setText("Название: только английские буквы/цифры, до 15 символов");
        errName_->show();
        ok = false;
    } else {
        errName_->hide();
    }

    QStringList vpParts;
    for (auto *e : versionPoSegments_) vpParts << e->text().trimmed();
    QString vp = vpParts.join(".");
    QRegularExpression verRe("^([A-Za-z0-9]{1,4}\\.){3}[A-Za-z0-9]{1,4}$");
    if (!verRe.match(vp).hasMatch()) {
        errVersionPo_->setText("Версия ПО: ");
        errVersionPo_->show();
        ok = false;
    } else { errVersionPo_->hide(); }

    QStringList veParts;
    for (auto *e : versionEplanSegments_) veParts << e->text().trimmed();
    QString ve = veParts.join(".");
    if (!verRe.match(ve).hasMatch()) {
        errVersionEplan_->setText("Версия EPLAN:");
        errVersionEplan_->show();
        ok = false;
    } else { errVersionEplan_->hide(); }

    bool capOk = false;
    const int capacity = capacityEdit_->text().toInt(&capOk);
    if (!capOk || capacityEdit_->text().trimmed().isEmpty() || capacity > 10000) {
        errCap_->setText("Грузоподъёмность: только число, максимум 10000 кг");
        errCap_->show();
        ok = false;
    } else {
        errCap_->hide();
    }

    return ok;
}

// ====================== TemplatePageWidget ======================

TemplatePageWidget::TemplatePageWidget(const ModelInfo &model,
                                       std::function<int(int)> scale,
                                       QWidget *parent)
    : QWidget(parent)
    , s_(scale)
    , model_(model)
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(s_(10), s_(10), s_(10), s_(10));
    root->setSpacing(s_(14));

    QLabel *title = new QLabel(
        QString("Создание модели по Шаблону ТО : %1").arg(model_.name),
        this
    );
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
    ).arg(s_(24)));
    root->addWidget(title);

    // scrollArea с контейнером строк
    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setStyleSheet("border:none;background:transparent;");

    QWidget *container = new QWidget(scrollArea_);
    rowsLayout_ = new QVBoxLayout(container);
    rowsLayout_->setContentsMargins(0, 0, 0, 0);
    rowsLayout_->setSpacing(s_(10));

    scrollArea_->setWidget(container);
    root->addWidget(scrollArea_, 1);

    // Хедер как первая строка таблицы
    {
        TaskRow *header = new TaskRow(s_, false, this);

        header->proc->setText("Деталь / Наименование работ");
        header->days->setText("Дни");
        header->mins->setText("Минуты");

        header->proc->setReadOnly(true);
        header->days->setReadOnly(true);
        header->mins->setReadOnly(true);

        header->proc->setStyleSheet("font-weight:800; font-size:16px; padding:4px 8px;");
        header->days->setStyleSheet("font-weight:800; font-size:16px; padding:4px 8px;");
        header->mins->setStyleSheet("font-weight:800; font-size:16px; padding:4px 8px;");

        header->check->setVisible(false);

        rowsLayout_->addWidget(header);
        rows_.push_back(header);
    }


    loadDefaultRows();


    // Панель кнопок
    QWidget *toolbar = new QWidget(this);
    toolbar->setStyleSheet("background:transparent;");
    QHBoxLayout *bottom = new QHBoxLayout(toolbar);
    bottom->setContentsMargins(0, 0, 0, 0);
    bottom->setSpacing(s_(12));

    // Добавить строку
    addBtn_ = new QPushButton("Добавить строку", this);
    addBtn_->setMinimumHeight(s_(40));
    addBtn_->setStyleSheet(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;font-size:16px;"
        "font-weight:800;border-radius:8px;padding:6px 16px;}"
        "QPushButton:hover{background:#1A4ACD;}"
    );
    bottom->addWidget(addBtn_);

    bottom->addStretch();

    // Удалить строки
    delModeBtn_ = new QPushButton("Удалить строки", this);
    delModeBtn_->setMinimumHeight(s_(40));
    delModeBtn_->setStyleSheet(
        "QPushButton{background:#FF3B30;color:white;font-family:Inter;font-size:16px;"
        "font-weight:800;border-radius:8px;padding:6px 16px;}"
        "QPushButton:hover{background:#E13228;}"
    );
    bottom->addWidget(delModeBtn_);

    // Отмена удаления
    QPushButton *cancelDeleteBtn = new QPushButton("Отмена удаления", this);
    cancelDeleteBtn->setObjectName("cancelDeleteBtn");
    cancelDeleteBtn->setMinimumHeight(s_(40));
    cancelDeleteBtn->setVisible(false);
    cancelDeleteBtn->setStyleSheet(
        "QPushButton{background:#F7F7F7;border-radius:8px;border:1px solid #C8C8C8;"
        "font-family:Inter;font-size:16px;font-weight:700;padding:6px 16px;}"
        "QPushButton:hover{background:#EDEDED;}"
    );
    bottom->addWidget(cancelDeleteBtn);

    root->addWidget(toolbar);

    // Нижние кнопки
    QHBoxLayout *btns = new QHBoxLayout();
    btns->setSpacing(s_(12));

    cancelBtn_ = new QPushButton("Отмена", this);
    saveBtn_   = new QPushButton("Сохранить модель и шаблон", this);

    cancelBtn_->setMinimumHeight(s_(44));
    saveBtn_->setMinimumHeight(s_(44));

    cancelBtn_->setStyleSheet(
        "QPushButton{background:#F7F7F7;border-radius:10px;border:1px solid #C8C8C8;"
        "font-family:Inter;font-size:15px;font-weight:700;padding:8px 16px;}"
        "QPushButton:hover{background:#EDEDED;}"
    );
    saveBtn_->setStyleSheet(
        "QPushButton{background:#28A745;color:white;border-radius:10px;"
        "font-family:Inter;font-size:15px;font-weight:800;padding:8px 20px;}"
        "QPushButton:hover{background:#2EC24F;}"
    );

    btns->addWidget(cancelBtn_);
    btns->addStretch();
    btns->addWidget(saveBtn_);
    root->addLayout(btns);

    connect(addBtn_, &QPushButton::clicked, this, &TemplatePageWidget::addRow);
    connect(delModeBtn_, &QPushButton::clicked, this, &TemplatePageWidget::toggleDeleteMode);
    connect(cancelBtn_, &QPushButton::clicked, this, &TemplatePageWidget::cancelRequested);
    connect(saveBtn_, &QPushButton::clicked, this, &TemplatePageWidget::onSaveClicked);

    connect(cancelDeleteBtn, &QPushButton::clicked, this, [this](){
        if (!deleteMode_) return;
        deleteMode_ = false;

        for (auto *r : rows_)
            r->check->setVisible(false);

        delModeBtn_->setText("Удалить строки");
        QPushButton *btn = this->findChild<QPushButton*>("cancelDeleteBtn");
        if (btn) btn->setVisible(false);

        disconnect(delModeBtn_, nullptr, nullptr, nullptr);
        connect(delModeBtn_, &QPushButton::clicked, this, &TemplatePageWidget::toggleDeleteMode, Qt::UniqueConnection);
    });
}

void TemplatePageWidget::loadDefaultRows()
{
    struct RowDef { const char *name; int days; int mins; };

    static const RowDef rows[] = {
        {"Корпус - очистка от загрязнений", 30, 20},
        {"Корпус - контроль резьбовых соединений, протяжка", 30, 90},
        {"Позиционирующие ролики - проверить на наличие люфтов", 30, 20},
        {"Сканер безопасности - очистка сканера безопасности", 30, 20},
        {"Сканер безопасности - проверка крепления кабеля сканера", 30, 30},
        {"Бампер - контроль резьбовых соединений, протяжка", 30, 90},
        {"Лицевая панель - контроль целостности защиты панели", 30, 20},
        {"Электрика - проверка работы звука", 30, 15},
        {"Электрика - контроль целостности кнопок и панели", 30, 20},
        {"PIN - очистка (с разборкой)", 90, 120},
        {"PIN - контроль резьбовых соединений, протяжка", 30, 60},
        {"PIN - осмотр, контроль люфтов", 30, 60},
        {"PIN - проверка укладки проводов", 30, 30},
        {"PIN - проверка работы", 30, 15},
        {"PIN - контроль верхнего положения → максимум 3 мм не доходит", 30, 30},
        {"PIN - смазка", 30, 20},
        {"Подъемник - проверка установленных штифтов на кулачке", 30, 30},
        {"Подъемник - контроль резьбовых соединений, протяжка", 30, 60},
        {"Подъемник - проверка крепления опорных подшипников", 30, 20},
        {"Подъемник - проверка работы подъемника", 30, 15},
        {"Подъемник - проверка срабатывания концевиков (настройка)", 30, 20},
        {"Drive unit - проверка крепления подъемной втулки", 30, 30},
        {"Drive unit - контроль резьбовых соединений, протяжка", 30, 60},
        {"Drive unit - проверка укладки и целостности кабеля", 30, 30},
        {"Drive unit - осмотр, контроль люфтов", 30, 30},
        {"Drive unit - контроль натяжения цепей", 90, 60},
        {"Drive unit - смазка цепей", 30, 30},
        {"Датчик трека и RFID - проверка крепления", 30, 20},
        {"Датчик трека и RFID - проверка целостности", 30, 15},
        {"Датчик трека и RFID - проверка целостности и укладки кабелей", 30, 30},
        {"Датчик трека и RFID - проверка крепления и наличия защиты", 30, 30},
        {"Датчик трека и RFID - высота от пола → не более 20 мм", 30, 30},
        {"Колеса приводные - контроль диаметра наружного → минимум Ø 145", 30, 15},
        {"Колеса приводные - проверка продольного люфта", 30, 15},
        {"Колеса приводные - проверка крепления крышки колеса → до упора, с LOCTITE 243", 30, 30},
        {"Приводные звездочки - очистка от мусора", 30, 20},
        {"Приводные звездочки - проверка установленных штифтов на звездах", 30, 30},
        {"Приводные звездочки - проверка крепления крышки звездочки → до упора, с LOCTITE 243", 30, 30},
        {"Колеса поворотные - очистка от загрязнений", 30, 20},
        {"Колеса поворотные - проверка продольного люфта", 30, 20},
        {"Колеса задние - проверка продольного люфта", 30, 20}
    };

    for (auto &r : rows) {
        TaskRow *row = new TaskRow(s_, false, this);
        row->proc->setText(QString::fromUtf8(r.name));
        row->days->setText(QString::number(r.days));
        row->mins->setText(QString::number(r.mins));
        rowsLayout_->addWidget(row);
        rows_.push_back(row);
    }
}

void TemplatePageWidget::addRow()
{
    TaskRow *row = new TaskRow(s_, deleteMode_, this);
    rowsLayout_->addWidget(row);
    rows_.push_back(row);

    QTimer::singleShot(0, this, [this](){
        scrollArea_->verticalScrollBar()->setValue(
            scrollArea_->verticalScrollBar()->maximum()
        );
    });
}

void TemplatePageWidget::toggleDeleteMode()
{
    deleteMode_ = !deleteMode_;

    for (auto *r : rows_)
        r->check->setVisible(deleteMode_);

    QPushButton *cancelDeleteBtn = this->findChild<QPushButton*>("cancelDeleteBtn");

    if (deleteMode_) {
        delModeBtn_->setText("Удалить выбранные");
        if (cancelDeleteBtn) cancelDeleteBtn->setVisible(true);

        disconnect(delModeBtn_, nullptr, nullptr, nullptr);
        connect(delModeBtn_, &QPushButton::clicked, this, &TemplatePageWidget::deleteSelected, Qt::UniqueConnection);
    } else {
        delModeBtn_->setText("Удалить строки");
        if (cancelDeleteBtn) cancelDeleteBtn->setVisible(false);

        disconnect(delModeBtn_, nullptr, nullptr, nullptr);
        connect(delModeBtn_, &QPushButton::clicked, this, &TemplatePageWidget::toggleDeleteMode, Qt::UniqueConnection);
    }
}

void TemplatePageWidget::deleteSelected()
{
    QVector<TaskRow*> keep;

    for (auto *r : rows_) {
        if (r->check->isChecked()) {
            r->deleteLater();
        } else {
            keep.push_back(r);
        }
    }

    rows_ = keep;

    deleteMode_ = false;
    for (auto *r : rows_)
        r->check->setVisible(false);

    delModeBtn_->setText("Удалить строки");

    QPushButton *cancelDeleteBtn = this->findChild<QPushButton*>("cancelDeleteBtn");
    if (cancelDeleteBtn) cancelDeleteBtn->setVisible(false);

    disconnect(delModeBtn_, nullptr, nullptr, nullptr);
    connect(delModeBtn_, &QPushButton::clicked, this, &TemplatePageWidget::toggleDeleteMode, Qt::UniqueConnection);
}

void TemplatePageWidget::onSaveClicked()
{
    QVector<MaintenanceTask> tasks;

    for (auto *r : rows_) {
        QString name = r->proc->text().trimmed();
        int d = r->days->text().toInt();
        int m = r->mins->text().toInt();

        if (name.isEmpty() || d <= 0 || m <= 0)
            continue;

        MaintenanceTask t;
        t.name = name;
        t.intervalDays = d;
        t.durationMinutes = m;
        tasks.push_back(t);
    }

    emit saveRequested(model_, tasks);
}

// ====================== ModelDetailsPageWidget ======================

ModelDetailsPageWidget::ModelDetailsPageWidget(const ModelInfo &model,
                                               std::function<int(int)> scale,
                                               QWidget *parent)
    : QWidget(parent)
{
    auto showWarning = [this](const QString &title, const QString &text){
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Warning);
        msg.setWindowTitle(title);
        msg.setText(text);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setStyleSheet(
            "QMessageBox{background:#FFFFFF;}"
            "QLabel{color:#1A1A1A;font-family:Inter;font-size:14px;}"
            "QPushButton{min-width:90px;background:#EDEFF3;color:#1A1A1A;border:1px solid #C9CED8;border-radius:6px;padding:6px 10px;}"
            "QPushButton:hover{background:#E1E5EC;}"
        );
        msg.exec();
    };
    auto showInfo = [this](const QString &title, const QString &text){
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Information);
        msg.setWindowTitle(title);
        msg.setText(text);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setStyleSheet(
            "QMessageBox{background:#FFFFFF;}"
            "QLabel{color:#1A1A1A;font-family:Inter;font-size:14px;}"
            "QPushButton{min-width:90px;background:#EDEFF3;color:#1A1A1A;border:1px solid #C9CED8;border-radius:6px;padding:6px 10px;}"
            "QPushButton:hover{background:#E1E5EC;}"
        );
        msg.exec();
    };

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(scale(10), scale(10), scale(10), scale(10));
    root->setSpacing(scale(12));

    QHBoxLayout *top = new QHBoxLayout();
    QPushButton *backBtn = new QPushButton("   Назад", this);
    backBtn->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    backBtn->setIconSize(QSize(scale(24), scale(24)));
    backBtn->setFixedSize(scale(150), scale(50));
    backBtn->setStyleSheet(QString(
        "QPushButton { background-color:#E6E6E6; border-radius:%1px; border:1px solid #C8C8C8;"
        "font-family:Inter; font-size:%2px; font-weight:800; color:black; text-align:left; padding-left:%3px; }"
        "QPushButton:hover { background-color:#D5D5D5; }"
    ).arg(scale(10)).arg(scale(16)).arg(scale(10)));

    QLabel *title = new QLabel("Информация о модели AGV", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
    ).arg(scale(26)));

    top->addWidget(backBtn);
    top->addStretch();
    top->addWidget(title);
    top->addStretch();
    root->addLayout(top);

    auto modelName = std::make_shared<QString>(model.name);
    auto versionPo = std::make_shared<QString>(model.versionPo);
    auto versionEplan = std::make_shared<QString>(model.versionEplan);
    auto category = std::make_shared<QString>(model.category);
    auto capacity = std::make_shared<int>(model.capacityKg);
    auto maxSpeed = std::make_shared<int>(model.maxSpeed);
    auto dimensions = std::make_shared<QString>(model.dimensions);
    auto couplingCount = std::make_shared<int>(model.couplingCount);
    auto direction = std::make_shared<QString>(model.direction);
    auto modelDirty = std::make_shared<bool>(false);
    auto modelEditMode = std::make_shared<bool>(false);

    QFrame *infoFrame = new QFrame(this);
    infoFrame->setStyleSheet("background:white;border-radius:10px;border:1px solid #E0E0E0;");
    QVBoxLayout *infoLay = new QVBoxLayout(infoFrame);
    infoLay->setContentsMargins(scale(16), scale(14), scale(16), scale(14));
    infoLay->setSpacing(scale(8));

    auto makeRow = [&](const QString &label, QWidget *edit){
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(scale(12));
        QLabel *lbl = new QLabel(label + ":", infoFrame);
        lbl->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:700;color:#555;"
            "background:transparent;border:none;padding:0;"
        ).arg(scale(14)));
        lbl->setMinimumWidth(0); lbl->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        row->addWidget(lbl);
        edit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        row->addWidget(edit);
        row->addStretch();
        return row;
    };
    QString inlineNameEditStyle = QString(
        "QLineEdit{font-family:Inter;font-size:%1px;font-weight:700;color:#1A1A1A;border:none;background:transparent;padding:0;}"
        "QLineEdit:focus{border:none;}"
    ).arg(scale(17));
    QString versionSegmentStyle = QString(
        "QLineEdit{font-family:Inter;font-size:%1px;font-weight:700;color:#1A1A1A;"
        "background:transparent;border:none;border-bottom:2px solid #333;padding:4px 2px;}"
        "QLineEdit:focus{border-bottom:2px solid #0F00DB;}"
    ).arg(scale(15));
    QString styleRounded = QString(
        "QLineEdit{font-family:Inter;font-size:%1px;font-weight:700;color:#1A1A1A;"
        "border:1px solid #C8C8C8;border-radius:%2px;background:white;padding:6px 10px;}"
        "QLineEdit:focus{border:1px solid #0F00DB;}"
    ).arg(scale(16)).arg(scale(10));

    // Формат: "Название: [edit]" — лейбл и поле на одной строке. Левая колонка: название, версия по, версия eplan
    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(scale(12));
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    QLineEdit *nameEdit = new QLineEdit(*modelName, infoFrame);
    nameEdit->setStyleSheet(styleRounded);
    grid->addLayout(makeRow("Название модели", nameEdit), 0, 0);

    auto createVersionSegments = [&](const QString &val){
        QStringList parts = val.split(".");
        while (parts.size() < 4) parts << "";
        QWidget *w = new QWidget(infoFrame);
        w->setStyleSheet("background:transparent;border:none;");
        w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        QHBoxLayout *h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(0);
        QVector<QLineEdit*> segs;
        for (int i = 0; i < 4; i++) {
            if (i > 0) {
                QLabel *dot = new QLabel(".", w);
                dot->setStyleSheet("font-size:18px;font-weight:900;color:#333;");
                dot->setFixedWidth(scale(12));
                dot->setAlignment(Qt::AlignCenter);
                h->addWidget(dot);
            }
            QLineEdit *e = new QLineEdit(i < parts.size() ? parts[i] : "", w);
            e->setMaxLength(4);
            e->setFixedWidth(scale(60));
            e->setMinimumHeight(scale(36));
            e->setAlignment(Qt::AlignCenter);
            e->setValidator(new QRegularExpressionValidator(QRegularExpression("^[A-Za-z0-9]*$"), e));
            e->setStyleSheet(versionSegmentStyle);
            segs << e;
            h->addWidget(e);
        }
        h->addStretch();
        return qMakePair(w, segs);
    };
    auto vp = createVersionSegments(*versionPo);
    QWidget *versionPoWidget = vp.first;
    QVector<QLineEdit*> versionPoSegments = vp.second;
    grid->addLayout(makeRow("Версия ПО", versionPoWidget), 1, 0);

    auto ve = createVersionSegments(*versionEplan);
    QWidget *versionEplanWidget = ve.first;
    QVector<QLineEdit*> versionEplanSegments = ve.second;
    grid->addLayout(makeRow("Версия EPLAN", versionEplanWidget), 2, 0);

    auto createDimSegments = [&](const QString &val){
        QStringList parts = val.split("x", Qt::KeepEmptyParts);
        while (parts.size() < 4) parts << "";

        QWidget *w = new QWidget(infoFrame);
        w->setStyleSheet("background:transparent;border:none;");
        w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        QHBoxLayout *h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(0);

        QVector<QLineEdit*> segs;
        for (int i = 0; i < 4; i++) {
            if (i > 0) {
                QLabel *x = new QLabel("x", w);
                x->setStyleSheet("font-size:18px;font-weight:900;color:#333;background:transparent;border:none;");
                x->setFixedWidth(scale(12));
                x->setAlignment(Qt::AlignCenter);
                h->addWidget(x);
            }
            QLineEdit *e = new QLineEdit(i < parts.size() ? parts[i] : "", w);
            e->setMaxLength(4);
            e->setFixedWidth(scale(60));
            e->setMinimumHeight(scale(36));
            e->setAlignment(Qt::AlignCenter);
            e->setValidator(new QRegularExpressionValidator(QRegularExpression("^[0-9]*$"), e));
            e->setStyleSheet(versionSegmentStyle);
            segs << e;
            h->addWidget(e);
        }
        h->addStretch();
        return qMakePair(w, segs);
    };

    auto dm = createDimSegments(*dimensions);
    QWidget *dimWidget = dm.first;
    QVector<QLineEdit*> dimSegments = dm.second;

    QComboBox *dirCombo = new QComboBox(infoFrame);
    dirCombo->addItems(QStringList() << "—" << "1" << "2" << "4");
    int dirIdx = 0;
    if (*direction == "1") dirIdx = 1;
    else if (*direction == "2") dirIdx = 2;
    else if (*direction == "4") dirIdx = 3;
    dirCombo->setCurrentIndex(dirIdx);
    dirCombo->setMinimumHeight(scale(40));
    dirCombo->setMinimumWidth(scale(100));
    dirCombo->setStyleSheet(QString(
        "QComboBox{font-family:Inter;font-size:%1px;font-weight:700;color:#1A1A1A;"
        "border:2px solid #333;border-radius:%2px;background:white;padding:6px 12px;padding-right:30px;}"
        "QComboBox:hover{border:2px solid #0F00DB;}"
        "QComboBox:focus{border:2px solid #0F00DB;}"
        "QComboBox::drop-down{subcontrol-origin:padding;subcontrol-position:right center;width:28px;border:none;background:transparent;}"
        "QComboBox::down-arrow{width:14px;height:14px;}"
        "QComboBox QAbstractItemView{background:white;color:#1A1A1A;selection-background-color:#E6E6E6;border:1px solid #C8C8C8;}"
        "QComboBox QAbstractItemView::item{padding:8px 12px;min-height:32px;}"
    ).arg(scale(15)).arg(scale(8)));
    grid->addLayout(makeRow("Направление движения", dirCombo), 0, 1);

    QLineEdit *couplingEdit = new QLineEdit(*couplingCount > 0 ? QString::number(*couplingCount) : QString(), infoFrame);
    couplingEdit->setValidator(new QIntValidator(1, 4, couplingEdit));
    couplingEdit->setMaxLength(1);
    couplingEdit->setStyleSheet(inlineNameEditStyle);
    grid->addLayout(makeRow("Сцепные устройства (1-4)", couplingEdit), 1, 1);

    grid->addLayout(makeRow("Габариты (мм)", dimWidget), 2, 1);

    for (int i = 0; i < versionPoSegments.size() - 1; i++) {
        QLineEdit *cur = versionPoSegments[i];
        QLineEdit *next = versionPoSegments[i + 1];
        connect(cur, &QLineEdit::textChanged, this, [cur, next](const QString &t){
            if (t.length() >= 4) next->setFocus();
        });
    }
    for (int i = 0; i < versionEplanSegments.size() - 1; i++) {
        QLineEdit *cur = versionEplanSegments[i];
        QLineEdit *next = versionEplanSegments[i + 1];
        connect(cur, &QLineEdit::textChanged, this, [cur, next](const QString &t){
            if (t.length() >= 4) next->setFocus();
        });
    }
    for (int i = 0; i < dimSegments.size() - 1; i++) {
        QLineEdit *cur = dimSegments[i];
        QLineEdit *next = dimSegments[i + 1];
        connect(cur, &QLineEdit::textChanged, this, [cur, next](const QString &t){
            if (t.length() >= 4) next->setFocus();
        });
    }

    QLineEdit *categoryEdit = new QLineEdit(*category, infoFrame);
    categoryEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("^[\\p{L}\\p{N} /-]*$"), categoryEdit));
    categoryEdit->setStyleSheet(inlineNameEditStyle);
    grid->addLayout(makeRow("Категория", categoryEdit), 0, 2);

    QLineEdit *speedEdit = new QLineEdit(*maxSpeed > 0 ? QString::number(*maxSpeed) : QString(), infoFrame);
    speedEdit->setValidator(new QIntValidator(0, 100, speedEdit));
    speedEdit->setMaxLength(3);
    speedEdit->setStyleSheet(inlineNameEditStyle);
    grid->addLayout(makeRow("Макс. скорость (км/ч)", speedEdit), 1, 2);

    QLineEdit *capEdit = new QLineEdit(QString::number(*capacity), infoFrame);
    capEdit->setValidator(new QIntValidator(0, 100000, capEdit));
    capEdit->setMaxLength(6);
    capEdit->setStyleSheet(styleRounded);
    grid->addLayout(makeRow("Грузоподъёмность (кг)", capEdit), 2, 2);

    nameEdit->setReadOnly(true);
    for (auto *e : versionPoSegments) e->setReadOnly(true);
    for (auto *e : versionEplanSegments) e->setReadOnly(true);
    dirCombo->setEnabled(false);
    couplingEdit->setReadOnly(true);
    for (auto *e : dimSegments) e->setReadOnly(true);
    categoryEdit->setReadOnly(true);
    speedEdit->setReadOnly(true);
    capEdit->setReadOnly(true);

    infoLay->addLayout(grid);

    auto fitAllWidths = [=]() { (void)0; };

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, scale(8), 0, 0);
    btnRow->setSpacing(scale(8));

    QPushButton *editModelBtn = new QPushButton("Редактировать данные", infoFrame);
    QPushButton *saveModelBtn = new QPushButton("Сохранить", infoFrame);
    QPushButton *cancelModelBtn = new QPushButton("Отмена", infoFrame);

    editModelBtn->setStyleSheet(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;font-size:15px;font-weight:800;border-radius:8px;padding:6px 14px;}"
        "QPushButton:hover{background:#1A4ACD;}"
    );
    saveModelBtn->setStyleSheet(
        "QPushButton{background:#28A745;color:white;font-family:Inter;font-size:15px;font-weight:800;border-radius:8px;padding:6px 14px;}"
        "QPushButton:hover{background:#2EC24F;}"
    );
    cancelModelBtn->setStyleSheet(
        "QPushButton{background:#F7F7F7;border:1px solid #C8C8C8;color:#222;font-family:Inter;font-size:15px;font-weight:700;border-radius:8px;padding:6px 14px;}"
        "QPushButton:hover{background:#EDEDED;}"
    );

    saveModelBtn->hide();
    cancelModelBtn->hide();

    btnRow->addStretch();
    btnRow->addWidget(editModelBtn, 0, Qt::AlignVCenter);
    btnRow->addWidget(cancelModelBtn, 0, Qt::AlignVCenter);
    btnRow->addWidget(saveModelBtn, 0, Qt::AlignVCenter);
    infoLay->addLayout(btnRow);
    infoLay->addStretch();
    root->addWidget(infoFrame);

    bool canEditModel = (getUserRole(AppSession::currentUsername()) != "viewer");
    if (!canEditModel)
        editModelBtn->hide();

    auto enterModelEdit = [=](){
        *modelEditMode = true;
        editModelBtn->hide();
        nameEdit->setReadOnly(false);
        for (auto *e : versionPoSegments) { e->setReadOnly(false); e->setStyleSheet(versionSegmentStyle); }
        for (auto *e : versionEplanSegments) { e->setReadOnly(false); e->setStyleSheet(versionSegmentStyle); }
        capEdit->setReadOnly(false);
        categoryEdit->setReadOnly(false);
        speedEdit->setReadOnly(false);
        for (auto *e : dimSegments) { e->setReadOnly(false); e->setStyleSheet(versionSegmentStyle); }
        couplingEdit->setReadOnly(false);
        dirCombo->setEnabled(true);
        nameEdit->setStyleSheet(styleRounded);
        capEdit->setStyleSheet(styleRounded);
        categoryEdit->setStyleSheet(styleRounded);
        speedEdit->setStyleSheet(styleRounded);
        couplingEdit->setStyleSheet(styleRounded);
        nameEdit->setFocus();
        nameEdit->setCursorPosition(nameEdit->text().size());
    };
    auto leaveModelEdit = [=](){
        *modelEditMode = false;
        if (canEditModel)
            editModelBtn->show();
        nameEdit->setReadOnly(true);
        for (auto *e : versionPoSegments) { e->setReadOnly(true); e->setStyleSheet(versionSegmentStyle); }
        for (auto *e : versionEplanSegments) { e->setReadOnly(true); e->setStyleSheet(versionSegmentStyle); }
        capEdit->setReadOnly(true);
        categoryEdit->setReadOnly(true);
        speedEdit->setReadOnly(true);
        for (auto *e : dimSegments) { e->setReadOnly(true); e->setStyleSheet(versionSegmentStyle); }
        couplingEdit->setReadOnly(true);
        dirCombo->setEnabled(false);
        nameEdit->setStyleSheet(styleRounded);
        capEdit->setStyleSheet(styleRounded);
        categoryEdit->setStyleSheet(inlineNameEditStyle);
        speedEdit->setStyleSheet(inlineNameEditStyle);
        couplingEdit->setStyleSheet(inlineNameEditStyle);
        *modelDirty = false;
        saveModelBtn->hide();
        cancelModelBtn->hide();
        saveModelBtn->setEnabled(false);
    };
    auto versionPoStr = [=](){
        QStringList p; for (auto *e : versionPoSegments) p << e->text().trimmed();
        return p.join(".");
    };
    auto versionEplanStr = [=](){
        QStringList p; for (auto *e : versionEplanSegments) p << e->text().trimmed();
        return p.join(".");
    };
    auto dimStr = [=](){
        QStringList p; for (auto *e : dimSegments) p << e->text().trimmed();
        return p.join("x");
    };
    auto getDirValue = [=]() -> QString {
        int idx = dirCombo->currentIndex();
        if (idx == 1) return "1";
        if (idx == 2) return "2";
        if (idx == 3) return "4";
        return "";
    };
    auto setDirFromValue = [=](const QString &val) {
        if (val == "1") dirCombo->setCurrentIndex(1);
        else if (val == "2") dirCombo->setCurrentIndex(2);
        else if (val == "4") dirCombo->setCurrentIndex(3);
        else dirCombo->setCurrentIndex(0);
    };
    auto refreshModelDirty = [=](){
        bool spdOk = false;
        int spd = speedEdit->text().toInt(&spdOk);
        if (!spdOk) spd = 0;
        bool coupOk = false;
        int coup = couplingEdit->text().toInt(&coupOk);
        if (!coupOk) coup = 0;
        const bool changed =
            nameEdit->text().trimmed() != *modelName ||
            versionPoStr() != *versionPo ||
            versionEplanStr() != *versionEplan ||
            capEdit->text().toInt() != *capacity ||
            categoryEdit->text().trimmed() != *category ||
            spd != *maxSpeed ||
            dimStr() != *dimensions ||
            coup != *couplingCount ||
            getDirValue() != *direction;
        *modelDirty = changed;
        saveModelBtn->setVisible(*modelEditMode);
        cancelModelBtn->setVisible(*modelEditMode);
        saveModelBtn->setEnabled(changed);
    };

    connect(editModelBtn, &QPushButton::clicked, this, [=](){
        enterModelEdit();
        refreshModelDirty();
    });

    connect(nameEdit, &QLineEdit::textChanged, this, [=](const QString &){ refreshModelDirty(); fitAllWidths(); });
    for (auto *e : versionPoSegments)
        connect(e, &QLineEdit::textChanged, this, [=](const QString &){ refreshModelDirty(); });
    for (auto *e : versionEplanSegments)
        connect(e, &QLineEdit::textChanged, this, [=](const QString &){ refreshModelDirty(); });
    connect(categoryEdit, &QLineEdit::textChanged, this, [=](const QString &){ refreshModelDirty(); });
    connect(capEdit, &QLineEdit::textChanged, this, [=](const QString &){
        QString text = capEdit->text();
        if (!text.isEmpty()) {
            bool okNum = false;
            const int value = text.toInt(&okNum);
            if (okNum && value > 100000)
                capEdit->setText("100000");
        }
        refreshModelDirty();
        fitAllWidths();
    });
    connect(speedEdit, &QLineEdit::textChanged, this, [=](const QString &){
        QString text = speedEdit->text();
        if (!text.isEmpty()) {
            bool okNum = false;
            const int value = text.toInt(&okNum);
            if (okNum && value > 100)
                speedEdit->setText("100");
        }
        refreshModelDirty();
    });
    for (auto *e : dimSegments)
        connect(e, &QLineEdit::textChanged, this, [=](const QString &){ refreshModelDirty(); });
    connect(couplingEdit, &QLineEdit::textChanged, this, [=](const QString &){ refreshModelDirty(); });
    connect(dirCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int){ refreshModelDirty(); });

    connect(cancelModelBtn, &QPushButton::clicked, this, [=](){
        nameEdit->setText(*modelName);
        QStringList vpParts = (*versionPo).split(".");
        for (int i = 0; i < versionPoSegments.size(); i++)
            versionPoSegments[i]->setText(i < vpParts.size() ? vpParts[i] : "");
        QStringList veParts = (*versionEplan).split(".");
        for (int i = 0; i < versionEplanSegments.size(); i++)
            versionEplanSegments[i]->setText(i < veParts.size() ? veParts[i] : "");
        capEdit->setText(QString::number(*capacity));
        categoryEdit->setText(*category);
        speedEdit->setText(*maxSpeed > 0 ? QString::number(*maxSpeed) : QString());
        QStringList dimParts = (*dimensions).split("x", Qt::KeepEmptyParts);
        while (dimParts.size() < 4) dimParts << "";
        for (int i = 0; i < dimSegments.size(); i++)
            dimSegments[i]->setText(i < dimParts.size() ? dimParts[i] : "");
        couplingEdit->setText(*couplingCount > 0 ? QString::number(*couplingCount) : QString());
        setDirFromValue(*direction);
        fitAllWidths();
        leaveModelEdit();
    });

    connect(saveModelBtn, &QPushButton::clicked, this, [=](){
        const QString newName = nameEdit->text().trimmed();
        const QString newVersionPo = versionPoStr();
        const QString newVersionEplan = versionEplanStr();
        const QString newCategory = categoryEdit->text().trimmed();
        bool capOk = false;
        const int newCap = capEdit->text().toInt(&capOk);
        bool speedOk = false;
        const int newSpeed = speedEdit->text().toInt(&speedOk);
        const QString newDim = dimStr();
        bool coupOk = false;
        const int newCoup = couplingEdit->text().toInt(&coupOk);
        const QString newDir = getDirValue();

        if (newName.isEmpty() || !capOk || newCap > 100000) {
            showWarning("Модель AGV", "Проверьте данные: название и грузоподъёмность до 100000.");
            return;
        }
        if (newSpeed > 100) {
            showWarning("Модель AGV", "Макс. скорость до 100 км/ч.");
            return;
        }
        if (newCoup > 0 && (newCoup < 1 || newCoup > 4)) {
            showWarning("Модель AGV", "Сцепные устройства: 1-4.");
            return;
        }

        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (!db.isValid() || !db.isOpen()) {
            showWarning("Модель AGV", "База данных не открыта.");
            return;
        }
        const bool txSupported = db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions);
        bool txStarted = false;
        if (txSupported) {
            txStarted = db.transaction();
            if (!txStarted)
                qDebug() << "ModelDetails: transaction start failed, fallback to non-transaction mode:" << db.lastError().text();
        }

        QSqlQuery q(db);
        q.prepare("UPDATE agv_models SET name=:new_name, version_po=:vpo, version_eplan=:veplan, category=:cat, capacityKg=:cap, maxSpeed=:spd, dimensions=:dim, coupling_count=:coup, direction=:dir WHERE name=:old_name");
        q.bindValue(":new_name", newName);
        q.bindValue(":vpo", newVersionPo);
        q.bindValue(":veplan", newVersionEplan);
        q.bindValue(":cat", newCategory);
        q.bindValue(":cap", newCap);
        q.bindValue(":spd", newSpeed);
        q.bindValue(":dim", newDim);
        q.bindValue(":coup", newCoup > 0 ? newCoup : 0);
        q.bindValue(":dir", newDir);
        q.bindValue(":old_name", *modelName);

        if (!q.exec()) {
            if (txStarted) db.rollback();
            showWarning("Модель AGV", "Не удалось обновить модель: " + q.lastError().text());
            return;
        }

        if (newName != *modelName) {
            QSqlQuery q2(db);
            q2.prepare("UPDATE model_maintenance_template SET model_name=:new_name WHERE model_name=:old_name");
            q2.bindValue(":new_name", newName);
            q2.bindValue(":old_name", *modelName);
            if (!q2.exec()) {
                if (txStarted)
                    db.rollback();
                showWarning("Модель AGV", "Не удалось обновить шаблон: " + q2.lastError().text());
                return;
            }
        }

        if (txStarted) {
            if (!db.commit()) {
                db.rollback();
                showWarning("Модель AGV", "Ошибка сохранения.");
                return;
            }
        } else {
            if (db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions))
                db.commit();
        }
        qDebug() << "Модель обновлена в БД:" << db.hostName() << db.databaseName() << "name=" << newName;

        *modelName = newName;
        *versionPo = newVersionPo;
        *versionEplan = newVersionEplan;
        *category = newCategory;
        *capacity = newCap;
        *maxSpeed = newSpeed;
        *dimensions = newDim;
        *couplingCount = newCoup > 0 ? newCoup : 0;
        *direction = newDir;

        fitAllWidths();
        leaveModelEdit();
    });

    // ==== Таблица задач (визуал как "Создание модели по шаблону ТО") ====
    QFrame *tasksFrame = new QFrame(this);
    tasksFrame->setStyleSheet("background:transparent;border:none;");
    QVBoxLayout *tasksRoot = new QVBoxLayout(tasksFrame);
    tasksRoot->setContentsMargins(0, 0, 0, 0);
    tasksRoot->setSpacing(scale(10));

    QLabel *tasksTitle = new QLabel("Задачи ТО модели", tasksFrame);
    tasksTitle->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
    ).arg(scale(26)));
    tasksTitle->setAlignment(Qt::AlignHCenter);
    tasksRoot->addWidget(tasksTitle);

    QScrollArea *scrollArea = new QScrollArea(tasksFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("border:none;background:transparent;");

    QWidget *rowsHost = new QWidget(scrollArea);
    QVBoxLayout *rowsLay = new QVBoxLayout(rowsHost);
    rowsLay->setContentsMargins(0, 0, 0, 0);
    rowsLay->setSpacing(scale(10));
    scrollArea->setWidget(rowsHost);
    tasksRoot->addWidget(scrollArea, 1);

    auto rows = std::make_shared<QVector<TaskRow*>>();
    auto deleteMode = std::make_shared<bool>(false);
    auto tasksDirty = std::make_shared<bool>(false);

    QPushButton *addRowBtn = new QPushButton("Добавить строку", tasksFrame);
    QPushButton *delModeBtn = new QPushButton("Удалить строки", tasksFrame);
    QPushButton *cancelDeleteBtn = new QPushButton("Отмена удаления", tasksFrame);
    QPushButton *saveTasksBtn = new QPushButton("Сохранить", tasksFrame);
    QPushButton *cancelTasksBtn = new QPushButton("Отмена", tasksFrame);

    addRowBtn->setStyleSheet(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;font-size:16px;font-weight:800;border-radius:8px;padding:6px 16px;}"
        "QPushButton:hover{background:#1A4ACD;}"
    );
    delModeBtn->setStyleSheet(
        "QPushButton{background:#FF3B30;color:white;font-family:Inter;font-size:16px;font-weight:800;border-radius:8px;padding:6px 16px;}"
        "QPushButton:hover{background:#E13228;}"
    );
    cancelDeleteBtn->setStyleSheet(
        "QPushButton{background:#F7F7F7;border-radius:8px;border:1px solid #C8C8C8;font-family:Inter;font-size:16px;font-weight:700;padding:6px 16px;}"
        "QPushButton:hover{background:#EDEDED;}"
    );
    saveTasksBtn->setStyleSheet(
        "QPushButton{background:#28A745;color:white;border-radius:10px;font-family:Inter;font-size:15px;font-weight:800;padding:8px 20px;}"
        "QPushButton:hover{background:#2EC24F;}"
    );
    cancelTasksBtn->setStyleSheet(
        "QPushButton{background:#F7F7F7;border-radius:10px;border:1px solid #C8C8C8;font-family:Inter;font-size:15px;font-weight:700;padding:8px 16px;}"
        "QPushButton:hover{background:#EDEDED;}"
    );

    cancelDeleteBtn->hide();
    saveTasksBtn->hide();
    cancelTasksBtn->hide();

    auto setTasksDirtyUi = [=](bool dirty){
        *tasksDirty = dirty;
        if (canEditModel) {
            saveTasksBtn->setVisible(dirty);
            cancelTasksBtn->setVisible(dirty);
        }
    };

    auto attachRowSignals = [=](TaskRow *row){
        connect(row->proc, &QLineEdit::textChanged, this, [=](const QString &){ setTasksDirtyUi(true); });
        connect(row->days, &QLineEdit::textChanged, this, [=](const QString &){ setTasksDirtyUi(true); });
        connect(row->mins, &QLineEdit::textChanged, this, [=](const QString &){ setTasksDirtyUi(true); });
    };

    auto clearRows = [=](){
        rows->clear();
        QLayoutItem *child = nullptr;
        while ((child = rowsLay->takeAt(0)) != nullptr) {
            if (child->widget())
                child->widget()->deleteLater();
            delete child;
        }
    };

    auto loadRowsFromDb = [=](){
        clearRows();

        // заголовок
        TaskRow *header = new TaskRow(scale, false, rowsHost);
        header->proc->setText("Деталь / Наименование работ");
        header->days->setText("Дни");
        header->mins->setText("Минуты");
        header->proc->setReadOnly(true);
        header->days->setReadOnly(true);
        header->mins->setReadOnly(true);
        header->proc->setStyleSheet("font-weight:800; font-size:16px; padding:4px 8px;");
        header->days->setStyleSheet("font-weight:800; font-size:16px; padding:4px 8px;");
        header->mins->setStyleSheet("font-weight:800; font-size:16px; padding:4px 8px;");
        header->check->setVisible(false);
        rowsLay->addWidget(header);
        rows->push_back(header);

        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare("SELECT task_name, interval_days, duration_minutes "
                      "FROM model_maintenance_template "
                      "WHERE model_name = :m ORDER BY id ASC");
            q.bindValue(":m", *modelName);
            if (q.exec()) {
                while (q.next()) {
                    TaskRow *r = new TaskRow(scale, *deleteMode, rowsHost);
                    r->proc->setText(q.value(0).toString());
                    r->days->setText(q.value(1).toString());
                    r->mins->setText(q.value(2).toString());
                    if (!canEditModel) {
                        r->proc->setReadOnly(true);
                        r->days->setReadOnly(true);
                        r->mins->setReadOnly(true);
                        r->check->setVisible(false);
                    }
                    rowsLay->addWidget(r);
                    rows->push_back(r);
                    attachRowSignals(r);
                }
            } else {
                qDebug() << "load model template failed:" << q.lastError().text();
            }
        }

        rowsLay->addStretch();
        setTasksDirtyUi(false);
        *deleteMode = false;
        delModeBtn->setText("Удалить строки");
        cancelDeleteBtn->hide();
    };

    loadRowsFromDb();

    QHBoxLayout *toolbar = new QHBoxLayout();
    toolbar->setSpacing(scale(10));
    toolbar->addWidget(addRowBtn);
    toolbar->addStretch();
    toolbar->addWidget(cancelDeleteBtn);
    toolbar->addWidget(delModeBtn);
    tasksRoot->addLayout(toolbar);

    if (!canEditModel) {
        addRowBtn->hide();
        delModeBtn->hide();
    }

    QHBoxLayout *saveBar = new QHBoxLayout();
    saveBar->addWidget(cancelTasksBtn);
    saveBar->addStretch();
    saveBar->addWidget(saveTasksBtn);
    tasksRoot->addLayout(saveBar);

    connect(addRowBtn, &QPushButton::clicked, this, [=](){
        TaskRow *r = new TaskRow(scale, *deleteMode, rowsHost);
        rowsLay->insertWidget(rowsLay->count() - 1, r);
        rows->push_back(r);
        attachRowSignals(r);
        setTasksDirtyUi(true);
    });

    connect(delModeBtn, &QPushButton::clicked, this, [=](){
        if (!*deleteMode) {
            *deleteMode = true;
            delModeBtn->setText("Удалить выбранные");
            cancelDeleteBtn->show();
            for (int i = 1; i < rows->size(); ++i)
                (*rows)[i]->check->setVisible(true);
            return;
        }

        QVector<TaskRow*> keep;
        keep.push_back((*rows)[0]); // header
        for (int i = 1; i < rows->size(); ++i) {
            TaskRow *r = (*rows)[i];
            if (r->check->isChecked())
                r->deleteLater();
            else
                keep.push_back(r);
        }
        *rows = keep;
        *deleteMode = false;
        delModeBtn->setText("Удалить строки");
        cancelDeleteBtn->hide();
        for (int i = 1; i < rows->size(); ++i)
            (*rows)[i]->check->setVisible(false);
        setTasksDirtyUi(true);
    });

    connect(cancelDeleteBtn, &QPushButton::clicked, this, [=](){
        *deleteMode = false;
        delModeBtn->setText("Удалить строки");
        cancelDeleteBtn->hide();
        for (int i = 1; i < rows->size(); ++i)
            (*rows)[i]->check->setVisible(false);
    });

    connect(cancelTasksBtn, &QPushButton::clicked, this, [=](){
        loadRowsFromDb();
    });

    connect(saveTasksBtn, &QPushButton::clicked, this, [=](){
        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (!db.isValid() || !db.isOpen()) {
            showWarning("Модель AGV", "База данных не открыта.");
            return;
        }
        const bool txSupported = db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions);
        bool txStarted = false;
        if (txSupported) {
            txStarted = db.transaction();
            if (!txStarted)
                qDebug() << "ModelDetails template: transaction start failed, fallback to non-transaction mode:" << db.lastError().text();
        }

        QSqlQuery delQ(db);
        delQ.prepare("DELETE FROM model_maintenance_template WHERE model_name = :m");
        delQ.bindValue(":m", *modelName);
        if (!delQ.exec()) {
            if (txStarted)
                db.rollback();
            showWarning("Модель AGV", "Ошибка удаления старого шаблона: " + delQ.lastError().text());
            return;
        }

        for (int i = 1; i < rows->size(); ++i) {
            TaskRow *r = (*rows)[i];
            const QString taskName = r->proc->text().trimmed();
            const int d = r->days->text().toInt();
            const int m = r->mins->text().toInt();
            if (taskName.isEmpty() || d <= 0 || m <= 0)
                continue;

            QSqlQuery insQ(db);
            insQ.prepare("INSERT INTO model_maintenance_template "
                         "(model_name, task_name, task_description, interval_days, duration_minutes, is_default) "
                         "VALUES (:model, :name, :dsc, :days, :mins, 1)");
            insQ.bindValue(":model", *modelName);
            insQ.bindValue(":name", taskName);
            insQ.bindValue(":dsc", QString());
            insQ.bindValue(":days", d);
            insQ.bindValue(":mins", m);
            if (!insQ.exec()) {
                if (txStarted)
                    db.rollback();
                showWarning("Модель AGV", "Ошибка сохранения строки шаблона: " + insQ.lastError().text());
                return;
            }
        }

        if (txStarted) {
            if (!db.commit()) {
                db.rollback();
                showWarning("Модель AGV", "Не удалось сохранить шаблон.");
                return;
            }
        }

        loadRowsFromDb();
        showInfo("Модель AGV", "Шаблон ТО сохранён.");
    });

    root->addWidget(tasksFrame, 1);

    connect(backBtn, &QPushButton::clicked, this, &ModelDetailsPageWidget::backRequested);
}

// ====================== ModelListPage ======================
// (оставлен без изменений)



// ====================== ModelListPage ======================

ModelListPage::ModelListPage(std::function<int(int)> scale, QWidget *parent)
    : QFrame(parent)
    , s_(scale)
    , mode_(Mode_List)
    , backBtn_(nullptr)
    , deleteBtn_(nullptr)
    , titleLbl_(nullptr)
    , scrollArea_(nullptr)
    , contentWidget_(nullptr)
    , listLayout_(nullptr)
    , emptyLabel_(nullptr)
    , loadMoreModelsBtn_(nullptr)
    , addBtn_(nullptr)
    , templatePage_(nullptr)
    , detailsPage_(nullptr)
{
    setStyleSheet("background-color:#F1F2F4;border-radius:12px;");

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(s_(10), s_(10), s_(10), s_(10));
    root->setSpacing(s_(10));

    QHBoxLayout *hdr = new QHBoxLayout();

    backBtn_ = new QPushButton("   Назад", this);
    backBtn_->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    backBtn_->setIconSize(QSize(s_(24), s_(24)));
    backBtn_->setFixedSize(s_(150), s_(50));
    backBtn_->setStyleSheet(QString(
        "QPushButton { background-color:#E6E6E6; border-radius:%1px; border:1px solid #C8C8C8;"
        "font-family:Inter; font-size:%2px; font-weight:800; color:black; text-align:left; padding-left:%3px; }"
        "QPushButton:hover { background-color:#D5D5D5; }"
    ).arg(s_(10)).arg(s_(16)).arg(s_(10)));

    connect(backBtn_, &QPushButton::clicked, this, &ModelListPage::backRequested);

    hdr->addWidget(backBtn_);
    hdr->addStretch();

    titleLbl_ = new QLabel("Модели AGV", this);
    titleLbl_->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
    ).arg(s_(26)));
    hdr->addWidget(titleLbl_);
    hdr->addStretch();

    deleteBtn_ = new QPushButton("Удалить", this);
    deleteBtn_->setCursor(Qt::PointingHandCursor);
    deleteBtn_->setFixedSize(s_(165), s_(50));
    deleteBtn_->setStyleSheet(QString(
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
    ).arg(s_(10)).arg(s_(16)));

    connect(deleteBtn_, &QPushButton::clicked, this, &ModelListPage::openDeleteDialog);
    hdr->addWidget(deleteBtn_);

    root->addLayout(hdr);

    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setStyleSheet("border:none;background:transparent;");
    scrollArea_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    contentWidget_ = new QWidget(scrollArea_);
    listLayout_ = new QVBoxLayout(contentWidget_);
    listLayout_->setSpacing(s_(8));
    listLayout_->setContentsMargins(0,0,0,0);

    scrollArea_->setWidget(contentWidget_);
    root->addWidget(scrollArea_, 1);

    emptyLabel_ = new QLabel("Здесь ничего нет", this);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setStyleSheet(QString(
        "font-family:Inter;"
        "font-size:%1px;"
        "font-weight:900;"
        "color:#555;"
    ).arg(s_(28)));

    listLayout_->addStretch();
    listLayout_->addWidget(emptyLabel_, 0, Qt::AlignCenter);
    listLayout_->addStretch();

    addBtn_ = new QPushButton("+ Добавить модель", this);
    addBtn_->setFixedSize(s_(320), s_(50));
    addBtn_->raise();
    addBtn_->setStyleSheet(QString(
        "QPushButton { background-color:#0F00DB; border-radius:%1px; font-family:Inter; font-size:%2px; font-weight:800; color:white; }"
        "QPushButton:hover { background-color:#1A4ACD; }"
    ).arg(s_(10)).arg(s_(16)));

    connect(addBtn_, &QPushButton::clicked, this, &ModelListPage::openAddModelDialog);

    root->addWidget(addBtn_, 0, Qt::AlignHCenter);

    QString curRole = getUserRole(AppSession::currentUsername());
    bool canEditModels = (curRole == "admin" || curRole == "tech");
    if (!canEditModels) {
        addBtn_->hide();
        deleteBtn_->hide();
    }

    loadMoreModelsBtn_ = new QPushButton(QStringLiteral("Показать ещё 50"), this);
    loadMoreModelsBtn_->setFixedSize(s_(260), s_(44));
    loadMoreModelsBtn_->setStyleSheet(QString(
        "QPushButton { background-color:#E6E6E6; border:1px solid #C8C8C8; border-radius:%1px;"
        "font-family:Inter; font-size:%2px; font-weight:800; color:#1A1A1A; }"
        "QPushButton:hover { background-color:#D5D5D5; }"
    ).arg(s_(10)).arg(s_(15)));
    loadMoreModelsBtn_->hide();
    connect(loadMoreModelsBtn_, &QPushButton::clicked, this, [this]() {
        modelsShownCount_ = qMin(modelsShownCount_ + kModelsPageBatch, modelsAll_.size());
        rebuildModelsVisible();
    });

    reloadFromDatabase();
}

void ModelListPage::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    if (addBtn_) {
        int x = (width() - addBtn_->width()) / 2;
        int y = height() - addBtn_->height() - s_(20);
        addBtn_->move(x, y);
    }
}

void ModelListPage::clearList()
{
    if (!listLayout_)
        return;

    QLayoutItem *child;
    while ((child = listLayout_->takeAt(0)) != nullptr) {
        QWidget *w = child->widget();
        if (w && w != emptyLabel_ && w != loadMoreModelsBtn_)
            w->deleteLater();
        delete child;
    }
}

void ModelListPage::addModel(const ModelInfo &m)
{
    QFrame *item = new QFrame(contentWidget_);
    item->setObjectName("modelItem");
    item->setStyleSheet(
        "#modelItem{background:white;border-radius:10px;border:1px solid #E0E0E0;}"
        "#modelItem:hover{background:#F7F7F7;}"
    );

    QHBoxLayout *root = new QHBoxLayout(item);
    root->setContentsMargins(s_(12), s_(10), s_(12), s_(10));
    root->setSpacing(s_(10));

    QLabel *icon = new QLabel(item);
    icon->setFixedSize(s_(40), s_(40));
    icon->setPixmap(
        QPixmap(":/new/mainWindowIcons/noback/agvSetting.png")
            .scaled(icon->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );

    QVBoxLayout *textCol = new QVBoxLayout();
    textCol->setSpacing(s_(2));

    QLabel *title = new QLabel(m.name, item);
    title->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:800;color:#000;"
    ).arg(s_(16)));

    QString speedStr = m.maxSpeed > 0 ? QString::number(m.maxSpeed) : "—";
    QLabel *subtitle = new QLabel(
        QString("Грузоподъёмность: %1 кг   •   Скорость: %2 км/ч")
            .arg(m.capacityKg)
            .arg(speedStr),
        item
    );
    subtitle->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:700;color:#777;"
    ).arg(s_(13)));

    textCol->addWidget(title);
    textCol->addWidget(subtitle);

    QPushButton *showBtn = new QPushButton("Показать", item);
    showBtn->setFixedSize(s_(110), s_(36));
    showBtn->setStyleSheet(QString(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;"
        "font-size:%1px;font-weight:800;border-radius:%2px;}"
        "QPushButton:hover{background:#1A4ACD;}"
    ).arg(s_(13)).arg(s_(6)));

    connect(showBtn, &QPushButton::clicked, this, [this, m](){
        showModelDetailsMode(m);
    });

    root->addWidget(icon);
    root->addLayout(textCol);
    root->addStretch();
    root->addWidget(showBtn);

    listLayout_->addWidget(item);
}

void ModelListPage::rebuildModelsVisible()
{
    if (!listLayout_)
        return;

    clearList();

    if (modelsAll_.isEmpty()) {
        listLayout_->addStretch();
        listLayout_->addWidget(emptyLabel_, 0, Qt::AlignCenter);
        listLayout_->addStretch();
        emptyLabel_->show();
        if (loadMoreModelsBtn_)
            loadMoreModelsBtn_->hide();
        return;
    }

    emptyLabel_->hide();

    const int n = qMin(modelsShownCount_, modelsAll_.size());
    for (int i = 0; i < n; ++i)
        addModel(modelsAll_.at(i));

    if (modelsShownCount_ < modelsAll_.size()) {
        const int rem = modelsAll_.size() - modelsShownCount_;
        const int nextN = qMin(kModelsPageBatch, rem);
        loadMoreModelsBtn_->setText(QStringLiteral("Показать ещё %1").arg(nextN));
        loadMoreModelsBtn_->show();
        listLayout_->addWidget(loadMoreModelsBtn_, 0, Qt::AlignHCenter);
    } else if (loadMoreModelsBtn_) {
        loadMoreModelsBtn_->hide();
    }

    listLayout_->addStretch();
}

void ModelListPage::reloadFromDatabase()
{
    mode_ = Mode_List;

    if (templatePage_) {
        templatePage_->deleteLater();
        templatePage_ = nullptr;
    }
    if (detailsPage_) {
        detailsPage_->deleteLater();
        detailsPage_ = nullptr;
    }

    backBtn_->show();
    titleLbl_->show();
    QString curRole = getUserRole(AppSession::currentUsername());
    bool canEditModels = (curRole == "admin" || curRole == "tech");
    if (canEditModels) {
        deleteBtn_->show();
        addBtn_->show();
    } else {
        deleteBtn_->hide();
        addBtn_->hide();
    }

    modelsAll_ = loadModelList();
    modelsShownCount_ = qMin(kModelsPageBatch, modelsAll_.size());
    rebuildModelsVisible();
}

void ModelListPage::showTemplateMode(const ModelInfo &model)
{
    mode_ = Mode_Template;

    clearList();
    emptyLabel_->hide();

    backBtn_->hide();
    deleteBtn_->hide();
    titleLbl_->hide();
    addBtn_->hide();

    templatePage_ = new TemplatePageWidget(model, s_, contentWidget_);

    connect(templatePage_, &TemplatePageWidget::cancelRequested,
            this, [this](){
                reloadFromDatabase();
            });

    connect(templatePage_, &TemplatePageWidget::saveRequested,
            this, [this](const ModelInfo &model, const QVector<MaintenanceTask> &tasks){
                insertModelToDb(model);

                QSqlDatabase db = QSqlDatabase::database("main_connection");
                if (!db.isOpen()) {
                    qDebug() << "TemplatePageWidget: main_connection НЕ ОТКРЫТА!";
                } else {
                    for (const auto &t : tasks) {
                        QSqlQuery q(db);
                        q.prepare("INSERT INTO model_maintenance_template "
                                  "(model_name, task_name, task_description, interval_days, duration_minutes, is_default) "
                                  "VALUES (:m, :n, :dsc, :d, :min, :def)");
                        q.bindValue(":m",   model.name);
                        q.bindValue(":n",   t.name);
                        q.bindValue(":dsc", QString());
                        q.bindValue(":d",   t.intervalDays);
                        q.bindValue(":min", t.durationMinutes);
                        q.bindValue(":def", 1);
                        if (!q.exec()) {
                            qDebug() << "Ошибка вставки шаблона ТО:" << q.lastError().text();
                        }
                    }
                }

                reloadFromDatabase();
            });

    listLayout_->addWidget(templatePage_);
}

void ModelListPage::showModelDetailsMode(const ModelInfo &model)
{
    mode_ = Mode_Details;

    clearList();
    emptyLabel_->hide();

    backBtn_->hide();
    deleteBtn_->hide();
    titleLbl_->hide();
    addBtn_->hide();

    if (templatePage_) {
        templatePage_->deleteLater();
        templatePage_ = nullptr;
    }
    if (detailsPage_) {
        detailsPage_->deleteLater();
        detailsPage_ = nullptr;
    }

    detailsPage_ = new ModelDetailsPageWidget(model, s_, contentWidget_);
    connect(detailsPage_, &ModelDetailsPageWidget::backRequested,
            this, [this](){
                reloadFromDatabase();
            });

    listLayout_->addWidget(detailsPage_);
}

void ModelListPage::showListMode()
{
    reloadFromDatabase();
}

void ModelListPage::openAddModelDialog()
{
    AddModelDialog dlg(s_, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    auto res = dlg.result();

    if (res.useTemplate) {
        showTemplateMode(res.model);
        return;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.prepare("SELECT COUNT(*) FROM agv_models WHERE LOWER(name) = LOWER(:n)");
        q.bindValue(":n", res.model.name);
        q.exec();
        q.next();
        if (q.value(0).toInt() > 0) {
            qDebug() << "Модель уже существует";
            return;
        }
    }

    insertModelToDb(res.model);
    reloadFromDatabase();
}

void ModelListPage::openDeleteDialog()
{
    if (mode_ != Mode_List) {
        reloadFromDatabase();
        return;
    }

    QVector<ModelInfo> list = loadModelList();
    if (list.isEmpty())
        return;

    QDialog dlg(this);
    dlg.setWindowTitle("Удалить модели");
    dlg.setFixedSize(s_(460), s_(520));
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
    v->setContentsMargins(s_(18), s_(16), s_(18), s_(16));
    v->setSpacing(s_(10));

    QLabel *titleLbl = new QLabel("Удаление моделей", &dlg);
    titleLbl->setObjectName("title");
    v->addWidget(titleLbl);

    QLabel *selectedLbl = new QLabel("Выбрано: 0", &dlg);
    selectedLbl->setObjectName("subtitle");
    v->addWidget(selectedLbl);

    QFrame *listBox = new QFrame(&dlg);
    listBox->setObjectName("listBox");
    QVBoxLayout *listBoxLay = new QVBoxLayout(listBox);
    listBoxLay->setContentsMargins(s_(10), s_(10), s_(10), s_(10));
    listBoxLay->setSpacing(s_(8));

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
    listHostLay->setSpacing(s_(6));

    QVector<QCheckBox*> boxes;
    for (const ModelInfo &m : list) {
        QCheckBox *cb = new QCheckBox(m.name, listHost);
        cb->setProperty("model_name", m.name);
        boxes.push_back(cb);
        listHostLay->addWidget(cb);
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
            if (boxes[i]->isChecked())
                ++selectedCount;
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
        for (int i = 0; i < boxes.size(); ++i)
            boxes[i]->setChecked(targetState);
        updateSelectionUi();
    });

    for (int i = 0; i < boxes.size(); ++i) {
        connect(boxes[i], &QCheckBox::toggled, &dlg, [updateSelectionUi](bool checked){
            Q_UNUSED(checked)
            updateSelectionUi();
        });
    }

    updateSelectionUi();

    QPushButton *ok = new QPushButton("Удалить", &dlg);
    ok->setObjectName("ok");
    QPushButton *cancel = new QPushButton("Отмена", &dlg);
    cancel->setObjectName("cancel");

    QHBoxLayout *btns = new QHBoxLayout();
    btns->addWidget(cancel);
    btns->addStretch();
    btns->addWidget(ok);

    v->addLayout(btns);

    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QStringList selectedModelNames;
    for (QCheckBox *cb : boxes) {
        if (cb->isChecked()) {
            const QString modelName = cb->property("model_name").toString().trimmed();
            if (!modelName.isEmpty())
                selectedModelNames << modelName;
        }
    }

    if (selectedModelNames.isEmpty()) {
        QMessageBox::information(this, "Удаление моделей", "Выберите хотя бы одну модель.");
        return;
    }

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) {
        QMessageBox::warning(this, "Удаление моделей", "База данных не открыта.");
        return;
    }

    auto deleteModelsInChunks = [&](const QString &table, const QString &column) -> bool {
        const int chunkSize = 300;
        for (int offset = 0; offset < selectedModelNames.size(); offset += chunkSize) {
            const int count = qMin(chunkSize, selectedModelNames.size() - offset);
            QStringList placeholders;
            placeholders.reserve(count);
            for (int i = 0; i < count; ++i)
                placeholders << QString(":n%1").arg(i);

            QSqlQuery q(db);
            q.prepare(QString("DELETE FROM %1 WHERE %2 IN (%3)")
                      .arg(table, column, placeholders.join(",")));
            for (int i = 0; i < count; ++i)
                q.bindValue(QString(":n%1").arg(i), selectedModelNames[offset + i]);

            if (!q.exec()) {
                qDebug() << "Model batch delete failed for" << table << ":" << q.lastError().text();
                return false;
            }
        }
        return true;
    };

    const bool txSupported = db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions);
    bool txStarted = false;
    if (txSupported)
        txStarted = db.transaction();

    // Сначала шаблоны, потом сами модели.
    const bool deleted = deleteModelsInChunks("model_maintenance_template", "model_name")
                      && deleteModelsInChunks("agv_models", "name");
    if (!deleted) {
        if (txStarted)
            db.rollback();
        QMessageBox::warning(this, "Удаление моделей", "Не удалось удалить выбранные модели.");
        return;
    }

    if (txStarted && !db.commit()) {
        db.rollback();
        QMessageBox::warning(this, "Удаление моделей", "Ошибка сохранения удаления.");
        return;
    }

    reloadFromDatabase();
}
