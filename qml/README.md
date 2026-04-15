# QML Страница Регистрации

## Обзор

Этот модуль заменяет визуальную часть страницы регистрации (ранее реализованную на C++ Widgets) на QML, сохраняя всю бизнес-логику в C++.

## Архитектура

```
┌─────────────────────────────────────────────┐
│                QML Layer                     │
│  ┌─────────────────────────────────────┐    │
│  │     RegisterPage.qml                │    │
│  │  - Поля ввода (логин, пароль, роль) │    │
│  │  - Валидация UI                     │    │
│  │  - Индикатор силы пароля            │    │
│  │  - Кнопки (Создать, Назад)          │    │
│  └──────────────┬──────────────────────┘    │
│                 │ Сигналы                    │
└─────────────────┼───────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────────┐
│            C++ Controller                    │
│  ┌─────────────────────────────────────┐    │
│  │  RegisterPageController             │    │
│  │  - Валидация данных                 │    │
│  │  - Проверка ключей (admin/tech)     │    │
│  │  - Вызов registerUser()             │    │
│  │  - Генерация ключа восстановления   │    │
│  └──────────────┬──────────────────────┘    │
│                 │                            │
└─────────────────┼───────────────────────────┘
                  ▼
         db_users.h функции:
         - registerUser()
         - verifyAdminInviteKey()
         - verifyTechInviteKey()
         - hasAnyAdmin(), hasAnyTech()
```

## Структура файлов

```
qml/
├── pages/
│   └── RegisterPage.qml        # Главная страница регистрации
├── components/
│   ├── AppInput.qml            # Кастомное поле ввода
│   ├── AppButton.qml           # Кастомная кнопка (primary/secondary/ghost)
│   └── AppComboBox.qml         # Кастомный ComboBox
├── style/
│   └── Theme.qml               # Цветовая схема
└── js/
    └── auth.js                 # (можно удалить, логика теперь в C++)

src/features/account/ui/
├── registerpage_controller.h   # Заголовочный файл контроллера
├── registerpage_controller.cpp # Реализация контроллера
└── logindialog.cpp             # (старая версия на Widgets, можно оставить)
```

## Использование

### Вариант 1: Интеграция в существующий LoginDialog

В `app_bootstrap.cpp` замените показ `LoginDialog` на QML версию:

```cpp
#include "registerpage_controller.h"
#include <QQuickView>
#include <QQmlContext>

// Вместо:
// LoginDialog dlg;
// if (dlg.exec() != QDialog::Accepted)
//     return 0;
// user = dlg.user();

// Используйте:
QQuickView *regView = new QQuickView();
RegisterPageController *regController = new RegisterPageController(regView);

// Подключение сигналов
QObject::connect(regController, &RegisterPageController::requestGoBack,
                 [&]() { 
                     // Возврат к странице входа
                     regView->close();
                     // Показать LoginDialog или другую страницу
                 });

QObject::connect(regController, &RegisterPageController::requestShowRecoveryKey,
                 [](const QString &username, const QString &key) {
                     // Показать диалог с ключом восстановления
                     QMessageBox::information(nullptr, "Ключ восстановления",
                                            "Ваш ключ: " + key);
                 });

QObject::connect(regController, &RegisterPageController::requestLoginSuccess,
                 [&user](const UserInfo &userInfo) {
                     user = userInfo;
                 });

regController->show(regView);
regView->show();
```

### Вариант 2: Отдельное окно регистрации

```cpp
#include "registerpage_controller.h"
#include <QQuickView>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    QQuickView view;
    view.setTitle("Регистрация - AGV Manager");
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setMinimumSize(QSize(500, 700));
    
    RegisterPageController controller(&view);
    
    QObject::connect(&controller, &RegisterPageController::requestGoBack,
                     [&]() { view.close(); });
    
    controller.show(&view);
    view.show();
    
    return app.exec();
}
```

## Сигналы QML → C++

| Сигнал | Параметры | Описание |
|--------|-----------|----------|
| `registerClicked` | login, password, confirmPassword, role, adminKey, techKey | Нажата кнопка "Создать аккаунт" |
| `backClicked` | - | Нажата кнопка "Назад" |
| `password1Changed` | text | Изменено первое поле пароля (для расчёта силы) |
| `loginTextChanged` | text | Изменён логин |
| `password2TextChanged` | text | Изменено подтверждение пароля |
| `roleChanged` | index | Изменена роль в ComboBox |

## Свойства QML (для установки из C++)

| Свойство | Тип | Описание |
|----------|-----|----------|
| `loginText` | string | Текст поля логина |
| `password1Text` | string | Текст первого поля пароля |
| `password2Text` | string | Текст поля подтверждения пароля |
| `errorMessage` | string | Текст ошибки (показывается красным) |
| `passwordStrength` | int | Сила пароля (0-100) |
| `passwordStrengthText` | string | Текстовое описание силы пароля |
| `passwordStrengthColor` | color | Цвет индикатора силы пароля |
| `adminKeyVisible` | bool | Показать поле ключа администратора |
| `techKeyVisible` | bool | Показать поле ключа техника |

## C++ Функции контроллера

### Публичные слоты (вызываются из QML)

```cpp
void onRegisterClicked(const QString &login, const QString &password,
                       const QString &confirmPassword, const QString &role,
                       const QString &adminKey, const QString &techKey);
void onBackClicked();
void onPassword1Changed(const QString &text);
void onLoginTextChanged(const QString &text);
void onPassword2TextChanged(const QString &text);
void onRoleChanged(int index);
```

### Сигналы (отправляются в QML/приложение)

```cpp
void requestGoBack();
void requestShowRecoveryKey(const QString &username, const QString &recoveryKey);
void requestLoginSuccess(const UserInfo &user);
```

## Валидация

### Логин
- Только латинские буквы и цифры: `^[A-Za-z0-9]+$`
- Не может быть пустым

### Пароль
- Минимум 8 символов
- Только печатные ASCII символы: `^[\x21-\x7E]+$`
- Сила пароля рассчитывается автоматически:
  - **Слабый** (0-34): красный
  - **Средний** (35-59): жёлтый
  - **Надёжный** (60-84): зелёный
  - **Отличный** (85-100): тёмно-зелёный

### Роли и ключи
- **Пользователь (viewer)**: без ключа
- **Администратор (admin)**: требуется ключ от действующего админа (если админы уже есть)
- **Техник (tech)**: требуется ключ от действующего техника (если техники уже есть)

## Отличия от C++ версии

| Функция | C++ Widgets | QML |
|---------|-------------|-----|
| Визуал | QWidget | QML |
| Логика валидации | В logindialog_wiring.cpp | В registerpage_controller.cpp |
| Восстановление доступа | Есть | TODO (отдельная страница) |
| Смена пароля | Есть | TODO (отдельная страница) |
| Автозапоминание | enableRememberMe() | TODO |

## TODO

- [ ] Страница входа (LoginPage.qml)
- [ ] Страница восстановления доступа
- [ ] Страница смены пароля
- [ ] Интеграция с enableRememberMe()
- [ ] Анимации переходов между страницами
- [ ] Unit тесты для RegisterPageController

## Заметки

- Вся бизнес-логика осталась в `db_users.h` функциях
- QML отвечает только за отображение и ввод данных
- Контроллер (`RegisterPageController`) связывает QML и C++ логику
- Валидация дублируется и в QML (для UX), и в C++ (для безопасности)
