#Requires -RunAsAdministrator
# Устарело: используйте setup_server.bat в корне проекта.
& (Join-Path $PSScriptRoot "..\server\setup_server.ps1") -RepoRoot (Join-Path $PSScriptRoot "..")
