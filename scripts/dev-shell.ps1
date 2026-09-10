#Requires -Version 5.1
<#
.SYNOPSIS
    Opens a Visual Studio 2026 x64 build environment in the current shell.
.DESCRIPTION
    Imports VsDevShell so cl.exe / MSBuild are on PATH, then verifies the
    Stage 0 toolchain (MSVC + CMake + Ninja). Run this before cmake --preset.
#>
$ErrorActionPreference = 'Stop'

# winget installs (CMake, Ninja) land on Machine/User PATH but are invisible
# until the shell restarts. Refresh first, then import the VS environment
# (which appends cl.exe etc. — order matters, do not reverse these steps).
$env:Path = [System.Environment]::GetEnvironmentVariable('Path', 'Machine') +
    ';' + [System.Environment]::GetEnvironmentVariable('Path', 'User')

$vsWhere = "${Env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vsWhere)) {
    throw "vswhere.exe not found at $vsWhere"
}
$installPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installPath) {
    $installPath = & $vsWhere -latest -products * -property installationPath
}
if (-not $installPath) { throw "No Visual Studio with VC.Tools found." }
Write-Host "VS installation: $installPath"

$devShell = Join-Path $installPath 'Common7\Tools\Launch-VsDevShell.ps1'
& $devShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation

foreach ($tool in @('cl', 'cmake', 'ninja')) {
    $found = Get-Command $tool -ErrorAction SilentlyContinue
    if (-not $found) { throw "$tool not found on PATH after dev-shell import." }
    Write-Host "$tool -> $($found.Source)"
}
$prevEA = $ErrorActionPreference
$ErrorActionPreference = 'SilentlyContinue'
cmd /c "cl 2>&1" | Select-Object -First 2
$ErrorActionPreference = $prevEA
cmake --version | Select-Object -First 1
ninja --version
Write-Host "Toolchain OK."
