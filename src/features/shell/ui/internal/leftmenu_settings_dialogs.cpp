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
#include <QEventLoop>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
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
    Q_UNUSED(parent);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("dbSettingsHost", getDbHost());
    static const char kDbSettingsQml[] = R"QML(
import QtQuick 2.12
import QtQuick.Controls 2.12

ApplicationWindow {
    id: root
    visible: true
    width: 460
    height: 260
    minimumWidth: 420
    maximumWidth: 520
    minimumHeight: 240
    maximumHeight: 320
    title: "Настройки подключения к базе данных"
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowSystemMenuHint

    property string dbHost: typeof dbSettingsHost !== "undefined" ? dbSettingsHost : "localhost"
    property bool applyRequested: false
    property string selectedHost: dbHost

    Rectangle {
        anchors.fill: parent
        color: "#0F0F1A"

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Text {
                text: "Настройки подключения"
                font.pixelSize: 22
                font.bold: true
                color: "#FFFFFF"
            }

            Text {
                text: "Укажите IP-адрес или хост базы данных"
                font.pixelSize: 12
                color: "#A0A0B0"
            }

            Rectangle {
                width: parent.width
                radius: 12
                color: "#1a1a2e"
                border.color: "#333355"
                border.width: 1
                height: 92

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    Text {
                        text: "Хост БД"
                        font.pixelSize: 13
                        color: "#A0A0B0"
                    }

                    TextField {
                        id: hostInput
                        width: parent.width
                        height: 42
                        padding: 12
                        placeholderText: "localhost"
                        placeholderTextColor: "#888888"
                        text: root.dbHost
                        color: "#FFFFFF"
                        selectionColor: "#6C63FF"
                        selectedTextColor: "#FFFFFF"
                        font.pixelSize: 14
                        background: Rectangle {
                            color: hostInput.activeFocus ? "#252540" : "#1a1a2e"
                            radius: 10
                            border.color: hostInput.activeFocus ? "#6C63FF" : "#333355"
                            border.width: 1
                        }
                    }
                }
            }

            Item { width: 1; height: 2 }

            Row {
                width: parent.width
                spacing: 10

                Item { width: 1; height: 1 }

                Button {
                    width: 120
                    height: 42
                    text: "Отмена"
                    onClicked: root.close()
                }

                Button {
                    width: 160
                    height: 42
                    text: "Сохранить"
                    onClicked: {
                        root.selectedHost = hostInput.text
                        root.applyRequested = true
                        root.close()
                    }
                }
            }
        }
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.selectedHost = hostInput.text
            root.applyRequested = true
            root.close()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            root.close()
            event.accepted = true
        }
    }
}
)QML";
    engine.loadData(QByteArray(kDbSettingsQml), QUrl(QStringLiteral("inmemory:/DbSettingsDialog.qml")));
    if (engine.rootObjects().isEmpty())
        return;

    QObject *rootObj = engine.rootObjects().first();
    QQuickWindow *window = qobject_cast<QQuickWindow*>(rootObj);
    if (!window)
        return;

    QEventLoop loop;
    QObject::connect(window, &QWindow::visibleChanged, &loop, [window, &loop]() {
        if (!window->isVisible())
            loop.quit();
    });

    window->show();
    loop.exec();

    const bool applyRequested = rootObj->property("applyRequested").toBool();
    if (!applyRequested)
        return;

    QString host = rootObj->property("selectedHost").toString().trimmed();
    if (host.isEmpty())
        host = QStringLiteral("localhost");

    const bool reconnectOk = reconnectWithHost(host);
    if (reconnectOk) {
        QMessageBox::information(nullptr, "Настройки", "Настройки сохранены.");
    } else {
        QMessageBox::warning(nullptr, "Ошибка", "Не удалось подключиться к базе данных.\nПроверьте IP-адрес.");
    }
}

} // namespace LeftMenuDialogs
