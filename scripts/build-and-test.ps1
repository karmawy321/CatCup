#Requires -Version 5.1
param([string]$Preset = 'debug')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
& (Join-Path $root 'scripts\dev-shell.ps1')
cmake --build --preset $Preset
ctest --preset debug --test-dir (Join-Path $root 'build\debug') --output-on-failure
