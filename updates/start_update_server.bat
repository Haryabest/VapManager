@echo off
setlocal
cd /d "%~dp0"

set "PORT=8765"
set "CONFIG=..\config.ini"
set "IP=127.0.0.1"

for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b "db_host=" "%CONFIG%" 2^>nul`) do set "IP=%%B"

echo.
echo VapManager update server (port %PORT%)
echo   Folder: %CD%
echo   URL:    http://%IP%:%PORT%/version.json
echo   Files:  http://%IP%:%PORT%/files/
echo.
echo Press Ctrl+C to stop.
echo.

where python >nul 2>&1
if errorlevel 1 (
  echo [ERROR] Python not found. Install Python 3 or use PowerShell script as admin.
  exit /b 1
)

python -m http.server %PORT% --bind 0.0.0.0
