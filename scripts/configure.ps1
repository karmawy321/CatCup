#Requires -Version 5.1
param([string]$Preset = 'windows-ninja-debug')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
& (Join-Path $root 'scripts\dev-shell.ps1')
cmake --preset $Preset --source $root
