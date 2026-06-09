# VapManager — PostgreSQL

Отдельная копия проекта на рабочем столе: **`C:\Users\Dima\Desktop\VapManager-PostgreSQL`**

Клиенты подключаются к одной БД PostgreSQL (локально или по сети). Параметры — в `config.ini` рядом с `VapManager.exe`.

## 1. Установка PostgreSQL (сервер)

1. Скачайте [PostgreSQL для Windows](https://www.postgresql.org/download/windows/) (15 или 16).
2. Запомните пароль суперпользователя `postgres`.
3. После установки выполните скрипт удалённого доступа (от имени администратора):

```powershell
cd C:\Users\Dima\Desktop\VapManager-PostgreSQL\postgresql
.\configure_remote.ps1
```

Скрипт включает прослушивание всех интерфейсов (`listen_addresses = '*'`), правило в `pg_hba.conf` для сети и открывает порт **5432** в брандмауэре Windows.

Перезапустите службу PostgreSQL из «Службы» Windows или:

```powershell
Restart-Service postgresql-x64-16
```

## 2. Создание схемы

```powershell
cd C:\Users\Dima\Desktop\VapManager-PostgreSQL
& "C:\Program Files\PostgreSQL\16\bin\psql.exe" -U postgres -f install_agv_manager_db.sql
```

Смените пароль пользователя приложения:

```sql
ALTER ROLE vapmanager PASSWORD 'ваш_надёжный_пароль';
```

Тот же пароль укажите в `config.ini` → `db_password`.

## 3. Перенос данных из MySQL

Установите зависимости Python:

```powershell
pip install mysql-connector-python psycopg2-binary
```

Запустите миграцию (подставьте свои пароли):

```powershell
python tools\migrate_mysql_to_postgres.py ^
  --mysql-host localhost --mysql-user root --mysql-password "" ^
  --pg-host localhost --pg-user vapmanager --pg-password vapmanager_change_me
```

Для удалённого MySQL: `--mysql-host 10.x.x.x`  
Для удалённого PostgreSQL: `--pg-host IP_СЕРВЕРА`

## 4. Настройка клиентов (config.ini)

```ini
db_host=192.168.1.10
db_port=5432
db_name=agv_manager_db
db_user=vapmanager
db_password=ваш_пароль
language=ru
```

Формат `db_host=IP:5432` тоже поддерживается (порт можно вынести в `db_port`).

## 5. Сборка и запуск

1. Откройте `AgvNewUi.pro` в Qt Creator (kit MinGW 64-bit, Qt 5.14+).
2. Соберите Release → `VapManager.exe`.
3. Рядом с exe: `deploy_runtime.bat "путь\к\VapManager.exe"`.
4. Скопируйте `config.ini` в папку с exe.

## Отличия от версии с MySQL

| | MySQL (старый проект) | PostgreSQL (этот проект) |
|--|----------------------|---------------------------|
| Драйвер Qt | QMYSQL | QPSQL |
| DLL | libmysql.dll, qsqlmysql.dll | libpq.dll, qsqlpsql.dll |
| Порт по умолчанию | 3306 | 5432 |
| SQL-скрипт | install (MySQL) | install_agv_manager_db.sql |

Исходный проект MySQL: `C:\Users\Dima\Desktop\AGV_NewUI\VapManager` — не изменялся.
