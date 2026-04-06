#include "leftmenu_settings_dialogs.h"
#include "leftmenu_calendar_utils.h"
#include "db.h"

#include <QApplication>
#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

namespace {

class CalendarSettingsDialog : public QDialog {
public:
    explicit CalendarSettingsDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Настройки календаря");
        setModal(true);
        setFixedSize(400, 340);
        setObjectName("calendarSettingsDialog");

        setStyleSheet(
            "QDialog#calendarSettingsDialog{"
            "  background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #F7FAFF,stop:1 #EEF4FF);"
            "}"
            "QLabel#title{"
            "  font-family:Inter;font-size:22px;font-weight:900;color:#1F2937;"
            "  background:transparent;"
            "}"
            "QLabel#subtitle{"
            "  font-family:Inter;font-size:12px;font-weight:500;color:#6B7280;"
            "  background:transparent;"
            "}"
            "QLabel[class=\"field\"]{"
            "  font-family:Inter;font-size:13px;font-weight:700;color:#374151;"
            "  background:transparent;"
            "}"
            "QComboBox{"
            "  background:#FFFFFF;border:1px solid #C7D2FE;border-radius:10px;"
            "  padding:7px 12px;font-family:Inter;font-size:13px;color:#111827;min-height:20px;"
            "}"
            "QComboBox:hover{border:1px solid #8EA2FF;}"
            "QComboBox:focus{border:2px solid #1976FF;padding:6px 11px;}"
            "QComboBox::drop-down{border:none;width:20px;}"
            "QComboBox QAbstractItemView{"
            "  background:#FFFFFF;border:1px solid #C7D2FE;selection-background-color:#DDE8FF;"
            "}"
            "QPushButton#okBtn{"
            "  background:#1976FF;color:white;border:none;border-radius:10px;"
            "  padding:8px 16px;font-family:Inter;font-size:13px;font-weight:800;"
            "}"
            "QPushButton#okBtn:hover{background:#0F66EA;}"
            "QPushButton#cancelBtn{"
            "  background:#FFFFFF;color:#374151;border:1px solid #CBD5E1;border-radius:10px;"
            "  padding:8px 16px;font-family:Inter;font-size:13px;font-weight:700;"
            "}"
            "QPushButton#cancelBtn:hover{background:#F3F4F6;}"
        );

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(10);

        QLabel *title = new QLabel("Настройки календаря", this);
        title->setObjectName("title");
        title->setAutoFillBackground(false);
        QLabel *subtitle = new QLabel("Выберите месяц и конкретный день или неделю", this);
        subtitle->setObjectName("subtitle");
        subtitle->setAutoFillBackground(false);
        subtitle->setWordWrap(true);
        layout->addWidget(title);
        layout->addWidget(subtitle);

        yearBox_ = new QComboBox(this);
        monthBox_ = new QComboBox(this);
        weekBox_ = new QComboBox(this);
        dayBox_ = new QComboBox(this);

        for (int y = LeftMenuCalendar::minYear(); y <= LeftMenuCalendar::maxYear(); ++y)
            yearBox_->addItem(QString::number(y), y);

        const QStringList months = {
            "Январь","Февраль","Март","Апрель","Май","Июнь",
            "Июль","Август","Сентябрь","Октябрь","Ноябрь","Декабрь"
        };
        for (int i = 0; i < months.size(); ++i)
            monthBox_->addItem(months[i], i + 1);

        weekBox_->addItem("—", 0);
        for (int w = 1; w <= 4; ++w)
            weekBox_->addItem(QString("Неделя %1").arg(w), w);

        dayBox_->addItem(QStringLiteral("—"), 0);

        auto mkFieldLabel = [this](const QString &text) {
            QLabel *lbl = new QLabel(text, this);
            lbl->setProperty("class", "field");
            lbl->setAutoFillBackground(false);
            return lbl;
        };

