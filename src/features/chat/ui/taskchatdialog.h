#pragma once

#include <QDialog>
#include <QWidget>
#include <QString>
#include <QVector>
#include <functional>
#include <QSet>
#include <QPixmap>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QResizeEvent>

class ImagePreviewDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ImagePreviewDialog(const QPixmap &image,
                                const QString &fileName,
                                const QByteArray &imageData,
                                QWidget *parent = nullptr,
                                const QString &currentUser = QString(),
                                int messageId = 0);
    
    int getDialogResult() const { return result_; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void centerImage();
    void updateGeometry();
    
    QPixmap image_;
    QByteArray imageData_;
    QString fileName_;
    QString currentUser_;
    int messageId_;
    int result_;
    
    QLabel *imageLabel_;
    QPushButton *saveBtn_;
    QPushButton *deleteBtn_;
    QWidget *buttonsWidget_;
    
    QRect imageRect_;
    bool mousePressed_;
};

struct TaskChatRecipient {
    QString displayName;  // "ФИО (логин)" or "Все админы"
    QString username;     // empty = all admins
};

struct TaskChatTaskChoice {
    QString displayName;  // "Общий запрос" or task name
    int taskId;
    QString taskName;
};

struct TaskChatMessage;

class QComboBox;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QTimer;
class QLabel;
class QCheckBox;
class QToolButton;
class QDragEnterEvent;
class QDropEvent;

/// Виджет одного чата для встраивания в главное окно (список чатов → этот виджет замещает список)
class TaskChatWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TaskChatWidget(int threadId, const QString &currentUser, bool isAdmin,
                           std::function<int(int)> scaleFn, QWidget *parent = nullptr);
    void setThreadId(int id, const QString &peerHint = QString());
    int threadId() const { return threadId_; }
    bool autotestSendTextMessage(const QString &message, QString *error = nullptr);
    bool autotestRejectsEmptyMessage(QString *error = nullptr);
    bool autotestTextSelection(QString *error = nullptr);

signals:
    void backRequested();
    void showProfileRequested(const QString &username);

private:
    void setupUi();
    void refreshMessages(bool fullReload = true);
    void loadOlderMessages();
    void sendReply();
    void sendAttachment();
    void showMediaHistoryDialog();
    void jumpToMessage(int messageId);
    void setPendingAttachment(const QString &name, const QString &mime, const QByteArray &data);
    void clearPendingAttachment();
    void setPendingForward(const QString &fromUser, const QString &rawPayload);
    void clearPendingForward();
    void refreshPendingPreview();
    void startReplyToMessage(int messageId, const QString &text);
    void deleteThreadByAdmin();
    void deleteThreadForMe();
    void deleteThreadForAll();

    int threadId_;
    QString currentUser_;
    bool isAdmin_;
    std::function<int(int)> s_;
    QPushButton *backBtn_ = nullptr;
    QScrollArea *scrollArea_ = nullptr;
    QVBoxLayout *messagesLayout_ = nullptr;
    QLineEdit *replyEdit_ = nullptr;
    QPushButton *attachBtn_ = nullptr;
    QPushButton *sendBtn_ = nullptr;
    QLabel *attachPreviewLbl_ = nullptr;
    QPushButton *attachClearBtn_ = nullptr;
    QPushButton *deleteChatBtn_ = nullptr;
    QPushButton *deleteForMeBtn_ = nullptr;
    QPushButton *deleteForAllBtn_ = nullptr;
    QTimer *liveRefreshTimer_ = nullptr;
    QTimer *presenceRefreshTimer_ = nullptr;
    bool forceScrollToBottom_ = true;
    QPushButton *deleteSelectedBtn_ = nullptr;
    QPushButton *cancelSelectionBtn_ = nullptr;
    QLabel *selectionCountLbl_ = nullptr;
    QLabel *peerAvatarLbl_ = nullptr;
    QLabel *peerLbl_ = nullptr;
    QLabel *peerLastSeenLbl_ = nullptr;
    QLabel *peerStatusDot_ = nullptr;
    QLabel *headerLbl_ = nullptr;
    QToolButton *moreBtn_ = nullptr;
    QLabel *pinnedLbl_ = nullptr;
    QPushButton *unpinnedBtn_ = nullptr;
    QLabel *replyBannerLbl_ = nullptr;
    QLabel *dayLabel_ = nullptr;
    int replyToMessageId_ = 0;
    int pinnedMessageId_ = 0;
    bool selectionMode_ = false;
    bool draggingSelection_ = false;
    bool dragSelectValue_ = true;
    QString peerHint_;
    QString peerUsername_;
    QSet<int> selectedMessageIds_;
    static constexpr int kMessagesPageSize = 75;
    QString pendingAttachName_;
    QString pendingAttachMime_;
    QByteArray pendingAttachData_;
    QString pendingForwardFrom_;
    QString pendingForwardRaw_;
    qint64 lastSentMs_ = 0;
    int oldestLoadedMessageId_ = 0;
    int lastLoadedMessageId_ = 0;
    bool historyExhausted_ = false;
    bool loadingOlderMessages_ = false;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void updatePeerHeaderMeta();
    QWidget* createMessageRow(const TaskChatMessage &m);
    void enterSelectionMode();
    void exitSelectionMode();
    void showMessageContextMenu(const QPoint &globalPos);
    QWidget* messageRowAtGlobalPos(const QPoint &globalPos) const;
    void setMessageRowSelected(QWidget *row, bool selected);
    bool isMessageRowSelected(QWidget *row) const;
    void updateDeleteButtonVisibility();
    void deleteSelectedMessages();
    void updateDayLabel();
};

