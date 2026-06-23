@echo off
setlocal
cd /d "%~dp0"

set "ISCC="
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if exist "C:\Program Files\Inno Setup 6\ISCC.exe" set "ISCC=C:\Program Files\Inno Setup 6\ISCC.exe"
if exist "C:\Program Files\Inno Setup 7\ISCC.exe" set "ISCC=C:\Program Files\Inno Setup 7\ISCC.exe"
if exist "C:\Program Files (x86)\Inno Setup 7\ISCC.exe" set "ISCC=C:\Program Files (x86)\Inno Setup 7\ISCC.exe"

if "%ISCC%"=="" (
  echo [ERROR] Inno Setup 6+ not found. Install from https://jrsoftware.org/isinfo.php
  exit /b 1
)

if not exist "dist\VapManager\VapManager.exe" (
  echo [ERROR] dist\VapManager\VapManager.exe not found.
  echo Run pack_vapmanager.bat after Release build first.
  exit /b 1
)

echo Building installer...
"%ISCC%" "installer\VapManagerSetup.iss"
if errorlevel 1 exit /b 1

echo Done: dist\setup.exe
exit /b 0
