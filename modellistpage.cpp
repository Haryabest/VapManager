#include "modellistpage.h"
#include "db_models.h"

struct MaintenanceTask {
    QString name;
    QString description;
    int intervalDays;
    int durationMinutes;
    bool isDefault;
};

// глобальные указатели для скрытия/показа шапки и кнопки добавления
static QPushButton *g_backBtn  = nullptr;
static QPushButton *g_delBtn   = nullptr;
static QPushButton *g_addBtn   = nullptr;
static QLabel      *g_titleLbl = nullptr;

//
// ============================================================================
//   ModelItemWidget — карточка модели
// ============================================================================
//

class ModelItemWidget : public QFrame
{
public:
    ModelItemWidget(const ModelInfo &info,
                    std::function<int(int)> scale,
                    QWidget *parent = nullptr)
        : QFrame(parent), m(info), s(scale)
    {
        setObjectName("modelItem");
        setStyleSheet(
            "#modelItem{background:white;border-radius:10px;border:1px solid #E0E0E0;}"
            "#modelItem:hover{background:#F7F7F7;}"
        );

        QHBoxLayout *root = new QHBoxLayout(this);
        root->setContentsMargins(s(12), s(10), s(12), s(10));
        root->setSpacing(s(10));

        QLabel *icon = new QLabel(this);
        icon->setFixedSize(s(40), s(40));
        icon->setPixmap(
            QPixmap(":/new/mainWindowIcons/noback/agvSetting.png")
                .scaled(icon->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );

        QVBoxLayout *textCol = new QVBoxLayout();
        textCol->setSpacing(s(2));

        QLabel *title = new QLabel(
            QString("%1 — %2").arg(m.name, m.category),
            this
        );
        title->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:800;color:#000;"
        ).arg(s(16)));

        QLabel *subtitle = new QLabel(
            QString("Грузоподъёмность: %1 кг   •   Скорость: %2 км/ч")
                .arg(m.capacityKg)
                .arg(m.maxSpeed),
            this
        );
        subtitle->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:700;color:#777;"
        ).arg(s(13)));

        textCol->addWidget(title);
        textCol->addWidget(subtitle);

        QPushButton *showBtn = new QPushButton("Показать", this);
        showBtn->setFixedSize(s(110), s(36));
        showBtn->setStyleSheet(QString(
            "QPushButton{background:#0F00DB;color:white;font-family:Inter;"
            "font-size:%1px;font-weight:800;border-radius:%2px;}"
            "QPushButton:hover{background:#1A4ACD;}"
        ).arg(s(13)).arg(s(6)));

        connect(showBtn, &QPushButton::clicked, this, [this](){
            qDebug() << "[ModelItemWidget] Показать модель:" << m.name;
        });

        root->addWidget(icon);
        root->addLayout(textCol);
        root->addWidget(showBtn);
    }

private:
    ModelInfo m;
    std::function<int(int)> s;
};

//
// ============================================================================
//   AddModelDialog — диалог добавления модели + кнопка шаблона ТО
// ============================================================================
//

class AddModelDialog : public QDialog
{
public:
    struct Result {
        ModelInfo model;
    } result;

    bool useTemplate = false;