class TaskChatDialog : public QDialog
{
    Q_OBJECT
public:
    enum Mode { CreateThread, ViewThread };

    explicit TaskChatDialog(Mode mode,
                            const QString &agvId,
                            const QVector<TaskChatRecipient> &recipients,
                            const QVector<TaskChatTaskChoice> &tasks,
                            QWidget *parent = nullptr);

    explicit TaskChatDialog(int threadId, const QString &currentUser, bool isAdmin, QWidget *parent = nullptr);

    static void showNewThreadDialog(const QString &agvId, const QString &currentUser, QWidget *parent,
                                    const QString &preferredRecipient = QString());
    /// Показать список пользователей (как в Telegram): клик по пользователю — открыть пустой чат с ним
    static void showStartChatDialog(const QString &currentUser, QWidget *parent);
    /// Открыть или создать чат с тем, кто делегировал задачу или назначил AGV текущему пользователю
    static void showOpenOrCreateChatWithDelegator(const QString &agvId, const QString &currentUser, QWidget *parent);
    /// Логины делегаторов (assigned_by / delegated_by) в том же порядке, что и для «Перейти в диалог».
    static QVector<QString> delegatorUsernamesForAgv(const QString &agvId, const QString &currentUser);
    /// Открыть существующий чат с пользователем или создать новый
    static void openOrCreateChatWith(const QString &currentUser, const QString &otherUser, const QString &agvId, QWidget *parent,
                                     const QString &specialContext = QString());
    static void markNextMessageSpecial(int threadId, const QString &specialContext);
    /// Найти или создать тред и вернуть его id (0 если ошибка)
    static int ensureThreadWithUser(const QString &currentUser, const QString &otherUser, const QString &agvId, QString *outError = nullptr);
    static void showThreadListDialog(const QString &currentUser, bool isAdmin, QWidget *parent);

private:
    void setupCreateUi();
    void setupViewUi();
    void sendNewThread();
    void sendReply();
    void sendAttachment();
    void jumpToMessage(int messageId);
    void setPendingAttachment(const QString &name, const QString &mime, const QByteArray &data);
    void clearPendingAttachment();
    void setPendingForward(const QString &fromUser, const QString &rawPayload);
    void clearPendingForward();
    void refreshPendingPreview();
    void deleteThreadByAdmin();
    void deleteThreadForMe();
    void deleteThreadForAll();
    void refreshMessages(bool fullReload = true);
    void loadOlderMessages();

    Mode mode_;
    QString agvId_;
    QVector<TaskChatRecipient> recipients_;
    QVector<TaskChatTaskChoice> tasks_;
    int threadId_ = 0;
    QString currentUser_;
    bool isAdmin_ = false;

    QComboBox *recipientCombo_ = nullptr;
    QComboBox *taskCombo_ = nullptr;
    QTextEdit *messageEdit_ = nullptr;
    QLineEdit *replyEdit_ = nullptr;
    QPushButton *attachBtn_ = nullptr;
    QPushButton *sendBtn_ = nullptr;
    QLabel *attachPreviewLbl_ = nullptr;
    QPushButton *attachClearBtn_ = nullptr;
    QPushButton *deleteChatBtn_ = nullptr;
    QPushButton *deleteForMeBtn_ = nullptr;
    QPushButton *deleteForAllBtn_ = nullptr;
    QVBoxLayout *messagesLayout_ = nullptr;
    QScrollArea *scrollArea_ = nullptr;
    QTimer *liveRefreshTimer_ = nullptr;
    bool forceScrollToBottom_ = true;
    QPushButton *deleteSelectedBtn_ = nullptr;
    QSet<int> selectedMessageIds_;
    QString pendingAttachName_;
    QString pendingAttachMime_;
    QByteArray pendingAttachData_;
    QString pendingForwardFrom_;
    QString pendingForwardRaw_;
    qint64 lastSentMs_ = 0;
    int oldestLoadedMessageId_ = 0;
    int lastLoadedMessageId_ = 0;
    bool historyExhausted_ = false;
    bool loadingOlderMessages_ = false;
    static constexpr int kMessagesPageSize = 75;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void updateDeleteButtonVisibility();
    void deleteSelectedMessages();
};