        QFrame *card = new QFrame(this);
        card->setStyleSheet("QFrame{background:transparent;border:none;}");
        QFormLayout *form = new QFormLayout(card);
        form->setContentsMargins(14, 14, 14, 14);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(10);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        form->addRow(mkFieldLabel("Год"), yearBox_);
        form->addRow(mkFieldLabel("Месяц"), monthBox_);
        form->addRow(mkFieldLabel("Неделя"), weekBox_);
        form->addRow(mkFieldLabel("День"), dayBox_);
        layout->addWidget(card);

        connect(weekBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx){
            const int w = weekBox_->itemData(idx).toInt();
            dayBox_->setEnabled(w == 0);
            if (w != 0)
                dayBox_->setCurrentIndex(0);
        });

        connect(dayBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx){
            const int d = dayBox_->itemData(idx).toInt();
            weekBox_->setEnabled(d == 0);
            if (d != 0)
                weekBox_->setCurrentIndex(0);
        });

        connect(yearBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { rebuildDayItems(); });
        connect(monthBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { rebuildDayItems(); });

        QDialogButtonBox *btns =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        QPushButton *okButton = btns->button(QDialogButtonBox::Ok);
        QPushButton *cancelButton = btns->button(QDialogButtonBox::Cancel);
        if (okButton) {
            okButton->setText("Применить");
            okButton->setObjectName("okBtn");
        }
        if (cancelButton) {
            cancelButton->setText("Отмена");
            cancelButton->setObjectName("cancelBtn");
        }

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

        const QDate now = QDate::currentDate();
        const int yearIndex = yearBox_->findData(now.year());
        const int monthIndex = monthBox_->findData(now.month());
        if (yearIndex >= 0) yearBox_->setCurrentIndex(yearIndex);
        if (monthIndex >= 0) monthBox_->setCurrentIndex(monthIndex);
        rebuildDayItems();
        weekBox_->setCurrentIndex(0);
        dayBox_->setCurrentIndex(0);
    }

    int year() const { return yearBox_->itemData(yearBox_->currentIndex(), Qt::UserRole).toInt(); }
    int month() const { return monthBox_->itemData(monthBox_->currentIndex(), Qt::UserRole).toInt(); }
    int week() const { return weekBox_->itemData(weekBox_->currentIndex(), Qt::UserRole).toInt(); }
    int day() const { return dayBox_->itemData(dayBox_->currentIndex(), Qt::UserRole).toInt(); }

private:
    void rebuildDayItems()
    {
        bool yOk = false;
        bool mOk = false;
        const int y = yearBox_->itemData(yearBox_->currentIndex(), Qt::UserRole).toInt(&yOk);
        const int m = monthBox_->itemData(monthBox_->currentIndex(), Qt::UserRole).toInt(&mOk);
        const int prevDay = dayBox_->itemData(dayBox_->currentIndex(), Qt::UserRole).toInt();

        QSignalBlocker blockDay(dayBox_);
        dayBox_->clear();
        dayBox_->addItem(QStringLiteral("—"), 0);

        int dim = 31;
        if (yOk && mOk && y > 0 && m >= 1 && m <= 12)
            dim = LeftMenuCalendar::daysInMonth(y, m);

        for (int d = 1; d <= dim; ++d)
            dayBox_->addItem(QString::number(d), d);

        if (prevDay <= 0) {
            dayBox_->setCurrentIndex(0);
        } else if (prevDay <= dim) {
            const int ix = dayBox_->findData(prevDay);
            dayBox_->setCurrentIndex(ix >= 0 ? ix : 0);
        } else {
            const int ix = dayBox_->findData(dim);
            dayBox_->setCurrentIndex(ix >= 0 ? ix : 0);
        }
    }

    QComboBox *yearBox_ = nullptr;
    QComboBox *monthBox_ = nullptr;
    QComboBox *weekBox_ = nullptr;
    QComboBox *dayBox_ = nullptr;
};

