@echo off
setlocal ENABLEDELAYEDEXPANSION
cd /d "%~dp0"

set "OUT_DIR=%~dp0dist\VapManager"
set "UPD_DIR=%~dp0updates"
set "FILES_DIR=%UPD_DIR%\files"
set "APP_VERSION=1.0.0"
set "APP_BUILD=141"

echo === Publish local updates (this PC as server) ===

call "%~dp0pack_vapmanager.bat"
if errorlevel 1 exit /b 1

if not exist "%UPD_DIR%" mkdir "%UPD_DIR%"
if not exist "%FILES_DIR%" mkdir "%FILES_DIR%"

echo Copying files to updates\files ...
robocopy "%OUT_DIR%" "%FILES_DIR%" /MIR /NFL /NDL /NJH /NJS /NC /NS >nul
if errorlevel 8 (
  echo [ERROR] robocopy failed
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$repoRoot = '%~dp0'.TrimEnd('\');" ^
  "$outDir = '%FILES_DIR%';" ^
  "$updDir = '%UPD_DIR%';" ^
  "$version = '%APP_VERSION%';" ^
  "$build = [int]'%APP_BUILD%';" ^
  "$ip = '';" ^
  "$cfg = Join-Path $repoRoot 'config.ini';" ^
  "if (Test-Path $cfg) {" ^
  "  $line = Select-String -Path $cfg -Pattern '^\s*db_host=(.+)$' -ErrorAction SilentlyContinue | Select-Object -First 1;" ^
  "  if ($line) { $ip = $line.Matches[0].Groups[1].Value.Trim() }" ^
  "}" ^
  "if ($ip -notmatch '^\d+\.\d+\.\d+\.\d+$') {" ^
  "  $ip = (Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object {" ^
  "    $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' -and" ^
  "    $_.IPAddress -notlike '172.1[6-9].*' -and $_.IPAddress -notlike '172.2[0-9].*' -and $_.IPAddress -notlike '172.3[0-1].*'" ^
  "  } | Sort-Object { if ($_.IPAddress -like '192.168.*') { 0 } elseif ($_.IPAddress -like '10.*') { 1 } else { 2 } } | Select-Object -First 1 -ExpandProperty IPAddress);" ^
  "}" ^
  "if (-not $ip) { $ip = '127.0.0.1' };" ^
  "$baseUrl = ('http://' + $ip + ':8765/files/');" ^
  "$checkUrl = ('http://' + $ip + ':8765/version.json');" ^
  "$files = Get-ChildItem -Path $outDir -Recurse -File | Where-Object {" ^
  "    $rel = $_.FullName.Substring($outDir.Length).TrimStart('\').Replace('\','/');" ^
  "    $rel -ne 'config.ini' -and -not $rel.StartsWith('logs/') -and -not $rel.StartsWith('config/')" ^
  "  } | ForEach-Object {" ^
  "    $rel = $_.FullName.Substring($outDir.Length).TrimStart('\').Replace('\','/');" ^
  "    [ordered]@{ path = $rel; size = $_.Length }" ^
  "  };" ^
  "$obj = [ordered]@{" ^
  "    version = $version;" ^
  "    build = $build;" ^
  "    notes = 'Local VapManager update from this PC';" ^
  "    baseUrl = $baseUrl;" ^
  "    setupUrl = '';" ^
  "    files = @($files)" ^
  "  };" ^
  "$json = $obj | ConvertTo-Json -Depth 5;" ^
  "[System.IO.File]::WriteAllText((Join-Path $updDir 'version.json'), $json, [System.Text.UTF8Encoding]::new($false));" ^
  "Write-Host ('[OK] version.json  build=' + $build);" ^
  "Write-Host ('[OK] update_check_url=' + $checkUrl);" ^
  "Write-Host ('[OK] baseUrl=' + $baseUrl);" ^
  "Write-Host ('[OK] Server IP: ' + $ip);"

echo.
echo Next: run (keep window open):
echo   %UPD_DIR%\start_update_server.bat
echo.
echo Clients: no config changes needed (updates URL is derived from db_host).
exit /b 0
