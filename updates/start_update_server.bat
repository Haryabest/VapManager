@echo off
setlocal
cd /d "%~dp0"

set "PORT=8765"
set "CONFIG=..\config.ini"
set "IP=192.168.0.1"
set "PYTHON="

for /f "usebackq tokens=1,* delims==" %%A in (`findstr /i /b "db_host=" "%CONFIG%" 2^>nul`) do (
  if /i not "%%B"=="127.0.0.1" set "IP=%%B"
)

if exist "%LOCALAPPDATA%\Python\bin\python.exe" set "PYTHON=%LOCALAPPDATA%\Python\bin\python.exe"
if not defined PYTHON if exist "%ProgramFiles%\Python312\python.exe" set "PYTHON=%ProgramFiles%\Python312\python.exe"
if not defined PYTHON if exist "%ProgramFiles%\Python311\python.exe" set "PYTHON=%ProgramFiles%\Python311\python.exe"
if not defined PYTHON if exist "%ProgramFiles%\Python314\python.exe" set "PYTHON=%ProgramFiles%\Python314\python.exe"
if not defined PYTHON (
  for /f "delims=" %%P in ('where python 2^>nul') do (
    echo %%P | findstr /i "WindowsApps" >nul
    if errorlevel 1 set "PYTHON=%%P"
  )
)
if not defined PYTHON set "PYTHON=python"

echo.
echo VapManager update server (port %PORT%)
echo   Python: %PYTHON%
echo   Folder: %CD%
echo   URL:    http://%IP%:%PORT%/version.json
echo   Files:  http://%IP%:%PORT%/files/
echo.
echo Press Ctrl+C to stop.
echo.

"%PYTHON%" -m http.server %PORT% --bind 0.0.0.0