class AppSettingsDialog : public QDialog {
public:
    explicit AppSettingsDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Настройки");
        setModal(true);
        setFixedSize(400, 160);
        setStyleSheet(
            "QDialog{background:#F7FAFF;border:1px solid #E2E8F0;border-radius:12px;}"
            "QLabel{font-family:Inter;font-size:13px;font-weight:700;color:#374151;}"
            "QLineEdit{background:#FFFFFF;border:1px solid #C7D2FE;border-radius:8px;"
            "padding:8px 12px;font-family:Inter;font-size:13px;}"
            "QPushButton{font-family:Inter;font-size:13px;font-weight:700;border-radius:8px;padding:8px 16px;}"
            "QPushButton#okBtn{background:#1976FF;color:white;border:none;}"
            "QPushButton#okBtn:hover{background:#0F66EA;}"
            "QPushButton#cancelBtn{background:#FFFFFF;color:#374151;border:1px solid #CBD5E1;}"
            "QPushButton#cancelBtn:hover{background:#F3F4F6;}"
        );

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 18, 20, 18);
        layout->setSpacing(14);

        QLabel *dbLbl = new QLabel("IP-адрес базы данных:", this);
        dbLbl->setStyleSheet("background:transparent;color:#374151;font-family:Inter;font-size:13px;font-weight:700;");
        dbHostEdit_ = new QLineEdit(this);
        dbHostEdit_->setPlaceholderText("localhost");
        dbHostEdit_->setText(getDbHost());
        layout->addWidget(dbLbl);
        layout->addWidget(dbHostEdit_);

        layout->addSpacing(10);

        QHBoxLayout *btnRow = new QHBoxLayout();
        btnRow->addStretch();

        QPushButton *cancelBtn = new QPushButton("Отмена", this);
        cancelBtn->setObjectName("cancelBtn");
        cancelBtn->setStyleSheet(
            "QPushButton{background:#FFFFFF;color:#374151;border:1px solid #CBD5E1;"
            "font-family:Inter;font-size:13px;font-weight:700;border-radius:8px;padding:8px 16px;}"
            "QPushButton:hover{background:#F3F4F6;}"
        );
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

        QPushButton *okBtn = new QPushButton("ОК", this);
        okBtn->setObjectName("okBtn");
        okBtn->setStyleSheet(
            "QPushButton{background:#1976FF;color:white;border:none;"
            "font-family:Inter;font-size:13px;font-weight:700;border-radius:8px;padding:8px 16px;}"
            "QPushButton:hover{background:#0F66EA;}"
        );
        connect(okBtn, &QPushButton::clicked, this, [this]() {
            QString host = dbHostEdit_->text().trimmed();
            if (host.isEmpty()) host = "localhost";
            const bool reconnectOk = reconnectWithHost(host);
            if (reconnectOk) {
                QMessageBox::information(this, "Настройки", "Настройки сохранены.");
            } else {
                QMessageBox::warning(this, "Ошибка", "Не удалось подключиться к базе данных.\nПроверьте IP-адрес.");
            }
            accept();
        });

        btnRow->addWidget(cancelBtn);
        btnRow->addWidget(okBtn);
        layout->addLayout(btnRow);

        if (qApp->property("autotest_running").toBool()) {
            for (int attempt = 0; attempt < 6; ++attempt) {
                QTimer::singleShot(120 + attempt * 120, this, [this]() {
                    if (isVisible())
                        reject();
                });
            }
        }
    }

private:
    QLineEdit *dbHostEdit_ = nullptr;
};

} // namespace

namespace LeftMenuDialogs {

bool showCalendarSettingsDialog(QWidget *parent, CalendarDialogSelection &selection)
{
    CalendarSettingsDialog dlg(parent);
    if (dlg.exec() != QDialog::Accepted)
        return false;

    selection.year = dlg.year();
    selection.month = dlg.month();
    selection.week = dlg.week();
    selection.day = dlg.day();
    return true;
}

void showAppSettingsDialog(QWidget *parent)
{
    AppSettingsDialog dlg(parent);
    dlg.exec();
}

} // namespace LeftMenuDialogs
