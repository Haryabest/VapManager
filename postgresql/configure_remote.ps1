#Requires -RunAsAdministrator
<#
  Enables remote TCP connections to PostgreSQL on Windows.
  Run: powershell -ExecutionPolicy Bypass -File configure_remote.ps1
#>

$ErrorActionPreference = "Stop"

function Find-PostgresDataDir {
    $services = Get-Service -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "postgresql*" }
    foreach ($svc in $services) {
        $pathName = (Get-CimInstance Win32_Service -Filter "Name='$($svc.Name)'").PathName
        if ($pathName -match '-D\s+"([^"]+)"') {
            $dataDir = $Matches[1]
            if (Test-Path (Join-Path $dataDir "postgresql.conf")) {
                return $dataDir
            }
        }
        if ($pathName -match '"([^"]+)\\bin\\pg_ctl\.exe"') {
            $installRoot = $Matches[1]
            $dataDir = Join-Path $installRoot "data"
            if (Test-Path (Join-Path $dataDir "postgresql.conf")) {
                return $dataDir
            }
        }
    }
    foreach ($ver in @(18, 17, 16, 15, 14)) {
        $p = "C:\Program Files\PostgreSQL\$ver\data"
        if (Test-Path (Join-Path $p "postgresql.conf")) {
            return $p
        }
    }
    throw "PostgreSQL data directory not found. Install PostgreSQL first."
}

$dataDir = Find-PostgresDataDir
Write-Host "PostgreSQL data: $dataDir"

$conf = Join-Path $dataDir "postgresql.conf"
$hba  = Join-Path $dataDir "pg_hba.conf"
$utf8NoBom = New-Object System.Text.UTF8Encoding $false

# listen_addresses
$lines = [System.IO.File]::ReadAllLines($conf)
$foundListen = $false
$out = foreach ($line in $lines) {
    if ($line -match '^\s*#?\s*listen_addresses\s*=') {
        $foundListen = $true
        "listen_addresses = '*'"
    } else {
        $line
    }
}
if (-not $foundListen) {
    $out = $out + "listen_addresses = '*'"
}
[System.IO.File]::WriteAllLines($conf, $out, $utf8NoBom)
Write-Host "[OK] listen_addresses = '*'"

# pg_hba: allow password auth from LAN
$ruleV4 = "host    all             all             0.0.0.0/0               scram-sha-256"
$ruleV6 = "host    all             all             ::0/0                   scram-sha-256"
$hbaLines = [System.IO.File]::ReadAllLines($hba)
if ($hbaLines -notcontains $ruleV4) {
    $add = @("", "# VapManager remote clients", $ruleV4, $ruleV6)
    [System.IO.File]::WriteAllLines($hba, ($hbaLines + $add), $utf8NoBom)
    Write-Host "[OK] pg_hba.conf: remote rules added"
} else {
    Write-Host "[OK] pg_hba.conf: remote rules already present"
}

try {
    $existing = Get-NetFirewallRule -DisplayName "PostgreSQL VapManager (5432)" -ErrorAction SilentlyContinue
    if (-not $existing) {
        New-NetFirewallRule -DisplayName "PostgreSQL VapManager (5432)" -Direction Inbound `
            -Protocol TCP -LocalPort 5432 -Action Allow | Out-Null
        Write-Host "[OK] Firewall rule for port 5432"
    } else {
        Write-Host "[OK] Firewall rule already exists"
    }
} catch {
    Write-Host "[WARN] Firewall: $_"
}

$svc = Get-Service | Where-Object { $_.Name -like "postgresql*" -and $_.Status -eq 'Running' } | Select-Object -First 1
if (-not $svc) {
    $svc = Get-Service | Where-Object { $_.Name -like "postgresql*" } | Select-Object -First 1
}
if ($svc) {
    Restart-Service $svc.Name -Force
    Write-Host "[OK] Restarted service: $($svc.Name)"
}

Write-Host ""
Write-Host "Done. On client PCs set in config.ini:"
Write-Host "  db_host=<this PC IP>"
Write-Host "  db_port=5432"
Write-Host "  db_user=vapmanager"
Write-Host "  db_password=<your password from config.ini>"
