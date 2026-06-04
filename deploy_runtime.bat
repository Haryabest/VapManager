@echo off
setlocal ENABLEDELAYEDEXPANSION

echo === VapManager: Qt + MySQL runtime deploy ===
echo.

set "EXE_PATH=%~1"
if "%EXE_PATH%"=="" (
  if exist "%~dp0debug\VapManager.exe" set "EXE_PATH=%~dp0debug\VapManager.exe"
  if exist "%~dp0release\VapManager.exe" set "EXE_PATH=%~dp0release\VapManager.exe"
  if exist "%~dp0debug\AgvNewUi.exe" set "EXE_PATH=%~dp0debug\AgvNewUi.exe"
  if exist "%~dp0release\AgvNewUi.exe" set "EXE_PATH=%~dp0release\AgvNewUi.exe"
)

if "%EXE_PATH%"=="" (
  echo Usage: deploy_runtime.bat "path\to\VapManager.exe"
  echo Example:
  echo   deploy_runtime.bat "path\to\debug\VapManager.exe"
  exit /b 1
)

if not exist "%EXE_PATH%" (
  echo [ERROR] EXE not found: %EXE_PATH%
  exit /b 1
)

for %%I in ("%EXE_PATH%") do set "EXE_DIR=%%~dpI"

echo Target: %EXE_PATH%
echo.

set "WINDEPLOYQT="
set "QSQLPLUGIN="
set "QT_KIT="

if exist "C:\Qt\Qt5.14.2\5.14.2\mingw73_64\bin\windeployqt.exe" (
  set "WINDEPLOYQT=C:\Qt\Qt5.14.2\5.14.2\mingw73_64\bin\windeployqt.exe"
  set "QSQLPLUGIN=C:\Qt\Qt5.14.2\5.14.2\mingw73_64\plugins\sqldrivers\qsqlmysql.dll"
  set "QT_KIT=mingw73_64"
)
if "%WINDEPLOYQT%"=="" if exist "C:\Qt\Qt5.14.2\5.14.2\mingw73_32\bin\windeployqt.exe" (
  set "WINDEPLOYQT=C:\Qt\Qt5.14.2\5.14.2\mingw73_32\bin\windeployqt.exe"
  set "QSQLPLUGIN=C:\Qt\Qt5.14.2\5.14.2\mingw73_32\plugins\sqldrivers\qsqlmysql.dll"
  set "QT_KIT=mingw73_32"
)

if "%WINDEPLOYQT%"=="" (
  where windeployqt >nul 2>nul
  if not errorlevel 1 (
    for /f "delims=" %%W in ('where windeployqt') do set "WINDEPLOYQT=%%W"
  )
)

if "%WINDEPLOYQT%"=="" (
  echo [ERROR] windeployqt not found. Add Qt bin to PATH, e.g.:
  echo   C:\Qt\Qt5.14.2\5.14.2\mingw73_64\bin
  exit /b 1
)

echo Using windeployqt: %WINDEPLOYQT%
echo.

echo %EXE_PATH% | findstr /I "\debug\" >nul
if errorlevel 1 (
  "%WINDEPLOYQT%" --compiler-runtime "%EXE_PATH%"
) else (
  "%WINDEPLOYQT%" --debug --compiler-runtime "%EXE_PATH%"
)

if not exist "%EXE_DIR%sqldrivers" mkdir "%EXE_DIR%sqldrivers"

if exist "%QSQLPLUGIN%" (
  copy /Y "%QSQLPLUGIN%" "%EXE_DIR%sqldrivers\qsqlmysql.dll" >nul
  echo [OK] qsqlmysql.dll ^(%QT_KIT%^)
) else if not exist "%EXE_DIR%sqldrivers\qsqlmysql.dll" (
  echo [MISS] qsqlmysql.dll — copy manually to %EXE_DIR%sqldrivers\
)

set "LIBMYSQL_SRC="
if exist "C:\Program Files\MySQL\MySQL Server 9.6\lib\libmysql.dll" set "LIBMYSQL_SRC=C:\Program Files\MySQL\MySQL Server 9.6\lib\libmysql.dll"
if "%LIBMYSQL_SRC%"=="" if exist "C:\Program Files\MySQL\MySQL Server 8.4\lib\libmysql.dll" set "LIBMYSQL_SRC=C:\Program Files\MySQL\MySQL Server 8.4\lib\libmysql.dll"
if "%LIBMYSQL_SRC%"=="" if exist "C:\Program Files\MySQL\MySQL Server 8.0\lib\libmysql.dll" set "LIBMYSQL_SRC=C:\Program Files\MySQL\MySQL Server 8.0\lib\libmysql.dll"
if "%LIBMYSQL_SRC%"=="" if exist "%~dp0dist\VapManager\libmysql.dll" set "LIBMYSQL_SRC=%~dp0dist\VapManager\libmysql.dll"

if not "%LIBMYSQL_SRC%"=="" (
  copy /Y "%LIBMYSQL_SRC%" "%EXE_DIR%libmysql.dll" >nul
  echo [OK] libmysql.dll
) else (
  echo [MISS] libmysql.dll — install MySQL Server or copy next to EXE
)

if not exist "%EXE_DIR%config.ini" (
  echo db_host=localhost> "%EXE_DIR%config.ini"
  echo [OK] config.ini created
)

echo.
if exist "%EXE_DIR%sqldrivers\qsqlmysql.dll" if exist "%EXE_DIR%libmysql.dll" (
  echo Deploy complete. Run VapManager.exe from:
  echo   %EXE_DIR%
) else (
  echo Deploy finished with missing files — see messages above.
  exit /b 2
)
exit /b 0
