#Requires -RunAsAdministrator
<#
  VapManager - polnaya nastroyka servera (odin zapusk).
  PostgreSQL: BD, polzovatel, udalennyy dostup, port 5432.
  Obnovleniya: brandmauer 8765, avtozapusk HTTP-servera.
  config.ini s IP etogo PK.

  Zapusk: setup_server.bat (ot administratora)
#>
param(
    [string]$RepoRoot = "",
    [string]$ServerIp = "",
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
    throw "PostgreSQL ne nayden. Ustanovite PostgreSQL 15+ s https://www.postgresql.org/download/windows/"
}

function Get-LanIPv4 {
    $ips = @(Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object {
        $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' -and
        $_.IPAddress -notlike '172.1[6-9].*' -and $_.IPAddress -notlike '172.2[0-9].*' -and
        $_.IPAddress -notlike '172.3[0-1].*'
    })

    $preferred = $ips | Where-Object { $_.IPAddress -like '192.168.0.*' } |
        Sort-Object IPAddress | Select-Object -First 1 -ExpandProperty IPAddress
    if ($preferred) { return $preferred }

    $lan = $ips | Sort-Object @{
        Expression = {
            if ($_.IPAddress -like '192.168.*') { 0 }
            elseif ($_.IPAddress -like '10.*') { 1 }
            else { 2 }
        }
    }, IPAddress | Select-Object -First 1 -ExpandProperty IPAddress
    if ($lan) { return $lan }
    return '127.0.0.1'
}

function Ensure-FirewallRule($DisplayName, [int]$Port) {
    $existing = Get-NetFirewallRule -DisplayName $DisplayName -ErrorAction SilentlyContinue
    if ($existing) {
        Write-Host "[OK] Brandmauer: $DisplayName"
        return
    }
    New-NetFirewallRule -DisplayName $DisplayName -Direction Inbound -Protocol TCP -LocalPort $Port -Action Allow | Out-Null
    Write-Host "[OK] Brandmauer: otkryt TCP $Port ($DisplayName)"
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
    Write-Host "[OK] config.ini -> $Path (db_host=$HostIp)"
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
        Write-Host "[OK] pg_hba.conf: dostup iz seti"
    } else {
        Write-Host "[OK] pg_hba.conf: uzhe nastroen"
    }
}

function Find-PythonExe {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Python\bin\python.exe"),
        "C:\Program Files\Python314\python.exe",
        "C:\Program Files\Python313\python.exe",
        "C:\Program Files\Python312\python.exe",
        "C:\Program Files\Python311\python.exe"
    )
    foreach ($path in $candidates) {
        if ($path -and (Test-Path $path)) { return $path }
    }
    foreach ($cmd in @(Get-Command python.exe -All -ErrorAction SilentlyContinue)) {
        if ($cmd.Source -notlike '*WindowsApps*') { return $cmd.Source }
    }
    return $null
}

function Register-UpdateServerTask($RepoRoot) {
    $updatesDir = Join-Path $RepoRoot "updates"
    if (-not (Test-Path $updatesDir)) {
        New-Item -ItemType Directory -Path $updatesDir -Force | Out-Null
    }

    $python = Find-PythonExe
    if (-not $python) {
        Write-Host "[WARN] Python ne nayden - avtozapusk servera obnovleniy propushchen."
        Write-Host "       Ustanovite Python 3 i snova zapustite setup_server.bat"
        return $false
    }

    $taskName = "VapManager Update Server"
    Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue

    $logFile = Join-Path $updatesDir "update_server.log"
    $action = New-ScheduledTaskAction -Execute $python -Argument "-m http.server 8765 --bind 0.0.0.0" -WorkingDirectory $updatesDir
    $trigger = New-ScheduledTaskTrigger -AtStartup
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1)
    Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -RunLevel Highest -User "SYSTEM" | Out-Null
    Write-Host "[OK] Zadacha avtozapuska: $taskName (Python: $python)"
    Write-Host "     Log: $logFile"

    Start-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    return $true
}

Write-Host ""
Write-Host "VapManager - nastroyka servera" -ForegroundColor Green
Write-Host "Papka proekta: $RepoRoot"

if ($ServerIp -ne "") {
    $lanIp = $ServerIp
} else {
    $lanIp = Get-LanIPv4
}
Write-Host "IP servera dlya klientov: $lanIp"

# --- PostgreSQL ---
Write-Step "1/5 PostgreSQL: sluzhba i set"
$dataDir = Find-PostgresDataDir
Write-Host "Katalog dannykh: $dataDir"

