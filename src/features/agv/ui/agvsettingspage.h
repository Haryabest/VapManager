#ifndef AGVSETTINGSPAGE_H
#define AGVSETTINGSPAGE_H

#include "db_agv_tasks.h"

#include <QWidget>
#include <QString>
#include <QDate>
#include <QList>
#include <functional>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QCheckBox;
class QLineEdit;
class QComboBox;
class QFrame;
class QTimer;

class AgvSettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit AgvSettingsPage(std::function<int(int)> scale, QWidget *parent = nullptr);

    void loadAgv(const QString &agvId);

    // 🔥 Подсветка задачи из блока "Предстоящее ТО"
    void highlightTask(const QString &taskName);

signals:
    void backRequested();
    void tasksChanged();   // ★ ДОБАВЛЕНО — сигнал для leftMenu
    /// Открыть встроенный чат с делегатором по текущему AGV (раздел «Чаты», не модальное окно).
    void openDelegatorChatRequested(const QString &agvId);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    // ===== Контекст и масштаб =====
    std::function<int(int)> s;
    QString currentAgvId;

    // ===== Верхняя панель =====
    QPushButton *backButton = nullptr;
    QLabel *titleLabel = nullptr;

    // ===== Информация об AGV =====
    QLabel *idLabel = nullptr;
    QLabel *modelLabel = nullptr;
    QLineEdit *modelEdit = nullptr;
    QLabel *serialLabel = nullptr;
    QLabel *statusLabel = nullptr;
    QLabel *kmLabel = nullptr;
    QLabel *currentTaskLabel = nullptr;
    QLabel *blueprintLabel = nullptr;
    QLineEdit *idEdit = nullptr;
    QLineEdit *serialEdit = nullptr;
    QComboBox *statusCombo = nullptr;
    QLineEdit *kmEdit = nullptr;
    QLineEdit *currentTaskEdit = nullptr;
    QPushButton *editAgvBtn = nullptr;
    QPushButton *saveAgvBtn = nullptr;
    QPushButton *cancelAgvBtn = nullptr;
    QLabel *assignedUserLabel = nullptr;
    QComboBox *assignedUserCombo = nullptr;
    QPushButton *pinAgvBtn = nullptr;
    bool agvEditMode = false;
    QString originalAgvId;
    QString originalSerial;
    QString originalStatus;
    int originalKm = 0;
    QString originalCurrentTask;
    QString originalAssignedUser;
    QString originalAssignedBy;

    // ===== Основной layout =====
    QVBoxLayout *rootLayout = nullptr;

    // ===== Таблица задач =====
    QWidget *tableWrapper = nullptr;
    QVBoxLayout *tasksLayout = nullptr;
    QWidget *headerRow = nullptr;

    // ===== Форма (добавление/редактирование) =====
    QWidget *formWrapper = nullptr;
    bool addFormOpened = false;

    // ===== Кнопки управления =====
    QPushButton *addTaskBtn = nullptr;
    QPushButton *editModeBtn = nullptr;
    QPushButton *deleteSelectedBtn = nullptr;
    QPushButton *undoDeleteBtn = nullptr;
    QPushButton *historyTasksBtn = nullptr;

    bool editMode = false;

    // ===== Undo удаления =====
    QList<AgvTask> recentlyDeleted;
    QTimer *undoTimer = nullptr;

    // ===== Таблица задач =====
    void clearTasks();
    void buildTasksHeader();
    void addTaskRow(const AgvTask &task, const QString &taskId);

    // ===== Формы задач =====
    void openAddTaskForm();
    void openEditTaskForm(const QString &taskId, const AgvTask &task);

    // ===== Операции над задачами =====
    void deleteTask(const QString &taskId);
    void deleteSelectedTasks();
    QDate computeNextDate(const QDate &lastService, int intervalDays) const;

    // ===== Редактирование AGV =====
    void toggleEditMode();
    void updateCheckboxVisibility();
    void enterAgvEditMode();
    void leaveAgvEditMode();
    void refreshAgvEditButtons();
    bool saveAgvInfo();

    // ===== Работа с историей/чатом =====
    AgvTask loadTaskById(const QString &taskId) const;
    void showForm(QWidget *form);
    void closeForm();
    bool ensureTaskHistoryTable() const;
    bool completeTaskNow(const QString &taskId, const AgvTask &task);
    void openTaskHistoryDialog();

    // ===== Undo удаления =====
    void startUndoTimer();
    void cancelUndo();
    void restoreDeletedTasks();
};

#endif // AGVSETTINGSPAGE_H
