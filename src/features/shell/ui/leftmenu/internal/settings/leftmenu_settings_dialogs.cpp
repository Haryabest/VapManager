#include "leftmenu_settings_dialogs.h"
#include "../calendar/leftmenu_calendar_utils.h"
#include "leftmenu.h"
#include "db.h"
#include "app_updater.h"
#include "app_version.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QUrl>
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
        setFixedSize(460, 390);
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
            "QPushButton#updateBtn{background:#0F766E;color:white;border:none;}"
            "QPushButton#updateBtn:hover{background:#0D655E;}"
            "QPushButton#copyBtn{background:#FFFFFF;color:#374151;border:1px solid #CBD5E1;}"
            "QPushButton#copyBtn:hover{background:#F3F4F6;}"
            "QPushButton#checkBtn{background:#1D4ED8;color:white;border:none;}"
            "QPushButton#checkBtn:hover{background:#1E40AF;}"
        );

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 18, 20, 18);
        layout->setSpacing(12);

        QLabel *dbLbl = new QLabel("IP-адрес базы данных:", this);
        dbLbl->setStyleSheet("background:transparent;color:#374151;font-family:Inter;font-size:13px;font-weight:700;");
        dbHostEdit_ = new QLineEdit(this);
        dbHostEdit_->setPlaceholderText("localhost");
        dbHostEdit_->setText(getDbHost());
        layout->addWidget(dbLbl);

        QHBoxLayout *hostRow = new QHBoxLayout();
        hostRow->setSpacing(8);
        hostRow->addWidget(dbHostEdit_, 1);

        QPushButton *copyBtn = new QPushButton(QStringLiteral("Копировать IP"), this);
        copyBtn->setObjectName("copyBtn");
        connect(copyBtn, &QPushButton::clicked, this, [this]() {
            const QString host = dbHostEdit_->text().trimmed();
            if (host.isEmpty()) {
                QMessageBox::information(this, QStringLiteral("Копирование"),
                                         QStringLiteral("IP-адрес пуст."));
                return;
            }
            QApplication::clipboard()->setText(host);
            if (serverStatusLbl_) {
                serverStatusLbl_->setStyleSheet(
                    "background:transparent;color:#16A34A;font-family:Inter;font-size:12px;font-weight:600;");
                serverStatusLbl_->setText(QStringLiteral("IP скопирован: %1").arg(host));
            }
        });
        hostRow->addWidget(copyBtn);
        layout->addLayout(hostRow);

        QPushButton *checkBtn = new QPushButton(QStringLiteral("Проверить сервер"), this);
        checkBtn->setObjectName("checkBtn");
        connect(checkBtn, &QPushButton::clicked, this, &AppSettingsDialog::checkServer);
        layout->addWidget(checkBtn);

        serverStatusLbl_ = new QLabel(QStringLiteral("Нажмите «Проверить сервер» для диагностики."), this);
        serverStatusLbl_->setWordWrap(true);
        serverStatusLbl_->setStyleSheet(
            "background:transparent;color:#64748B;font-family:Inter;font-size:12px;font-weight:600;");
        layout->addWidget(serverStatusLbl_);

        QLabel *verLbl = new QLabel(QStringLiteral("Версия: %1").arg(AppVersion::label()), this);
        verLbl->setStyleSheet("background:transparent;color:#64748B;font-family:Inter;font-size:12px;font-weight:600;");
        layout->addWidget(verLbl);

        QPushButton *updateBtn = new QPushButton(QStringLiteral("Обновить программу"), this);
        updateBtn->setObjectName("updateBtn");
        updateBtn->setStyleSheet(
            "QPushButton{background:#0F766E;color:white;border:none;"
            "font-family:Inter;font-size:13px;font-weight:800;border-radius:8px;padding:8px 16px;}"
            "QPushButton:hover{background:#0D655E;}"
        );
        connect(updateBtn, &QPushButton::clicked, this, [this]() {
            AppUpdater::checkAndUpdate(this, updateStatusLbl_);
        });
        layout->addWidget(updateBtn);

        AppUpdater::ensureLastUpdateDateFromInstall();
        updateStatusLbl_ = new QLabel(this);
        updateStatusLbl_->setWordWrap(true);
        updateStatusLbl_->setStyleSheet(
            "background:transparent;color:#64748B;font-family:Inter;font-size:12px;font-weight:600;");
        updateStatusLbl_->setText(
            QStringLiteral("Последнее обновление: %1").arg(AppUpdater::formattedLastUpdateDate()));
        layout->addWidget(updateStatusLbl_);

        QHBoxLayout *updExtraRow = new QHBoxLayout();
        updExtraRow->setContentsMargins(0, 0, 0, 0);
        updExtraRow->setSpacing(8);

        QPushButton *historyBtn = new QPushButton(QStringLiteral("История обновлений"), this);
        historyBtn->setStyleSheet(
            "QPushButton{background:#E2E8F0;color:#0F172A;border:1px solid #C8C8C8;"
            "font-family:Inter;font-size:12px;font-weight:700;border-radius:8px;padding:7px 12px;}"
            "QPushButton:hover{background:#CBD5E1;}");
        connect(historyBtn, &QPushButton::clicked, this, [this]() {
            AppUpdater::showChangelogDialog(this);
        });

        QPushButton *reinstallBtn = new QPushButton(QStringLiteral("Переустановить"), this);
        reinstallBtn->setStyleSheet(
            "QPushButton{background:#FEF3C7;color:#92400E;border:1px solid #FCD34D;"
            "font-family:Inter;font-size:12px;font-weight:700;border-radius:8px;padding:7px 12px;}"
            "QPushButton:hover{background:#FDE68A;}");
        connect(reinstallBtn, &QPushButton::clicked, this, [this]() {
            AppUpdater::reinstallCurrentVersion(this);
        });

        updExtraRow->addWidget(historyBtn);
        updExtraRow->addWidget(reinstallBtn);
        updExtraRow->addStretch();
        layout->addLayout(updExtraRow);

        QPushButton *openCfgBtn = new QPushButton(QStringLiteral("Открыть config.ini"), this);
        openCfgBtn->setObjectName("copyBtn");
        connect(openCfgBtn, &QPushButton::clicked, this, []() {
            const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini");
            if (!QFile::exists(path)) {
                QMessageBox::warning(nullptr, QStringLiteral("config.ini"),
                                     QStringLiteral("Файл не найден:\n%1").arg(path));
                return;
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
        layout->addWidget(openCfgBtn);

        layout->addStretch();

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
            if (!reconnectOk) {
                QMessageBox::warning(this, "Ошибка", "Не удалось подключиться к базе данных.\nПроверьте IP-адрес.");
            } else if (auto *menu = qobject_cast<leftMenu *>(parentWidget())) {
                menu->refreshConnectionUi();
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
    void checkServer()
    {
        if (!serverStatusLbl_)
            return;

        serverStatusLbl_->setStyleSheet(
            "background:transparent;color:#64748B;font-family:Inter;font-size:12px;font-weight:600;");
        serverStatusLbl_->setText(QStringLiteral("Проверка сервера..."));

        QString host = dbHostEdit_->text().trimmed();
        if (host.isEmpty())
            host = QStringLiteral("localhost");

        bool dbOk = false;
        QString dbDetail;
        {
            QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
            if (!db.isOpen()) {
                dbDetail = QStringLiteral("нет соединения");
            } else {
                QSqlQuery q(db);
                if (q.exec(QStringLiteral("SELECT 1")) && q.next()) {
                    dbOk = true;
                    dbDetail = QStringLiteral("OK");
                } else {
                    dbDetail = q.lastError().text();
                    if (dbDetail.isEmpty())
                        dbDetail = QStringLiteral("ошибка запроса");
                }
            }
        }

        const QString savedHost = getDbHost();
        bool hostChanged = false;
        if (host.compare(savedHost, Qt::CaseInsensitive) != 0) {
            hostChanged = true;
            if (reconnectWithHost(host)) {
                QSqlDatabase db = QSqlDatabase::database(QStringLiteral("main_connection"));
                if (db.isOpen()) {
                    QSqlQuery q(db);
                    dbOk = q.exec(QStringLiteral("SELECT 1")) && q.next();
                    dbDetail = dbOk ? QStringLiteral("OK") : QStringLiteral("ошибка после переподключения");
                }
            } else {
                dbOk = false;
                dbDetail = QStringLiteral("не удалось подключиться к %1").arg(host);
            }
        }

        bool updateOk = false;
        QString updateDetail;
        const QString updateUrl = AppUpdater::updateCheckUrlForHost(host);
        QNetworkAccessManager nam;
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QNetworkReply *reply = nam.get(QNetworkRequest(QUrl(updateUrl)));
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(5000);
        loop.exec();

        if (timer.isActive()) {
            timer.stop();
            if (reply->error() == QNetworkReply::NoError) {
                updateOk = true;
                updateDetail = QStringLiteral("OK");
            } else {
                updateDetail = reply->errorString();
            }
        } else {
            reply->abort();
            updateDetail = QStringLiteral("таймаут");
        }
        reply->deleteLater();

        if (hostChanged)
            reconnectWithHost(savedHost);

        const QString dbLine = dbOk
            ? QStringLiteral("БД (%1): OK").arg(host)
            : QStringLiteral("БД (%1): %2").arg(host, dbDetail);
        const QString updateLine = updateOk
            ? QStringLiteral("Обновления (%1): OK").arg(updateUrl)
            : QStringLiteral("Обновления (%1): %2").arg(updateUrl, updateDetail);

        const bool allOk = dbOk && updateOk;
        serverStatusLbl_->setStyleSheet(allOk
            ? QStringLiteral("background:transparent;color:#16A34A;font-family:Inter;font-size:12px;font-weight:700;")
            : QStringLiteral("background:transparent;color:#DC2626;font-family:Inter;font-size:12px;font-weight:700;"));
        serverStatusLbl_->setText(dbLine + QStringLiteral("\n") + updateLine);
    }

    QLineEdit *dbHostEdit_ = nullptr;
    QLabel *updateStatusLbl_ = nullptr;
    QLabel *serverStatusLbl_ = nullptr;
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
    if (auto *menu = qobject_cast<leftMenu *>(parent))
        menu->refreshConnectionUi();
}

} // namespace LeftMenuDialogs
