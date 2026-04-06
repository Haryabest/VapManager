#include "app_bootstrap.h"

#include "mainwindow.h"
#include "leftmenu.h"
#include "db.h"
#include "db_users.h"
#include "db_agv_tasks.h"
#include "db_task_chat.h"
#include "notifications_logs.h"
#include "logindialog.h"
#include "app_session.h"
#include "ui_action_logger.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDialog>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QProxyStyle>
#include <QPushButton>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QTranslator>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

class NoFocusRectStyle : public QProxyStyle
{
public:
    explicit NoFocusRectStyle(const QString &baseStyleName)
        : QProxyStyle(baseStyleName) {}

    void drawPrimitive(PrimitiveElement element,
                       const QStyleOption *option,
                       QPainter *painter,
                       const QWidget *widget = nullptr) const override
    {
        if (element == PE_FrameFocusRect)
            return;
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

bool connectToDbWithRetries(int attempts, int delayMs, QString &outError)
{
    for (int i = 0; i < attempts; ++i) {
        outError.clear();
        if (connectToDB(&outError))
            return true;
        if (i + 1 < attempts)
            QThread::msleep(delayMs);
    }
    return false;
}

bool installAppTranslator(QApplication &app, const QString &lang)
{
    if (lang == "ru")
        return true;

    QTranslator *translator = new QTranslator(&app);
    const QString qmFile = QString("AgvNewUi_%1.qm").arg(lang);
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/" + qmFile,
        QCoreApplication::applicationDirPath() + "/translations/" + qmFile,
        ":/i18n/" + qmFile
    };

    for (const QString &path : candidates) {
        if (translator->load(path)) {
            app.installTranslator(translator);
            qInfo() << "Translator loaded:" << path;
            return true;
        }
    }

    qWarning() << "Translator not loaded. Tried:" << candidates;
    delete translator;
    return false;
}

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    QString line;
    switch (type) {
    case QtDebugMsg:
        line = QStringLiteral("Debug: %1 (%2:%3, %4)")
                   .arg(msg, QString::fromLocal8Bit(file))
                   .arg(context.line)
                   .arg(QString::fromLocal8Bit(function));
        break;
    case QtInfoMsg:
        line = QStringLiteral("Info: %1 (%2:%3, %4)")
                   .arg(msg, QString::fromLocal8Bit(file))
                   .arg(context.line)
                   .arg(QString::fromLocal8Bit(function));
        break;
    case QtWarningMsg:
        line = QStringLiteral("Warning: %1 (%2:%3, %4)")
                   .arg(msg, QString::fromLocal8Bit(file))
                   .arg(context.line)
                   .arg(QString::fromLocal8Bit(function));
        break;
    case QtCriticalMsg:
        line = QStringLiteral("Critical: %1 (%2:%3, %4)")
                   .arg(msg, QString::fromLocal8Bit(file))
                   .arg(context.line)
                   .arg(QString::fromLocal8Bit(function));
        break;
    case QtFatalMsg:
        line = QStringLiteral("Fatal: %1 (%2:%3, %4)")
                   .arg(msg, QString::fromLocal8Bit(file))
                   .arg(context.line)
                   .arg(QString::fromLocal8Bit(function));
        break;
    }

#ifdef Q_OS_WIN
    const QString withNewline = line + QLatin1Char('\n');
    OutputDebugStringW(reinterpret_cast<LPCWSTR>(withNewline.utf16()));
    const QByteArray localMsg = withNewline.toLocal8Bit();
#else
    const QByteArray localMsg = (line + QLatin1Char('\n')).toUtf8();
#endif
    fprintf(stderr, "%s", localMsg.constData());
    fflush(stderr);

    if (type == QtFatalMsg)
        abort();
}

} // namespace