    AddModelDialog(std::function<int(int)> scale, QWidget *parent = nullptr)
        : QDialog(parent), s(scale)
    {
        setWindowTitle("Добавить модель AGV");
        setFixedSize(s(420), s(520));

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(s(20), s(20), s(20), s(20));
        root->setSpacing(s(12));

        auto makeTitle = [&](QString t){
            QLabel *lbl = new QLabel(t, this);
            lbl->setStyleSheet("font-family:Inter;font-size:15px;font-weight:600;color:#222;");
            return lbl;
        };

        auto makeError = [&](){
            QLabel *e = new QLabel(this);
            e->setStyleSheet("color:#D00000;font-size:13px;font-family:Inter;margin-top:2px;");
            e->hide();
            return e;
        };

        auto styleEdit = [&] (QLineEdit *e){
            e->setStyleSheet(
                "QLineEdit{border:1px solid #C8C8C8;border-radius:6px;padding:4px;font-family:Inter;font-size:14px;}"
                "QLineEdit:focus{border:1px solid #0F00DB;}"
            );
        };

        root->addWidget(makeTitle("Название модели"));
        nameEdit = new QLineEdit(this);
        styleEdit(nameEdit);
        root->addWidget(nameEdit);
        errName = makeError();
        root->addWidget(errName);

        root->addWidget(makeTitle("Категория"));
        categoryEdit = new QLineEdit(this);
        styleEdit(categoryEdit);
        root->addWidget(categoryEdit);
        errCat = makeError();
        root->addWidget(errCat);

        root->addWidget(makeTitle("Грузоподъёмность (кг)"));
        capacityEdit = new QLineEdit(this);
        styleEdit(capacityEdit);
        root->addWidget(capacityEdit);
        errCap = makeError();
        root->addWidget(errCap);

        root->addWidget(makeTitle("Макс. скорость (км/ч)"));
        speedEdit = new QLineEdit(this);
        styleEdit(speedEdit);
        root->addWidget(speedEdit);
        errSpeed = makeError();
        root->addWidget(errSpeed);

        QPushButton *tplBtn = new QPushButton("Использовать шаблон ТО", this);
        tplBtn->setStyleSheet(
            "QPushButton{background:#E6E6E6;border-radius:10px;font-family:Inter;"
            "font-size:15px;font-weight:800;color:#333;padding:8px 12px;}"
            "QPushButton:hover{background:#D5D5D5;}"
        );
        root->addWidget(tplBtn);

        connect(tplBtn, &QPushButton::clicked, this, [this](){
            useTemplate = true;
            accept();
        });

        root->addStretch();

        QHBoxLayout *btns = new QHBoxLayout();
        QPushButton *cancel = new QPushButton("Отмена", this);
        QPushButton *ok = new QPushButton("Добавить", this);

        cancel->setMinimumHeight(s(40));
        ok->setMinimumHeight(s(40));

        cancel->setStyleSheet(
            "QPushButton{background:#F7F7F7;border-radius:10px;border:1px solid #C8C8C8;"
            "font-family:Inter;font-size:15px;font-weight:700;padding:8px 16px;}"
            "QPushButton:hover{background:#EDEDED;}"
        );
        ok->setStyleSheet(
            "QPushButton{background:#28A745;color:white;border-radius:10px;"
            "font-family:Inter;font-size:15px;font-weight:800;padding:8px 20px;}"
            "QPushButton:hover{background:#2EC24F;}"
        );

        btns->addWidget(cancel);
        btns->addStretch();
        btns->addWidget(ok);

        root->addLayout(btns);

        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

        connect(ok, &QPushButton::clicked, this, [this](){
            if (!validateAll()) return;

            result.model.name = nameEdit->text().trimmed();
            result.model.category = categoryEdit->text().trimmed();
            result.model.capacityKg = capacityEdit->text().toInt();
            result.model.maxSpeed = speedEdit->text().toDouble();

            accept();
        });
    }

private:
    bool validateAll()
    {
        bool ok = true;

        if (nameEdit->text().trimmed().isEmpty()) {
            errName->setText("Введите название");
            errName->show();
            ok = false;
        } else errName->hide();

        // категория не обязательна
        errCat->hide();

        bool okCap;
        capacityEdit->text().toInt(&okCap);
        if (!okCap && !capacityEdit->text().trimmed().isEmpty()) {
            errCap->setText("Введите число");
            errCap->show();
            ok = false;
        } else errCap->hide();

        bool okSpeed;
        speedEdit->text().toDouble(&okSpeed);
        if (!okSpeed && !speedEdit->text().trimmed().isEmpty()) {
            errSpeed->setText("Введите число");
            errSpeed->show();
            ok = false;
        } else errSpeed->hide();

        return ok;
    }

private:
    std::function<int(int)> s;

    QLineEdit *nameEdit;
    QLineEdit *categoryEdit;
    QLineEdit *capacityEdit;
    QLineEdit *speedEdit;

    QLabel *errName;
    QLabel *errCat;
    QLabel *errCap;
    QLabel *errSpeed;
};

