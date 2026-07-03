@echo off
setlocal
cd /d "%~dp0"

set "PORT=8765"
set "LOG=%~dp0update_server.log"
set "PYTHON="

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

echo [%date% %time%] Starting update server on port %PORT% (python=%PYTHON%) >> "%LOG%"
"%PYTHON%" -m http.server %PORT% --bind 0.0.0.0 >> "%LOG%" 2>&1
