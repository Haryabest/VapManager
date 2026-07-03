@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

echo.
echo ============================================================
echo   VapManager — восстановление agv_manager_full.dump
echo ============================================================
echo Запускайте на НОВОМ сервере после setup_server.bat
echo.

set "DUMP_FILE=%~dp0agv_manager_full.dump"
set "DB_NAME=agv_manager_db"

if not exist "%DUMP_FILE%" (
    echo [ОШИБКА] Не найден %DUMP_FILE%
    echo Положите дамп в эту папку или создайте его на старом сервере:
    echo   pg_dump -U postgres -Fc -f agv_manager_full.dump agv_manager_db
    pause
    exit /b 1
)

set "PG_RESTORE="
set "PSQL="
for %%V in (18 17 16 15 14) do (
    if exist "C:\Program Files\PostgreSQL\%%V\bin\pg_restore.exe" (
        set "PG_RESTORE=C:\Program Files\PostgreSQL\%%V\bin\pg_restore.exe"
        set "PSQL=C:\Program Files\PostgreSQL\%%V\bin\psql.exe"
        goto :found
    )
)

:found
if "%PG_RESTORE%"=="" (
    echo [ОШИБКА] pg_restore.exe не найден. Установите PostgreSQL 15+.
    pause
    exit /b 1
)

echo Введите пароль postgres:
set /p PGPASSWORD=

echo Восстановление...
"%PG_RESTORE%" -U postgres -h localhost -d %DB_NAME% --clean --if-exists --no-owner --role=vapmanager -v "%DUMP_FILE%"
set "RC=%ERRORLEVEL%"
if %RC% neq 0 (
    set "PGPASSWORD="
    echo [ОШИБКА] код %RC%
    pause
    exit /b %RC%
)

set "PGPASSWORD=51525354"
"%PSQL%" -U vapmanager -h localhost -d %DB_NAME% -c "SELECT 'users' AS tbl, COUNT(*) FROM users UNION ALL SELECT 'agv_list', COUNT(*) FROM agv_list;"
set "PGPASSWORD="

echo.
echo [OK] На серверном ПК в config.ini у VapManager: db_host=127.0.0.1
echo      На клиентах: db_host=IP сервера (см. config.ini.client.example)
pause
exit /b 0
