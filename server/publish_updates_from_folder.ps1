param(
    [string]$SourceDir,
    [string]$RepoRoot = "",
    [string]$Version = "1.0.2",
    [int]$Build = 151,
    [string]$ServerIp = "192.168.0.1"
)

$ErrorActionPreference = "Stop"

if ($RepoRoot -eq "") {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

if (-not (Test-Path $SourceDir)) {
    throw "Source folder not found: $SourceDir"
}

$filesDir = Join-Path $RepoRoot "updates\files"
$updDir = Join-Path $RepoRoot "updates"
New-Item -ItemType Directory -Path $filesDir -Force | Out-Null
$filesDir = (Resolve-Path $filesDir).Path
$SourceDir = (Resolve-Path $SourceDir).Path

Write-Host "Source: $SourceDir"
Write-Host "Target: $filesDir"

if ($SourceDir.TrimEnd('\') -ne $filesDir.TrimEnd('\')) {
    Write-Host "Copying files..."
    robocopy $SourceDir $filesDir /MIR /NFL /NDL /NJH /NJS /NC /NS `
        /XF config.ini `
        /XD logs config translations | Out-Null
    if ($LASTEXITCODE -ge 8) {
        throw "robocopy failed with code $LASTEXITCODE"
    }
} else {
    Write-Host "Regenerating version.json only (source = target)."
}

$excludeNames = @('config.ini', 'unins000.exe', 'Uninstall.exe', 'unins000.dat')
$files = Get-ChildItem -Path $filesDir -Recurse -File | Where-Object {
    $rel = $_.FullName.Substring($filesDir.Length + 1).Replace('\', '/')
    $name = $_.Name
    if ($excludeNames -contains $name) { return $false }
    if ($rel.StartsWith('logs/')) { return $false }
    if ($rel.StartsWith('config/')) { return $false }
    return $true
} | ForEach-Object {
    $rel = $_.FullName.Substring($filesDir.Length + 1).Replace('\', '/')
    [ordered]@{ path = $rel; size = $_.Length }
}

if ($files.Count -eq 0) {
    throw "No files to publish in $filesDir"
}

$baseUrl = "http://${ServerIp}:8765/files/"
$cfg = Join-Path $RepoRoot "config.ini"
if (Test-Path $cfg) {
    $hostLine = Select-String -Path $cfg -Pattern '^\s*db_host=(.+)$' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($hostLine) {
        $dbHost = $hostLine.Matches[0].Groups[1].Value.Trim()
        if ($dbHost) {
            $baseUrl = "http://${dbHost}:8765/files/"
        }
    }
}
$obj = [ordered]@{
    version = $Version
    build = $Build
    notes = "Build 160: fixes chat input bug on Windows tablets (virtual keyboard resize no longer kicks user back to chat list, preserves draft and focus). Build 159: chat row context menu (mark read / archive / restore), advanced deep message search with date/AGV/sender filters, tray notification on update when window minimized, download size shown in update dialog before start."
    baseUrl = $baseUrl
    setupUrl = ""
    files = @($files)
}

$jsonPath = Join-Path $updDir "version.json"
$json = $obj | ConvertTo-Json -Depth 5
[System.IO.File]::WriteAllText($jsonPath, $json, [System.Text.UTF8Encoding]::new($false))

Write-Host "[OK] version.json build=$Build files=$($files.Count)"
Write-Host "[OK] baseUrl=$baseUrl"
Write-Host "[OK] VapManager.exe size=$((Get-Item (Join-Path $filesDir 'VapManager.exe')).Length)"
