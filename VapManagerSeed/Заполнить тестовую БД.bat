@echo off
chcp 65001 >nul
setlocal ENABLEDELAYEDEXPANSION
cd /d "%~dp0"

set "PG_HOST=127.0.0.1"
set "PG_PORT=5432"
set "PG_USER=vapmanager"
set "PG_PASSWORD=51525354"
set "PG_DATABASE=agv_manager_db"

set "CFG=%~dp0config.ini"
if not exist "%CFG%" (
  echo [ERROR] Ne nayden config.ini v papke VapManagerSeed.
  echo Skopiruyte config.ini.server-local iz kornya proekta.
  pause
  exit /b 1
)

if exist "%CFG%" (
  echo Config: %CFG%
  for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b "db_host=" "%CFG%" 2^>nul`) do set "PG_HOST=%%B"
  for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b "db_port=" "%CFG%" 2^>nul`) do set "PG_PORT=%%B"
  for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b "db_user=" "%CFG%" 2^>nul`) do set "PG_USER=%%B"
  for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b "db_password=" "%CFG%" 2^>nul`) do set "PG_PASSWORD=%%B"
  for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b "db_name=" "%CFG%" 2^>nul`) do set "PG_DATABASE=%%B"
) else (
  echo [WARN] config.ini не найден — используются значения по умоложанию.
)

echo === Заполнение тестовой БД VapManager ===
echo 100 моделей, 100 AGV, 10 пользователей
echo Host: %PG_HOST%:%PG_PORT%  DB: %PG_DATABASE%  User: %PG_USER%
echo.

where python >nul 2>&1
if errorlevel 1 (
  echo [ERROR] Python не найден. Установите Python 3 и выполните:
  echo   pip install psycopg2-binary
  pause
  exit /b 1
)

python "%~dp0seed_test_data.py" ^
  --pg-host "%PG_HOST%" ^
  --pg-port %PG_PORT% ^
  --pg-user "%PG_USER%" ^
  --pg-password "%PG_PASSWORD%" ^
  --pg-database "%PG_DATABASE%" ^
  --model-count 100 ^
  --agv-count 100 ^
  --clear-test %*

if errorlevel 1 (
  echo.
  echo [ERROR] Ошибка. Установите зависимость: pip install psycopg2-binary
  pause
  exit /b 1
)

echo.
echo [OK] Тестовые данные загружены.
echo Пароль всех test_* пользователей: Test12345!
pause
exit /b 0