$pgService = Get-Service -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "postgresql*" } | Select-Object -First 1
if (-not $pgService) { throw "Sluzhba PostgreSQL ne naydena." }

Set-Service $pgService.Name -StartupType Automatic
if ($pgService.Status -ne 'Running') { Start-Service $pgService.Name }
Write-Host "[OK] Sluzhba $($pgService.Name): avtozapusk, zapushchena"

Configure-PostgresRemote $dataDir
Ensure-FirewallRule "PostgreSQL VapManager (5432)" 5432

Restart-Service $pgService.Name -Force
Write-Host "[OK] PostgreSQL perezapushchen"

# --- Database ---
if (-not $SkipDatabase) {
    Write-Step "2/5 PostgreSQL: sozdanie bazy i polzovatelya"
    $psql = Find-PsqlExe
    if (-not $psql) { throw "psql.exe ne nayden." }

    if ($PostgresPassword -eq "") {
        Write-Host "Vvedite parol superpolzovatelya postgres (zadavali pri ustanovke PostgreSQL):"
        $sec = Read-Host -AsSecureString
        $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($sec)
        try { $PostgresPassword = [Runtime.InteropServices.Marshal]::PtrToStringAuto($ptr) }
        finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr) }
    }

    $sqlFile = Join-Path $RepoRoot "install_agv_manager_db.sql"
    if (-not (Test-Path $sqlFile)) { throw "Ne nayden $sqlFile" }

    $env:PGPASSWORD = $PostgresPassword
    & $psql -U postgres -h localhost -v ON_ERROR_STOP=1 -f $sqlFile
    if ($LASTEXITCODE -ne 0) { throw "Oshibka vypolneniya install_agv_manager_db.sql" }

    $alterSql = "ALTER ROLE vapmanager WITH LOGIN PASSWORD '$AppDbPassword';"
    & $psql -U postgres -h localhost -d agv_manager_db -v ON_ERROR_STOP=1 -c $alterSql
    if ($LASTEXITCODE -ne 0) { throw "Ne udalos zadat parol vapmanager" }
    Remove-Item Env:PGPASSWORD -ErrorAction SilentlyContinue
    Write-Host "[OK] BD agv_manager_db, polzovatel vapmanager"
} else {
    Write-Step '2/5 PostgreSQL: propusk (SkipDatabase)'
}

# --- config.ini ---
Write-Step "3/5 config.ini"
Write-ConfigIni (Join-Path $RepoRoot "config.ini") $lanIp $AppDbPassword
Write-ConfigIni (Join-Path $RepoRoot "config.ini.client") $lanIp $AppDbPassword
Write-ConfigIni (Join-Path $RepoRoot "config.ini.server-local") "127.0.0.1" $AppDbPassword

$distCfg = Join-Path $RepoRoot "dist\VapManager\config.ini"
if (Test-Path (Split-Path $distCfg -Parent)) {
    Write-ConfigIni $distCfg $lanIp $AppDbPassword
}

# --- Updates ---
if (-not $SkipUpdates) {
    Write-Step "4/5 Server obnovleniy (port 8765)"
    Ensure-FirewallRule "VapManager Updates (8765)" 8765
    Register-UpdateServerTask $RepoRoot | Out-Null

    $distExe = Join-Path $RepoRoot "dist\VapManager\VapManager.exe"
    if (Test-Path $distExe) {
        Write-Host "Nayden dist\VapManager - publikatsiya obnovleniy..."
        $pub = Join-Path $RepoRoot "publish_local_updates.bat"
        if (Test-Path $pub) {
            & cmd.exe /c "`"$pub`""
        }
    } else {
        Write-Host "[INFO] dist\VapManager\VapManager.exe net - posle sborki zapustite publish_local_updates.bat"
    }
} else {
    Write-Step '4/5 Server obnovleniy: propusk (SkipUpdates)'
}

# --- Done ---
Write-Step "5/5 Gotovo"
Write-Host ""
Write-Host "Server nastroen." -ForegroundColor Green
Write-Host ""
Write-Host "  Klienty v seti:     db_host=$lanIp"
Write-Host "  Na etom PK (lokal): db_host=127.0.0.1  (config.ini.server-local)"
Write-Host "  PostgreSQL:         $lanIp`:5432 / agv_manager_db / vapmanager"
Write-Host "  Obnovleniya:        http://${lanIp}:8765/version.json"
Write-Host "  Parol BD:           $AppDbPassword"
Write-Host ""
Write-Host "Proverka:"
Write-Host '  psql -U vapmanager -h localhost -d agv_manager_db -c "SELECT 1"'
Write-Host "  http://${lanIp}:8765/version.json"
Write-Host ""