//
// ============================================================================
//   TemplatePageWidget — страница шаблона ТО
// ============================================================================
//

class TemplatePageWidget : public QWidget
{
public:
    std::function<void(const ModelInfo&, const QVector<MaintenanceTask>&)> onSave;
    std::function<void()> onCancel;

    TemplatePageWidget(std::function<int(int)> scale, QWidget *parent = nullptr)
        : QWidget(parent), s(scale)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // заголовок
        QLabel *title = new QLabel("Создание модели по шаблону ТО", this);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(QString(
            "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
        ).arg(s(32)));
        root->addWidget(title);

        // блок полей модели
        QWidget *modelBlock = new QWidget(this);
        QGridLayout *modelGrid = new QGridLayout(modelBlock);
        modelGrid->setHorizontalSpacing(s(10));
        modelGrid->setVerticalSpacing(s(8));
        modelGrid->setContentsMargins(s(10), s(10), s(10), s(10));

        auto styleEdit = [&] (QLineEdit *e){
            e->setStyleSheet(
                "QLineEdit{border:1px solid #C8C8C8;border-radius:6px;padding:4px;font-family:Inter;font-size:14px;}"
                "QLineEdit:focus{border:1px solid #0F00DB;}"
            );
        };

        QLabel *nameLbl = new QLabel("Название модели *", modelBlock);
        QLabel *catLbl  = new QLabel("Категория", modelBlock);
        QLabel *capLbl  = new QLabel("Грузоподъёмность (кг)", modelBlock);
        QLabel *spdLbl  = new QLabel("Макс. скорость (км/ч)", modelBlock);

        nameLbl->setStyleSheet("font-family:Inter;font-size:14px;font-weight:600;color:#222;");
        catLbl->setStyleSheet("font-family:Inter;font-size:14px;font-weight:600;color:#222;");
        capLbl->setStyleSheet("font-family:Inter;font-size:14px;font-weight:600;color:#222;");
        spdLbl->setStyleSheet("font-family:Inter;font-size:14px;font-weight:600;color:#222;");

        nameEdit = new QLineEdit(modelBlock);
        catEdit  = new QLineEdit(modelBlock);
        capEdit  = new QLineEdit(modelBlock);
        spdEdit  = new QLineEdit(modelBlock);

        styleEdit(nameEdit);
        styleEdit(catEdit);
        styleEdit(capEdit);
        styleEdit(spdEdit);

        modelGrid->addWidget(nameLbl, 0, 0);
        modelGrid->addWidget(nameEdit, 0, 1);
        modelGrid->addWidget(catLbl,  1, 0);
        modelGrid->addWidget(catEdit, 1, 1);
        modelGrid->addWidget(capLbl,  2, 0);
        modelGrid->addWidget(capEdit, 2, 1);
        modelGrid->addWidget(spdLbl,  3, 0);
        modelGrid->addWidget(spdEdit, 3, 1);

        root->addWidget(modelBlock);

        // scroll area со списком работ
        scroll = new QScrollArea(this);
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet("border:none;background:transparent;");
        scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        QWidget *content = new QWidget(scroll);
        listLayout = new QVBoxLayout(content);
        listLayout->setSpacing(s(8));
        listLayout->setContentsMargins(s(10), 0, s(10), 0);

        scroll->setWidget(content);
        root->addWidget(scroll, 1); // занимает всё оставшееся пространство

        loadDefaultTasks();

        // кнопка "Добавить свою работу" — над нижними кнопками, но вне скролла
        QWidget *addBlock = new QWidget(this);
        QHBoxLayout *addLay = new QHBoxLayout(addBlock);
        addLay->setContentsMargins(s(10), s(8), s(10), s(4));
        addLay->setSpacing(0);

        QPushButton *addCustom = new QPushButton("Добавить свою работу", addBlock);
        addCustom->setStyleSheet(
            "QPushButton{background:#E6E6E6;border-radius:8px;font-weight:700;"
            "font-family:Inter;font-size:14px;padding:8px 12px;}"
            "QPushButton:hover{background:#D5D5D5;}"
        );
        addLay->addWidget(addCustom, 0, Qt::AlignLeft);
        addLay->addStretch();

