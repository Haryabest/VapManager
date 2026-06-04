@echo off
setlocal ENABLEDELAYEDEXPANSION

echo === Pack portable VapManager (clean runtime) ===
cd /d "%~dp0"

set "OUT_DIR=%~dp0dist\VapManager"
set "SRC_EXE="
set "RUNTIME_SRC="

if exist "%~2" set "SRC_EXE=%~2"
if "%SRC_EXE%"=="" if exist "C:\Users\Dima\Desktop\agv admin backup\build-AgvNewUi-Desktop_Qt_5_14_2_MinGW_64_bit-Release\release\AgvNewUi.exe" set "SRC_EXE=C:\Users\Dima\Desktop\agv admin backup\build-AgvNewUi-Desktop_Qt_5_14_2_MinGW_64_bit-Release\release\AgvNewUi.exe"
if "%SRC_EXE%"=="" if exist "%~dp0release\AgvNewUi.exe" set "SRC_EXE=%~dp0release\AgvNewUi.exe"

if "%SRC_EXE%"=="" (
  echo [ERROR] Release AgvNewUi.exe not found. Build Release x64, then:
  echo   pack_vapmanager.bat "path\to\AgvNewUi.exe"
  exit /b 1
)

REM Qt runtime: from folder where windeployqt already ran (debug OK — Qt DLLs without 'd')
if exist "C:\Users\Dima\Desktop\agv admin backup\build-AgvNewUi-Desktop_Qt_5_14_2_MinGW_64_bit-Debug\debug\Qt5Core.dll" set "RUNTIME_SRC=C:\Users\Dima\Desktop\agv admin backup\build-AgvNewUi-Desktop_Qt_5_14_2_MinGW_64_bit-Debug\debug"
if "%RUNTIME_SRC%"=="" if exist "%~dp0deploy_staging\Qt5Core.dll" set "RUNTIME_SRC=%~dp0deploy_staging"

set "WINDEPLOYQT=C:\Qt\Qt5.14.2\5.14.2\mingw73_64\bin\windeployqt.exe"

echo Release EXE: %SRC_EXE%
echo Output:      %OUT_DIR%
echo.

if exist "%OUT_DIR%" rmdir /s /q "%OUT_DIR%"
mkdir "%OUT_DIR%"

if not "%RUNTIME_SRC%"=="" (
  echo Copying Qt runtime from: %RUNTIME_SRC%
  for %%D in (platforms sqldrivers imageformats styles iconengines printsupport) do (
    if exist "%RUNTIME_SRC%\%%D" xcopy /E /I /Y "%RUNTIME_SRC%\%%D" "%OUT_DIR%\%%D\" >nul
  )
  for %%F in (Qt5Core.dll Qt5Gui.dll Qt5PrintSupport.dll Qt5Sql.dll Qt5Svg.dll Qt5Widgets.dll libGLESv2.dll libEGL.dll D3Dcompiler_47.dll opengl32sw.dll libgcc_s_sjlj-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if exist "%RUNTIME_SRC%\%%F" copy /Y "%RUNTIME_SRC%\%%F" "%OUT_DIR%\" >nul
  )
) else if exist "%WINDEPLOYQT%" (
  copy /Y "%SRC_EXE%" "%OUT_DIR%\VapManager.exe" >nul
  echo Running windeployqt...
  "%WINDEPLOYQT%" --release --compiler-runtime "%OUT_DIR%\VapManager.exe"
)

copy /Y "%SRC_EXE%" "%OUT_DIR%\VapManager.exe" >nul

if not exist "%OUT_DIR%\platforms\qwindows.dll" (
  echo [ERROR] Qt platforms missing. Run once:
  echo   deploy_runtime.bat "path\to\debug\AgvNewUi.exe"
  echo Then pack_vapmanager.bat again.
  exit /b 1
)

if exist "%OUT_DIR%\sqldrivers\qsqlite.dll" del /q "%OUT_DIR%\sqldrivers\qsqlite.dll"
if exist "%OUT_DIR%\sqldrivers\qsqlodbc.dll" del /q "%OUT_DIR%\sqldrivers\qsqlodbc.dll"
if exist "%OUT_DIR%\sqldrivers\qsqlpsql.dll" del /q "%OUT_DIR%\sqldrivers\qsqlpsql.dll"

if not exist "%OUT_DIR%\sqldrivers\qsqlmysql.dll" if exist "C:\Qt\Qt5.14.2\5.14.2\mingw73_64\plugins\sqldrivers\qsqlmysql.dll" (
  copy /Y "C:\Qt\Qt5.14.2\5.14.2\mingw73_64\plugins\sqldrivers\qsqlmysql.dll" "%OUT_DIR%\sqldrivers\" >nul
)

set "LIBMYSQL_SRC="
if exist "C:\Program Files\MySQL\MySQL Server 9.6\lib\libmysql.dll" set "LIBMYSQL_SRC=C:\Program Files\MySQL\MySQL Server 9.6\lib\libmysql.dll"
if "%LIBMYSQL_SRC%"=="" if exist "C:\Program Files\MySQL\MySQL Server 8.4\lib\libmysql.dll" set "LIBMYSQL_SRC=C:\Program Files\MySQL\MySQL Server 8.4\lib\libmysql.dll"
if "%LIBMYSQL_SRC%"=="" if exist "C:\Program Files\MySQL\MySQL Server 8.0\lib\libmysql.dll" set "LIBMYSQL_SRC=C:\Program Files\MySQL\MySQL Server 8.0\lib\libmysql.dll"
if not "%LIBMYSQL_SRC%"=="" copy /Y "%LIBMYSQL_SRC%" "%OUT_DIR%\libmysql.dll" >nul

copy /Y "%~dp0install_agv_manager_db.sql" "%OUT_DIR%\install_agv_manager_db.sql" >nul

(
echo [General]
echo db_host=localhost
) > "%OUT_DIR%\config.ini"

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
dir /b "%OUT_DIR%\VapManager.exe" "%OUT_DIR%\libmysql.dll" "%OUT_DIR%\sqldrivers\qsqlmysql.dll" "%OUT_DIR%\platforms\qwindows.dll" 2>nul
exit /b 0
