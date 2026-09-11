#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;" + $env:PATH
$exe = Join-Path $root "build\s1-debug\src\shell\editor-ui.exe"
if (-not (Test-Path $exe)) {
    Write-Host "editor-ui.exe not found. Building preset s1..." -ForegroundColor Yellow
    & (Join-Path $root "scripts\dev-shell.ps1")
    cmake --build --preset s1
}
Write-Host "Launching CatCup..." -ForegroundColor Green
Start-Process $exe