        connect(addCustom, &QPushButton::clicked, this, [this](){
            if (!rows.isEmpty()) {
                Row last = rows.last();
                if (!last.isDefault) {
                    if (last.name->text().trimmed().isEmpty() ||
                        last.desc->text().trimmed().isEmpty() ||
                        last.minutes->text().trimmed().isEmpty())
                        return;
                }
            }
            addCustomTaskRow();
        });

        root->addWidget(addBlock);

        // нижние кнопки — всегда внизу окна
        QWidget *btnBlock = new QWidget(this);
        QHBoxLayout *btns = new QHBoxLayout(btnBlock);
        btns->setContentsMargins(s(10), s(10), s(10), s(16));
        btns->setSpacing(s(10));

        QPushButton *cancel = new QPushButton("Отмена", btnBlock);
        QPushButton *save = new QPushButton("Сохранить модель и шаблон", btnBlock);

        cancel->setMinimumHeight(s(48));
        save->setMinimumHeight(s(48));

        cancel->setStyleSheet(
            "QPushButton{background:#F7F7F7;border-radius:10px;border:1px solid #C8C8C8;"
            "font-family:Inter;font-size:16px;font-weight:700;padding:10px 18px;}"
            "QPushButton:hover{background:#EDEDED;}"
        );

        save->setStyleSheet(
            "QPushButton{background:#28A745;color:white;border-radius:10px;"
            "font-family:Inter;font-size:16px;font-weight:800;padding:10px 22px;}"
            "QPushButton:hover{background:#2EC24F;}"
        );

        connect(cancel, &QPushButton::clicked, this, [this](){
            if (onCancel) onCancel();
        });

        connect(save, &QPushButton::clicked, this, [this](){
            ModelInfo model;
            model.name = nameEdit->text().trimmed();
            model.category = catEdit->text().trimmed();
            model.capacityKg = capEdit->text().trimmed().isEmpty() ? 0 : capEdit->text().toInt();
            model.maxSpeed = spdEdit->text().trimmed().isEmpty() ? 0.0 : spdEdit->text().toDouble();

            if (model.name.isEmpty())
                return;

            QVector<MaintenanceTask> tasks;
            for (const Row &r : rows) {
                MaintenanceTask t;
                t.name = r.name->text().trimmed();
                t.description = r.desc->isVisible() ? r.desc->text().trimmed() : QString();
                t.intervalDays = r.intervalDays;
                t.durationMinutes = r.minutes->text().toInt();
                t.isDefault = r.isDefault;

                if (t.name.isEmpty())
                    continue;

                tasks.push_back(t);
            }

            if (onSave) onSave(model, tasks);
        });

        btns->addWidget(cancel);
        btns->addStretch();
        btns->addWidget(save);

        root->addWidget(btnBlock);
    }

