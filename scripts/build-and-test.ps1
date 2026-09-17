#Requires -Version 5.1
param([ValidateSet('debug', 's1')][string]$Preset = 'debug')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
& (Join-Path $root 'scripts\dev-shell.ps1')
$buildDir = if ($Preset -eq 's1') { 'build\s1-debug' } else { 'build\debug' }
cmake --build (Join-Path $root $buildDir)
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir (Join-Path $root $buildDir) --output-on-failure
exit $LASTEXITCODE
