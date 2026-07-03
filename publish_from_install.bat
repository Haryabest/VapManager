@echo off
setlocal
cd /d "%~dp0"

set "SOURCE=%~1"
if "%SOURCE%"=="" set "SOURCE=%LOCALAPPDATA%\Programs\VapManager"
if not exist "%SOURCE%\VapManager.exe" (
  echo [ERROR] VapManager.exe not found in:
  echo   %SOURCE%
  echo.
  echo Usage: publish_from_install.bat "C:\path\to\VapManager\folder"
  exit /b 1
)

echo === Publish updates from installed VapManager ===
echo Source: %SOURCE%
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0server\publish_updates_from_folder.ps1" ^
  -SourceDir "%SOURCE%" ^
  -RepoRoot "%~dp0." ^
  -Version "1.0.2" ^
  -Build 152 ^
  -ServerIp "192.168.0.1"
if errorlevel 1 exit /b 1

echo.
echo [OK] Files: updates\files\
echo [OK] Manifest: updates\version.json
echo.
echo Keep update server running:
echo   updates\start_update_server.bat
echo.
echo Client check URL:
echo   http://192.168.0.1:8765/version.json
exit /b 0
