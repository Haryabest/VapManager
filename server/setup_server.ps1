#Requires -RunAsAdministrator
<#
  VapManager — полная настройка сервера (один запуск).
  PostgreSQL: БД, пользователь, удалённый доступ, порт 5432.
  Обновления: брандмауэр 8765, автозапуск HTTP-сервера.
  config.ini с IP этого ПК.

  Запуск: setup_server.bat (от администратора)
#>
param(
    [string]$RepoRoot = "",
    [string]$PostgresPassword = "",
    [string]$AppDbPassword = "51525354",
    [switch]$SkipDatabase,
    [switch]$SkipUpdates
)

$ErrorActionPreference = "Stop"

if ($RepoRoot -eq "") {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Write-Step($Text) {
    Write-Host ""
    Write-Host "== $Text" -ForegroundColor Cyan
}

function Find-PsqlExe {
    foreach ($ver in @(18, 17, 16, 15, 14)) {
        $p = "C:\Program Files\PostgreSQL\$ver\bin\psql.exe"
        if (Test-Path $p) { return $p }
    }
    $cmd = Get-Command psql.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

function Find-PostgresDataDir {
    $services = Get-Service -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "postgresql*" }
    foreach ($svc in $services) {
        $pathName = (Get-CimInstance Win32_Service -Filter "Name='$($svc.Name)'").PathName
        if ($pathName -match '-D\s+"([^"]+)"') {
            $dataDir = $Matches[1]
            if (Test-Path (Join-Path $dataDir "postgresql.conf")) { return $dataDir }
        }
        if ($pathName -match '"([^"]+)\\bin\\pg_ctl\.exe"') {
            $dataDir = Join-Path $Matches[1] "data"
            if (Test-Path (Join-Path $dataDir "postgresql.conf")) { return $dataDir }
        }
    }
    foreach ($ver in @(18, 17, 16, 15, 14)) {
        $p = "C:\Program Files\PostgreSQL\$ver\data"
        if (Test-Path (Join-Path $p "postgresql.conf")) { return $p }
    }
    throw "PostgreSQL не найден. Установите PostgreSQL 15+ с https://www.postgresql.org/download/windows/"
}

function Get-LanIPv4 {
    $ip = (Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object {
        $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' -and
        $_.IPAddress -notlike '172.1[6-9].*' -and $_.IPAddress -notlike '172.2[0-9].*' -and $_.IPAddress -notlike '172.3[0-1].*'
    } | Sort-Object { if ($_.IPAddress -like '192.168.*') { 0 } elseif ($_.IPAddress -like '10.*') { 1 } else { 2 } } | Select-Object -First 1 -ExpandProperty IPAddress)
    if ($ip) { return $ip }
    return '127.0.0.1'
}

function Ensure-FirewallRule($DisplayName, [int]$Port) {
    $existing = Get-NetFirewallRule -DisplayName $DisplayName -ErrorAction SilentlyContinue
    if ($existing) {
        Write-Host "[OK] Брандмауэр: $DisplayName"
        return
    }
    New-NetFirewallRule -DisplayName $DisplayName -Direction Inbound -Protocol TCP -LocalPort $Port -Action Allow | Out-Null
    Write-Host "[OK] Брандмауэр: открыт TCP $Port ($DisplayName)"
}

function Write-ConfigIni($Path, $HostIp, $Password) {
    $dir = Split-Path $Path -Parent
    if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    @(
        "[General]"
        "db_driver=psql"
        "db_host=$HostIp"
        "db_port=5432"
        "db_name=agv_manager_db"
        "db_user=vapmanager"
        "db_password=$Password"
        "language=ru"
        ""
    ) | Set-Content -Path $Path -Encoding UTF8
    Write-Host "[OK] config.ini -> $Path"
}

function Configure-PostgresRemote($DataDir) {
    $conf = Join-Path $DataDir "postgresql.conf"
    $hba  = Join-Path $DataDir "pg_hba.conf"
    $utf8NoBom = New-Object System.Text.UTF8Encoding $false

    $lines = [System.IO.File]::ReadAllLines($conf)
    $foundListen = $false
    $out = foreach ($line in $lines) {
        if ($line -match '^\s*#?\s*listen_addresses\s*=') {
            $foundListen = $true
            "listen_addresses = '*'"
        } else { $line }
    }
    if (-not $foundListen) { $out = $out + "listen_addresses = '*'" }
    [System.IO.File]::WriteAllLines($conf, $out, $utf8NoBom)
    Write-Host "[OK] postgresql.conf: listen_addresses = '*'"

    $ruleV4 = "host    all             all             0.0.0.0/0               scram-sha-256"
    $ruleV6 = "host    all             all             ::0/0                   scram-sha-256"
    $hbaLines = [System.IO.File]::ReadAllLines($hba)
    if ($hbaLines -notcontains $ruleV4) {
        $add = @("", "# VapManager remote clients", $ruleV4, $ruleV6)
        [System.IO.File]::WriteAllLines($hba, ($hbaLines + $add), $utf8NoBom)
        Write-Host "[OK] pg_hba.conf: доступ из сети"
    } else {
        Write-Host "[OK] pg_hba.conf: уже настроен"
    }
}

function Register-UpdateServerTask($RepoRoot) {
    $updatesDir = Join-Path $RepoRoot "updates"
    if (-not (Test-Path $updatesDir)) {
        New-Item -ItemType Directory -Path $updatesDir -Force | Out-Null
    }

    $python = (Get-Command python.exe -ErrorAction SilentlyContinue).Source
    if (-not $python) {
        Write-Host "[WARN] Python не найден — автозапуск сервера обновлений пропущен."
        Write-Host "       Установите Python 3 и снова запустите setup_server.bat"
        return $false
    }

    $taskName = "VapManager Update Server"
    Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue

    $logFile = Join-Path $updatesDir "update_server.log"
    $action = New-ScheduledTaskAction -Execute $python -Argument "-m http.server 8765 --bind 0.0.0.0" -WorkingDirectory $updatesDir
    $trigger = New-ScheduledTaskTrigger -AtStartup
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1)
    Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -RunLevel Highest -User "SYSTEM" | Out-Null
    Write-Host "[OK] Задача автозапуска: $taskName (Python: $python)"
    Write-Host "     Лог: $logFile"

    Start-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    return $true
}