private:
    std::function<int(int)> s;

    QScrollArea *scroll = nullptr;
    QVBoxLayout *listLayout = nullptr;

    QLineEdit *nameEdit = nullptr;
    QLineEdit *catEdit = nullptr;
    QLineEdit *capEdit = nullptr;
    QLineEdit *spdEdit = nullptr;

    struct Row {
        QLineEdit *name;
        QLineEdit *desc;
        QLineEdit *minutes;
        int intervalDays;
        bool isDefault;
    };

    QVector<Row> rows;

    void limitWords(QLineEdit *edit, int maxWords)
    {
        QObject::connect(edit, &QLineEdit::textChanged, edit, [edit, maxWords](){
            QString t = edit->text();
            QStringList parts = t.split(' ', Qt::SkipEmptyParts);
            if (parts.size() > maxWords) {
                parts = parts.mid(0, maxWords);
                edit->setText(parts.join(' '));
                edit->setCursorPosition(edit->text().length());
            }
        });
    }

    void loadDefaultTasks()
    {
        QMap<QString, QPair<QString, QString>> mapDefaultTO;

        mapDefaultTO["Корпус - очистка от загрязнений"] = {"30", "20"};
        mapDefaultTO["Корпус - контроль резбовых соединений, протяжка"] =  {"30", "90"};
        mapDefaultTO["Позиционирующие ролики - проверить на наличие люфтов"] = {"30", "20"};

        mapDefaultTO["Сканер безопасности - очистка сканера безопасности"] = {"30", "20"};
        mapDefaultTO["Сканер безопасности - проверка крепления кабеля сканера"] = {"30", "30"};

        mapDefaultTO["Бампер - контроль резбовых соединений, протяжка"] = {"30", "90"};
        mapDefaultTO["Лицевая панель - контроль целостности защиты панели"] = {"30", "20"};

        mapDefaultTO["Электрика - проверка работы звука"] = {"30", "15"};
        mapDefaultTO["Электрика - Контроль целостности кнопок и панели"] = {"30", "20"};

        mapDefaultTO["PIN - Очистка (c разборкой)"] = {"90", "120"};
        mapDefaultTO["PIN - Контроль резбовых соединений, протяжка"] = {"30", "60"};
        mapDefaultTO["PIN - Осмотр, контроль люфтов"] = {"30", "60"};
        mapDefaultTO["PIN - Проверить укладку проводов"] = {"30", "30"};
        mapDefaultTO["PIN - Проверить работу"] = {"30", "15"};
        mapDefaultTO["PIN - Контроль верхнего положения -> max 3мм. не доходит"] = {"30", "30"};
        mapDefaultTO["PIN - Смазка"] = {"30", "20"};

        mapDefaultTO["Подъемник - Проверить устан. штифты на кулачке"] = {"30", "30"};
        mapDefaultTO["Подъемник - Контроль резбовых соединений, протяжка"] = {"30", "60"};
        mapDefaultTO["Подъемник - Проверить крепление опорных подшипников"] = {"30", "20"};
        mapDefaultTO["Подъемник - Проверить работу подъемника"] = {"30", "15"};
        mapDefaultTO["Подъемник - Проверка срабат. концевиков (настройка)"] = {"30", "20"};

        mapDefaultTO["Drive unit - Проверить крепление поъемной втулки"] = {"30", "30"};
        mapDefaultTO["Drive unit - Контроль резбовых соединений, протяжка"] = {"30", "60"};
        mapDefaultTO["Drive unit - Проверка укладки и целостности кабеля"] = {"30", "30"};
        mapDefaultTO["Drive unit - Осмотр контроль люфтов"] = {"30", "30"};
        mapDefaultTO["Drive unit - Контроль натяжения цепей"] = {"90", "60"};
        mapDefaultTO["Drive unit - Смазка цепей"] = {"30", "30"};

        mapDefaultTO["Датчик трека и RFID - Проверить крепление"] = {"30", "20"};
        mapDefaultTO["Датчик трека и RFID - Проверить целостность"] = {"30", "15"};
        mapDefaultTO["Датчик трека и RFID - Проверить целостность и укладку кабелей"] = {"30", "30"};
        mapDefaultTO["Датчик трека и RFID - Проверить крепление и наличие защиты"] = {"30", "30"};
        mapDefaultTO["Датчик трека и RFID - Высота от пола -> не более 20 мм."] = {"30", "30"};

        mapDefaultTO["Колеса приводные - Контроль диаметра наружного -> min Ø 145"] = {"30", "15"};
        mapDefaultTO["Колеса приводные - Проверить продольный люфт"] = {"30", "15"};
        mapDefaultTO["Колеса приводные - Проверить крепление крышки колеса -> до упора, с LOCTITE 243"] = {"30", "30"};

        mapDefaultTO["Приводные звездочки - Очистка от мусора"] = {"30", "20"};
        mapDefaultTO["Приводные звездочки - Проверить устан. штифты на звездах"] = {"30", "30"};
        mapDefaultTO["Приводные звездочки - Проверить крепление крышки звездочки -> до упора, с LOCTITE 243"] = {"30", "30"};

        mapDefaultTO["Колеса поворотные - Очистка от загрязнений"] = {"30", "20"};
        mapDefaultTO["Колеса поворотные - Проверить продольный люфт"] = {"30", "20"};

        mapDefaultTO["Колеса задние - Проверить продольный люфт"] = {"30", "20"};

        for (auto it = mapDefaultTO.begin(); it != mapDefaultTO.end(); ++it)
            addDefaultTaskRow(it.key(), it.value().first, it.value().second);
    }

    void addDefaultTaskRow(const QString &name, const QString &days, const QString &minutes)
    {
        QWidget *rowWidget = new QWidget(this);
        QHBoxLayout *h = new QHBoxLayout(rowWidget);
        h->setSpacing(s(8));
        h->setContentsMargins(0,0,0,0);

        QLineEdit *nameEdit = new QLineEdit(name, rowWidget);
        QLineEdit *descEdit = new QLineEdit(rowWidget);
        QLineEdit *minEdit  = new QLineEdit(minutes, rowWidget);

        nameEdit->setReadOnly(true);
        descEdit->setVisible(false); // у шаблонов описания нет

        nameEdit->setStyleSheet(
            "QLineEdit{border:none;font-family:Inter;font-size:14px;font-weight:600;color:#222;}"
        );
        descEdit->setStyleSheet(
            "QLineEdit{border:1px solid #C8C8C8;border-radius:6px;padding:4px;font-family:Inter;font-size:14px;}"
            "QLineEdit:focus{border:1px solid #0F00DB;}"
        );
        minEdit->setStyleSheet(
            "QLineEdit{border:1px solid #C8C8C8;border-radius:6px;padding:4px;font-family:Inter;font-size:14px;}"
            "QLineEdit:focus{border:1px solid #0F00DB;}"
        );

        minEdit->setFixedWidth(s(70));

        h->addWidget(nameEdit, 3);
        h->addWidget(descEdit, 2);
        h->addWidget(minEdit, 0);

        listLayout->addWidget(rowWidget);

        Row r;
        r.name = nameEdit;
        r.desc = descEdit;
        r.minutes = minEdit;
        r.intervalDays = days.toInt();
        r.isDefault = true;

        rows.push_back(r);
    }

    void addCustomTaskRow()
    {
        QWidget *rowWidget = new QWidget(this);
        QHBoxLayout *h = new QHBoxLayout(rowWidget);
        h->setSpacing(s(8));
        h->setContentsMargins(0,0,0,0);

        QLineEdit *nameEdit = new QLineEdit(rowWidget);
        QLineEdit *descEdit = new QLineEdit(rowWidget);
        QLineEdit *minEdit  = new QLineEdit(rowWidget);

        nameEdit->setPlaceholderText("Название (до 2 слов)");
        descEdit->setPlaceholderText("Описание (до 10 слов)");
        minEdit->setPlaceholderText("Минут");

        nameEdit->setStyleSheet(
            "QLineEdit{border:1px solid #C8C8C8;border-radius:6px;padding:4px;font-family:Inter;font-size:14px;}"
            "QLineEdit:focus{border:1px solid #0F00DB;}"
        );
        descEdit->setStyleSheet(
            "QLineEdit{border:1px solid #C8C8C8;border-radius:6px;padding:4px;font-family:Inter;font-size:14px;}"
            "QLineEdit:focus{border:1px solid #0F00DB;}"
        );
        minEdit->setStyleSheet(
            "QLineEdit{border:1px solid #C8C8C8;border-radius:6px;padding:4px;font-family:Inter;font-size:14px;}"
            "QLineEdit:focus{border:1px solid #0F00DB;}"
        );

        minEdit->setFixedWidth(s(70));

        limitWords(nameEdit, 2);
        limitWords(descEdit, 10);

        h->addWidget(nameEdit, 2);
        h->addWidget(descEdit, 3);
        h->addWidget(minEdit, 0);

        listLayout->addWidget(rowWidget);

        Row r;
        r.name = nameEdit;
        r.desc = descEdit;
        r.minutes = minEdit;
        r.intervalDays = 0; // для кастомных дней нет
        r.isDefault = false;

        rows.push_back(r);
        QTimer::singleShot(0, this, [this](){
            if (scroll && scroll->verticalScrollBar())
                scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        });

    }
};

