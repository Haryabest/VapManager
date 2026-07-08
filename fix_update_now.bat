@echo off
setlocal
cd /d "%~dp0"

set "SRC=%~dp0updates\files\VapManager.exe"
set "DST=%LOCALAPPDATA%\Programs\VapManager\VapManager.exe"

if not exist "%SRC%" (
  echo [ERROR] Not found: %SRC%
  echo Build first: cd build ^&^& mingw32-make -j4
  exit /b 1
)

echo Closing VapManager...
taskkill /IM VapManager.exe /F >nul 2>&1
timeout /t 2 /nobreak >nul

echo Copying update...
copy /Y "%SRC%" "%DST%.new" >nul
if errorlevel 1 (
  echo [ERROR] copy failed
  exit /b 1
)
move /Y "%DST%.new" "%DST%" >nul
if errorlevel 1 (
  echo [ERROR] move failed - close VapManager and retry
  exit /b 1
)

for %%A in ("%SRC%") do set FROM_SIZE=%%~zA
for %%A in ("%DST%") do set TO_SIZE=%%~zA
if not "%FROM_SIZE%"=="%TO_SIZE%" (
  echo [ERROR] size mismatch %FROM_SIZE% vs %TO_SIZE%
  exit /b 1
)

echo [OK] Updated: %DST% (%TO_SIZE% bytes)
echo Start VapManager and check build in Settings.
exit /b 0