Write-Host ""
Write-Host "VapManager — настройка сервера" -ForegroundColor Green
Write-Host "Папка проекта: $RepoRoot"

$lanIp = Get-LanIPv4
Write-Host "IP этого ПК в сети: $lanIp"

# --- PostgreSQL ---
Write-Step "1/5 PostgreSQL: служба и сеть"
$dataDir = Find-PostgresDataDir
Write-Host "Каталог данных: $dataDir"

$pgService = Get-Service -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "postgresql*" } | Select-Object -First 1
if (-not $pgService) { throw "Служба PostgreSQL не найдена." }

Set-Service $pgService.Name -StartupType Automatic
if ($pgService.Status -ne 'Running') { Start-Service $pgService.Name }
Write-Host "[OK] Служба $($pgService.Name): автозапуск, запущена"

Configure-PostgresRemote $dataDir
Ensure-FirewallRule "PostgreSQL VapManager (5432)" 5432

Restart-Service $pgService.Name -Force
Write-Host "[OK] PostgreSQL перезапущен"

# --- База данных ---
if (-not $SkipDatabase) {
    Write-Step "2/5 PostgreSQL: создание базы и пользователя"
    $psql = Find-PsqlExe
    if (-not $psql) { throw "psql.exe не найден." }

    if ($PostgresPassword -eq "") {
        Write-Host "Введите пароль суперпользователя postgres (задавали при установке PostgreSQL):"
        $sec = Read-Host -AsSecureString
        $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($sec)
        try { $PostgresPassword = [Runtime.InteropServices.Marshal]::PtrToStringAuto($ptr) }
        finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr) }
    }

    $sqlFile = Join-Path $RepoRoot "install_agv_manager_db.sql"
    if (-not (Test-Path $sqlFile)) { throw "Не найден $sqlFile" }

    $env:PGPASSWORD = $PostgresPassword
    & $psql -U postgres -h localhost -v ON_ERROR_STOP=1 -f $sqlFile
    if ($LASTEXITCODE -ne 0) { throw "Ошибка выполнения install_agv_manager_db.sql" }

    $alterSql = "ALTER ROLE vapmanager WITH LOGIN PASSWORD '$AppDbPassword';"
    & $psql -U postgres -h localhost -d agv_manager_db -v ON_ERROR_STOP=1 -c $alterSql
    if ($LASTEXITCODE -ne 0) { throw "Не удалось задать пароль vapmanager" }
    Remove-Item Env:PGPASSWORD -ErrorAction SilentlyContinue
    Write-Host "[OK] БД agv_manager_db, пользователь vapmanager"
} else {
    Write-Step "2/5 PostgreSQL: пропуск (--SkipDatabase)"
}

# --- config.ini ---
Write-Step "3/5 config.ini"
Write-ConfigIni (Join-Path $RepoRoot "config.ini") $lanIp $AppDbPassword
$distCfg = Join-Path $RepoRoot "dist\VapManager\config.ini"
if (Test-Path (Split-Path $distCfg -Parent)) {
    Write-ConfigIni $distCfg $lanIp $AppDbPassword
}

# --- Обновления ---
if (-not $SkipUpdates) {
    Write-Step "4/5 Сервер обновлений (порт 8765)"
    Ensure-FirewallRule "VapManager Updates (8765)" 8765
    Register-UpdateServerTask $RepoRoot | Out-Null

    $distExe = Join-Path $RepoRoot "dist\VapManager\VapManager.exe"
    if (Test-Path $distExe) {
        Write-Host "Найден dist\VapManager — публикация обновлений..."
        $pub = Join-Path $RepoRoot "publish_local_updates.bat"
        if (Test-Path $pub) {
            & cmd.exe /c "`"$pub`""
        }
    } else {
        Write-Host "[INFO] dist\VapManager\VapManager.exe нет — после сборки запустите publish_local_updates.bat"
    }
} else {
    Write-Step "4/5 Сервер обновлений: пропуск (--SkipUpdates)"
}

# --- Итог ---
Write-Step "5/5 Готово"
Write-Host ""
Write-Host "Сервер настроен. Клиентам достаточно установить VapManager — config.ini уже с IP сервера." -ForegroundColor Green
Write-Host ""
Write-Host "  PostgreSQL:  $lanIp`:5432 / agv_manager_db / vapmanager"
Write-Host "  Обновления:  http://${lanIp}:8765/version.json"
Write-Host "  Пароль БД:   $AppDbPassword  (config.ini на сервере и в dist)"
Write-Host ""
Write-Host "Сборка клиентов (на этом или другом ПК):"
Write-Host "  1. Qt Release -> pack_vapmanager.bat -> build_installer.bat"
Write-Host "  2. publish_local_updates.bat  (новые версии на сервер)"
Write-Host ""
Write-Host "Проверка:"
Write-Host "  psql -U vapmanager -h localhost -d agv_manager_db -c ""SELECT 1"""
Write-Host "  http://${lanIp}:8765/version.json"
Write-Host ""