//
// ============================================================================
//   ModelListPage — основной класс
// ============================================================================
//

ModelListPage::ModelListPage(std::function<int(int)> scale, QWidget *parent)
    : QFrame(parent), s(scale)
{
    setStyleSheet("background-color:#F1F2F4;border-radius:12px;");

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(s(10), s(10), s(10), s(10));
    root->setSpacing(s(10));

    QHBoxLayout *hdr = new QHBoxLayout();

    QPushButton *back = new QPushButton("   Назад", this);
    back->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    back->setIconSize(QSize(s(24), s(24)));
    back->setFixedSize(s(150), s(50));
    back->setStyleSheet(QString(
        "QPushButton { background-color:#E6E6E6; border-radius:%1px; border:1px solid #C8C8C8;"
        "font-family:Inter; font-size:%2px; font-weight:800; color:black; text-align:left; padding-left:%3px; }"
        "QPushButton:hover { background-color:#D5D5D5; }"
    ).arg(s(10)).arg(s(16)).arg(s(10)));

    connect(back, &QPushButton::clicked, this, &ModelListPage::backRequested);

    hdr->addWidget(back);
    hdr->addStretch();

    QLabel *title = new QLabel("Модели AGV", this);
    title->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
    ).arg(s(26)));
    hdr->addWidget(title);
    hdr->addStretch();

    QPushButton *del = new QPushButton("Удалить", this);
    del->setFixedSize(s(130), s(40));
    del->setStyleSheet(QString(
        "QPushButton{ background-color:#FF3B30; border-radius:%1px; font-family:Inter; font-size:%2px; font-weight:800; color:white; }"
        "QPushButton:hover{background-color:#E13228;}"
    ).arg(s(10)).arg(s(16)));

    connect(del, &QPushButton::clicked, this, &ModelListPage::openDeleteDialog);
    hdr->addWidget(del);

    root->addLayout(hdr);

    g_backBtn  = back;
    g_delBtn   = del;
    g_titleLbl = title;

    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("border:none;background:transparent;");
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    contentWidget = new QWidget(scrollArea);
    layout = new QVBoxLayout(contentWidget);
    layout->setSpacing(s(8));
    layout->setContentsMargins(0,0,0,0);

    scrollArea->setWidget(contentWidget);
    root->addWidget(scrollArea, 1);

    emptyLabel = new QLabel("Здесь ничего нет", this);
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

    addBtn = new QPushButton("+ Добавить модель", this);
    addBtn->setFixedSize(s(320), s(50));
    addBtn->raise();

    addBtn->setStyleSheet(QString(
        "QPushButton { background-color:#0F00DB; border-radius:%1px; font-family:Inter; font-size:%2px; font-weight:800; color:white; }"
        "QPushButton:hover { background-color:#1A4ACD; }"
    ).arg(s(10)).arg(s(16)));

    connect(addBtn, &QPushButton::clicked, this, &ModelListPage::openAddModelDialog);

    g_addBtn = addBtn;

    mode = MODE_LIST;
    templatePage = nullptr;

    reloadFromDatabase();
}

