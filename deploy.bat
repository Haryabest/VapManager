@echo off
setlocal ENABLEDELAYEDEXPANSION

echo === AGV New UI deploy helper ===
echo.

set "EXE_PATH=%~1"
if "%EXE_PATH%"=="" (
  if exist "release\AgvNewUi.exe" (
    set "EXE_PATH=release\AgvNewUi.exe"
  ) else if exist "debug\AgvNewUi.exe" (
    set "EXE_PATH=debug\AgvNewUi.exe"
  ) else if exist "AgvNewUi.exe" (
    set "EXE_PATH=AgvNewUi.exe"
  )
)

if "%EXE_PATH%"=="" (
  echo [ERROR] EXE not found.
  echo Usage: deploy.bat "path\to\AgvNewUi.exe"
  exit /b 1
)

if not exist "%EXE_PATH%" (
  echo [ERROR] EXE does not exist: %EXE_PATH%
  exit /b 1
)

for %%I in ("%EXE_PATH%") do (
  set "EXE_DIR=%%~dpI"
  set "EXE_NAME=%%~nxI"
)

echo Target EXE: %EXE_PATH%
echo Target dir: %EXE_DIR%
echo.

where windeployqt >nul 2>nul
if errorlevel 1 (
  echo [WARN] windeployqt not found in PATH. Skip automatic Qt deploy.
  echo Add Qt bin to PATH, example:
  echo   C:\Qt\5.15.2\mingw81_64\bin
  echo.
) else (
  echo Running windeployqt...
  windeployqt --release "%EXE_PATH%"
  echo.
)

set "QSQLMYSQL=%EXE_DIR%sqldrivers\qsqlmysql.dll"
set "LIBMYSQL=%EXE_DIR%libmysql.dll"

echo Checking required MySQL runtime files:
if exist "%QSQLMYSQL%" (
  echo [OK] %QSQLMYSQL%
) else (
  echo [MISS] %QSQLMYSQL%
)

if exist "%LIBMYSQL%" (
  echo [OK] %LIBMYSQL%
) else (
  echo [MISS] %LIBMYSQL%
)

echo.
echo Default locations where these files usually exist:
echo.
echo qsqlmysql.dll:
echo   C:\Qt\5.15.2\^<kit^>\plugins\sqldrivers\qsqlmysql.dll
echo   C:\Qt\6.x.x\^<kit^>\plugins\sqldrivers\qsqlmysql.dll
echo.
echo libmysql.dll:
echo   C:\Program Files\MySQL\MySQL Server 8.0\lib\libmysql.dll
echo   C:\Program Files\MySQL\MySQL Server 8.4\lib\libmysql.dll
echo.
echo Important:
echo - EXE, qsqlmysql.dll and libmysql.dll must be the same architecture ^(x64/x86^).
echo - qsqlmysql.dll must be in: ^<exe_dir^>\sqldrivers\
echo - libmysql.dll must be next to EXE.
echo.
echo Done.
exit /b 0

