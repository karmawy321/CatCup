#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

$releaseExe = Join-Path $root "build\s1-release\dist\editor-ui\editor-ui.exe"
$debugExe = Join-Path $root "build\s1-debug\src\shell\editor-ui.exe"

if (Test-Path $releaseExe) {
    Write-Host "Launching CatCup (Standalone Release)..." -ForegroundColor Green
    Start-Process $releaseExe
    exit 0
}

if (Test-Path $debugExe) {
    $env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;" + $env:PATH
    Write-Host "Launching CatCup (Debug)..." -ForegroundColor Green
    Start-Process $debugExe
    exit 0
}

Write-Host "editor-ui.exe not found. Building preset s1..." -ForegroundColor Yellow
& (Join-Path $root "scripts\dev-shell.ps1")
cmake --build --preset s1
Start-Process $debugExe
