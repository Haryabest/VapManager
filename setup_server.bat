@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

echo.
echo ============================================================
echo   VapManager — настройка сервера (БД + сеть + обновления)
echo ============================================================
echo.
echo Нужны права администратора.
echo Перед запуском установите PostgreSQL 15+ (запомните пароль postgres).
echo.

net session >nul 2>&1
if errorlevel 1 (
    echo Запрос прав администратора...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b 0
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0server\setup_server.ps1" -RepoRoot "%~dp0"
set "RC=%ERRORLEVEL%"
echo.
if %RC% neq 0 (
    echo [ОШИБКА] Код %RC%. Смотрите сообщения выше.
    pause
    exit /b %RC%
)

echo Нажмите любую клавишу для выхода...
pause >nul
exit /b 0
