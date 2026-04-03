#include "taskchatdialog.h"
#include "db_task_chat.h"
#include "db_users.h"
#include "notifications_logs.h"
#include "databus.h"
#include "app_session.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QApplication>
#include <QLabel>
#include <QComboBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSet>
#include <QScrollBar>
#include <QTimer>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QAction>
#include <QCheckBox>
#include <QInputDialog>
#include <QContextMenuEvent>
#include <QSignalBlocker>
#include <QHash>
#include <QDate>
#include <QToolButton>
#include <QSettings>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QMimeDatabase>
#include <QClipboard>
#include <QMimeData>
#include <QImage>
#include <QBuffer>
#include <QPainter>
#include <QStyle>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <climits>

static int scale(int v) { return v; }

namespace {
static const QString kSpecialPrefix = QStringLiteral("[[SPECIAL|");
static const QString kAttachmentPrefix = QStringLiteral("[[FILE|");
static const QString kForwardPrefix = QStringLiteral("[[FORWARD|");
static const QString kAttachmentSeparator = QStringLiteral("\n[[ATTACHMENT]]\n");
static QHash<int, QString> g_pendingSpecialByThread;

static bool openAttachmentData(QWidget *owner, const QString &fileName, const QByteArray &bytes);

static QString muteKey(const QString &currentUser, const QString &peerUser)
{
    return QString("chat/mute/%1/%2").arg(currentUser.trimmed(), peerUser.trimmed());
}

static bool isMuted(const QString &currentUser, const QString &peerUser)
{
    if (currentUser.trimmed().isEmpty() || peerUser.trimmed().isEmpty()) return false;
    QSettings s("AgvNewUi", "AgvNewUi");
    return s.value(muteKey(currentUser, peerUser), false).toBool();
}

static void setMuted(const QString &currentUser, const QString &peerUser, bool muted)
{
    if (currentUser.trimmed().isEmpty() || peerUser.trimmed().isEmpty()) return;
    QSettings s("AgvNewUi", "AgvNewUi");
    s.setValue(muteKey(currentUser, peerUser), muted);
}

static QString encodeSpecialMessage(const QString &label, const QString &plainText)
{
    return QString("%1%2]]\n%3").arg(kSpecialPrefix, label, plainText);
}

static bool decodeSpecialMessage(const QString &raw, QString &outLabel, QString &outText)
{
    if (!raw.startsWith(kSpecialPrefix)) {
        outLabel.clear();
        outText = raw;
        return false;
    }
    const int end = raw.indexOf(QStringLiteral("]]\n"));
    if (end <= 0) {
        outLabel.clear();
        outText = raw;
        return false;
    }
    outLabel = raw.mid(kSpecialPrefix.size(), end - kSpecialPrefix.size()).trimmed();
    outText = raw.mid(end + 3);
    return true;
}

static QString encodeForwardMessage(const QString &fromUser, const QString &payload)
{
    return QString("%1%2]]\n%3").arg(kForwardPrefix, fromUser.trimmed(), payload);
}

static bool decodeForwardMessage(const QString &raw, QString &outFromUser, QString &outPayload)
{
    if (!raw.startsWith(kForwardPrefix)) {
        outFromUser.clear();
        outPayload = raw;
        return false;
    }
    const int end = raw.indexOf(QStringLiteral("]]\n"));
    if (end <= 0) {
        outFromUser.clear();
        outPayload = raw;
        return false;
    }
    outFromUser = raw.mid(kForwardPrefix.size(), end - kForwardPrefix.size()).trimmed();
    outPayload = raw.mid(end + 3);
    return true;
}

static QString encodeAttachmentMessage(const QString &fileName, const QString &mimeType, const QByteArray &bytes)
{
    return QString("%1%2|%3|%4]]")
            .arg(kAttachmentPrefix, fileName, mimeType, QString::fromLatin1(bytes.toBase64()));
}

static bool decodeAttachmentMessage(const QString &raw, QString &outFileName, QString &outMimeType, QByteArray &outData)
{
    if (!raw.startsWith(kAttachmentPrefix) || !raw.endsWith(QStringLiteral("]]"))) {
        outFileName.clear();
        outMimeType.clear();
        outData.clear();
        return false;
    }
    const QString body = raw.mid(kAttachmentPrefix.size(), raw.size() - kAttachmentPrefix.size() - 2);
    const int p1 = body.indexOf('|');
    if (p1 <= 0) return false;
    const int p2 = body.indexOf('|', p1 + 1);
    if (p2 <= p1 + 1) return false;
    outFileName = body.left(p1).trimmed();
    outMimeType = body.mid(p1 + 1, p2 - p1 - 1).trimmed();
    const QByteArray b64 = body.mid(p2 + 1).toLatin1();
    outData = QByteArray::fromBase64(b64);
    return !outFileName.isEmpty() && !outData.isEmpty();
}

static bool decodeAttachmentMeta(const QString &raw, QString &outFileName, QString &outMimeType)
{
    if (!raw.startsWith(kAttachmentPrefix) || !raw.endsWith(QStringLiteral("]]"))) {
        outFileName.clear();
        outMimeType.clear();
        return false;
    }
    const QString body = raw.mid(kAttachmentPrefix.size(), raw.size() - kAttachmentPrefix.size() - 2);
    const int p1 = body.indexOf('|');
    if (p1 <= 0) return false;
    const int p2 = body.indexOf('|', p1 + 1);
    if (p2 <= p1 + 1) return false;
    outFileName = body.left(p1).trimmed();
    outMimeType = body.mid(p1 + 1, p2 - p1 - 1).trimmed();
    return !outFileName.isEmpty();
}

static bool decodeAttachmentFromStoredMessage(const QString &rawMessage, QString &outFileName, QString &outMimeType, QByteArray &outData)
{
    QString forwardedFrom;
    QString payload = rawMessage;
    decodeForwardMessage(rawMessage, forwardedFrom, payload);
    return decodeAttachmentMessage(payload, outFileName, outMimeType, outData);
}

static bool splitCombinedMessage(const QString &rawMessage, QString &outText, QString &outAttachment)
{
    const int sepPos = rawMessage.indexOf(kAttachmentSeparator);
    if (sepPos < 0) {
        outText = rawMessage;
        outAttachment.clear();
        return false;
    }
    outText = rawMessage.left(sepPos);
    outAttachment = rawMessage.mid(sepPos + kAttachmentSeparator.size());
    return true;
}

static bool isImageAttachment(const QString &mimeType, const QString &fileName)
{
    const QString m = mimeType.trimmed().toLower();
    if (m.startsWith("image/")) return true;
    const QString n = fileName.toLower();
    return n.endsWith(".png") || n.endsWith(".jpg") || n.endsWith(".jpeg")
        || n.endsWith(".bmp") || n.endsWith(".gif") || n.endsWith(".webp");
}

static QString shortFileTypeLabel(const QString &fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toUpper();
    if (ext.isEmpty()) return QStringLiteral("FILE");
    return ext.left(6);
}

static bool saveAttachmentAs(QWidget *owner, const QString &fileName, const QByteArray &bytes)
{
    QString startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (startDir.trimmed().isEmpty())
        startDir = QDir::tempPath();
    const QString path = QFileDialog::getSaveFileName(owner, "Сохранить как", QDir(startDir).filePath(fileName), "Все файлы (*)");
    if (path.trimmed().isEmpty()) return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(owner, "Файл", "Не удалось сохранить файл.");
        return false;
    }
    f.write(bytes);
    f.close();
    return true;
}

static bool openAttachmentInsideApp(QWidget *owner, const QString &fileName, const QString &mimeType, const QByteArray &bytes,
                                    const QString &currentUser = QString(), int messageId = 0)
{
    if (isImageAttachment(mimeType, fileName)) {
        QDialog dlg(owner);
        dlg.setWindowTitle(fileName);
        dlg.setMinimumSize(820, 560);
        QVBoxLayout *root = new QVBoxLayout(&dlg);
        QLabel *img = new QLabel(&dlg);
        img->setAlignment(Qt::AlignCenter);
        img->setStyleSheet("background:#0F172A;border-radius:10px;");
        QPixmap pm;
        pm.loadFromData(bytes);
        if (!pm.isNull())
            img->setPixmap(pm.scaled(780, 500, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        root->addWidget(img, 1);
        QHBoxLayout *btns = new QHBoxLayout();
        QPushButton *saveAs = new QPushButton(QStringLiteral("Сохранить как"), &dlg);
        QObject::connect(saveAs, &QPushButton::clicked, &dlg, [owner, fileName, bytes]() {
            saveAttachmentAs(owner, fileName, bytes);
        });
        btns->addWidget(saveAs);
        QPushButton *delBtn = new QPushButton(QStringLiteral("Удалить"), &dlg);
        delBtn->setStyleSheet("QPushButton{background:#FEE2E2;color:#B91C1C;border:none;border-radius:8px;padding:6px 10px;font-weight:800;}"
                              "QPushButton:hover{background:#FCA5A5;}");
        QObject::connect(delBtn, &QPushButton::clicked, &dlg, [owner, &dlg, currentUser, messageId]() {
            if (messageId <= 0 || currentUser.trimmed().isEmpty()) return;
            QString err;
            if (!hideMessageForUser(messageId, currentUser, err)) {
                QMessageBox::warning(owner, "Удаление", err);
                return;
            }
            dlg.accept();
        });
        btns->addWidget(delBtn);
        btns->addStretch();
        root->addLayout(btns);
        dlg.exec();
        return (dlg.result() == QDialog::Accepted);
    }

    openAttachmentData(owner, fileName, bytes);
    return false;
}

static bool openAttachmentData(QWidget *owner, const QString &fileName, const QByteArray &bytes)
{
    if (fileName.trimmed().isEmpty() || bytes.isEmpty())
        return false;
    QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (docs.trimmed().isEmpty())
        docs = QDir::tempPath();
    QDir dir(docs + "/VapManagerChatFiles");
    if (!dir.exists())
        dir.mkpath(".");
    const QString path = dir.filePath(fileName);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(owner, "Файл", "Не удалось сохранить вложение.");
        return false;
    }
    f.write(bytes);
    f.close();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        QMessageBox::information(owner, "Файл", QString("Файл сохранен: %1").arg(path));
    }
    return true;
}

static bool pickAttachmentPayload(QWidget *owner, QString &outName, QString &outMime, QByteArray &outBytes, QString &outError)
{
    const QString imgFilter = QStringLiteral("Изображения (*.png *.jpg *.jpeg *.bmp *.gif *.webp)");
    const QString docsFilter = QStringLiteral("Документы (*.pdf *.doc *.docx *.xls *.xlsx *.ppt *.pptx *.txt *.csv)");
    const QString archivesFilter = QStringLiteral("Архивы (*.zip *.rar *.7z)");
    const QString allSupported = QStringLiteral("Все поддерживаемые (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.pdf *.doc *.docx *.xls *.xlsx *.ppt *.pptx *.txt *.csv *.zip *.rar *.7z)");
    const QString allFiles = QStringLiteral("Все файлы (*)");
    const QString filter = QString("%1;;%2;;%3;;%4;;%5").arg(imgFilter, docsFilter, archivesFilter, allSupported, allFiles);

    QString selectedFilter = allSupported;
    const QString startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(owner, "Выберите файл", startDir, filter, &selectedFilter);
    if (path.trimmed().isEmpty())
        return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        outError = QStringLiteral("Не удалось открыть файл.");
        return false;
    }
    const QByteArray bytes = f.readAll();
    f.close();
    if (bytes.isEmpty()) {
        outError = QStringLiteral("Файл пуст.");
        return false;
    }
    // Ограничиваем размер, чтобы не перегружать чат и БД.
    static const int kMaxBytes = 8 * 1024 * 1024;
    if (bytes.size() > kMaxBytes) {
        outError = QStringLiteral("Файл слишком большой (максимум 8 МБ).");
        return false;
    }
    QFileInfo fi(path);
    QMimeDatabase mdb;
    outName = fi.fileName();
    outMime = mdb.mimeTypeForFile(fi).name();
    outBytes = bytes;
    return true;
}

static bool loadAttachmentFromLocalPath(const QString &path, QString &outName, QString &outMime, QByteArray &outBytes, QString &outError)
{
    if (path.trimmed().isEmpty()) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        outError = QStringLiteral("Не удалось открыть файл.");
        return false;
    }
    const QByteArray bytes = f.readAll();
    f.close();
    if (bytes.isEmpty()) {
        outError = QStringLiteral("Файл пуст.");
        return false;
    }
    static const int kMaxBytes = 8 * 1024 * 1024;
    if (bytes.size() > kMaxBytes) {
        outError = QStringLiteral("Файл слишком большой (максимум 8 МБ).");
        return false;
    }
    QFileInfo fi(path);
    QMimeDatabase mdb;
    outName = fi.fileName();
    outMime = mdb.mimeTypeForFile(fi).name();
    outBytes = bytes;
    return true;
}

static bool pickAttachmentFromClipboard(QString &outName, QString &outMime, QByteArray &outBytes, QString &outError)
{
    const QMimeData *md = QApplication::clipboard()->mimeData();
    if (!md) return false;

    if (md->hasImage()) {
        QImage img = qvariant_cast<QImage>(md->imageData());
        if (img.isNull()) {
            outError = QStringLiteral("Не удалось прочитать изображение из буфера.");
            return false;
        }
        QByteArray bytes;
        QBuffer buf(&bytes);
        if (!buf.open(QIODevice::WriteOnly) || !img.save(&buf, "PNG")) {
            outError = QStringLiteral("Не удалось подготовить изображение.");
            return false;
        }
        outName = QString("image_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
        outMime = QStringLiteral("image/png");
        outBytes = bytes;
        return true;
    }

    if (md->hasUrls()) {
        const QList<QUrl> urls = md->urls();
        for (const QUrl &u : urls) {
            if (!u.isLocalFile()) continue;
            QFile f(u.toLocalFile());
            if (!f.open(QIODevice::ReadOnly)) continue;
            const QByteArray bytes = f.readAll();
            f.close();
            if (bytes.isEmpty()) continue;
            static const int kMaxBytes = 8 * 1024 * 1024;
            if (bytes.size() > kMaxBytes) {
                outError = QStringLiteral("Файл из буфера слишком большой (максимум 8 МБ).");
                return false;
            }
            QFileInfo fi(u.toLocalFile());
            QMimeDatabase mdb;
            outName = fi.fileName();
            outMime = mdb.mimeTypeForFile(fi).name();
            outBytes = bytes;
            return true;
        }
    }
    return false;
}
}

static QString userDotColor(const QString &username)
{
    QString u = username.trimmed();
    if (u.isEmpty()) return "#999999";
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen()) return "#999999";
    QSqlQuery q(db);
    q.prepare("SELECT is_active, last_login FROM users WHERE username = :u");
    q.bindValue(":u", u);
    if (!q.exec() || !q.next()) return "#999999";
    const bool isActive = q.value(0).toInt() == 1;
    if (!isActive) return "#999999";

    QDateTime dt = q.value(1).toDateTime();
    if (!dt.isValid()) return "#999999";
    const qint64 lastLoginSecs = dt.secsTo(QDateTime::currentDateTime());
    if (lastLoginSecs < 0) return "#999999";
    if (lastLoginSecs < 600) return "#18CF00";
    if (lastLoginSecs < 7200) return "#F59E0B";
    if (lastLoginSecs < 86400) return "#F59E0B";
    if (lastLoginSecs < 172800) return "#EF4444";
    return "#999999";
}

static void applyUserDot(QLabel *dot, int sizePx, const QString &color)
{
    if (!dot) return;
    dot->setFixedSize(sizePx, sizePx);
    dot->setStyleSheet(QString("background:%1;border-radius:%2px;").arg(color).arg(sizePx / 2));
}

