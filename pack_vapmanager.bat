@echo off
setlocal ENABLEDELAYEDEXPANSION

echo === Pack portable VapManager (clean runtime) ===
cd /d "%~dp0"

set "OUT_DIR=%~dp0dist\VapManager"
set "SRC_EXE="
set "RUNTIME_SRC="

if exist "%~1" set "SRC_EXE=%~1"
if exist "%~2" set "SRC_EXE=%~2"
if "%SRC_EXE%"=="" if exist "%~dp0build_release\release\VapManager.exe" set "SRC_EXE=%~dp0build_release\release\VapManager.exe"
if "%SRC_EXE%"=="" if exist "C:\Users\Dima\Desktop\agv admin backup\build-AgvNewUi-Desktop_Qt_5_14_2_MinGW_64_bit-Release\release\VapManager.exe" set "SRC_EXE=C:\Users\Dima\Desktop\agv admin backup\build-AgvNewUi-Desktop_Qt_5_14_2_MinGW_64_bit-Release\release\VapManager.exe"
if "%SRC_EXE%"=="" if exist "C:\Users\Dima\Desktop\agv admin backup\build-AgvNewUi-Desktop_Qt_5_14_2_MinGW_64_bit-Release\release\AgvNewUi.exe" set "SRC_EXE=C:\Users\Dima\Desktop\agv admin backup\build-AgvNewUi-Desktop_Qt_5_14_2_MinGW_64_bit-Release\release\AgvNewUi.exe"
if "%SRC_EXE%"=="" if exist "%~dp0release\AgvNewUi.exe" set "SRC_EXE=%~dp0release\AgvNewUi.exe"

if "%SRC_EXE%"=="" (
  echo [ERROR] Release VapManager.exe not found. Build Release x64, then:
  echo   pack_vapmanager.bat "path\to\VapManager.exe"
  exit /b 1
)

REM Qt runtime: from folder where windeployqt already ran (debug OK — Qt DLLs without 'd')
if exist "C:\Users\Dima\Desktop\agv admin backup\build-AgvNewUi-Desktop_Qt_5_14_2_MinGW_64_bit-Debug\debug\Qt5Core.dll" set "RUNTIME_SRC=C:\Users\Dima\Desktop\agv admin backup\build-AgvNewUi-Desktop_Qt_5_14_2_MinGW_64_bit-Debug\debug"
if "%RUNTIME_SRC%"=="" if exist "%~dp0build_release\release\Qt5Core.dll" set "RUNTIME_SRC=%~dp0build_release\release"
if "%RUNTIME_SRC%"=="" if exist "%~dp0release\VapManager.exe" set "RUNTIME_SRC=%~dp0release"

set "WINDEPLOYQT=C:\Qt\Qt5.14.2\5.14.2\mingw73_64\bin\windeployqt.exe"
set "QT_BIN=C:\Qt\Qt5.14.2\5.14.2\mingw73_64\bin"
set "MINGW_BIN=C:\Qt\Qt5.14.2\Tools\mingw730_64\bin"

echo Release EXE: %SRC_EXE%
echo Output:      %OUT_DIR%
echo.

if exist "%OUT_DIR%" rmdir /s /q "%OUT_DIR%"
mkdir "%OUT_DIR%"

copy /Y "%SRC_EXE%" "%OUT_DIR%\VapManager.exe" >nul

if not "%RUNTIME_SRC%"=="" (
  echo Copying Qt runtime from: %RUNTIME_SRC%
  for %%D in (platforms sqldrivers imageformats styles iconengines printsupport) do (
    if exist "%RUNTIME_SRC%\%%D" xcopy /E /I /Y "%RUNTIME_SRC%\%%D" "%OUT_DIR%\%%D\" >nul
  )
  for %%F in (Qt5Core.dll Qt5Gui.dll Qt5Network.dll Qt5Concurrent.dll Qt5PrintSupport.dll Qt5Sql.dll Qt5Svg.dll Qt5Widgets.dll libGLESv2.dll libEGL.dll D3Dcompiler_47.dll opengl32sw.dll) do (
    if exist "%RUNTIME_SRC%\%%F" copy /Y "%RUNTIME_SRC%\%%F" "%OUT_DIR%\" >nul
  )
) else if exist "%WINDEPLOYQT%" (
  echo Running windeployqt...
  pushd "C:\Qt\Qt5.14.2\5.14.2\mingw73_64\bin"
  "%WINDEPLOYQT%" --release --compiler-runtime --no-translations "%OUT_DIR%\VapManager.exe"
  popd
) else (
  echo [ERROR] No Qt runtime source found.
  exit /b 1
)

for %%F in (Qt5Network.dll Qt5Concurrent.dll) do (
  if not exist "%OUT_DIR%\%%F" if exist "%QT_BIN%\%%F" copy /Y "%QT_BIN%\%%F" "%OUT_DIR%\" >nul
)

