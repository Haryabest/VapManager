@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

echo.
echo ============================================================
echo   VapManager - ispravlenie servera obnovleniy (port 8765)
echo ============================================================
echo.

net session >nul 2>&1
if errorlevel 1 (
    echo Zapros prav administratora...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b 0
)

set "PYTHON=%LOCALAPPDATA%\Python\bin\python.exe"
if not exist "%PYTHON%" (
    echo [ERROR] Ne nayden Python: %PYTHON%
    pause
    exit /b 1
)

echo [1/3] Otkrytie brandmayera TCP 8765...
powershell -NoProfile -Command ^
  "$r = Get-NetFirewallRule -DisplayName 'VapManager Updates (8765)' -ErrorAction SilentlyContinue;" ^
  "if (-not $r) { New-NetFirewallRule -DisplayName 'VapManager Updates (8765)' -Direction Inbound -Protocol TCP -LocalPort 8765 -Action Allow | Out-Null; Write-Host '[OK] Pravilo sozdano' } else { Write-Host '[OK] Pravilo uzhe est' }"

echo [2/3] Perezapis zadachi avtozapuska...
set "UPD=%~dp0updates"
for /f "tokens=5" %%P in ('netstat -ano ^| findstr ":8765" ^| findstr LISTENING') do taskkill /F /PID %%P >nul 2>&1
schtasks /Delete /TN "VapManager Update Server" /F >nul 2>&1
schtasks /Create /TN "VapManager Update Server" /TR "cmd /c cd /d \"%UPD%\" && \"%PYTHON%\" -m http.server 8765 --bind 0.0.0.0" /SC ONSTART /RL HIGHEST /RU SYSTEM /F
if errorlevel 1 (
    echo [WARN] Ne udalos sozdat zadachu, zapustite vruchnuyu: updates\start_update_server.bat
) else (
    echo [OK] Zadacha sozdana
    schtasks /Run /TN "VapManager Update Server" >nul 2>&1
)

echo [3/3] Proverka...
timeout /t 2 /nobreak >nul
powershell -NoProfile -Command ^
  "foreach ($url in @('http://127.0.0.1:8765/version.json','http://192.168.0.1:8765/version.json')) {" ^
  "  try { $r = Invoke-WebRequest $url -UseBasicParsing -TimeoutSec 5; Write-Host ('[OK] ' + $url + ' kod ' + $r.StatusCode) -ForegroundColor Green }" ^
  "  catch { Write-Host ('[WARN] ' + $url + ' - ' + $_.Exception.Message) -ForegroundColor Yellow } }"

echo.
echo Na KLIENTE v config.ini dobavte (esli net):
echo   update_check_url=http://192.168.0.1:8765/version.json
echo.
echo Proverka s drugogo PK v brauzere:
echo   http://192.168.0.1:8765/version.json
echo.
pause