void ModelListPage::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);

    int x = (width() - addBtn->width()) / 2;
    int y = height() - addBtn->height() - s(20);
    addBtn->move(x, y);
}

void ModelListPage::openAddModelDialog()
{
    AddModelDialog dlg(s, this);
    dlg.exec();

    if (dlg.useTemplate) {
        showTemplatePage();
        return;
    }

    if (!dlg.result.model.name.isEmpty()) {
        insertModelToDb(dlg.result.model);
        reloadFromDatabase();
    }
}

void ModelListPage::addModel(const ModelInfo &info)
{
    emptyLabel->hide();

    ModelItemWidget *item = new ModelItemWidget(info, s, this);
    layout->insertWidget(layout->count() - 1, item);
}

void ModelListPage::reloadFromDatabase()
{
    mode = MODE_LIST;

    if (g_backBtn)  g_backBtn->show();
    if (g_delBtn)   g_delBtn->show();
    if (g_titleLbl) g_titleLbl->show();
    if (g_addBtn)   g_addBtn->show();

    QLayoutItem *child;
    while ((child = layout->takeAt(0)) != nullptr) {
        QWidget *w = child->widget();
        if (w && w != emptyLabel)
            w->deleteLater();
        delete child;
    }

    QVector<ModelInfo> list = loadModelList();

    if (list.isEmpty()) {
        layout->addStretch();
        layout->addWidget(emptyLabel, 0, Qt::AlignCenter);
        layout->addStretch();
        emptyLabel->show();
        return;
    }

    emptyLabel->hide();

    for (const ModelInfo &m : list)
        addModel(m);

    layout->addStretch();
}

