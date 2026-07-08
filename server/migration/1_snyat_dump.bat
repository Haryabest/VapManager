@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

echo.
echo ============================================================
echo   VapManager — снять дамп agv_manager_db (старый сервер)
echo ============================================================
echo.

set "DUMP_FILE=%~dp0agv_manager_full.dump"
set "DB_NAME=agv_manager_db"

set "PG_DUMP="
for %%V in (18 17 16 15 14) do (
    if exist "C:\Program Files\PostgreSQL\%%V\bin\pg_dump.exe" (
        set "PG_DUMP=C:\Program Files\PostgreSQL\%%V\bin\pg_dump.exe"
        goto :found
    )
)

:found
if "%PG_DUMP%"=="" (
    echo [ОШИБКА] pg_dump.exe не найден.
    pause
    exit /b 1
)

echo Введите пароль postgres:
set /p PGPASSWORD=

echo Создание %DUMP_FILE% ...
"%PG_DUMP%" -U postgres -h localhost -Fc -f "%DUMP_FILE%" %DB_NAME%
set "RC=%ERRORLEVEL%"
set "PGPASSWORD="

if %RC% neq 0 (
    echo [ОШИБКА] код %RC%
    pause
    exit /b %RC%
)

for %%F in ("%DUMP_FILE%") do echo [OK] Дамп создан: %%~zF байт
echo Скопируйте server\migration\ на новый сервер и запустите 2_vosstanovit_bazu.bat
pause
exit /b 0
