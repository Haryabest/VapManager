# Refactor Structure Plan (Phase 1)

Цель: подготовить понятную модульную структуру без немедленного перемещения файлов.

## Созданный каркас папок

```text
src/
  app/
  core/
    session/
    events/
    logging/
  data/
    db/
    repositories/
  features/
    auth/
      ui/
    account/
      ui/
    agv/
      ui/
    chat/
      ui/
    notifications/
    users/
      ui/
  shared/
    widgets/
resources/
  icons/
  translations/
docs/
```

## Базовая карта переноса файлов

### App bootstrap
- `main.cpp` -> `src/app/main.cpp`
- `mainwindow.h/.cpp` -> `src/app/`

### Core
- `app_session.h/.cpp` -> `src/core/session/`
- `databus.h/.cpp` -> `src/core/events/`
- `diag_logger.h/.cpp` -> `src/core/logging/`
- `ui_action_logger.h/.cpp` -> `src/core/logging/`

### Data
- `db.h/.cpp` -> `src/data/db/`
- `db_users.h/.cpp` -> `src/data/repositories/`
- `db_agv_tasks.h/.cpp` -> `src/data/repositories/`
- `db_models.h/.cpp` -> `src/data/repositories/`
- `db_task_chat.h/.cpp` -> `src/data/repositories/`
- `notifications_logs.h/.cpp` -> `src/data/repositories/`

### Features / Auth
- `logindialog.h/.cpp` -> `src/features/auth/ui/`

### Features / Account
- `accountinfodialog.h/.cpp` -> `src/features/account/ui/`

### Features / AGV
- `leftmenu.h/.cpp` -> `src/features/agv/ui/`
- `listagvinfo.h/.cpp` -> `src/features/agv/ui/`
- `agvsettingspage.h/.cpp` -> `src/features/agv/ui/`
- `addagvdialog.h/.cpp` -> `src/features/agv/ui/`
- `modellistpage.h/.cpp` -> `src/features/agv/ui/`
- `maintenanceitemwidget.h/.cpp` -> `src/features/agv/ui/`

### Features / Chat
- `taskchatdialog.h/.cpp` -> `src/features/chat/ui/`

### Features / Users
- `userspage.h/.cpp` -> `src/features/users/ui/`

### Shared Widgets
- `multisectionwidget.h/.cpp` -> `src/shared/widgets/`

## Правила поэтапного переноса

1. Переносить по одному модулю (auth -> account -> agv ...), не всё сразу.
2. После каждого модуля:
   - поправить include-пути,
   - обновить `AgvNewUi.pro` (`SOURCES` и `HEADERS`),
   - собрать проект.
3. Не смешивать рефакторинг структуры с логическими изменениями.
4. На каждом шаге делать отдельный commit.

## Следующий шаг

Начать с `accountinfodialog.h/.cpp`, затем `logindialog.h/.cpp`.