void ModelListPage::showTemplatePage()
{
    mode = MODE_TEMPLATE;

    if (g_backBtn)  g_backBtn->hide();
    if (g_delBtn)   g_delBtn->hide();
    if (g_titleLbl) g_titleLbl->hide();
    if (g_addBtn)   g_addBtn->hide();

    QLayoutItem *child;
    while ((child = layout->takeAt(0)) != nullptr) {
        QWidget *w = child->widget();
        if (w && w != emptyLabel)
            w->deleteLater();
        delete child;
    }

    emptyLabel->hide();

    templatePage = new TemplatePageWidget(s, contentWidget);

    templatePage->onCancel = [this](){
        reloadFromDatabase();
    };

    templatePage->onSave = [this](const ModelInfo &model, const QVector<MaintenanceTask> &tasks){
        insertModelToDb(model);

        QSqlDatabase db = QSqlDatabase::database("main_connection");
        for (const auto &t : tasks) {
            QSqlQuery q(db);
            q.prepare("INSERT INTO model_maintenance_template "
                      "(model_name, task_name, task_description, interval_days, duration_minutes, is_default) "
                      "VALUES (:m, :n, :dsc, :d, :min, :def)");
            q.bindValue(":m", model.name);
            q.bindValue(":n", t.name);
            q.bindValue(":dsc", t.description);
            q.bindValue(":d", t.intervalDays);
            q.bindValue(":min", t.durationMinutes);
            q.bindValue(":def", t.isDefault ? 1 : 0);
            if (!q.exec()) {
                qDebug() << "Ошибка вставки шаблона ТО:" << q.lastError().text();
            }
        }

        reloadFromDatabase();
    };

    layout->addWidget(templatePage);

}

void ModelListPage::openDeleteDialog()
{
    if (mode != MODE_LIST) {
        reloadFromDatabase();
        return;
    }

    QVector<ModelInfo> list = loadModelList();
    if (list.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Удалить модели");
    dlg.setFixedSize(s(360), s(420));

    QVBoxLayout *v = new QVBoxLayout(&dlg);

    QLabel *lbl = new QLabel("Выберите модели:", &dlg);
    v->addWidget(lbl);

    QVector<QCheckBox*> boxes;
    for (const ModelInfo &m : list) {
        QCheckBox *cb = new QCheckBox(m.name, &dlg);
        boxes.push_back(cb);
        v->addWidget(cb);
    }

    v->addStretch();

    QPushButton *ok = new QPushButton("Удалить", &dlg);
    QPushButton *cancel = new QPushButton("Отмена", &dlg);

    ok->setStyleSheet(
        "QPushButton{background:#FF3B30;color:white;border-radius:8px;font-family:Inter;font-size:14px;font-weight:800;padding:6px 14px;}"
        "QPushButton:hover{background:#E13228;}"
    );
    cancel->setStyleSheet(
        "QPushButton{background:#F7F7F7;border-radius:8px;border:1px solid #C8C8C8;font-family:Inter;font-size:14px;font-weight:600;padding:6px 12px;}"
        "QPushButton:hover{background:#EDEDED;}"
    );

    QHBoxLayout *btns = new QHBoxLayout();
    btns->addWidget(cancel);
    btns->addStretch();
    btns->addWidget(ok);

    v->addLayout(btns);

    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted)
        return;

    for (QCheckBox *cb : boxes)
        if (cb->isChecked())
            deleteModelByName(cb->text());

    reloadFromDatabase();
}