static QPixmap makeRoundAvatarPixmap(const QPixmap &src, int size)
{
    if (src.isNull() || size <= 0)
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

static QString formatLastSeenLabel(const QString &username)
{
    if (username.trimmed().isEmpty())
        return QStringLiteral("не в сети");

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return QStringLiteral("не в сети");

    QSqlQuery q(db);
    q.prepare("SELECT is_active, last_login FROM users WHERE username = :u");
    q.bindValue(":u", username.trimmed());
    if (!q.exec() || !q.next())
        return QStringLiteral("не в сети");

    const bool isActive = q.value(0).toInt() == 1;
    const QDateTime lastLogin = q.value(1).toDateTime();
    if (isActive && lastLogin.isValid()) {
        const qint64 secs = lastLogin.secsTo(QDateTime::currentDateTime());
        if (secs >= 0 && secs < 180)
            return QStringLiteral("В сети");
    }

    if (!lastLogin.isValid())
        return QStringLiteral("Не в сети");

    const qint64 secs = lastLogin.secsTo(QDateTime::currentDateTime());
    if (secs < 60)
        return QStringLiteral("Был(а) в сети только что");
    if (secs < 3600)
        return QStringLiteral("Был(а) в сети %1 мин назад").arg(qMax<qint64>(1, secs / 60));
    if (secs < 86400)
        return QStringLiteral("Был(а) в сети %1 ч назад").arg(qMax<qint64>(1, secs / 3600));
    if (secs < 172800)
        return QStringLiteral("Был(а) в сети вчера");
    return QStringLiteral("Был(а) в сети %1").arg(lastLogin.toString(QStringLiteral("dd.MM.yyyy hh:mm")));
}

// ------------------------- TaskChatWidget (embedded chat) -------------------------
TaskChatWidget::TaskChatWidget(int threadId, const QString &currentUser, bool isAdmin,
                               std::function<int(int)> scaleFn, QWidget *parent)
    : QWidget(parent)
    , threadId_(threadId)
    , currentUser_(currentUser)
    , isAdmin_(isAdmin)
    , s_(scaleFn ? scaleFn : scale)
{
    setAcceptDrops(true);
    setStyleSheet(
        "QWidget{background:#F4F6FA;}"
        "QLineEdit{background:#FFFFFF;border:1px solid #D7DFEA;border-radius:14px;padding:10px 12px;font-size:13px;}"
        "QLineEdit:focus{border:1px solid #8BA7FF;}"
        "QPushButton{border-radius:12px;}"
        "QScrollBar:vertical{background:transparent;width:10px;}"
        "QScrollBar::handle:vertical{background:#BFCBE0;border-radius:5px;min-height:28px;}"
    );
    setupUi();
    if (scrollArea_ && scrollArea_->viewport())
        scrollArea_->viewport()->installEventFilter(this);
}

void TaskChatWidget::setThreadId(int id, const QString &peerHint)
{
    if (!peerHint.trimmed().isEmpty())
        peerHint_ = peerHint.trimmed();
    forceScrollToBottom_ = true;
    if (threadId_ == id) {
        refreshMessages(false);
        return;
    }
    clearPendingAttachment();
    clearPendingForward();
    if (replyEdit_) replyEdit_->clear();
    threadId_ = id;
    oldestLoadedMessageId_ = 0;
    lastLoadedMessageId_ = 0;
    historyExhausted_ = false;
    loadingOlderMessages_ = false;
    refreshMessages(true);
}

bool TaskChatWidget::autotestSendTextMessage(const QString &message, QString *error)
{
    if (threadId_ <= 0) {
        if (error) *error = QStringLiteral("Тестовый тред не открыт");
        return false;
    }
    if (!replyEdit_) {
        if (error) *error = QStringLiteral("Поле ввода чата не найдено");
        return false;
    }

    const QString expected = message.trimmed();
    if (expected.isEmpty()) {
        if (error) *error = QStringLiteral("Пустое сообщение надо проверять через autotestRejectsEmptyMessage()");
        return false;
    }

    const int beforeLastId = lastLoadedMessageId_;
    replyEdit_->setText(message);
    sendReply();
    QApplication::processEvents();

    const QVector<TaskChatMessage> msgs = getMessagesForThreadLastN(threadId_, currentUser_, 12);
    for (const TaskChatMessage &m : msgs) {
        if (m.id > beforeLastId && m.fromUser == currentUser_ && m.message.contains(expected))
            return true;
    }

    if (error) *error = QStringLiteral("Сообщение не найдено в последних сообщениях треда");
    return false;
}

bool TaskChatWidget::autotestRejectsEmptyMessage(QString *error)
{
    if (threadId_ <= 0) {
        if (error) *error = QStringLiteral("Тестовый тред не открыт");
        return false;
    }
    if (!replyEdit_) {
        if (error) *error = QStringLiteral("Поле ввода чата не найдено");
        return false;
    }

    const int beforeLastId = lastLoadedMessageId_;
    replyEdit_->setText(QStringLiteral("    \t   "));
    sendReply();
    QApplication::processEvents();

    if (lastLoadedMessageId_ != beforeLastId) {
        if (error) *error = QStringLiteral("Пустое сообщение не должно было отправиться");
        return false;
    }

    if (!replyEdit_->text().trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Поле ввода содержит неожиданный текст после пустой отправки");
        return false;
    }

    replyEdit_->clear();
    return true;
}

void TaskChatWidget::updatePeerHeaderMeta()
{
    QString peerDisplay = peerUsername_;
    UserInfo pi;
    if (!peerUsername_.isEmpty() && loadUserProfile(peerUsername_, pi) && !pi.fullName.isEmpty())
        peerDisplay = pi.fullName;

    if (peerLbl_)
        peerLbl_->setText(peerDisplay.isEmpty() ? QStringLiteral("—") : peerDisplay);

    if (peerLastSeenLbl_)
        peerLastSeenLbl_->setText(formatLastSeenLabel(peerUsername_));

    if (peerAvatarLbl_) {
        QPixmap peerAvatar = loadUserAvatarFromDb(peerUsername_);
        if (peerAvatar.isNull())
            peerAvatar = QPixmap(":/new/mainWindowIcons/noback/user.png");
        peerAvatarLbl_->setPixmap(peerAvatar.isNull() ? QPixmap()
                                                      : makeRoundAvatarPixmap(peerAvatar, s_(38)));
    }

    if (peerStatusDot_)
        applyUserDot(peerStatusDot_, s_(10), userDotColor(peerUsername_));
}

void TaskChatWidget::setupUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(s_(12), s_(12), s_(12), s_(12));

    QWidget *header = new QWidget(this);
    QHBoxLayout *headerL = new QHBoxLayout(header);
    headerL->setContentsMargins(0, 0, 0, 0);
    backBtn_ = new QPushButton("   Назад", header);
    backBtn_->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    backBtn_->setIconSize(QSize(s_(24), s_(24)));
    backBtn_->setFixedSize(s_(150), s_(50));
    backBtn_->setStyleSheet(QString(
        "QPushButton{background:#E6E6E6;border-radius:%1px;border:none;font-weight:800;text-align:left;padding-left:%2px;}"
        "QPushButton:hover{background:#D5D5D5;}"
    ).arg(s_(10)).arg(s_(10)));
    connect(backBtn_, &QPushButton::clicked, this, &TaskChatWidget::backRequested);
    headerL->addWidget(backBtn_, 0, Qt::AlignLeft);

    // peer display in header (left)
    TaskChatThread t0 = getThreadById(threadId_);
    peerUsername_ = (t0.createdBy == currentUser_) ? t0.recipientUser : t0.createdBy;
    if (peerUsername_.trimmed().isEmpty() || peerUsername_ == currentUser_)
        peerUsername_ = peerHint_;
    peerAvatarLbl_ = new QLabel(header);
    peerAvatarLbl_->setFixedSize(s_(38), s_(38));
    peerAvatarLbl_->setStyleSheet("background:transparent;");
    headerL->addWidget(peerAvatarLbl_, 0, Qt::AlignVCenter);
    QWidget *peerTextHost = new QWidget(header);
    peerTextHost->setStyleSheet("background:transparent;");
    QVBoxLayout *peerTextLayout = new QVBoxLayout(peerTextHost);
    peerTextLayout->setContentsMargins(0, 0, 0, 0);
    peerTextLayout->setSpacing(1);
    peerLbl_ = new QLabel(QStringLiteral("—"), peerTextHost);
    peerLbl_->setStyleSheet("font-weight:900; font-size:16px; color:#0F172A; text-decoration: underline;");
    peerLbl_->setCursor(Qt::PointingHandCursor);
    peerLbl_->setToolTip("Показать медиа и документы");
    peerLbl_->installEventFilter(this);
    peerLastSeenLbl_ = new QLabel(QStringLiteral("Не в сети"), peerTextHost);
    peerLastSeenLbl_->setStyleSheet("font-size:11px; color:#94A3B8; background:transparent;");
    peerTextLayout->addWidget(peerLbl_);
    peerTextLayout->addWidget(peerLastSeenLbl_);
    peerStatusDot_ = new QLabel(header);
    headerL->addWidget(peerTextHost, 0, Qt::AlignVCenter);
    headerL->addWidget(peerStatusDot_, 0, Qt::AlignVCenter);
    updatePeerHeaderMeta();

    headerL->addStretch();

    // menu "..."
    moreBtn_ = new QToolButton(header);
    moreBtn_->setText("⋮");
    moreBtn_->setPopupMode(QToolButton::InstantPopup);
    moreBtn_->setStyleSheet(
        "QToolButton{background:transparent;border:1px solid transparent;border-radius:18px;font-size:28px;font-weight:900;color:#334155;padding:0 14px 0 14px;}"
        "QToolButton:hover{background:rgba(80,118,251,36);border:1px solid rgb(80,118,251);color:#0F172A;}"
        "QToolButton::menu-indicator{image:none;width:0;height:0;}"
    );
    QMenu *menu = new QMenu(moreBtn_);
    menu->setStyleSheet(
        "QMenu{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:10px;padding:6px;}"
        "QMenu::item{padding:8px 14px;border-radius:8px;font-size:13px;}"
        "QMenu::item:selected{background:#EEF4FF;}"
    );
    QAction *muteAct = menu->addAction("Выключить уведомления");
    QAction *profileAct = menu->addAction("Показать профиль");
    QAction *deleteAct = menu->addAction("Удалить чат");
    moreBtn_->setMenu(menu);
    QWidget *rightPadDots = new QWidget(header);
    rightPadDots->setFixedWidth(s_(6));
    headerL->addWidget(moreBtn_, 0, Qt::AlignRight);
    headerL->addWidget(rightPadDots, 0, Qt::AlignRight);

    auto syncMuteText = [this, muteAct]() {
        const bool muted = isMuted(currentUser_, peerUsername_);
        muteAct->setText(muted ? QStringLiteral("Включить уведомления") : QStringLiteral("Выключить уведомления"));
    };
    syncMuteText();
    connect(menu, &QMenu::aboutToShow, this, syncMuteText);
    connect(muteAct, &QAction::triggered, this, [this, syncMuteText]() {
        const bool nowMuted = !isMuted(currentUser_, peerUsername_);
        setMuted(currentUser_, peerUsername_, nowMuted);
        syncMuteText();
    });
    connect(profileAct, &QAction::triggered, this, [this]() {
        if (!peerUsername_.trimmed().isEmpty())
            emit showProfileRequested(peerUsername_.trimmed());
    });
    connect(deleteAct, &QAction::triggered, this, [this]() {
        if (QMessageBox::question(this, "Удалить чат", "Удалить чат?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        QString err;
        if (isAdmin_) {
            if (!deleteThread(threadId_, err)) { QMessageBox::warning(this, "Ошибка", err); return; }
            logAction(currentUser_, "chat_deleted_admin", QString("thread=%1").arg(threadId_));
        } else {
            if (!hideThreadForUser(threadId_, currentUser_, err)) { QMessageBox::warning(this, "Ошибка", err); return; }
            logAction(currentUser_, "chat_hidden_for_me", QString("thread=%1").arg(threadId_));
        }
        DataBus::instance().triggerNotificationsChanged();
        emit backRequested();
    });
    selectionCountLbl_ = new QLabel("0", header);
    selectionCountLbl_->setStyleSheet("font-weight:800;color:#0F172A;");
    selectionCountLbl_->setVisible(false);
    headerL->addWidget(selectionCountLbl_, 0, Qt::AlignRight);
    cancelSelectionBtn_ = new QPushButton("Отмена", header);
    cancelSelectionBtn_->setMinimumHeight(40);
    cancelSelectionBtn_->setStyleSheet("QPushButton{background:#E2E8F0;color:#334155;font-weight:800;font-size:13px;padding:10px 16px;} QPushButton:hover{background:#CBD5E1;}");
    cancelSelectionBtn_->setVisible(false);
    connect(cancelSelectionBtn_, &QPushButton::clicked, this, &TaskChatWidget::exitSelectionMode);
    headerL->addWidget(cancelSelectionBtn_, 0, Qt::AlignRight);
    deleteSelectedBtn_ = new QPushButton("Удалить", header);
    deleteSelectedBtn_->setMinimumHeight(40);
    deleteSelectedBtn_->setStyleSheet("QPushButton{background:#DC2626;color:white;font-weight:800;font-size:13px;padding:10px 18px;} QPushButton:hover{background:#BE1F1F;}");
    deleteSelectedBtn_->setVisible(false);
    connect(deleteSelectedBtn_, &QPushButton::clicked, this, &TaskChatWidget::deleteSelectedMessages);
    headerL->addWidget(deleteSelectedBtn_, 0, Qt::AlignRight);
    root->addWidget(header);

    // peerLbl_ moved into header

    headerLbl_ = nullptr;
    QWidget *pinnedRow = new QWidget(this);
    QHBoxLayout *pinnedRowL = new QHBoxLayout(pinnedRow);
    pinnedRowL->setContentsMargins(0, 0, 0, 0);
    pinnedRowL->setSpacing(6);
    pinnedLbl_ = new QLabel(this);
    pinnedLbl_->setStyleSheet("font-weight:700; font-size:12px; color:#1E40AF; background:#DBEAFE; padding:6px; border-radius:8px;");
    pinnedLbl_->setCursor(Qt::PointingHandCursor);
    pinnedLbl_->setToolTip("Перейти к закрепленному сообщению");
    pinnedLbl_->setVisible(false);
    pinnedLbl_->installEventFilter(this);
    unpinnedBtn_ = new QPushButton(QString::fromUtf8("✕"), this);
    unpinnedBtn_->setFixedSize(s_(24), s_(24));
    unpinnedBtn_->setVisible(false);
    unpinnedBtn_->setToolTip("Открепить");
    unpinnedBtn_->setStyleSheet("QPushButton{background:#FEE2E2;color:#B91C1C;border:none;border-radius:12px;font-weight:900;}"
                                "QPushButton:hover{background:#FCA5A5;}");
    connect(unpinnedBtn_, &QPushButton::clicked, this, [this]() {
        pinnedMessageId_ = 0;
        if (pinnedLbl_) pinnedLbl_->setVisible(false);
        if (unpinnedBtn_) unpinnedBtn_->setVisible(false);
    });
    pinnedRowL->addWidget(pinnedLbl_, 1);
    pinnedRowL->addWidget(unpinnedBtn_, 0, Qt::AlignTop);
    root->addWidget(pinnedRow);
    replyBannerLbl_ = new QLabel(this);
    replyBannerLbl_->setStyleSheet("font-weight:700; font-size:12px; color:#475569; background:#F1F5F9; padding:6px; border-radius:8px;");
    replyBannerLbl_->setVisible(false);
    root->addWidget(replyBannerLbl_);

    QWidget *scrollContainer = new QWidget(this);
    scrollContainer->setStyleSheet("background:transparent;");
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContainer);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollArea_ = new QScrollArea(scrollContainer);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setStyleSheet("QScrollArea{background:transparent;border:none;}");
    scrollArea_->setMinimumHeight(200);
    QWidget *host = new QWidget(scrollArea_);
    host->setStyleSheet("background:transparent;");
    messagesLayout_ = new QVBoxLayout(host);
    messagesLayout_->setContentsMargins(4, 4, 4, 4);
    messagesLayout_->setSpacing(6);
    scrollArea_->setWidget(host);
    dayLabel_ = new QLabel(scrollContainer);
    dayLabel_->setAlignment(Qt::AlignCenter);
    dayLabel_->setStyleSheet("font-size:11px;font-weight:600;color:#64748B;background:rgba(244,246,250,0.95);padding:4px 12px;border-radius:8px;");
    dayLabel_->setFixedHeight(s_(28));
    dayLabel_->hide();
    scrollLayout->addWidget(dayLabel_);
    scrollLayout->addWidget(scrollArea_, 1);
    root->addWidget(scrollContainer, 1);
    if (QScrollBar *sb = scrollArea_->verticalScrollBar()) {
        connect(sb, &QScrollBar::valueChanged, this, [this](int value) {
            if (value <= 20 && threadId_ > 0)
                loadOlderMessages();
            updateDayLabel();
        });
    }

    QFrame *composer = new QFrame(this);
    composer->setStyleSheet("QFrame{background:#FFFFFF;border:none;border-radius:12px;}");
    QVBoxLayout *composerL = new QVBoxLayout(composer);
    composerL->setContentsMargins(8, 8, 8, 8);
    composerL->setSpacing(6);
    QWidget *attachRow = new QWidget(composer);
    QHBoxLayout *attachRowL = new QHBoxLayout(attachRow);
    attachRowL->setContentsMargins(0, 0, 0, 0);
    attachRowL->setSpacing(6);
    attachPreviewLbl_ = new QLabel(attachRow);
    attachPreviewLbl_->setVisible(false);
    attachPreviewLbl_->setWordWrap(true);
    attachPreviewLbl_->setStyleSheet("font-size:12px;font-weight:700;color:#334155;background:#EEF2FF;border:1px solid #C7D2FE;border-radius:8px;padding:6px 10px;");
    attachClearBtn_ = new QPushButton(QString::fromUtf8("✕"), attachRow);
    attachClearBtn_->setVisible(false);
    attachClearBtn_->setFixedSize(s_(26), s_(26));
    attachClearBtn_->setToolTip("Открепить вложение");
    attachClearBtn_->setStyleSheet("QPushButton{background:#FEE2E2;color:#B91C1C;border:none;border-radius:13px;font-weight:900;}"
                                   "QPushButton:hover{background:#FCA5A5;}");
    connect(attachClearBtn_, &QPushButton::clicked, this, [this]() {
        clearPendingAttachment();
        clearPendingForward();
    });
    attachRowL->addWidget(attachPreviewLbl_, 1);
    attachRowL->addWidget(attachClearBtn_, 0, Qt::AlignTop);
    composerL->addWidget(attachRow);
    QHBoxLayout *replyRow = new QHBoxLayout();
    replyRow->setContentsMargins(0, 0, 0, 0);
    replyRow->setSpacing(8);
    replyEdit_ = new QLineEdit(this);
    replyEdit_->setPlaceholderText("Введите ответ...");
    replyEdit_->installEventFilter(this);
    attachBtn_ = new QPushButton(this);
    attachBtn_->setFixedWidth(s_(44));
    attachBtn_->setToolTip("Прикрепить файл");
    attachBtn_->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    attachBtn_->setIconSize(QSize(s_(18), s_(18)));
    attachBtn_->setStyleSheet("QPushButton{background:#E2E8F0;color:#334155;font-size:18px;font-weight:900;padding:8px 10px;} QPushButton:hover{background:#CBD5E1;}");
    connect(attachBtn_, &QPushButton::clicked, this, &TaskChatWidget::sendAttachment);
    sendBtn_ = new QPushButton("Отправить", this);
    sendBtn_->setStyleSheet("QPushButton{background:#2B6BFF;color:white;font-weight:700;padding:8px 14px;} QPushButton:hover{background:#245DE0;}");
    connect(sendBtn_, &QPushButton::clicked, this, &TaskChatWidget::sendReply);
    connect(replyEdit_, &QLineEdit::returnPressed, this, &TaskChatWidget::sendReply);
    replyRow->addWidget(attachBtn_);
    replyRow->addWidget(replyEdit_, 1);
    replyRow->addWidget(sendBtn_);
    composerL->addLayout(replyRow);
    root->addWidget(composer);

    refreshMessages();
    liveRefreshTimer_ = new QTimer(this);
    // Более редкий polling заметно снижает нагрузку на удаленную БД и UI.
    liveRefreshTimer_->setInterval(3000);
    connect(liveRefreshTimer_, &QTimer::timeout, this, [this]() {
        if (!isVisible() || threadId_ <= 0)
            return;
        refreshMessages(false);
    });
    liveRefreshTimer_->start();
    presenceRefreshTimer_ = new QTimer(this);
    presenceRefreshTimer_->setInterval(180000);
    connect(presenceRefreshTimer_, &QTimer::timeout, this, [this]() {
        if (!isVisible() || peerUsername_.trimmed().isEmpty())
            return;
        updatePeerHeaderMeta();
    });
    presenceRefreshTimer_->start();
}

QWidget* TaskChatWidget::createMessageRow(const TaskChatMessage &m)
{
    const bool mine = (m.fromUser == currentUser_);
    QString forwardedFrom;
    QString payload = m.message;
    const bool isForwarded = decodeForwardMessage(m.message, forwardedFrom, payload);
    
    // Проверяем, есть ли комбинированное сообщение (текст + вложение)
    QString messageText;
    QString attachmentPayload;
    const bool isCombined = splitCombinedMessage(payload, messageText, attachmentPayload);
    
    QString fileName;
    QString fileMime;
    bool isAttachment = false;
    QString visibleText;
    QString specialLabel;
    bool isSpecial = false;
    
    if (isCombined) {
        // Комбинированное сообщение: есть и текст, и вложение
        visibleText = messageText;
        isAttachment = decodeAttachmentMeta(attachmentPayload, fileName, fileMime);
        // Для декодирования используем attachmentPayload
        payload = attachmentPayload;
    } else {
        // Обычное сообщение: проверяем, есть ли вложение
        isAttachment = decodeAttachmentMeta(payload, fileName, fileMime);
        if (isAttachment) {
            visibleText.clear();
        } else {
            isSpecial = decodeSpecialMessage(payload, specialLabel, visibleText);
        }
    }
    QWidget *row = new QWidget(this);
    const bool wasSelected = selectedMessageIds_.contains(m.id);
    row->setStyleSheet(wasSelected ? "background:rgba(59, 130, 246, 0.25);border-radius:8px;" : "background:transparent;");
    row->setProperty("messageId", m.id);
    row->setProperty("mine", mine);
    row->setProperty("fromUser", m.fromUser);
    row->setProperty("rawMessage", m.message);
    row->setProperty("text", visibleText);
    row->setProperty("isAttachment", isAttachment);
    if (isAttachment) {
        row->setProperty("attachmentFileName", fileName);
        row->setProperty("attachmentMime", fileMime);
    }
    row->setProperty("selected", wasSelected);
    row->setProperty("messageDate", m.createdAt.date());
    row->setObjectName("chatMessageRow");
    QHBoxLayout *rowL = new QHBoxLayout(row);
    rowL->setContentsMargins(0, 0, 0, 0);
    rowL->setSpacing(6);
    QCheckBox *chk = new QCheckBox(row);
    chk->setVisible(selectionMode_);
    chk->setChecked(wasSelected);
    chk->setProperty("messageCheck", true);
    connect(chk, &QCheckBox::toggled, this, [this, row](bool on) {
        setMessageRowSelected(row, on);
    });
    QFrame *bubble = new QFrame(row);
    bubble->setStyleSheet(QString("QFrame{background:%1;border:none;border-radius:14px;}").arg(mine ? "#2B6BFF" : "#FFFFFF"));
    QVBoxLayout *bubbleL = new QVBoxLayout(bubble);
    bubbleL->setContentsMargins(12, 8, 12, 6);
    bubbleL->setSpacing(4);
    QLabel *text = new QLabel(visibleText, bubble);
    text->setWordWrap(true);
    text->setTextInteractionFlags(Qt::NoTextInteraction);
    text->setContextMenuPolicy(Qt::NoContextMenu);
    text->setMaximumWidth(qMax(220, int(width() * 0.55)));
    text->setStyleSheet(QString("font-size:13px; color:%1;").arg(mine ? "#FFFFFF" : "#0F172A"));
    QString metaText = QString("%1 • %2").arg(m.fromUser, m.createdAt.toString("dd.MM.yy hh:mm"));
    if (mine)
        metaText += QStringLiteral(" ✓✓");
    QLabel *meta = new QLabel(metaText, bubble);
    meta->setContextMenuPolicy(Qt::NoContextMenu);
    meta->setStyleSheet(QString("font-size:10px; color:%1;").arg(mine ? "#DCE8FF" : "#64748B"));
    if (isSpecial) {
        QLabel *status = new QLabel(
            specialLabel.isEmpty() ? QStringLiteral("Особое сообщение")
                                   : QStringLiteral("По задаче: ") + specialLabel,
            bubble
        );
        status->setStyleSheet(QString("font-size:10px; font-weight:700; color:%1;")
                              .arg(mine ? "#CFE1FF" : "#2563EB"));
        bubbleL->addWidget(status, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
    }
    if (isForwarded) {
        QLabel *fwd = new QLabel(QString("Переслано от %1").arg(forwardedFrom.isEmpty() ? QStringLiteral("неизвестно") : forwardedFrom), bubble);
        fwd->setStyleSheet(QString("font-size:11px;font-weight:800;color:%1;background:%2;border-radius:6px;padding:4px 8px;")
                           .arg(mine ? "#DBEAFE" : "#1E3A8A")
                           .arg(mine ? "#1E40AF" : "#DBEAFE"));
        bubbleL->addWidget(fwd, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
    }
    // Сначала вложение (если есть), потом текст
    if (isAttachment) {
        if (isImageAttachment(fileMime, fileName)) {
            // Декодируем вложение и показываем превью
            QString decodedName;
            QString decodedMime;
            QByteArray imageData;
            // Для комбинированных сообщений используем attachmentPayload, для обычных - m.message
            const QString &msgToDecode = isCombined ? attachmentPayload : m.message;
            bool hasImage = decodeAttachmentMessage(msgToDecode, decodedName, decodedMime, imageData);

            if (hasImage && !imageData.isEmpty()) {
                // Создаём QLabel с pixmap превью
                QLabel *imagePreview = new QLabel(bubble);
                imagePreview->setAlignment(Qt::AlignCenter);
                imagePreview->setCursor(Qt::PointingHandCursor);
                imagePreview->setStyleSheet(
                    "QLabel{border:1px solid #BFDBFE;border-radius:10px;background:#EFF6FF;}"
                );
                imagePreview->setMinimumSize(200, 120);
                imagePreview->setMaximumWidth(240);

                QPixmap pixmap;
                if (pixmap.loadFromData(imageData)) {
                    // Масштабируем изображение для превью
                    QPixmap scaled = pixmap.scaled(200, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    imagePreview->setPixmap(scaled);
                } else {
                    imagePreview->setText(QStringLiteral("Изображение\n%1").arg(fileName));
                    imagePreview->setStyleSheet(
                        "QLabel{border:1px solid #BFDBFE;border-radius:10px;background:#EFF6FF;color:#1D4ED8;font-weight:800;padding:8px;text-align:center;}"
                    );
                }

                bubbleL->addWidget(imagePreview, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
            } else {
                // Если не удалось загрузить, показываем кнопку
                QPushButton *thumb = new QPushButton(QStringLiteral("Изображение\n%1").arg(fileName), bubble);
                thumb->setFlat(false);
                thumb->setFixedSize(180, 72);
                thumb->setStyleSheet(
                    "QPushButton{border:1px solid #BFDBFE;border-radius:10px;background:#EFF6FF;color:#1D4ED8;font-weight:800;padding:8px;text-align:left;}"
                    "QPushButton:hover{background:#DBEAFE;border:1px solid #60A5FA;}"
                );
                bubbleL->addWidget(thumb, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
            }
        } else {
            QPushButton *doc = new QPushButton(QString("Файл\n%1").arg(fileName), bubble);
            doc->setStyleSheet(QString("QPushButton{background:%1;border:none;border-radius:8px;padding:8px 10px;font-size:14px;font-weight:900;color:%2;}"
                                       "QPushButton:hover{background:%3;}")
                               .arg(mine ? "#1D4ED8" : "#E2E8F0")
                               .arg(mine ? "#FFFFFF" : "#334155")
                               .arg(mine ? "#1E40AF" : "#CBD5E1"));
            bubbleL->addWidget(doc, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
        }
    }
    // Текст после вложения
    if (!visibleText.isEmpty())
        bubbleL->addWidget(text);
    bubbleL->addWidget(meta, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
    if (mine) {
        rowL->addStretch();
        rowL->addWidget(chk, 0, Qt::AlignVCenter);
        rowL->addWidget(bubble, 0, Qt::AlignRight);
    } else {
        rowL->addWidget(bubble, 0, Qt::AlignLeft);
        rowL->addWidget(chk, 0, Qt::AlignVCenter);
        rowL->addStretch();
    }
    row->installEventFilter(this);
    bubble->installEventFilter(this);
    text->installEventFilter(this);
    meta->installEventFilter(this);
    chk->installEventFilter(this);
    return row;
}

void TaskChatWidget::updateDayLabel()
{
    if (!dayLabel_ || !scrollArea_ || !messagesLayout_)
        return;
    QScrollBar *sb = scrollArea_ ? scrollArea_->verticalScrollBar() : nullptr;
    QWidget *host = scrollArea_->widget();
    if (!sb || !host)
        return;
    const int scrollVal = sb->value();
    QWidget *topRow = nullptr;
    for (int i = 0; i < messagesLayout_->count(); ++i) {
        QLayoutItem *item = messagesLayout_->itemAt(i);
        if (!item || !item->widget()) continue;
        QWidget *w = item->widget();
        if (w->objectName() != "chatMessageRow") continue;
        int y = w->mapTo(host, QPoint(0, 0)).y();
        if (y + w->height() > scrollVal) {
            topRow = w;
            break;
        }
    }
    if (!topRow) {
        for (int i = messagesLayout_->count() - 1; i >= 0; --i) {
            QLayoutItem *item = messagesLayout_->itemAt(i);
            if (!item || !item->widget()) continue;
            QWidget *w = item->widget();
            if (w->objectName() == "chatMessageRow") {
                topRow = w;
                break;
            }
        }
    }
    if (!topRow) {
        dayLabel_->hide();
        return;
    }
    QDate d = topRow->property("messageDate").toDate();
    if (!d.isValid()) {
        dayLabel_->hide();
        return;
    }
    dayLabel_->show();
    QDate today = QDate::currentDate();
    if (d == today)
        dayLabel_->setText(QStringLiteral("Сегодня"));
    else if (d == today.addDays(-1))
        dayLabel_->setText(QStringLiteral("Вчера"));
    else {
        static const char *months[] = {"января","февраля","марта","апреля","мая","июня",
            "июля","августа","сентября","октября","ноября","декабря"};
        int m = d.month();
        dayLabel_->setText(QString("%1 %2").arg(d.day()).arg(QString::fromUtf8(months[m - 1])));
    }
}

void TaskChatWidget::refreshMessages(bool fullReload)
{
    if (threadId_ <= 0)
        return;

    bool keepAtBottom = forceScrollToBottom_;
    if (scrollArea_ && scrollArea_->verticalScrollBar()) {
        QScrollBar *sb = scrollArea_->verticalScrollBar();
        keepAtBottom = keepAtBottom || (sb->maximum() - sb->value()) < 30;
    }

    // Если forceScrollToBottom_, делаем полную перезагрузку
    if (forceScrollToBottom_)
        fullReload = true;

    QWidget *messagesHost = scrollArea_ ? scrollArea_->widget() : nullptr;
    if (messagesHost && fullReload)
        messagesHost->setUpdatesEnabled(false);

    QVector<TaskChatMessage> msgs;
    if (!fullReload && lastLoadedMessageId_ > 0) {
        msgs = getMessagesForThreadFrom(threadId_, currentUser_, lastLoadedMessageId_ + 1);
        if (msgs.isEmpty()) {
            forceScrollToBottom_ = false;
            return;
        }
    } else {
        while (QLayoutItem *item = messagesLayout_->takeAt(0)) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        msgs = getMessagesForThreadLastN(threadId_, currentUser_, kMessagesPageSize);
    }

    if (fullReload || peerUsername_.trimmed().isEmpty()) {
        TaskChatThread threadState = getThreadById(threadId_);
        QString otherUser = (threadState.createdBy == currentUser_) ? threadState.recipientUser : threadState.createdBy;
        if (otherUser.trimmed().isEmpty() || otherUser == currentUser_)
            otherUser = peerHint_;
        peerUsername_ = otherUser;
        updatePeerHeaderMeta();
    }

    if (fullReload) {
        oldestLoadedMessageId_ = 0;
        lastLoadedMessageId_ = 0;
        historyExhausted_ = false;
        for (const TaskChatMessage &m : msgs) {
            messagesLayout_->addWidget(createMessageRow(m));
            oldestLoadedMessageId_ = (oldestLoadedMessageId_ <= 0) ? m.id : qMin(oldestLoadedMessageId_, m.id);
            lastLoadedMessageId_ = qMax(lastLoadedMessageId_, m.id);
        }
        messagesLayout_->addStretch();
    } else if (!msgs.isEmpty()) {
        QLayoutItem *tail = nullptr;
        if (messagesLayout_->count() > 0) {
            tail = messagesLayout_->takeAt(messagesLayout_->count() - 1);
            if (tail && tail->widget()) {
                delete tail->widget();
                delete tail;
                tail = nullptr;
            }
        }
        for (const TaskChatMessage &m : msgs) {
            messagesLayout_->addWidget(createMessageRow(m));
            lastLoadedMessageId_ = qMax(lastLoadedMessageId_, m.id);
        }
        messagesLayout_->addStretch();
        if (tail && tail->spacerItem())
            delete tail;
    }

    if (fullReload) {
        if (msgs.isEmpty()) {
            oldestLoadedMessageId_ = 0;
            lastLoadedMessageId_ = 0;
            historyExhausted_ = true;
        } else if (msgs.size() < kMessagesPageSize) {
            historyExhausted_ = true;
        }
    }
    if (scrollArea_ && scrollArea_->verticalScrollBar() && keepAtBottom) {
        QTimer::singleShot(0, this, [this]() {
            if (scrollArea_ && scrollArea_->verticalScrollBar())
                scrollArea_->verticalScrollBar()->setValue(scrollArea_->verticalScrollBar()->maximum());
            updateDayLabel();
        });
    }
    forceScrollToBottom_ = false;
    updateDeleteButtonVisibility();
    updateDayLabel();
    if (messagesHost && fullReload) {
        messagesHost->setUpdatesEnabled(true);
        messagesHost->update();
    }
    if (selectionMode_) {
        if (peerLbl_) peerLbl_->setVisible(false);
        if (moreBtn_) moreBtn_->setVisible(false);
    }
}

void TaskChatWidget::loadOlderMessages()
{
    if (threadId_ <= 0 || !messagesLayout_ || !scrollArea_ || loadingOlderMessages_ || historyExhausted_ || oldestLoadedMessageId_ <= 0)
        return;

    loadingOlderMessages_ = true;
    QWidget *messagesHost = scrollArea_->widget();
    QScrollBar *sb = scrollArea_->verticalScrollBar();
    const int oldValue = sb ? sb->value() : 0;
    const int oldMax = sb ? sb->maximum() : 0;
    if (messagesHost)
        messagesHost->setUpdatesEnabled(false);

    QLayoutItem *tail = nullptr;
    if (messagesLayout_->count() > 0) {
        tail = messagesLayout_->takeAt(messagesLayout_->count() - 1);
        if (tail && tail->widget()) {
            delete tail->widget();
            delete tail;
            tail = nullptr;
        }
    }

    const QVector<TaskChatMessage> older = getMessagesForThreadOlderThan(
        threadId_, currentUser_, oldestLoadedMessageId_, kMessagesPageSize);
    if (older.isEmpty()) {
        historyExhausted_ = true;
        messagesLayout_->addStretch();
    } else {
        int insertIndex = 0;
        for (const TaskChatMessage &m : older) {
            messagesLayout_->insertWidget(insertIndex++, createMessageRow(m));
            oldestLoadedMessageId_ = (oldestLoadedMessageId_ <= 0) ? m.id : qMin(oldestLoadedMessageId_, m.id);
        }
        if (older.size() < kMessagesPageSize)
            historyExhausted_ = true;
        messagesLayout_->addStretch();
    }
    if (tail && tail->spacerItem())
        delete tail;

    if (messagesHost) {
        messagesHost->setUpdatesEnabled(true);
        messagesHost->update();
    }
    if (sb) {
        QTimer::singleShot(0, this, [this, sb, oldValue, oldMax]() {
            const int delta = sb->maximum() - oldMax;
            sb->setValue(oldValue + qMax(0, delta));
            updateDayLabel();
        });
    } else {
        updateDayLabel();
    }
    loadingOlderMessages_ = false;
}

void TaskChatWidget::sendReply()
{
    if (!replyEdit_) return;
    QString msg = replyEdit_->text().trimmed();
    if (msg.isEmpty() && pendingAttachData_.isEmpty() && pendingForwardRaw_.isEmpty()) return;
    QString err;
    bool sentAny = false;
    QString notifBody;

    // Если есть и текст, и вложение - объединяем в одно сообщение
    if (!msg.isEmpty() && !pendingAttachData_.isEmpty()) {
        if (replyToMessageId_ > 0)
            msg = QString("↩ %1\n%2").arg(replyToMessageId_).arg(msg);
        
        // Кодируем вложение
        const QString attachmentPayload = encodeAttachmentMessage(pendingAttachName_, pendingAttachMime_, pendingAttachData_);
        
        // Создаём комбинированное сообщение: текст + разделитель + вложение
        QString combinedMessage = msg + "\n[[ATTACHMENT]]\n" + attachmentPayload;
        
        if (g_pendingSpecialByThread.contains(threadId_)) {
            combinedMessage = encodeSpecialMessage(g_pendingSpecialByThread.value(threadId_), combinedMessage);
            g_pendingSpecialByThread.remove(threadId_);
        }
        
        if (!addChatMessage(threadId_, currentUser_, combinedMessage, err)) {
            QMessageBox::warning(this, "Ошибка", err);
            return;
        }
        sentAny = true;
        notifBody = QString("%1 + Файл: %2").arg(msg.left(50)).arg(pendingAttachName_);
        logAction(currentUser_, "chat_reply_with_file", QString("thread=%1 file=%2").arg(threadId_).arg(pendingAttachName_));
        clearPendingAttachment();
    } else {
        // Отправляем только текст
        if (!msg.isEmpty()) {
            if (replyToMessageId_ > 0)
                msg = QString("↩ %1\n%2").arg(replyToMessageId_).arg(msg);
            QString toStore = msg;
            if (g_pendingSpecialByThread.contains(threadId_)) {
                toStore = encodeSpecialMessage(g_pendingSpecialByThread.value(threadId_), msg);
                g_pendingSpecialByThread.remove(threadId_);
            }
            if (!addChatMessage(threadId_, currentUser_, toStore, err)) {
                QMessageBox::warning(this, "Ошибка", err);
                return;
            }
            sentAny = true;
            notifBody = msg.left(80);
            logAction(currentUser_, "chat_reply", QString("thread=%1").arg(threadId_));
        }

        // Отправляем только вложение
        if (!pendingAttachData_.isEmpty()) {
            const QString payload = encodeAttachmentMessage(pendingAttachName_, pendingAttachMime_, pendingAttachData_);
            if (!addChatMessage(threadId_, currentUser_, payload, err)) {
                QMessageBox::warning(this, "Ошибка", err);
                return;
            }
            sentAny = true;
            if (notifBody.isEmpty())
                notifBody = QString("Файл: %1").arg(pendingAttachName_);
            logAction(currentUser_, "chat_file_sent", QString("thread=%1 file=%2 size=%3").arg(threadId_).arg(pendingAttachName_).arg(pendingAttachData_.size()));
            clearPendingAttachment();
        }
    }

    if (!pendingForwardRaw_.isEmpty()) {
        const QString wrapped = encodeForwardMessage(pendingForwardFrom_.isEmpty() ? QStringLiteral("неизвестно") : pendingForwardFrom_, pendingForwardRaw_);
        if (!addChatMessage(threadId_, currentUser_, wrapped, err)) {
            QMessageBox::warning(this, "Ошибка", err);
            return;
        }
        sentAny = true;
        if (notifBody.isEmpty())
            notifBody = QString("Пересланное сообщение");
        clearPendingForward();
    }

    if (!sentAny) return;

    lastSentMs_ = QDateTime::currentMSecsSinceEpoch();
    replyEdit_->clear();
    forceScrollToBottom_ = true;
    TaskChatThread t = getThreadById(threadId_);
    QString title = "Новое сообщение в чате";
    const QString fromName = userDisplayName(currentUser_);
    QString body = QString("[chat:%1] %2: %3").arg(threadId_).arg(fromName, notifBody.left(80));
    if (currentUser_ == t.createdBy) {
        if (!t.recipientUser.isEmpty()) addNotificationForUser(t.recipientUser, title, body);
        else {
            QVector<UserInfo> all = getAllUsers(false);
            for (const UserInfo &u : all)
                if (u.role == "admin" || u.role == "tech") addNotificationForUser(u.username, title, body);
        }
    } else {
        addNotificationForUser(t.createdBy, title, body);
    }
    DataBus::instance().triggerNotificationsChanged();
    replyToMessageId_ = 0;
    if (replyBannerLbl_) replyBannerLbl_->setVisible(false);
    refreshMessages(false);
    
    // Принудительно скроллим вниз после отправки
    if (scrollArea_ && scrollArea_->verticalScrollBar()) {
        QTimer::singleShot(50, this, [this]() {
            if (scrollArea_ && scrollArea_->verticalScrollBar()) {
                scrollArea_->verticalScrollBar()->setValue(scrollArea_->verticalScrollBar()->maximum());
            }
        });
    }
}

void TaskChatWidget::sendAttachment()
{
    if (threadId_ <= 0)
        return;
    QString name;
    QString mime;
    QByteArray bytes;
    QString pickError;
    if (!pickAttachmentPayload(this, name, mime, bytes, pickError)) {
        if (!pickError.isEmpty())
            QMessageBox::warning(this, "Файл", pickError);
        return;
    }
    setPendingAttachment(name, mime, bytes);
}

void TaskChatWidget::setPendingAttachment(const QString &name, const QString &mime, const QByteArray &data)
{
    pendingAttachName_ = name;
    pendingAttachMime_ = mime;
    pendingAttachData_ = data;
    refreshPendingPreview();
}

void TaskChatWidget::clearPendingAttachment()
{
    pendingAttachName_.clear();
    pendingAttachMime_.clear();
    pendingAttachData_.clear();
    refreshPendingPreview();
}

void TaskChatWidget::setPendingForward(const QString &fromUser, const QString &rawPayload)
{
    pendingForwardFrom_ = fromUser.trimmed();
    pendingForwardRaw_ = rawPayload;
    refreshPendingPreview();
}

void TaskChatWidget::clearPendingForward()
{
    pendingForwardFrom_.clear();
    pendingForwardRaw_.clear();
    refreshPendingPreview();
}

void TaskChatWidget::refreshPendingPreview()
{
    if (!attachPreviewLbl_) return;
    QStringList lines;
    if (!pendingForwardRaw_.isEmpty()) {
        lines << QString("Пересланное сообщение от: %1").arg(pendingForwardFrom_.isEmpty() ? QStringLiteral("неизвестно") : pendingForwardFrom_);
    }
    if (!pendingAttachData_.isEmpty()) {
        lines << QString("Вложение: %1 (%2 KB)").arg(pendingAttachName_).arg(qMax(1, pendingAttachData_.size() / 1024));
    }
    if (lines.isEmpty()) {
        attachPreviewLbl_->clear();
        attachPreviewLbl_->setVisible(false);
        if (attachClearBtn_) attachClearBtn_->setVisible(false);
        return;
    }
    attachPreviewLbl_->setText(lines.join("\n"));
    attachPreviewLbl_->setVisible(true);
    if (attachClearBtn_) attachClearBtn_->setVisible(true);
}

void TaskChatWidget::jumpToMessage(int messageId)
{
    if (messageId <= 0 || !messagesLayout_ || !scrollArea_)
        return;

    forceScrollToBottom_ = false;
    int attempts = 0;
    while (attempts < 10) {
        QWidget *target = nullptr;
        for (int i = 0; i < messagesLayout_->count(); ++i) {
            QLayoutItem *it = messagesLayout_->itemAt(i);
            if (!it || !it->widget()) continue;
            QWidget *w = it->widget();
            if (w->objectName() != "chatMessageRow") continue;
            if (w->property("messageId").toInt() == messageId) {
                target = w;
                break;
            }
        }
        if (target) {
            scrollArea_->ensureWidgetVisible(target, 0, s_(80));
            target->setStyleSheet("background:rgba(80, 118, 251, 0.25);border-radius:8px;");
            QTimer::singleShot(1200, target, [target]() {
                if (target)
                    target->setStyleSheet(target->property("selected").toBool()
                                              ? "background:rgba(59, 130, 246, 0.25);border-radius:8px;"
                                              : "background:transparent;");
            });
            return;
        }
        if (historyExhausted_ || oldestLoadedMessageId_ <= 0)
            break;
        loadOlderMessages();
        ++attempts;
    }
}

void TaskChatWidget::showMediaHistoryDialog()
{
    if (threadId_ <= 0)
        return;

    const QVector<TaskChatMessage> msgs = getMessagesForThread(threadId_, currentUser_);
    struct MediaItem { int id; QString fn; QString mt; QByteArray data; QString meta; };
    QVector<MediaItem> media;
    for (const TaskChatMessage &m : msgs) {
        QString fn, mt;
        QByteArray data;
        if (!decodeAttachmentMessage(m.message, fn, mt, data))
            continue;
        media.push_back({m.id, fn, mt, data, QString("%1\n%2").arg(m.fromUser, m.createdAt.toString("dd.MM.yy hh:mm"))});
    }

    QDialog dlg(this);
    dlg.setWindowTitle("Медиа и документы");
    dlg.setMinimumSize(s_(900), s_(620));
    QVBoxLayout *root = new QVBoxLayout(&dlg);
    QScrollArea *scroll = new QScrollArea(&dlg);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *host = new QWidget(scroll);
    QGridLayout *grid = new QGridLayout(host);
    grid->setContentsMargins(6, 6, 6, 6);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    if (media.isEmpty()) {
        QLabel *empty = new QLabel("Нет медиа/документов", host);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("font-size:14px;font-weight:700;color:#64748B;");
        grid->addWidget(empty, 0, 0, 1, 5);
    } else {
        for (int i = 0; i < media.size(); ++i) {
            const MediaItem &it = media[i];
            QToolButton *card = new QToolButton(host);
            card->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            card->setFixedSize(160, 170);
            card->setContextMenuPolicy(Qt::CustomContextMenu);
            card->setText(QString("%1\n%2").arg(it.fn, it.meta));
            card->setProperty("messageId", it.id);
            card->setProperty("fileName", it.fn);
            card->setProperty("mimeType", it.mt);
            card->setProperty("bytes", it.data);
            if (isImageAttachment(it.mt, it.fn)) {
                QPixmap pm;
                pm.loadFromData(it.data);
                if (!pm.isNull())
                    card->setIcon(QIcon(pm.scaled(136, 96, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)));
            } else {
                QPixmap docPm(136, 96);
                docPm.fill(QColor("#E2E8F0"));
                QPainter p(&docPm);
                p.setPen(QColor("#334155"));
                QFont f("Inter", 18, QFont::Bold);
                p.setFont(f);
                p.drawText(docPm.rect(), Qt::AlignCenter, shortFileTypeLabel(it.fn));
                card->setIcon(QIcon(docPm));
            }
            card->setIconSize(QSize(136, 96));
            card->setStyleSheet(
                "QToolButton{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:10px;padding:6px;font-size:11px;text-align:left;}"
                "QToolButton:hover{background:rgba(80,118,251,36);border:1px solid rgb(80,118,251);}"
            );
            connect(card, &QToolButton::clicked, &dlg, [this, card]() {
                const QString fn = card->property("fileName").toString();
                const QString mt = card->property("mimeType").toString();
                const QByteArray data = card->property("bytes").toByteArray();
                openAttachmentInsideApp(this, fn, mt, data);
            });
            connect(card, &QToolButton::customContextMenuRequested, &dlg, [this, card, &dlg](const QPoint &pos) {
                QMenu menu(card);
                QAction *goAct = menu.addAction("Перейти к сообщению");
                QAction *chosen = menu.exec(card->mapToGlobal(pos));
                if (chosen == goAct) {
                    jumpToMessage(card->property("messageId").toInt());
                    dlg.accept();
                }
            });
            grid->addWidget(card, i / 5, i % 5);
        }
    }
    scroll->setWidget(host);
    root->addWidget(scroll, 1);

    QPushButton *closeBtn = new QPushButton("Закрыть", &dlg);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    root->addWidget(closeBtn, 0, Qt::AlignRight);
    dlg.exec();
}

void TaskChatWidget::deleteThreadByAdmin()
{
    if (!isAdmin_) return;
    if (QMessageBox::question(this, "Удаление чата", "Удалить чат полностью?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    QString err;
    if (!deleteThread(threadId_, err)) { QMessageBox::warning(this, "Ошибка", err); return; }
    logAction(currentUser_, "chat_deleted_admin", QString("thread=%1").arg(threadId_));
    DataBus::instance().triggerNotificationsChanged();
    emit backRequested();
}

void TaskChatWidget::deleteThreadForMe()
{
    if (QMessageBox::question(this, "Удаление у себя", "Скрыть чат только у вас?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    QString err;
    if (!hideThreadForUser(threadId_, currentUser_, err)) { QMessageBox::warning(this, "Ошибка", err); return; }
    logAction(currentUser_, "chat_hidden_for_me", QString("thread=%1").arg(threadId_));
    DataBus::instance().triggerNotificationsChanged();
    emit backRequested();
}

void TaskChatWidget::deleteThreadForAll()
{
    if (QMessageBox::question(this, "Удаление у всех", "Удалить чат у всех?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    QString err;
    if (!deleteThread(threadId_, err)) { QMessageBox::warning(this, "Ошибка", err); return; }
    logAction(currentUser_, "chat_deleted_for_all", QString("thread=%1").arg(threadId_));
    DataBus::instance().triggerNotificationsChanged();
    emit backRequested();
}

bool TaskChatWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == pinnedLbl_ && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && pinnedMessageId_ > 0) {
            jumpToMessage(pinnedMessageId_);
            return true;
        }
    }

    if (obj == peerLbl_ && event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            showMediaHistoryDialog();
            return true;
        }
    }

    if (obj == replyEdit_ && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->matches(QKeySequence::Paste)) {
            QString name;
            QString mime;
            QByteArray bytes;
            QString err;
            if (pickAttachmentFromClipboard(name, mime, bytes, err)) {
                setPendingAttachment(name, mime, bytes);
                return true;
            }
            if (!err.isEmpty()) {
                QMessageBox::warning(this, "Файл", err);
                return true;
            }
        }
    }

    if (!scrollArea_ || !messagesLayout_)
        return QWidget::eventFilter(obj, event);

    QWidget *objWidget = qobject_cast<QWidget*>(obj);
    const bool fromChatArea = (obj == scrollArea_->viewport()) ||
                              (objWidget && scrollArea_->widget() && scrollArea_->widget()->isAncestorOf(objWidget));
    if (!fromChatArea) {
        return QWidget::eventFilter(obj, event);
    }

    if (event->type() == QEvent::ContextMenu) {
        QContextMenuEvent *ce = static_cast<QContextMenuEvent*>(event);
        showMessageContextMenu(ce->globalPos());
        return true;
    }

    if (!selectionMode_ && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            // Проверяем, кликнули ли по превью изображения (QLabel с pixmap)
            QLabel *imageLabel = qobject_cast<QLabel*>(obj);
            if (imageLabel && imageLabel->pixmap() && !imageLabel->pixmap()->isNull()) {
                // Нашли QLabel с изображением - ищем родительский row
                QWidget *row = imageLabel;
                while (row && row->objectName() != "chatMessageRow") {
                    row = row->parentWidget();
                }
                if (row && row->property("isAttachment").toBool()) {
                    const QString fn = row->property("attachmentFileName").toString();
                    const QString mt = row->property("attachmentMime").toString();
                    const QString raw = row->property("rawMessage").toString();
                    QString decodedName;
                    QString decodedMime;
                    QByteArray data;
                    if (decodeAttachmentFromStoredMessage(raw, decodedName, decodedMime, data) && !data.isEmpty()) {
                        const int mid = row->property("messageId").toInt();
                        openAttachmentInsideApp(this, fn, mt, data, currentUser_, mid);
                        return true;
                    }
                }
            }

            QWidget *row = messageRowAtGlobalPos(me->globalPos());
            if (row && row->property("isAttachment").toBool()) {
                const QString fn = row->property("attachmentFileName").toString();
                const QString mt = row->property("attachmentMime").toString();
                const QString raw = row->property("rawMessage").toString();
                QString decodedName;
                QString decodedMime;
                QByteArray data;
                if (!decodeAttachmentFromStoredMessage(raw, decodedName, decodedMime, data) || data.isEmpty())
                    return true;
                const int mid = row->property("messageId").toInt();
                if (isImageAttachment(mt, fn)) {
                    const bool deleted = openAttachmentInsideApp(this, fn, mt, data, currentUser_, mid);
                    if (deleted) refreshMessages();
                } else {
                    openAttachmentData(this, fn, data);
                }
                return true;
            }
        }
    }

    if (selectionMode_ && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            QWidget *row = messageRowAtGlobalPos(me->globalPos());
            if (row) {
                draggingSelection_ = true;
                dragSelectValue_ = !isMessageRowSelected(row);
                setMessageRowSelected(row, dragSelectValue_);
                return true;
            }
        }
    }
    if (selectionMode_ && event->type() == QEvent::MouseMove && draggingSelection_) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        QWidget *row = messageRowAtGlobalPos(me->globalPos());
        if (row) {
            setMessageRowSelected(row, dragSelectValue_);
            return true;
        }
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        draggingSelection_ = false;
    }

    return QWidget::eventFilter(obj, event);
}

void TaskChatWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event || !event->mimeData()) return;
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &u : event->mimeData()->urls()) {
            if (u.isLocalFile()) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void TaskChatWidget::dropEvent(QDropEvent *event)
{
    if (!event || !event->mimeData()) return;
    if (!event->mimeData()->hasUrls()) return;
    for (const QUrl &u : event->mimeData()->urls()) {
        if (!u.isLocalFile()) continue;
        QString name, mime, err;
        QByteArray bytes;
        if (!loadAttachmentFromLocalPath(u.toLocalFile(), name, mime, bytes, err)) {
            if (!err.isEmpty()) QMessageBox::warning(this, "Файл", err);
            continue;
        }
        setPendingAttachment(name, mime, bytes);
        event->acceptProposedAction();
        return;
    }
}

void TaskChatWidget::enterSelectionMode()
{
    selectionMode_ = true;
    if (peerLbl_) peerLbl_->setVisible(false);
    if (cancelSelectionBtn_) cancelSelectionBtn_->setVisible(true);
    updateDeleteButtonVisibility();
    refreshMessages(true);
}

void TaskChatWidget::exitSelectionMode()
{
    selectionMode_ = false;
    selectedMessageIds_.clear();
    if (peerLbl_) peerLbl_->setVisible(true);
    if (moreBtn_) moreBtn_->setVisible(true);
    if (deleteSelectedBtn_) deleteSelectedBtn_->setVisible(false);
    if (cancelSelectionBtn_) cancelSelectionBtn_->setVisible(false);
    if (selectionCountLbl_) selectionCountLbl_->setVisible(false);
    refreshMessages(true);
}

QWidget* TaskChatWidget::messageRowAtGlobalPos(const QPoint &globalPos) const
{
    if (!scrollArea_ || !messagesLayout_) return nullptr;
    QWidget *host = scrollArea_->widget();
    if (!host) return nullptr;
    const QPoint posInHost = host->mapFromGlobal(globalPos);
    QWidget *nearest = nullptr;
    int nearestDist = INT_MAX;
    for (int i = 0; i < messagesLayout_->count(); ++i) {
        QLayoutItem *item = messagesLayout_->itemAt(i);
        if (!item || !item->widget()) continue;
        QWidget *row = item->widget();
        if (row->objectName() != "chatMessageRow") continue;
        if (row->geometry().contains(posInHost)) return row;
        const int dy = qAbs(row->geometry().center().y() - posInHost.y());
        if (dy < nearestDist) {
            nearestDist = dy;
            nearest = row;
        }
    }
    return nearest;
}

void TaskChatWidget::setMessageRowSelected(QWidget *row, bool selected)
{
    if (!row) return;
    const int messageId = row->property("messageId").toInt();
    if (messageId > 0) {
        if (selected) selectedMessageIds_.insert(messageId);
        else selectedMessageIds_.remove(messageId);
    }
    row->setProperty("selected", selected);
    row->setStyleSheet(selected ? "background:rgba(59, 130, 246, 0.25);border-radius:8px;" : "background:transparent;");
    if (QCheckBox *chk = row->findChild<QCheckBox*>()) {
        if (chk->isChecked() != selected) {
            QSignalBlocker b(chk);
            chk->setChecked(selected);
        }
    }
    updateDeleteButtonVisibility();
    if (selectionMode_ && selectedMessageIds_.isEmpty()) {
        exitSelectionMode();
    }
}

bool TaskChatWidget::isMessageRowSelected(QWidget *row) const
{
    return row && row->property("selected").toBool();
}

void TaskChatWidget::showMessageContextMenu(const QPoint &globalPos)
{
    QWidget *row = messageRowAtGlobalPos(globalPos);
    if (!row) return;
    const int messageId = row->property("messageId").toInt();
    const QString text = row->property("text").toString();
    const bool mine = row->property("mine").toBool();
    const bool isAttachment = row->property("isAttachment").toBool();
    const QString sourceFromUser = row->property("fromUser").toString().trimmed();
    const QString sourceRawMessage = row->property("rawMessage").toString();
    QMenu menu;
    menu.setStyleSheet(
        "QMenu{background:#FFFFFF;border:1px solid #D5DCE8;border-radius:10px;padding:6px;}"
        "QMenu::item{padding:8px 14px;border-radius:8px;font-size:13px;}"
        "QMenu::item:selected{background:#EEF4FF;}"
    );
    QAction *replyAct = menu.addAction("Ответить");
    QAction *editAct = menu.addAction("Изменить");
    QAction *pinAct = menu.addAction("Закрепить");
    QAction *deleteAct = menu.addAction("Удалить");
    QAction *forwardAct = menu.addAction("Переслать");
    QAction *selectAct = menu.addAction("Выделить");
    if (!mine || isAttachment) editAct->setEnabled(false);
    QAction *chosen = menu.exec(globalPos);
    if (!chosen) return;
    if (chosen == replyAct) {
        startReplyToMessage(messageId, text);
        return;
    }
    if (chosen == editAct) {
        bool ok = false;
        const QString edited = QInputDialog::getText(this, "Изменить сообщение", "Текст:", QLineEdit::Normal, text, &ok);
        if (ok && !edited.trimmed().isEmpty()) {
            QSqlDatabase db = QSqlDatabase::database("main_connection");
            QSqlQuery q(db);
            q.prepare("UPDATE task_chat_messages SET message = :m WHERE id = :id");
            q.bindValue(":m", edited.trimmed());
            q.bindValue(":id", messageId);
            q.exec();
            logAction(currentUser_, "chat_message_edited", QString("msg=%1").arg(messageId));
            refreshMessages();
        }
        return;
    }
    if (chosen == pinAct) {
        pinnedMessageId_ = (pinnedMessageId_ == messageId) ? 0 : messageId;
        if (pinnedLbl_) {
            if (pinnedMessageId_ > 0) {
                pinnedLbl_->setText(QString("Закрепленное сообщение\n%1").arg(text.left(140)));
                pinnedLbl_->setVisible(true);
                if (unpinnedBtn_) unpinnedBtn_->setVisible(true);
            } else {
                pinnedLbl_->setVisible(false);
                if (unpinnedBtn_) unpinnedBtn_->setVisible(false);
            }
        }
        logAction(currentUser_, "chat_message_pinned", QString("msg=%1").arg(messageId));
        return;
    }
    if (chosen == deleteAct) {
        QMessageBox mb(this);
        mb.setWindowTitle("Удаление");
        mb.setText("Удалить сообщение?");
        QCheckBox *forUserCb = new QCheckBox("Удалить также для пользователя", &mb);
        mb.setCheckBox(forUserCb);
        QPushButton *ok = mb.addButton("Удалить", QMessageBox::AcceptRole);
        mb.addButton("Отмена", QMessageBox::RejectRole);
        mb.exec();
        if (mb.clickedButton() != ok) return;
        QString err;
        if (forUserCb->isChecked()) {
            if (!deleteMessage(messageId, currentUser_, err)) {
                QMessageBox::warning(this, "Ошибка", err);
                return;
            }
        } else {
            if (!hideMessageForUser(messageId, currentUser_, err)) {
                QMessageBox::warning(this, "Ошибка", err);
                return;
            }
        }
        refreshMessages(true);
        return;
    }
    if (chosen == forwardAct) {
        QVector<UserInfo> allUsers = getAllUsers(false);
        QVector<UserInfo> candidates;
        for (const UserInfo &u : allUsers) {
            if (u.username.trimmed().isEmpty() || u.username == currentUser_) continue;
            candidates.push_back(u);
        }
        if (candidates.isEmpty()) {
            QMessageBox::information(this, "Переслать", "Нет доступных пользователей.");
            return;
        }
        QDialog pick(this);
        pick.setWindowTitle("Переслать");
        pick.setMinimumSize(420, 500);
        QVBoxLayout *root = new QVBoxLayout(&pick);
        QLabel *lbl = new QLabel("Выберите пользователя", &pick);
        root->addWidget(lbl);
        QListWidget *list = new QListWidget(&pick);
        list->setIconSize(QSize(34, 34));
        for (const UserInfo &u : candidates) {
            const QString display = u.fullName.isEmpty() ? u.username : (u.fullName + " (" + u.username + ")");
            QListWidgetItem *it = new QListWidgetItem(display, list);
            QPixmap a = loadUserAvatarFromDb(u.username);
            if (!a.isNull()) it->setIcon(QIcon(a));
            it->setData(Qt::UserRole, u.username);
        }
        root->addWidget(list, 1);
        connect(list, &QListWidget::itemDoubleClicked, &pick, [&pick](QListWidgetItem*) { pick.accept(); });
        if (pick.exec() != QDialog::Accepted || !list->currentItem()) return;
        const QString other = list->currentItem()->data(Qt::UserRole).toString().trimmed();
        if (other.isEmpty()) return;
        QString err;
        int tid = TaskChatDialog::ensureThreadWithUser(currentUser_, other, QString(), &err);
        if (tid <= 0) {
            QMessageBox::warning(this, "Переслать", err.isEmpty() ? QStringLiteral("Не удалось открыть чат") : err);
            return;
        }
        QString rawPayload = sourceRawMessage;
        if (rawPayload.trimmed().isEmpty()) {
            rawPayload = text;
        }
        setThreadId(tid, other);
        setPendingForward(sourceFromUser.isEmpty() ? QStringLiteral("неизвестно") : sourceFromUser, rawPayload);
        if (replyEdit_) replyEdit_->setFocus();
        return;
    }
    if (chosen == selectAct) {
        const int selectId = messageId;
        enterSelectionMode();
        for (int i = 0; i < messagesLayout_->count(); ++i) {
            QLayoutItem *item = messagesLayout_->itemAt(i);
            if (!item || !item->widget()) continue;
            QWidget *r = item->widget();
            if (r->objectName() != "chatMessageRow") continue;
            if (r->property("messageId").toInt() == selectId) {
                setMessageRowSelected(r, true);
                break;
            }
        }
        return;
    }
}

void TaskChatWidget::startReplyToMessage(int messageId, const QString &text)
{
    replyToMessageId_ = messageId;
    if (replyBannerLbl_) {
        replyBannerLbl_->setText(QString("Ответ на: %1").arg(text.left(120)));
        replyBannerLbl_->setVisible(true);
    }
    if (replyEdit_) replyEdit_->setFocus();
}

void TaskChatWidget::updateDeleteButtonVisibility()
{
    if (!deleteSelectedBtn_ || !messagesLayout_) return;
    QSet<int> visibleSelected;
    for (int i = 0; i < messagesLayout_->count(); ++i) {
        QLayoutItem *item = messagesLayout_->itemAt(i);
        if (!item || !item->widget()) continue;
        QWidget *w = item->widget();
        if (w->objectName() != "chatMessageRow") continue;
        const int mid = w->property("messageId").toInt();
        if (mid > 0 && w->property("selected").toBool()) visibleSelected.insert(mid);
    }
    selectedMessageIds_ = visibleSelected;
    const int count = selectedMessageIds_.size();
    deleteSelectedBtn_->setVisible(selectionMode_ && count > 0);
    if (selectionCountLbl_) {
        selectionCountLbl_->setVisible(selectionMode_ && count > 0);
        selectionCountLbl_->setText(QString("Выбрано: %1").arg(count));
    }
}

void TaskChatWidget::deleteSelectedMessages()
{
    if (!messagesLayout_) return;
    QVector<int> ids = selectedMessageIds_.values().toVector();
    if (ids.isEmpty()) return;
    QDialog dlg(this);
    dlg.setWindowTitle("Удалить сообщения");
    dlg.setMinimumWidth(420);
    QVBoxLayout *root = new QVBoxLayout(&dlg);
    QLabel *title = new QLabel(QString("Выбрано сообщений: %1").arg(ids.size()), &dlg);
    title->setStyleSheet("font-weight:800;font-size:14px;");
    root->addWidget(title);
    QFrame *box = new QFrame(&dlg);
    box->setStyleSheet("QFrame{border:1px solid #CBD5E1;border-radius:10px;background:#F8FAFC;}");
    QVBoxLayout *boxL = new QVBoxLayout(box);
    QCheckBox *cbMe = new QCheckBox("Удалить у себя", box);
    QCheckBox *cbAll = new QCheckBox("Удалить также для пользователя", box);
    boxL->addWidget(cbMe);
    boxL->addWidget(cbAll);
    root->addWidget(box);
    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *cancel = new QPushButton("Отмена", &dlg);
    cancel->setMinimumHeight(42);
    cancel->setStyleSheet("QPushButton{background:#E2E8F0;font-weight:800;padding:10px 16px;}QPushButton:hover{background:#CBD5E1;}");
    QPushButton *ok = new QPushButton("Удалить", &dlg);
    ok->setMinimumHeight(42);
    ok->setStyleSheet("QPushButton{background:#DC2626;color:white;font-weight:800;padding:10px 16px;}QPushButton:hover{background:#BE1F1F;}");
    btns->addWidget(cancel);
    btns->addWidget(ok);
    root->addLayout(btns);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    if (dlg.exec() != QDialog::Accepted) return;
    if (cbAll->isChecked()) {
        QString err;
        for (int id : ids) {
            if (!deleteMessage(id, currentUser_, err))
                QMessageBox::warning(this, "Ошибка", err);
        }
        logAction(currentUser_, "chat_messages_deleted_for_all", QString("count=%1").arg(ids.size()));
    } else if (cbMe->isChecked() || !cbAll->isChecked()) {
        QString err;
        for (int id : ids) {
            if (!hideMessageForUser(id, currentUser_, err))
                QMessageBox::warning(this, "Ошибка", err);
        }
        logAction(currentUser_, "chat_messages_hidden_for_me", QString("count=%1").arg(ids.size()));
    }
    DataBus::instance().triggerNotificationsChanged();
    selectedMessageIds_.clear();
    refreshMessages();
    if (deleteSelectedBtn_) deleteSelectedBtn_->setVisible(false);
}

// ------------------------- TaskChatDialog -------------------------
TaskChatDialog::TaskChatDialog(Mode mode,
                               const QString &agvId,
                               const QVector<TaskChatRecipient> &recipients,
                               const QVector<TaskChatTaskChoice> &tasks,
                               QWidget *parent)
    : QDialog(parent)
    , mode_(mode)
    , agvId_(agvId)
    , recipients_(recipients)
    , tasks_(tasks)
{
    setWindowTitle("Написать по задаче");
    setMinimumSize(480, 400);
    setupCreateUi();
}

TaskChatDialog::TaskChatDialog(int threadId, const QString &currentUser, bool isAdmin, QWidget *parent)
    : QDialog(parent)
    , mode_(ViewThread)
    , threadId_(threadId)
    , currentUser_(currentUser)
    , isAdmin_(isAdmin)
{
    setWindowTitle("Чат по задаче");
    setMinimumSize(500, 450);
    setupViewUi();
}

void TaskChatDialog::setupCreateUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setSpacing(12);
    root->setContentsMargins(16, 16, 16, 16);

    QLabel *l1 = new QLabel("Кому:", this);
    l1->setStyleSheet("font-weight:700;");
    recipientCombo_ = new QComboBox(this);
    for (const TaskChatRecipient &r : recipients_)
        recipientCombo_->addItem(r.displayName, r.username);
    root->addWidget(l1);
    root->addWidget(recipientCombo_);

    QLabel *l2 = new QLabel("Задача:", this);
    l2->setStyleSheet("font-weight:700;");
    taskCombo_ = new QComboBox(this);
    for (const TaskChatTaskChoice &t : tasks_)
        taskCombo_->addItem(t.displayName, QVariantList() << t.taskId << t.taskName);
    root->addWidget(l2);
    root->addWidget(taskCombo_);

    QLabel *l3 = new QLabel("Сообщение:", this);
    l3->setStyleSheet("font-weight:700;");
    messageEdit_ = new QTextEdit(this);
    messageEdit_->setPlaceholderText("Например: запрос на отсрочку обслуживания...");
    messageEdit_->setMinimumHeight(120);
    root->addWidget(l3);
    root->addWidget(messageEdit_);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &TaskChatDialog::sendNewThread);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);
}

void TaskChatDialog::sendNewThread()
{
    QString msg = messageEdit_->toPlainText().trimmed();
    if (msg.isEmpty()) {
        QMessageBox::warning(this, "Сообщение", "Введите текст сообщения.");
        return;
    }
    QString recipientUser = recipientCombo_->currentData().toString();
    if (recipientUser.isEmpty() && recipientCombo_->currentIndex() == 0) {
        QMessageBox::warning(this, "Кому", "Выберите получателя.");
        return;
    }
    QVariantList taskData = taskCombo_->currentData().toList();
    int taskId = taskData.isEmpty() ? 0 : taskData[0].toInt();
    QString taskName = (taskData.size() > 1) ? taskData[1].toString() : QString();

    QString err;
    int tid = createThread(agvId_, taskId, taskName, currentUser_, recipientUser, msg, err);
    if (tid <= 0) {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать чат: " + err);
        return;
    }
    logAction(currentUser_, "chat_created",
              QString("thread=%1 agv=%2 to=%3").arg(tid).arg(agvId_, recipientUser));

    QString title = "Новое сообщение по задаче";
    const QString fromName = userDisplayName(currentUser_);
    QString body = QString("[chat:%1] %2 — сообщение по задаче, AGV %3: %4")
                       .arg(tid).arg(fromName, agvId_, msg.left(100));
    addNotificationForUser(recipientUser, title, body);
    DataBus::instance().triggerNotificationsChanged();

    QMessageBox::information(this, "Отправлено", "Сообщение отправлено.");
    accept();
}

void TaskChatDialog::setupViewUi()
{
    setStyleSheet(
        "QDialog{background:#F4F6FA;}"
        "QLineEdit{background:#FFFFFF;border:1px solid #D7DFEA;border-radius:14px;padding:10px 12px;font-size:13px;}"
        "QLineEdit:focus{border:1px solid #8BA7FF;}"
        "QPushButton{border-radius:12px;}"
        "QScrollBar:vertical{background:transparent;width:10px;margin:2px 2px 2px 2px;}"
        "QScrollBar::handle:vertical{background:#BFCBE0;border-radius:5px;min-height:28px;}"
        "QScrollBar::handle:vertical:hover{background:#9EB0CD;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}"
    );

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(12, 12, 12, 12);

    TaskChatThread t = getThreadById(threadId_);
    QFrame *headerCard = new QFrame(this);
    headerCard->setStyleSheet("QFrame{background:#FFFFFF;border:none;border-radius:12px;}");
    QVBoxLayout *headerL = new QVBoxLayout(headerCard);
    headerL->setContentsMargins(12, 10, 12, 10);
    headerL->setSpacing(2);

    QString otherUser = (t.createdBy == currentUser_) ? t.recipientUser : t.createdBy;
    QString otherFio = otherUser;
    UserInfo otherInfo;
    if (!otherUser.isEmpty() && loadUserProfile(otherUser, otherInfo) && !otherInfo.fullName.isEmpty())
        otherFio = otherInfo.fullName;
    QLabel *peerLbl = new QLabel(QString("Собеседник: %1").arg(otherFio.isEmpty() ? "—" : otherFio), headerCard);
    peerLbl->setStyleSheet("font-weight:700; font-size:12px; color:#64748B;");
    headerL->addWidget(peerLbl);

    root->addWidget(headerCard);

    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setStyleSheet("QScrollArea{background:transparent;border:none;}");
    scrollArea_->setMinimumHeight(260);
    QWidget *host = new QWidget(scrollArea_);
    host->setStyleSheet("background:transparent;");
    messagesLayout_ = new QVBoxLayout(host);
    messagesLayout_->setContentsMargins(4, 4, 4, 4);
    messagesLayout_->setSpacing(6);
    scrollArea_->setWidget(host);
    root->addWidget(scrollArea_, 1);

    QFrame *composer = new QFrame(this);
    composer->setStyleSheet("QFrame{background:#FFFFFF;border:none;border-radius:12px;}");
    QVBoxLayout *composerL = new QVBoxLayout(composer);
    composerL->setContentsMargins(8, 8, 8, 8);
    composerL->setSpacing(6);
    QWidget *attachRow = new QWidget(composer);
    QHBoxLayout *attachRowL = new QHBoxLayout(attachRow);
    attachRowL->setContentsMargins(0, 0, 0, 0);
    attachRowL->setSpacing(6);
    attachPreviewLbl_ = new QLabel(attachRow);
    attachPreviewLbl_->setVisible(false);
    attachPreviewLbl_->setWordWrap(true);
    attachPreviewLbl_->setStyleSheet("font-size:12px;font-weight:700;color:#334155;background:#EEF2FF;border:1px solid #C7D2FE;border-radius:8px;padding:6px 10px;");
    attachClearBtn_ = new QPushButton(QString::fromUtf8("✕"), attachRow);
    attachClearBtn_->setVisible(false);
    attachClearBtn_->setFixedSize(26, 26);
    attachClearBtn_->setToolTip("Открепить вложение");
    attachClearBtn_->setStyleSheet("QPushButton{background:#FEE2E2;color:#B91C1C;border:none;border-radius:13px;font-weight:900;}"
                                   "QPushButton:hover{background:#FCA5A5;}");
    connect(attachClearBtn_, &QPushButton::clicked, this, [this]() {
        clearPendingAttachment();
        clearPendingForward();
    });
    attachRowL->addWidget(attachPreviewLbl_, 1);
    attachRowL->addWidget(attachClearBtn_, 0, Qt::AlignTop);
    composerL->addWidget(attachRow);
    QHBoxLayout *replyRow = new QHBoxLayout();
    replyRow->setContentsMargins(0, 0, 0, 0);
    replyRow->setSpacing(8);
    replyEdit_ = new QLineEdit(this);
    replyEdit_->setPlaceholderText("Введите ответ...");
    replyEdit_->installEventFilter(this);
    attachBtn_ = new QPushButton(this);
    attachBtn_->setFixedWidth(40);
    attachBtn_->setToolTip("Прикрепить файл");
    attachBtn_->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    attachBtn_->setIconSize(QSize(18, 18));
    attachBtn_->setStyleSheet("QPushButton{background:#E2E8F0;color:#334155;font-size:18px;font-weight:900;padding:8px 10px;}"
                              "QPushButton:hover{background:#CBD5E1;}");
    connect(attachBtn_, &QPushButton::clicked, this, &TaskChatDialog::sendAttachment);
    sendBtn_ = new QPushButton("Отправить", this);
    sendBtn_->setStyleSheet("QPushButton{background:#2B6BFF;color:white;font-weight:700;padding:8px 14px;}"
                            "QPushButton:hover{background:#245DE0;}");
    connect(sendBtn_, &QPushButton::clicked, this, &TaskChatDialog::sendReply);
    connect(replyEdit_, &QLineEdit::returnPressed, this, &TaskChatDialog::sendReply);
    replyRow->addWidget(attachBtn_);
    replyRow->addWidget(replyEdit_, 1);
    replyRow->addWidget(sendBtn_);
    composerL->addLayout(replyRow);
    root->addWidget(composer);

    if (isAdmin_) {
        deleteChatBtn_ = new QPushButton("Удалить чат", this);
        deleteChatBtn_->setFixedWidth(150);
        deleteChatBtn_->setStyleSheet("QPushButton{background:#6B7280;color:white;font-weight:700;padding:8px 12px;}"
                                      "QPushButton:hover{background:#4B5563;}");
        connect(deleteChatBtn_, &QPushButton::clicked, this, &TaskChatDialog::deleteThreadByAdmin);
        root->addWidget(deleteChatBtn_);
    }

    QHBoxLayout *delRow = new QHBoxLayout();
    delRow->setContentsMargins(0, 0, 0, 0);
    delRow->setSpacing(8);
    deleteForMeBtn_ = new QPushButton("Удалить у себя", this);
    deleteForMeBtn_->setStyleSheet("QPushButton{background:#94A3B8;color:white;font-weight:700;padding:8px 10px;}"
                                   "QPushButton:hover{background:#7C8DAA;}");
    connect(deleteForMeBtn_, &QPushButton::clicked, this, &TaskChatDialog::deleteThreadForMe);
    delRow->addWidget(deleteForMeBtn_);
    delRow->addStretch();
    deleteForAllBtn_ = new QPushButton("Удалить у всех", this);
    deleteForAllBtn_->setStyleSheet("QPushButton{background:#475569;color:white;font-weight:700;padding:8px 10px;}"
                                    "QPushButton:hover{background:#334155;}");
    connect(deleteForAllBtn_, &QPushButton::clicked, this, &TaskChatDialog::deleteThreadForAll);
    delRow->addWidget(deleteForAllBtn_);
    deleteSelectedBtn_ = new QPushButton("Удалить", this);
    deleteSelectedBtn_->setStyleSheet("QPushButton{background:#DC2626;color:white;font-weight:700;padding:8px 14px;} QPushButton:hover{background:#BE1F1F;}");
    deleteSelectedBtn_->setVisible(false);
    connect(deleteSelectedBtn_, &QPushButton::clicked, this, &TaskChatDialog::deleteSelectedMessages);
    delRow->addWidget(deleteSelectedBtn_);
    root->addLayout(delRow);

    scrollArea_->viewport()->installEventFilter(this);
    if (QScrollBar *sb = scrollArea_->verticalScrollBar()) {
        connect(sb, &QScrollBar::valueChanged, this, [this](int value) {
            if (value <= 20 && threadId_ > 0)
                loadOlderMessages();
        });
    }

    refreshMessages();

    liveRefreshTimer_ = new QTimer(this);
    liveRefreshTimer_->setInterval(1500);
    connect(liveRefreshTimer_, &QTimer::timeout, this, [this]() {
        if (!isVisible() || threadId_ <= 0)
            return;
        refreshMessages(false);
    });
    liveRefreshTimer_->start();
}

void TaskChatDialog::refreshMessages(bool fullReload)
{
    if (threadId_ <= 0)
        return;

    bool keepAtBottom = forceScrollToBottom_;
    if (scrollArea_ && scrollArea_->verticalScrollBar()) {
        QScrollBar *sb = scrollArea_->verticalScrollBar();
        keepAtBottom = keepAtBottom || (sb->maximum() - sb->value()) < 30;
    }

    // Если forceScrollToBottom_, делаем полную перезагрузку
    if (forceScrollToBottom_)
        fullReload = true;

    if (!fullReload && lastLoadedMessageId_ > 0) {
        if (getMessagesForThreadFrom(threadId_, currentUser_, lastLoadedMessageId_ + 1).isEmpty()) {
            forceScrollToBottom_ = false;
            return;
        }
        fullReload = true;
    }

    QWidget *messagesHost = scrollArea_ ? scrollArea_->widget() : nullptr;
    if (messagesHost)
        messagesHost->setUpdatesEnabled(false);
    while (QLayoutItem *item = messagesLayout_->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    QVector<TaskChatMessage> msgs = getMessagesForThreadLastN(threadId_, currentUser_, kMessagesPageSize);
    oldestLoadedMessageId_ = 0;
    lastLoadedMessageId_ = 0;
    historyExhausted_ = false;
    for (const TaskChatMessage &m : msgs) {
        const bool mine = (m.fromUser == currentUser_);
        QString forwardedFrom;
        QString payload = m.message;
        const bool isForwarded = decodeForwardMessage(m.message, forwardedFrom, payload);
        
        // Проверяем, есть ли комбинированное сообщение (текст + вложение)
        QString messageText;
        QString attachmentPayload;
        const bool isCombined = splitCombinedMessage(payload, messageText, attachmentPayload);
        
        QString fileName;
        QString fileMime;
        bool isAttachment = false;
        QString visibleText;
        QString specialLabel;
        bool isSpecial = false;
        
        if (isCombined) {
            // Комбинированное сообщение: есть и текст, и вложение
            visibleText = messageText;
            isAttachment = decodeAttachmentMeta(attachmentPayload, fileName, fileMime);
            payload = attachmentPayload;
        } else {
            // Обычное сообщение: проверяем, есть ли вложение
            isAttachment = decodeAttachmentMeta(payload, fileName, fileMime);
            if (isAttachment) {
                visibleText.clear();
            } else {
                isSpecial = decodeSpecialMessage(payload, specialLabel, visibleText);
            }
        }
        const bool wasSelected = selectedMessageIds_.contains(m.id);

        QWidget *row = new QWidget(this);
        row->setStyleSheet(wasSelected ? "background:rgba(59, 130, 246, 0.25);border-radius:8px;" : "background:transparent;");
        row->setProperty("messageId", m.id);
        row->setProperty("fromUser", m.fromUser);
        row->setProperty("rawMessage", m.message);
        row->setProperty("selected", wasSelected);
        row->setProperty("isAttachment", isAttachment);
        if (isAttachment) {
            row->setProperty("attachmentFileName", fileName);
            row->setProperty("attachmentMime", fileMime);
        }
        row->setObjectName("chatMessageRow");
        QHBoxLayout *rowL = new QHBoxLayout(row);
        rowL->setContentsMargins(0, 0, 0, 0);
        rowL->setSpacing(6);

        QFrame *bubble = new QFrame(row);
        bubble->setStyleSheet(QString(
            "QFrame{background:%1;border:none;border-radius:14px;}"
        ).arg(mine ? "#2B6BFF" : "#FFFFFF"));
        QVBoxLayout *bubbleL = new QVBoxLayout(bubble);
        bubbleL->setContentsMargins(12, 8, 12, 6);
        bubbleL->setSpacing(4);

        QLabel *text = new QLabel(visibleText, bubble);
        text->setWordWrap(true);
        text->setTextInteractionFlags(Qt::NoTextInteraction);
        text->setContextMenuPolicy(Qt::NoContextMenu);
        text->setMaximumWidth(qMax(220, int(width() * 0.55)));
        text->setStyleSheet(QString(
            "font-size:13px; color:%1;"
        ).arg(mine ? "#FFFFFF" : "#0F172A"));

        QString metaText = QString("%1 • %2").arg(m.fromUser, m.createdAt.toString("dd.MM.yy hh:mm"));
        if (mine)
            metaText += QStringLiteral(" ✓✓");
        QLabel *meta = new QLabel(metaText, bubble);
        meta->setContextMenuPolicy(Qt::NoContextMenu);
        meta->setStyleSheet(QString(
            "font-size:10px; color:%1;"
        ).arg(mine ? "#DCE8FF" : "#64748B"));

        if (isSpecial) {
            QLabel *status = new QLabel(
                specialLabel.isEmpty() ? QStringLiteral("Особое сообщение")
                                       : QStringLiteral("По задаче: ") + specialLabel,
                bubble
            );
            status->setStyleSheet(QString("font-size:10px; font-weight:700; color:%1;")
                                  .arg(mine ? "#CFE1FF" : "#2563EB"));
            bubbleL->addWidget(status, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
        }
        if (isForwarded) {
            QLabel *fwd = new QLabel(QString("Переслано от %1").arg(forwardedFrom.isEmpty() ? QStringLiteral("неизвестно") : forwardedFrom), bubble);
            fwd->setStyleSheet(QString("font-size:11px;font-weight:800;color:%1;background:%2;border-radius:6px;padding:4px 8px;")
                               .arg(mine ? "#DBEAFE" : "#1E3A8A")
                               .arg(mine ? "#1E40AF" : "#DBEAFE"));
            bubbleL->addWidget(fwd, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
        }
        // Сначала вложение (если есть), потом текст
        if (isAttachment) {
            if (isImageAttachment(fileMime, fileName)) {
                // Декодируем вложение и показываем превью
                QString decodedName;
                QString decodedMime;
                QByteArray imageData;
                const QString &msgToDecode = isCombined ? attachmentPayload : m.message;
                bool hasImage = decodeAttachmentMessage(msgToDecode, decodedName, decodedMime, imageData);

                if (hasImage && !imageData.isEmpty()) {
                    QLabel *imagePreview = new QLabel(bubble);
                    imagePreview->setAlignment(Qt::AlignCenter);
                    imagePreview->setCursor(Qt::PointingHandCursor);
                    imagePreview->setStyleSheet(
                        "QLabel{border:1px solid #BFDBFE;border-radius:10px;background:#EFF6FF;}"
                    );
                    imagePreview->setMinimumSize(200, 120);
                    imagePreview->setMaximumWidth(240);

                    QPixmap pixmap;
                    if (pixmap.loadFromData(imageData)) {
                        QPixmap scaled = pixmap.scaled(200, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        imagePreview->setPixmap(scaled);
                    } else {
                        imagePreview->setText(QStringLiteral("Изображение\n%1").arg(fileName));
                        imagePreview->setStyleSheet(
                            "QLabel{border:1px solid #BFDBFE;border-radius:10px;background:#EFF6FF;color:#1D4ED8;font-weight:800;padding:8px;text-align:center;}"
                        );
                    }

                    bubbleL->addWidget(imagePreview, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
                } else {
                    QPushButton *thumb = new QPushButton(QStringLiteral("Изображение\n%1").arg(fileName), bubble);
                    thumb->setFlat(false);
                    thumb->setFixedSize(180, 72);
                    const QString rawMessage = m.message;
                    thumb->setStyleSheet(
                        "QPushButton{border:1px solid #BFDBFE;border-radius:10px;background:#EFF6FF;color:#1D4ED8;font-weight:800;padding:8px;text-align:left;}"
                        "QPushButton:hover{background:#DBEAFE;border:1px solid #60A5FA;}"
                    );
                    connect(thumb, &QPushButton::clicked, this, [this, rawMessage]() {
                        QString fileName;
                        QString fileMime;
                        QByteArray fileData;
                        if (!decodeAttachmentFromStoredMessage(rawMessage, fileName, fileMime, fileData) || fileData.isEmpty())
                            return;
                        openAttachmentInsideApp(this, fileName, fileMime, fileData);
                    });
                    bubbleL->addWidget(thumb, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
                }
            } else {
                QPushButton *doc = new QPushButton(QString("Файл\n%1").arg(fileName), bubble);
                const QString rawMessage = m.message;
                doc->setStyleSheet(QString("QPushButton{background:%1;border:none;border-radius:8px;padding:8px 10px;font-size:14px;font-weight:900;color:%2;}"
                                           "QPushButton:hover{background:%3;}")
                                   .arg(mine ? "#1D4ED8" : "#E2E8F0")
                                   .arg(mine ? "#FFFFFF" : "#334155")
                                   .arg(mine ? "#1E40AF" : "#CBD5E1"));
                connect(doc, &QPushButton::clicked, this, [this, rawMessage]() {
                    QString fileName;
                    QString fileMime;
                    QByteArray fileData;
                    if (!decodeAttachmentFromStoredMessage(rawMessage, fileName, fileMime, fileData) || fileData.isEmpty())
                        return;
                    openAttachmentInsideApp(this, fileName, fileMime, fileData);
                });
                bubbleL->addWidget(doc, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
            }
        }
        // Текст после вложения
        if (!visibleText.isEmpty())
            bubbleL->addWidget(text);
        bubbleL->addWidget(meta, 0, mine ? Qt::AlignRight : Qt::AlignLeft);

        if (mine) {
            rowL->addStretch();
            rowL->addWidget(bubble, 0, Qt::AlignRight);
        } else {
            rowL->addWidget(bubble, 0, Qt::AlignLeft);
            rowL->addStretch();
        }
        row->installEventFilter(this);
        bubble->installEventFilter(this);
        text->installEventFilter(this);
        meta->installEventFilter(this);
        messagesLayout_->addWidget(row);
        oldestLoadedMessageId_ = (oldestLoadedMessageId_ <= 0) ? m.id : qMin(oldestLoadedMessageId_, m.id);
        lastLoadedMessageId_ = qMax(lastLoadedMessageId_, m.id);
    }
    messagesLayout_->addStretch();
    if (msgs.isEmpty()) {
        oldestLoadedMessageId_ = 0;
        lastLoadedMessageId_ = 0;
        historyExhausted_ = true;
    } else if (msgs.size() < kMessagesPageSize) {
        historyExhausted_ = true;
    }

    if (scrollArea_ && scrollArea_->verticalScrollBar() && (forceScrollToBottom_ || keepAtBottom)) {
        QTimer::singleShot(0, this, [this]() {
            if (!scrollArea_ || !scrollArea_->verticalScrollBar()) return;
            scrollArea_->verticalScrollBar()->setValue(scrollArea_->verticalScrollBar()->maximum());
        });
    }
    forceScrollToBottom_ = false;
    updateDeleteButtonVisibility();
    if (messagesHost) {
        messagesHost->setUpdatesEnabled(true);
        messagesHost->update();
    }
}

void TaskChatDialog::loadOlderMessages()
{
    if (threadId_ <= 0 || !messagesLayout_ || !scrollArea_ || loadingOlderMessages_ || historyExhausted_ || oldestLoadedMessageId_ <= 0)
        return;

    loadingOlderMessages_ = true;
    QWidget *messagesHost = scrollArea_->widget();
    QScrollBar *sb = scrollArea_->verticalScrollBar();
    const int oldValue = sb ? sb->value() : 0;
    const int oldMax = sb ? sb->maximum() : 0;
    if (messagesHost)
        messagesHost->setUpdatesEnabled(false);

    QLayoutItem *tail = nullptr;
    if (messagesLayout_->count() > 0) {
        tail = messagesLayout_->takeAt(messagesLayout_->count() - 1);
        if (tail && tail->widget()) {
            delete tail->widget();
            delete tail;
            tail = nullptr;
        }
    }

    const QVector<TaskChatMessage> older = getMessagesForThreadOlderThan(
        threadId_, currentUser_, oldestLoadedMessageId_, kMessagesPageSize);
    if (older.isEmpty()) {
        historyExhausted_ = true;
        messagesLayout_->addStretch();
        if (tail && tail->spacerItem())
            delete tail;
        if (messagesHost) {
            messagesHost->setUpdatesEnabled(true);
            messagesHost->update();
        }
        loadingOlderMessages_ = false;
        return;
    }

    int insertIndex = 0;
    for (const TaskChatMessage &m : older) {
        const bool mine = (m.fromUser == currentUser_);
        QString forwardedFrom;
        QString payload = m.message;
        const bool isForwarded = decodeForwardMessage(m.message, forwardedFrom, payload);
        
        // Проверяем, есть ли комбинированное сообщение (текст + вложение)
        QString messageText;
        QString attachmentPayload;
        const bool isCombined = splitCombinedMessage(payload, messageText, attachmentPayload);
        
        QString fileName;
        QString fileMime;
        bool isAttachment = false;
        QString visibleText;
        QString specialLabel;
        bool isSpecial = false;
        
        if (isCombined) {
            visibleText = messageText;
            isAttachment = decodeAttachmentMeta(attachmentPayload, fileName, fileMime);
            payload = attachmentPayload;
        } else {
            // Обычное сообщение: проверяем, есть ли вложение
            isAttachment = decodeAttachmentMeta(payload, fileName, fileMime);
            if (isAttachment) {
                visibleText.clear();
            } else {
                isSpecial = decodeSpecialMessage(payload, specialLabel, visibleText);
            }
        }
        const bool wasSelected = selectedMessageIds_.contains(m.id);

        QWidget *row = new QWidget(this);
        row->setStyleSheet(wasSelected ? "background:rgba(59, 130, 246, 0.25);border-radius:8px;" : "background:transparent;");
        row->setProperty("messageId", m.id);
        row->setProperty("fromUser", m.fromUser);
        row->setProperty("rawMessage", m.message);
        row->setProperty("selected", wasSelected);
        row->setProperty("isAttachment", isAttachment);
        if (isAttachment) {
            row->setProperty("attachmentFileName", fileName);
            row->setProperty("attachmentMime", fileMime);
        }
        row->setObjectName("chatMessageRow");
        QHBoxLayout *rowL = new QHBoxLayout(row);
        rowL->setContentsMargins(0, 0, 0, 0);
        rowL->setSpacing(6);

        QFrame *bubble = new QFrame(row);
        bubble->setStyleSheet(QString(
            "QFrame{background:%1;border:none;border-radius:14px;}"
        ).arg(mine ? "#2B6BFF" : "#FFFFFF"));
        QVBoxLayout *bubbleL = new QVBoxLayout(bubble);
        bubbleL->setContentsMargins(12, 8, 12, 6);
        bubbleL->setSpacing(4);

        QLabel *text = new QLabel(visibleText, bubble);
        text->setWordWrap(true);
        text->setTextInteractionFlags(Qt::NoTextInteraction);
        text->setContextMenuPolicy(Qt::NoContextMenu);
        text->setMaximumWidth(qMax(220, int(width() * 0.55)));
        text->setStyleSheet(QString(
            "font-size:13px; color:%1;"
        ).arg(mine ? "#FFFFFF" : "#0F172A"));

        QString metaText = QString("%1 • %2").arg(m.fromUser, m.createdAt.toString("dd.MM.yy hh:mm"));
        if (mine)
            metaText += QStringLiteral(" ✓✓");
        QLabel *meta = new QLabel(metaText, bubble);
        meta->setContextMenuPolicy(Qt::NoContextMenu);
        meta->setStyleSheet(QString(
            "font-size:10px; color:%1;"
        ).arg(mine ? "#DCE8FF" : "#64748B"));

        if (isSpecial) {
            QLabel *status = new QLabel(
                specialLabel.isEmpty() ? QStringLiteral("Особое сообщение")
                                       : QStringLiteral("По задаче: ") + specialLabel,
                bubble
            );
            status->setStyleSheet(QString("font-size:10px; font-weight:700; color:%1;")
                                  .arg(mine ? "#CFE1FF" : "#2563EB"));
            bubbleL->addWidget(status, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
        }
        if (isForwarded) {
            QLabel *fwd = new QLabel(QString("Переслано от %1").arg(forwardedFrom.isEmpty() ? QStringLiteral("неизвестно") : forwardedFrom), bubble);
            fwd->setStyleSheet(QString("font-size:11px;font-weight:800;color:%1;background:%2;border-radius:6px;padding:4px 8px;")
                               .arg(mine ? "#DBEAFE" : "#1E3A8A")
                               .arg(mine ? "#1E40AF" : "#DBEAFE"));
            bubbleL->addWidget(fwd, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
        }
        // Сначала вложение (если есть), потом текст
        if (isAttachment) {
            if (isImageAttachment(fileMime, fileName)) {
                // Декодируем вложение и показываем превью
                QString decodedName;
                QString decodedMime;
                QByteArray imageData;
                const QString &msgToDecode = isCombined ? attachmentPayload : m.message;
                bool hasImage = decodeAttachmentMessage(msgToDecode, decodedName, decodedMime, imageData);

                if (hasImage && !imageData.isEmpty()) {
                    QLabel *imagePreview = new QLabel(bubble);
                    imagePreview->setAlignment(Qt::AlignCenter);
                    imagePreview->setCursor(Qt::PointingHandCursor);
                    imagePreview->setStyleSheet(
                        "QLabel{border:1px solid #BFDBFE;border-radius:10px;background:#EFF6FF;}"
                    );
                    imagePreview->setMinimumSize(200, 120);
                    imagePreview->setMaximumWidth(240);

                    QPixmap pixmap;
                    if (pixmap.loadFromData(imageData)) {
                        QPixmap scaled = pixmap.scaled(200, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        imagePreview->setPixmap(scaled);
                    } else {
                        imagePreview->setText(QStringLiteral("Изображение\n%1").arg(fileName));
                        imagePreview->setStyleSheet(
                            "QLabel{border:1px solid #BFDBFE;border-radius:10px;background:#EFF6FF;color:#1D4ED8;font-weight:800;padding:8px;text-align:center;}"
                        );
                    }

                    bubbleL->addWidget(imagePreview, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
                } else {
                    QPushButton *thumb = new QPushButton(QStringLiteral("Изображение\n%1").arg(fileName), bubble);
                    thumb->setFlat(false);
                    thumb->setFixedSize(180, 72);
                    const QString rawMessage = m.message;
                    thumb->setStyleSheet(
                        "QPushButton{border:1px solid #BFDBFE;border-radius:10px;background:#EFF6FF;color:#1D4ED8;font-weight:800;padding:8px;text-align:left;}"
                        "QPushButton:hover{background:#DBEAFE;border:1px solid #60A5FA;}"
                    );
                    connect(thumb, &QPushButton::clicked, this, [this, rawMessage]() {
                        QString fileName;
                        QString fileMime;
                        QByteArray fileData;
                        if (!decodeAttachmentFromStoredMessage(rawMessage, fileName, fileMime, fileData) || fileData.isEmpty())
                            return;
                        openAttachmentInsideApp(this, fileName, fileMime, fileData);
                    });
                    bubbleL->addWidget(thumb, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
                }
            } else {
                QPushButton *doc = new QPushButton(QString("Файл\n%1").arg(fileName), bubble);
                const QString rawMessage = m.message;
                doc->setStyleSheet(QString("QPushButton{background:%1;border:none;border-radius:8px;padding:8px 10px;font-size:14px;font-weight:900;color:%2;}"
                                           "QPushButton:hover{background:%3;}")
                                   .arg(mine ? "#1D4ED8" : "#E2E8F0")
                                   .arg(mine ? "#FFFFFF" : "#334155")
                                   .arg(mine ? "#1E40AF" : "#CBD5E1"));
                connect(doc, &QPushButton::clicked, this, [this, rawMessage]() {
                    QString fileName;
                    QString fileMime;
                    QByteArray fileData;
                    if (!decodeAttachmentFromStoredMessage(rawMessage, fileName, fileMime, fileData) || fileData.isEmpty())
                        return;
                    openAttachmentInsideApp(this, fileName, fileMime, fileData);
                });
                bubbleL->addWidget(doc, 0, mine ? Qt::AlignRight : Qt::AlignLeft);
            }
        }
        // Текст после вложения
        if (!visibleText.isEmpty())
            bubbleL->addWidget(text);
        bubbleL->addWidget(meta, 0, mine ? Qt::AlignRight : Qt::AlignLeft);

        if (mine) {
            rowL->addStretch();
            rowL->addWidget(bubble, 0, Qt::AlignRight);
        } else {
            rowL->addWidget(bubble, 0, Qt::AlignLeft);
            rowL->addStretch();
        }
        row->installEventFilter(this);
        bubble->installEventFilter(this);
        text->installEventFilter(this);
        meta->installEventFilter(this);
        messagesLayout_->insertWidget(insertIndex++, row);
        oldestLoadedMessageId_ = (oldestLoadedMessageId_ <= 0) ? m.id : qMin(oldestLoadedMessageId_, m.id);
    }
    if (older.size() < kMessagesPageSize)
        historyExhausted_ = true;
    messagesLayout_->addStretch();
    if (tail && tail->spacerItem())
        delete tail;

    if (messagesHost) {
        messagesHost->setUpdatesEnabled(true);
        messagesHost->update();
    }
    if (sb) {
        QTimer::singleShot(0, this, [sb, oldValue, oldMax]() {
            const int delta = sb->maximum() - oldMax;
            sb->setValue(oldValue + qMax(0, delta));
        });
    }
    loadingOlderMessages_ = false;
}

void TaskChatDialog::sendReply()
{
    if (!replyEdit_) return;
    QString msg = replyEdit_->text().trimmed();
    if (msg.isEmpty() && pendingAttachData_.isEmpty() && pendingForwardRaw_.isEmpty()) return;
    QString err;
    bool sentAny = false;
    QString notifBody;

    // Если есть и текст, и вложение - объединяем в одно сообщение
    if (!msg.isEmpty() && !pendingAttachData_.isEmpty()) {
        // Кодируем вложение
        const QString attachmentPayload = encodeAttachmentMessage(pendingAttachName_, pendingAttachMime_, pendingAttachData_);
        
        // Создаём комбинированное сообщение: текст + разделитель + вложение
        QString combinedMessage = msg + "\n[[ATTACHMENT]]\n" + attachmentPayload;
        
        if (g_pendingSpecialByThread.contains(threadId_)) {
            combinedMessage = encodeSpecialMessage(g_pendingSpecialByThread.value(threadId_), combinedMessage);
            g_pendingSpecialByThread.remove(threadId_);
        }
        
        if (!addChatMessage(threadId_, currentUser_, combinedMessage, err)) {
            QMessageBox::warning(this, "Ошибка", err);
            return;
        }
        sentAny = true;
        notifBody = QString("%1 + Файл: %2").arg(msg.left(50)).arg(pendingAttachName_);
        logAction(currentUser_, "chat_reply_with_file", QString("thread=%1 file=%2").arg(threadId_).arg(pendingAttachName_));
        clearPendingAttachment();
    } else {
        // Отправляем только текст
        if (!msg.isEmpty()) {
            QString toStore = msg;
            if (g_pendingSpecialByThread.contains(threadId_)) {
                toStore = encodeSpecialMessage(g_pendingSpecialByThread.value(threadId_), msg);
                g_pendingSpecialByThread.remove(threadId_);
            }
            if (!addChatMessage(threadId_, currentUser_, toStore, err)) {
                QMessageBox::warning(this, "Ошибка", err);
                return;
            }
            sentAny = true;
            notifBody = msg.left(80);
            logAction(currentUser_, "chat_reply", QString("thread=%1").arg(threadId_));
        }

        // Отправляем только вложение
        if (!pendingAttachData_.isEmpty()) {
            const QString payload = encodeAttachmentMessage(pendingAttachName_, pendingAttachMime_, pendingAttachData_);
            if (!addChatMessage(threadId_, currentUser_, payload, err)) {
                QMessageBox::warning(this, "Ошибка", err);
                return;
            }
            sentAny = true;
            if (notifBody.isEmpty())
                notifBody = QString("Файл: %1").arg(pendingAttachName_);
            logAction(currentUser_, "chat_file_sent", QString("thread=%1 file=%2 size=%3").arg(threadId_).arg(pendingAttachName_).arg(pendingAttachData_.size()));
            clearPendingAttachment();
        }
    }

    if (!pendingForwardRaw_.isEmpty()) {
        const QString wrapped = encodeForwardMessage(pendingForwardFrom_.isEmpty() ? QStringLiteral("неизвестно") : pendingForwardFrom_, pendingForwardRaw_);
        if (!addChatMessage(threadId_, currentUser_, wrapped, err)) {
            QMessageBox::warning(this, "Ошибка", err);
            return;
        }
        sentAny = true;
        if (notifBody.isEmpty())
            notifBody = QString("Пересланное сообщение");
        clearPendingForward();
    }

    if (!sentAny) return;
    lastSentMs_ = QDateTime::currentMSecsSinceEpoch();
    replyEdit_->clear();
    forceScrollToBottom_ = true;

    TaskChatThread t = getThreadById(threadId_);
    QString title = "Новое сообщение в чате";
    const QString fromName = userDisplayName(currentUser_);
    QString body = QString("[chat:%1] %2: %3").arg(threadId_).arg(fromName, notifBody.left(80));
    if (currentUser_ == t.createdBy) {
        if (t.recipientUser.isEmpty()) {
            QVector<UserInfo> all = getAllUsers(false);
            for (const UserInfo &u : all)
                if (u.role == "admin" || u.role == "tech")
                    addNotificationForUser(u.username, title, body);
        } else {
            addNotificationForUser(t.recipientUser, title, body);
        }
    } else {
        addNotificationForUser(t.createdBy, title, body);
    }
    DataBus::instance().triggerNotificationsChanged();

    refreshMessages(false);
    
    // Принудительно скроллим вниз после отправки
    if (scrollArea_ && scrollArea_->verticalScrollBar()) {
        QTimer::singleShot(50, this, [this]() {
            if (scrollArea_ && scrollArea_->verticalScrollBar()) {
                scrollArea_->verticalScrollBar()->setValue(scrollArea_->verticalScrollBar()->maximum());
            }
        });
    }
}

void TaskChatDialog::sendAttachment()
{
    if (threadId_ <= 0)
        return;
    QString name;
    QString mime;
    QByteArray bytes;
    QString pickError;
    if (!pickAttachmentPayload(this, name, mime, bytes, pickError)) {
        if (!pickError.isEmpty())
            QMessageBox::warning(this, "Файл", pickError);
        return;
    }
    setPendingAttachment(name, mime, bytes);
}

void TaskChatDialog::deleteThreadByAdmin()
{
    if (!isAdmin_ || !deleteChatBtn_) return;

    auto ret = QMessageBox::question(this, "Удаление чата",
                                     "Удалить чат полностью? Это действие нельзя отменить.",
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    QString err;
    if (!deleteThread(threadId_, err)) {
        QMessageBox::warning(this, "Ошибка", err);
        return;
    }
    logAction(currentUser_, "chat_deleted_admin", QString("thread=%1").arg(threadId_));

    DataBus::instance().triggerNotificationsChanged();
    accept();
}

void TaskChatDialog::deleteThreadForMe()
{
    auto ret = QMessageBox::question(this, "Удаление у себя",
                                     "Скрыть чат только у вас?",
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    QString err;
    if (!hideThreadForUser(threadId_, currentUser_, err)) {
        QMessageBox::warning(this, "Ошибка", err);
        return;
    }
    logAction(currentUser_, "chat_hidden_for_me", QString("thread=%1").arg(threadId_));
    DataBus::instance().triggerNotificationsChanged();
    accept();
}

void TaskChatDialog::deleteThreadForAll()
{
    auto ret = QMessageBox::question(this, "Удаление у всех",
                                     "Удалить чат у всех участников?",
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    QString err;
    if (!deleteThread(threadId_, err)) {
        QMessageBox::warning(this, "Ошибка", err);
        return;
    }
    logAction(currentUser_, "chat_deleted_for_all", QString("thread=%1").arg(threadId_));
    DataBus::instance().triggerNotificationsChanged();
    accept();
}

bool TaskChatDialog::eventFilter(QObject *obj, QEvent *event)
{
    // Обработка кликов по превью изображений (QLabel с pixmap)
    if (event->type() == QEvent::MouseButtonPress) {
        QLabel *imageLabel = qobject_cast<QLabel*>(obj);
        if (imageLabel && imageLabel->pixmap() && !imageLabel->pixmap()->isNull()) {
            QWidget *row = imageLabel;
            while (row && row->objectName() != "chatMessageRow") {
                row = row->parentWidget();
            }
            if (row && row->property("isAttachment").toBool()) {
                const QString fn = row->property("attachmentFileName").toString();
                const QString mt = row->property("attachmentMime").toString();
                const QString raw = row->property("rawMessage").toString();
                QString decodedName;
                QString decodedMime;
                QByteArray data;
                if (decodeAttachmentFromStoredMessage(raw, decodedName, decodedMime, data) && !data.isEmpty()) {
                    openAttachmentInsideApp(this, fn, mt, data);
                    return true;
                }
            }
        }
    }

    if (obj == replyEdit_ && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->matches(QKeySequence::Paste)) {
            QString name;
            QString mime;
            QByteArray bytes;
            QString err;
            if (pickAttachmentFromClipboard(name, mime, bytes, err)) {
                setPendingAttachment(name, mime, bytes);
                return true;
            }
            if (!err.isEmpty()) {
                QMessageBox::warning(this, "Файл", err);
                return true;
            }
        }
    }

    if (event->type() == QEvent::MouseButtonPress && scrollArea_ && obj == scrollArea_->viewport() && messagesLayout_) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        QWidget *host = scrollArea_->widget();
        if (!host) return QDialog::eventFilter(obj, event);
        QPoint posInHost = host->mapFromGlobal(me->globalPos());
        for (int i = 0; i < messagesLayout_->count(); ++i) {
            QLayoutItem *item = messagesLayout_->itemAt(i);
            if (!item || !item->widget()) continue;
            QWidget *row = item->widget();
            if (row->objectName() != "chatMessageRow") continue;
            if (!row->geometry().contains(posInHost)) continue;
            bool sel = row->property("selected").toBool();
            const int mid = row->property("messageId").toInt();
            if (mid > 0) {
                if (!sel) selectedMessageIds_.insert(mid);
                else selectedMessageIds_.remove(mid);
            }
            row->setProperty("selected", !sel);
            row->setStyleSheet(sel ? "background:transparent;" : "background:rgba(59, 130, 246, 0.25);border-radius:8px;");
            updateDeleteButtonVisibility();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

void TaskChatDialog::jumpToMessage(int messageId)
{
    if (messageId <= 0 || !messagesLayout_ || !scrollArea_)
        return;
    int attempts = 0;
    while (attempts < 10) {
        QWidget *target = nullptr;
        for (int i = 0; i < messagesLayout_->count(); ++i) {
            QLayoutItem *it = messagesLayout_->itemAt(i);
            if (!it || !it->widget()) continue;
            QWidget *w = it->widget();
            if (w->objectName() != "chatMessageRow") continue;
            if (w->property("messageId").toInt() == messageId) {
                target = w;
                break;
            }
        }
        if (target) {
            scrollArea_->ensureWidgetVisible(target, 0, 60);
            return;
        }
        if (historyExhausted_ || oldestLoadedMessageId_ <= 0)
            break;
        loadOlderMessages();
        ++attempts;
    }
}

void TaskChatDialog::setPendingAttachment(const QString &name, const QString &mime, const QByteArray &data)
{
    pendingAttachName_ = name;
    pendingAttachMime_ = mime;
    pendingAttachData_ = data;
    refreshPendingPreview();
}

void TaskChatDialog::clearPendingAttachment()
{
    pendingAttachName_.clear();
    pendingAttachMime_.clear();
    pendingAttachData_.clear();
    refreshPendingPreview();
}

void TaskChatDialog::setPendingForward(const QString &fromUser, const QString &rawPayload)
{
    pendingForwardFrom_ = fromUser.trimmed();
    pendingForwardRaw_ = rawPayload;
    refreshPendingPreview();
}

void TaskChatDialog::clearPendingForward()
{
    pendingForwardFrom_.clear();
    pendingForwardRaw_.clear();
    refreshPendingPreview();
}

void TaskChatDialog::refreshPendingPreview()
{
    if (!attachPreviewLbl_) return;
    QStringList lines;
    if (!pendingForwardRaw_.isEmpty()) {
        lines << QString("Пересланное сообщение от: %1").arg(pendingForwardFrom_.isEmpty() ? QStringLiteral("неизвестно") : pendingForwardFrom_);
    }
    if (!pendingAttachData_.isEmpty()) {
        lines << QString("Вложение: %1 (%2 KB)").arg(pendingAttachName_).arg(qMax(1, pendingAttachData_.size() / 1024));
    }
    if (lines.isEmpty()) {
        attachPreviewLbl_->clear();
        attachPreviewLbl_->setVisible(false);
        if (attachClearBtn_) attachClearBtn_->setVisible(false);
        return;
    }
    attachPreviewLbl_->setText(lines.join("\n"));
    attachPreviewLbl_->setVisible(true);
    if (attachClearBtn_) attachClearBtn_->setVisible(true);
}

void TaskChatDialog::updateDeleteButtonVisibility()
{
    if (!deleteSelectedBtn_ || !messagesLayout_) return;
    QSet<int> visibleSelected;
    for (int i = 0; i < messagesLayout_->count(); ++i) {
        QLayoutItem *item = messagesLayout_->itemAt(i);
        if (!item || !item->widget()) continue;
        QWidget *w = item->widget();
        if (w->objectName() != "chatMessageRow") continue;
        const int mid = w->property("messageId").toInt();
        if (mid > 0 && w->property("selected").toBool()) visibleSelected.insert(mid);
    }
    selectedMessageIds_ = visibleSelected;
    const int count = selectedMessageIds_.size();
    deleteSelectedBtn_->setVisible(count > 0);
}

void TaskChatDialog::deleteSelectedMessages()
{
    if (!messagesLayout_) return;
    QVector<int> ids = selectedMessageIds_.values().toVector();
    if (ids.isEmpty()) return;
    QDialog dlg(this);
    dlg.setWindowTitle("Удалить сообщения");
    dlg.setMinimumWidth(420);
    QVBoxLayout *root = new QVBoxLayout(&dlg);
    QLabel *title = new QLabel(QString("Выбрано сообщений: %1").arg(ids.size()), &dlg);
    title->setStyleSheet("font-weight:800;font-size:14px;");
    root->addWidget(title);
    QFrame *box = new QFrame(&dlg);
    box->setStyleSheet("QFrame{border:1px solid #CBD5E1;border-radius:10px;background:#F8FAFC;}");
    QVBoxLayout *boxL = new QVBoxLayout(box);
    QCheckBox *cbMe = new QCheckBox("Удалить у себя", box);
    QCheckBox *cbAll = new QCheckBox("Удалить также для пользователя", box);
    boxL->addWidget(cbMe);
    boxL->addWidget(cbAll);
    root->addWidget(box);
    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *cancel = new QPushButton("Отмена", &dlg);
    cancel->setMinimumHeight(42);
    cancel->setStyleSheet("QPushButton{background:#E2E8F0;font-weight:800;padding:10px 16px;}QPushButton:hover{background:#CBD5E1;}");
    QPushButton *ok = new QPushButton("Удалить", &dlg);
    ok->setMinimumHeight(42);
    ok->setStyleSheet("QPushButton{background:#DC2626;color:white;font-weight:800;padding:10px 16px;}QPushButton:hover{background:#BE1F1F;}");
    btns->addWidget(cancel);
    btns->addWidget(ok);
    root->addLayout(btns);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    if (dlg.exec() != QDialog::Accepted) return;
    if (cbAll->isChecked()) {
        QString err;
        for (int id : ids) {
            if (!deleteMessage(id, currentUser_, err))
                QMessageBox::warning(this, "Ошибка", err);
        }
        logAction(currentUser_, "chat_messages_deleted_for_all", QString("count=%1").arg(ids.size()));
    } else if (cbMe->isChecked() || !cbAll->isChecked()) {
        QString err;
        for (int id : ids) {
            if (!hideMessageForUser(id, currentUser_, err))
                QMessageBox::warning(this, "Ошибка", err);
        }
        logAction(currentUser_, "chat_messages_hidden_for_me", QString("count=%1").arg(ids.size()));
    }
    DataBus::instance().triggerNotificationsChanged();
    selectedMessageIds_.clear();
    refreshMessages();
    if (deleteSelectedBtn_) deleteSelectedBtn_->setVisible(false);
}

void TaskChatDialog::showNewThreadDialog(const QString &agvId, const QString &currentUser, QWidget *parent,
                                         const QString &preferredRecipient)
{
    QVector<TaskChatRecipient> recipients;
    QVector<TaskChatTaskChoice> tasks;

    tasks.append({"Общий запрос", 0, QString()});

    QSet<QString> seenRecipients;

    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.prepare("SELECT DISTINCT delegated_by FROM agv_tasks WHERE agv_id = :agv AND assigned_to = :u AND delegated_by != '' AND delegated_by != :u");
        q.bindValue(":agv", agvId);
        q.bindValue(":u", currentUser);
        if (q.exec()) {
            while (q.next()) {
                QString del = q.value(0).toString().trimmed();
                if (del.isEmpty()) continue;
                UserInfo ui;
                QString display = del;
                if (loadUserProfile(del, ui) && (!ui.fullName.isEmpty() || !ui.username.isEmpty()))
                    display = ui.fullName.isEmpty() ? ui.username : (ui.fullName + " (" + ui.username + ")");
                if (!seenRecipients.contains(del)) {
                    recipients.append({display, del});
                    seenRecipients.insert(del);
                }
            }
        }
        q.prepare("SELECT id, task_name FROM agv_tasks WHERE agv_id = :agv AND assigned_to = :u ORDER BY task_name");
        q.bindValue(":agv", agvId);
        q.bindValue(":u", currentUser);
        if (q.exec()) {
            while (q.next()) {
                int taskId = q.value(0).toInt();
                QString taskName = q.value(1).toString();
                tasks.append({taskName, taskId, taskName});
            }
        }
    }

    recipients.prepend({"— Кому: выберите —", QString()});
    QVector<UserInfo> allUsers = getAllUsers(false);
    for (const UserInfo &u : allUsers) {
        if (u.username == currentUser) continue;
        QString display = u.fullName.isEmpty() ? u.username : (u.fullName + " (" + u.username + ")");
        if (!seenRecipients.contains(u.username)) {
            recipients.append({display, u.username});
            seenRecipients.insert(u.username);
        }
    }

    TaskChatDialog dlg(TaskChatDialog::CreateThread, agvId, recipients, tasks, parent);
    dlg.currentUser_ = currentUser;
    if (!preferredRecipient.trimmed().isEmpty()) {
        int idx = dlg.recipientCombo_ ? dlg.recipientCombo_->findData(preferredRecipient.trimmed()) : -1;
        if (idx >= 0) dlg.recipientCombo_->setCurrentIndex(idx);
    }
    dlg.exec();
}

void TaskChatDialog::showStartChatDialog(const QString &currentUser, QWidget *parent)
{
    QVector<UserInfo> allUsers = getAllUsers(false);
    QVector<TaskChatRecipient> list;
    for (const UserInfo &u : allUsers) {
        if (u.username == currentUser) continue;
        QString display = u.fullName.isEmpty() ? u.username : (u.fullName + " (" + u.username + ")");
        list.append({display, u.username});
    }
    if (list.isEmpty()) {
        QMessageBox::information(parent, "Начать чат", "Нет других пользователей для начала чата.");
        return;
    }

    QDialog dlg(parent);
    dlg.setWindowTitle("Начать чат");
    dlg.setMinimumSize(360, 420);
    dlg.setStyleSheet(
        "QDialog{background:#F4F6FA;}"
        "QListWidget{background:#FFFFFF;border:1px solid #E5EAF2;border-radius:12px;padding:4px;}"
        "QListWidget::item{min-height:52px;padding:8px 12px;border-radius:8px;}"
        "QListWidget::item:hover{background:#E8EEFF;}"
        "QListWidget::item:selected{background:#D0DCFF;}"
        "QLabel{font-family:Inter;font-size:13px;color:#64748B;}"
    );
    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 16, 16, 16);
    QLabel *hint = new QLabel("Выберите пользователя — откроется чат с ним", &dlg);
    hint->setStyleSheet("font-weight:600;color:#475569;font-size:13px;");
    root->addWidget(hint);
    QListWidget *userList = new QListWidget(&dlg);
    for (const TaskChatRecipient &r : list)
        userList->addItem(r.displayName);
    for (int i = 0; i < list.size(); ++i)
        userList->item(i)->setData(Qt::UserRole, list[i].username);
    root->addWidget(userList, 1);

    connect(userList, &QListWidget::itemClicked, &dlg, [&dlg, userList, list, currentUser, parent](QListWidgetItem *item) {
        if (!item) return;
        int row = userList->row(item);
        if (row < 0 || row >= list.size()) return;
        QString recipientUser = list[row].username;
        if (recipientUser.isEmpty()) return;
        dlg.accept();
        QString err;
        const QString placeHolderAgv("—");
        int tid = createThread(placeHolderAgv, 0, QString(), currentUser, recipientUser, QStringLiteral("—"), err);
        if (tid <= 0) {
            QMessageBox::warning(parent, "Ошибка", "Не удалось создать чат: " + err);
            return;
        }
        logAction(currentUser, "chat_created",
                  QString("thread=%1 start_chat to=%2").arg(tid).arg(recipientUser));
        QString title = "Новый чат";
        const QString fromName = userDisplayName(currentUser);
        QString body = QString("[chat:%1] %2 начал(а) чат с вами.").arg(tid).arg(fromName);
        addNotificationForUser(recipientUser, title, body);
        DataBus::instance().triggerNotificationsChanged();
        const QString role = getUserRole(currentUser);
        const bool isAdmin = (role == "admin" || role == "tech");
        TaskChatDialog chatDlg(tid, currentUser, isAdmin, parent);
        chatDlg.exec();
    });
    QPushButton *cancelBtn = new QPushButton("Отмена", &dlg);
    cancelBtn->setStyleSheet("QPushButton{background:#E2E8F0;color:#334155;font-weight:700;padding:10px 20px;border-radius:10px;} QPushButton:hover{background:#CBD5E1;}");
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    root->addWidget(cancelBtn);
    dlg.exec();
}

void TaskChatDialog::openOrCreateChatWith(const QString &currentUser, const QString &otherUser,
                                          const QString &agvId, QWidget *parent, const QString &specialContext)
{
    QString err;
    int tid = ensureThreadWithUser(currentUser, otherUser, agvId, &err);
    if (tid <= 0) {
        QMessageBox::warning(parent, "Ошибка", err.isEmpty() ? QStringLiteral("Не удалось открыть чат") : err);
        return;
    }
    if (!specialContext.trimmed().isEmpty())
        g_pendingSpecialByThread.insert(tid, specialContext.trimmed());
    const QString role = getUserRole(currentUser);
    const bool isAdmin = (role == "admin" || role == "tech");
    TaskChatDialog chatDlg(tid, currentUser, isAdmin, parent);
    chatDlg.exec();
}

void TaskChatDialog::markNextMessageSpecial(int threadId, const QString &specialContext)
{
    if (threadId <= 0 || specialContext.trimmed().isEmpty())
        return;
    g_pendingSpecialByThread.insert(threadId, specialContext.trimmed());
}

int TaskChatDialog::ensureThreadWithUser(const QString &currentUser, const QString &otherUser, const QString &agvId, QString *outError)
{
    if (otherUser.trimmed().isEmpty()) {
        if (outError) *outError = QStringLiteral("Не выбран собеседник");
        return 0;
    }
    const QString safeAgvId = agvId.trimmed().isEmpty() ? QStringLiteral("—") : agvId.trimmed();
    int tid = getThreadBetweenUsers(currentUser, otherUser, safeAgvId);
    if (tid <= 0)
        tid = getThreadBetweenUsers(currentUser, otherUser, QString());
    if (tid > 0) return tid;
    QString err;
    tid = createThread(safeAgvId, 0, QString(), currentUser, otherUser, QStringLiteral("—"), err);
    if (tid <= 0) {
        if (outError) *outError = err;
        return 0;
    }
    logAction(currentUser, "chat_created",
              QString("thread=%1 open_dialog to=%2 agv=%3").arg(tid).arg(otherUser, safeAgvId));
    const QString fromName = userDisplayName(currentUser);
    addNotificationForUser(otherUser, "Новый чат",
                           QString("[chat:%1] %2 открыл(а) диалог с вами.").arg(tid).arg(fromName));
    DataBus::instance().triggerNotificationsChanged();
    return tid;
}

static void openOrCreateChatWithLocal(const QString &currentUser, const QString &otherUser,
                                     const QString &agvId, QWidget *parent, const QString &specialContext = QString())
{
    TaskChatDialog::openOrCreateChatWith(currentUser, otherUser, agvId, parent, specialContext);
}

QVector<QString> TaskChatDialog::delegatorUsernamesForAgv(const QString &agvId, const QString &currentUser)
{
    QVector<QString> order;
    QSet<QString> unique;
    QSqlDatabase db = QSqlDatabase::database("main_connection");
    if (!db.isOpen())
        return order;
    QSqlQuery q(db);
    q.prepare("SELECT assigned_by FROM agv_list WHERE agv_id = :id AND assigned_user = :u AND COALESCE(TRIM(assigned_by), '') != ''");
    q.bindValue(":id", agvId.trimmed());
    q.bindValue(":u", currentUser);
    if (q.exec()) {
        while (q.next()) {
            QString who = q.value(0).toString().trimmed();
            if (!who.isEmpty() && !unique.contains(who)) {
                unique.insert(who);
                order.append(who);
            }
        }
    }
    q.prepare("SELECT delegated_by FROM agv_tasks WHERE agv_id = :id AND assigned_to = :u AND COALESCE(TRIM(delegated_by), '') != ''");
    q.bindValue(":id", agvId.trimmed());
    q.bindValue(":u", currentUser);
    if (q.exec()) {
        while (q.next()) {
            QString who = q.value(0).toString().trimmed();
            if (!who.isEmpty() && !unique.contains(who)) {
                unique.insert(who);
                order.append(who);
            }
        }
    }
    return order;
}

void TaskChatDialog::showOpenOrCreateChatWithDelegator(const QString &agvId, const QString &currentUser, QWidget *parent)
{
    if (agvId.trimmed().isEmpty()) {
        QMessageBox::information(parent, "Перейти в диалог", "AGV не выбран.");
        return;
    }
    const QVector<QString> order = delegatorUsernamesForAgv(agvId, currentUser);
    if (order.isEmpty()) {
        QMessageBox::information(parent, "Перейти в диалог",
            "По этому AGV вам никто не назначал задачи и не закреплял его за вами. Используйте «Начать чат» в разделе Чаты.");
        return;
    }
    const QString specialContext = QStringLiteral("AGV %1").arg(agvId.trimmed());
    if (order.size() == 1) {
        openOrCreateChatWithLocal(currentUser, order[0], QString(), parent, specialContext);
        return;
    }
    QDialog dlg(parent);
    dlg.setWindowTitle("Перейти в диалог");
    dlg.setMinimumSize(360, 300);
    dlg.setStyleSheet(
        "QDialog{background:#F4F6FA;} QListWidget{background:#FFFFFF;border:1px solid #E5EAF2;border-radius:12px;padding:4px;} "
        "QListWidget::item{min-height:48px;padding:8px 12px;border-radius:8px;} QListWidget::item:hover{background:#E8EEFF;} "
        "QLabel{font-weight:600;color:#475569;font-size:13px;}"
    );
    QVBoxLayout *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 16, 16, 16);
    QLabel *hint = new QLabel("С кем открыть диалог?", &dlg);
    root->addWidget(hint);
    QListWidget *list = new QListWidget(&dlg);
    QVector<UserInfo> allUsers = getAllUsers(false);
    for (const QString &username : order) {
        QString display = username;
        for (const UserInfo &u : allUsers) {
            if (u.username.compare(username, Qt::CaseInsensitive) == 0) {
                display = u.fullName.trimmed().isEmpty() ? u.username : (u.fullName + " (" + u.username + ")");
                break;
            }
        }
        list->addItem(display);
        list->item(list->count() - 1)->setData(Qt::UserRole, username);
    }
    root->addWidget(list, 1);
    connect(list, &QListWidget::itemClicked, &dlg, [&dlg, list, currentUser, specialContext, parent](QListWidgetItem *item) {
        if (!item) return;
        QString otherUser = item->data(Qt::UserRole).toString().trimmed();
        if (otherUser.isEmpty()) return;
        dlg.accept();
        openOrCreateChatWithLocal(currentUser, otherUser, QString(), parent, specialContext);
    });
    QPushButton *cancelBtn = new QPushButton("Отмена", &dlg);
    cancelBtn->setStyleSheet("QPushButton{background:#E2E8F0;color:#334155;font-weight:700;padding:10px 20px;border-radius:10px;} QPushButton:hover{background:#CBD5E1;}");
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    root->addWidget(cancelBtn);
    dlg.exec();
}

void TaskChatDialog::showThreadListDialog(const QString &currentUser, bool isAdmin, QWidget *parent)
{
    QVector<TaskChatThread> threads = isAdmin ? getThreadsForAdmin(currentUser) : getThreadsForUser(currentUser);
    if (threads.isEmpty()) {
        QMessageBox::information(parent, "Чаты по задачам", "Нет чатов.");
        return;
    }

    QDialog listDlg(parent);
    listDlg.setWindowTitle("Чаты по задачам");
    listDlg.setMinimumSize(500, 400);
    QVBoxLayout *root = new QVBoxLayout(&listDlg);
    QScrollArea *scroll = new QScrollArea(&listDlg);
    scroll->setWidgetResizable(true);
    QWidget *host = new QWidget(scroll);
    QVBoxLayout *hostL = new QVBoxLayout(host);
    for (const TaskChatThread &t : threads) {
        QString line = QString("AGV %1").arg(t.agvId);
        if (!t.taskName.isEmpty()) line += " — " + t.taskName;
        line += " · " + t.createdAt.toString("dd.MM.yy hh:mm");
        if (t.isClosed()) line += " [закрыт]";
        QPushButton *btn = new QPushButton(line, host);
        btn->setStyleSheet("text-align:left; padding:10px;");
        connect(btn, &QPushButton::clicked, &listDlg, [&listDlg, t, currentUser, isAdmin, parent]() {
            listDlg.accept();
            TaskChatDialog dlg(t.id, currentUser, isAdmin, parent);
            dlg.exec();
        });
        hostL->addWidget(btn);
    }
    hostL->addStretch();
    scroll->setWidget(host);
    root->addWidget(scroll);
    QPushButton *closeBtn = new QPushButton("Закрыть", &listDlg);
    connect(closeBtn, &QPushButton::clicked, &listDlg, &QDialog::accept);
    root->addWidget(closeBtn);
    listDlg.exec();
}