if not exist "%OUT_DIR%\Qt5Network.dll" (
  echo [ERROR] Qt5Network.dll missing. Required for updates/network features.
  exit /b 1
)

REM Release build needs SEH MinGW runtime, not sjlj from debug folder
if exist "%MINGW_BIN%\libgcc_s_seh-1.dll" copy /Y "%MINGW_BIN%\libgcc_s_seh-1.dll" "%OUT_DIR%\" >nul
if exist "%MINGW_BIN%\libstdc++-6.dll" copy /Y "%MINGW_BIN%\libstdc++-6.dll" "%OUT_DIR%\" >nul
if exist "%MINGW_BIN%\libwinpthread-1.dll" copy /Y "%MINGW_BIN%\libwinpthread-1.dll" "%OUT_DIR%\" >nul
if exist "%OUT_DIR%\libgcc_s_sjlj-1.dll" del /q "%OUT_DIR%\libgcc_s_sjlj-1.dll"

if not exist "%OUT_DIR%\platforms\qwindows.dll" (
  echo [ERROR] Qt platforms missing. Run once:
  echo   deploy_runtime.bat "path\to\debug\VapManager.exe"
  echo Then pack_vapmanager.bat again.
  exit /b 1
)

if exist "%OUT_DIR%\sqldrivers\qsqlite.dll" del /q "%OUT_DIR%\sqldrivers\qsqlite.dll"
if exist "%OUT_DIR%\sqldrivers\qsqlodbc.dll" del /q "%OUT_DIR%\sqldrivers\qsqlodbc.dll"
if exist "%OUT_DIR%\sqldrivers\qsqlmysql.dll" del /q "%OUT_DIR%\sqldrivers\qsqlmysql.dll"
if exist "%OUT_DIR%\libmysql.dll" del /q "%OUT_DIR%\libmysql.dll"

if not exist "%OUT_DIR%\sqldrivers\qsqlpsql.dll" if exist "C:\Qt\Qt5.14.2\5.14.2\mingw73_64\plugins\sqldrivers\qsqlpsql.dll" (
  copy /Y "C:\Qt\Qt5.14.2\5.14.2\mingw73_64\plugins\sqldrivers\qsqlpsql.dll" "%OUT_DIR%\sqldrivers\" >nul
)

set "PG_BIN="
if exist "C:\Program Files\PostgreSQL\18\bin\libpq.dll" set "PG_BIN=C:\Program Files\PostgreSQL\18\bin"
if "%PG_BIN%"=="" if exist "C:\Program Files\PostgreSQL\16\bin\libpq.dll" set "PG_BIN=C:\Program Files\PostgreSQL\16\bin"
if "%PG_BIN%"=="" if exist "C:\Program Files\PostgreSQL\15\bin\libpq.dll" set "PG_BIN=C:\Program Files\PostgreSQL\15\bin"
if not "%PG_BIN%"=="" (
  copy /Y "%PG_BIN%\libpq.dll" "%OUT_DIR%\" >nul
  copy /Y "%PG_BIN%\libssl-3-x64.dll" "%OUT_DIR%\" >nul 2>nul
  copy /Y "%PG_BIN%\libcrypto-3-x64.dll" "%OUT_DIR%\" >nul 2>nul
  copy /Y "%PG_BIN%\libintl-9.dll" "%OUT_DIR%\" >nul 2>nul
  copy /Y "%PG_BIN%\libintl-8.dll" "%OUT_DIR%\" >nul 2>nul
  copy /Y "%PG_BIN%\libiconv-2.dll" "%OUT_DIR%\" >nul 2>nul
)

copy /Y "%~dp0install_agv_manager_db.sql" "%OUT_DIR%\install_agv_manager_db.sql" >nul

copy /Y "%~dp0config.ini" "%OUT_DIR%\config.ini" >nul 2>nul
if not exist "%OUT_DIR%\config.ini" (
  (
  echo db_driver=psql
  echo db_host=192.168.0.1
  echo db_port=5432
  echo db_name=agv_manager_db
  echo db_user=vapmanager
  echo db_password=51525354
  echo language=ru
  ) > "%OUT_DIR%\config.ini"
)

(
echo [Paths]
echo Plugins=.
) > "%OUT_DIR%\qt.conf"

if exist "%OUT_DIR%\translations" rmdir /s /q "%OUT_DIR%\translations"
mkdir "%OUT_DIR%\config" 2>nul
mkdir "%OUT_DIR%\logs" 2>nul

echo.
echo [OK] Portable VapManager:
echo   %OUT_DIR%\VapManager.exe
echo.
dir /b "%OUT_DIR%\VapManager.exe" "%OUT_DIR%\Qt5Network.dll" "%OUT_DIR%\libgcc_s_seh-1.dll" "%OUT_DIR%\libpq.dll" "%OUT_DIR%\sqldrivers\qsqlpsql.dll" "%OUT_DIR%\platforms\qwindows.dll" 2>nul
exit /b 0