namespace AppBootstrap {

int run(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    app.setStyle(new NoFocusRectStyle(QApplication::style()->objectName()));

    qInstallMessageHandler(myMessageOutput);

    {
        QString cfgPath = QCoreApplication::applicationDirPath() + "/config.ini";
        QSettings cfg(cfgPath, QSettings::IniFormat);
        QString lang = cfg.value("language", "ru").toString();
        installAppTranslator(app, lang);
    }

    for (;;) {
        QString dbError;
        if (connectToDbWithRetries(3, 700, dbError))
            break;

        QDialog dbDlg(nullptr);
        dbDlg.setWindowTitle("Настройки подключения к базе данных");
        dbDlg.setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint);
        dbDlg.setMinimumWidth(420);
        dbDlg.setModal(true);

        QVBoxLayout *layout = new QVBoxLayout(&dbDlg);
        QLabel *title = new QLabel("Подключение к базе данных", &dbDlg);
        layout->addWidget(title);
        QLabel *lab = new QLabel("Введите IP-адрес или хост сервера MySQL", &dbDlg);
        lab->setWordWrap(true);
        layout->addWidget(lab);

        QLineEdit *hostEdit = new QLineEdit(&dbDlg);
        hostEdit->setText(getDbHost());
        layout->addWidget(hostEdit);

        QLabel *errLabel = new QLabel(&dbDlg);
        errLabel->setText(dbError);
        errLabel->setWordWrap(true);
        layout->addWidget(errLabel);

        QHBoxLayout *btnRow = new QHBoxLayout();
        btnRow->addStretch();
        QPushButton *cancelBtn = new QPushButton("Отмена", &dbDlg);
        QPushButton *connBtn = new QPushButton("OK", &dbDlg);
        btnRow->addWidget(cancelBtn);
        btnRow->addWidget(connBtn);
        layout->addLayout(btnRow);

        QObject::connect(connBtn, &QPushButton::clicked, &dbDlg, [&dbDlg, hostEdit, errLabel]() {
            QString host = hostEdit->text().trimmed();
            if (host.isEmpty()) host = "localhost";
            QString err;
            if (reconnectWithHost(host, &err)) {
                dbDlg.accept();
                return;
            }
            errLabel->setText(err);
        });
        QObject::connect(cancelBtn, &QPushButton::clicked, &dbDlg, &QDialog::reject);

        if (dbDlg.exec() != QDialog::Accepted)
            return 0;
    }

    initUsersTable();
    initNotificationsTable();
    initMaintenanceNotificationSentTable();
    initTaskChatTables();
    ensureAssignedToColumn();
    ensureAgvListAssignedUserColumn();

    UserInfo user;
    bool autoLoginOk = tryAutoLogin(user);
    bool needLogin = !autoLoginOk || !user.isActive || user.expired;

    if (needLogin) {
        LoginDialog dlg;
        if (dlg.exec() != QDialog::Accepted)
            return 0;
        user = dlg.user();
    }

    AppSession::setCurrentUsername(user.username);
    logAction(AppSession::currentUsername(), "app_start", "Приложение запущено");

    UiActionLogger *uiLogger = new UiActionLogger(&app);
    app.installEventFilter(uiLogger);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [](){
        logAction(AppSession::currentUsername(), "app_exit", "Приложение закрыто");
    });

    MainWindow w;
    app.setWindowIcon(QIcon(":/new/mainWindowIcons/noback/agvIcon.png"));
    w.show();

    const QStringList args = QCoreApplication::arguments();
    const bool runStressCli = args.contains(QStringLiteral("--stress-autotest"))
                              || args.contains(QStringLiteral("--stress-test"));
    if (runStressCli) {
        const QString role = getUserRole(AppSession::currentUsername());
        if (role == QStringLiteral("admin") || role == QStringLiteral("tech")) {
            QTimer::singleShot(1500, &w, [&w]() {
                if (w.leftMenuWidget())
                    w.leftMenuWidget()->runFullStressAutotest();
            });
        } else {
            qWarning() << "--stress-autotest: ignored (need admin or tech role; use admin/tech account or auto-login as admin/tech)";
        }
    }

    return app.exec();
}

} // namespace AppBootstrap
