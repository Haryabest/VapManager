@echo off
chcp 65001 >nul
setlocal ENABLEDELAYEDEXPANSION
cd /d "%~dp0.."

set "PG_HOST=127.0.0.1"
set "PG_PORT=5432"
set "PG_USER=vapmanager"
set "PG_PASSWORD=51525354"
set "PG_DATABASE=agv_manager_db"

if exist "config.ini" (
  for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b "db_host=" "config.ini" 2^>nul`) do set "PG_HOST=%%B"
  for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b "db_port=" "config.ini" 2^>nul`) do set "PG_PORT=%%B"
  for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b "db_user=" "config.ini" 2^>nul`) do set "PG_USER=%%B"
  for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b "db_password=" "config.ini" 2^>nul`) do set "PG_PASSWORD=%%B"
  for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b "db_name=" "config.ini" 2^>nul`) do set "PG_DATABASE=%%B"
)

echo === Seed test data (100 models, 100 AGV, 10 users) ===
echo Host: %PG_HOST%:%PG_PORT%  DB: %PG_DATABASE%  User: %PG_USER%
echo.

python tools\seed_test_data.py ^
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
  echo [ERROR] Seed failed. Install deps: pip install psycopg2-binary
  pause
  exit /b 1
)

echo.
echo [OK] Test data loaded.
pause
exit /b 0
