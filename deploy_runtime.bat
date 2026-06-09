@echo off
setlocal ENABLEDELAYEDEXPANSION

echo === VapManager PostgreSQL: Qt runtime deploy ===
echo.

set "EXE_PATH=%~1"
if "%EXE_PATH%"=="" (
  if exist "%~dp0debug\VapManager.exe" set "EXE_PATH=%~dp0debug\VapManager.exe"
  if exist "%~dp0release\VapManager.exe" set "EXE_PATH=%~dp0release\VapManager.exe"
)

if "%EXE_PATH%"=="" (
  echo Usage: deploy_runtime.bat "path\to\VapManager.exe"
  exit /b 1
)

if not exist "%EXE_PATH%" (
  echo [ERROR] EXE not found: %EXE_PATH%
  exit /b 1
)

for %%I in ("%EXE_PATH%") do set "EXE_DIR=%%~dpI"

set "WINDEPLOYQT="
set "QSQLPLUGIN="
set "PG_BIN="

if exist "C:\Qt\Qt5.14.2\5.14.2\mingw73_64\bin\windeployqt.exe" (
  set "WINDEPLOYQT=C:\Qt\Qt5.14.2\5.14.2\mingw73_64\bin\windeployqt.exe"
  set "QSQLPLUGIN=C:\Qt\Qt5.14.2\5.14.2\mingw73_64\plugins\sqldrivers\qsqlpsql.dll"
)
if "%WINDEPLOYQT%"=="" (
  where windeployqt >nul 2>nul
  if not errorlevel 1 for /f "delims=" %%W in ('where windeployqt') do set "WINDEPLOYQT=%%W"
)

if "%WINDEPLOYQT%"=="" (
  echo [ERROR] windeployqt not found
  exit /b 1
)

echo %EXE_PATH% | findstr /I "\debug\" >nul
if errorlevel 1 (
  "%WINDEPLOYQT%" --compiler-runtime "%EXE_PATH%"
) else (
  "%WINDEPLOYQT%" --debug --compiler-runtime "%EXE_PATH%"
)

if not exist "%EXE_DIR%sqldrivers" mkdir "%EXE_DIR%sqldrivers"

if exist "%QSQLPLUGIN%" (
  copy /Y "%QSQLPLUGIN%" "%EXE_DIR%sqldrivers\qsqlpsql.dll" >nul
  echo [OK] qsqlpsql.dll
) else (
  echo [MISS] qsqlpsql.dll
)

if exist "C:\Program Files\PostgreSQL\18\bin" set "PG_BIN=C:\Program Files\PostgreSQL\18\bin"
if "%PG_BIN%"=="" if exist "C:\Program Files\PostgreSQL\16\bin" set "PG_BIN=C:\Program Files\PostgreSQL\16\bin"
if "%PG_BIN%"=="" if exist "C:\Program Files\PostgreSQL\15\bin" set "PG_BIN=C:\Program Files\PostgreSQL\15\bin"

if not "%PG_BIN%"=="" (
  copy /Y "%PG_BIN%\libpq.dll" "%EXE_DIR%\" >nul 2>nul
  copy /Y "%PG_BIN%\libssl-3-x64.dll" "%EXE_DIR%\" >nul 2>nul
  copy /Y "%PG_BIN%\libcrypto-3-x64.dll" "%EXE_DIR%\" >nul 2>nul
  echo [OK] libpq.dll from PostgreSQL
) else (
  echo [WARN] PostgreSQL bin not found — copy libpq.dll manually next to exe
)

if exist "%EXE_DIR%sqldrivers\qsqlmysql.dll" del "%EXE_DIR%sqldrivers\qsqlmysql.dll" >nul 2>nul
if exist "%EXE_DIR%libmysql.dll" del "%EXE_DIR%libmysql.dll" >nul 2>nul

if not exist "%EXE_DIR%config.ini" (
  (
    echo db_host=localhost
    echo db_port=5432
    echo db_name=agv_manager_db
    echo db_user=vapmanager
    echo db_password=vapmanager_change_me
    echo language=ru
  ) > "%EXE_DIR%config.ini"
  echo [OK] config.ini created
)

echo.
echo Deploy complete: %EXE_DIR%
exit /b 0
