@echo off

setlocal

cd /d "%~dp0"



set "PORT=8765"

set "LOG=%~dp0update_server.log"



where python >nul 2>&1

if errorlevel 1 (

    echo [%date% %time%] Python not found >> "%LOG%"

    exit /b 1

)



echo [%date% %time%] Starting update server on port %PORT% >> "%LOG%"

python -m http.server %PORT% --bind 0.0.0.0 >> "%LOG%" 2>&1

