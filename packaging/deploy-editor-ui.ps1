#Requires -Version 5.1
<#
.SYNOPSIS
    Stage a redistributable editor-ui folder (windeployqt + FFmpeg runtime).
.DESCRIPTION
    Output goes to build/dist/<config>/ by default (git-ignored, never
    committed). Pass -DistDir to override. Expects a built preset dir
    (default: build/s1-debug) and Qt 6.8 installed at C:/Qt/6.8.3.

    GPL NOTE: the pinned FFmpeg build is gpl-shared (libx264). A folder
    produced by this script is fine for local QA, but PUBLIC distribution
    needs the GPL compliance review tracked in third_party/ffmpeg/PINNED.md
    (or a switch to the lgpl-shared pin + mfenc re-test).
#>
param(
    [string]$BuildDir = 'build/s1-release',
    [string]$DistDir = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
if ([string]::IsNullOrEmpty($DistDir)) {
    $DistDir = Join-Path $root (Join-Path $BuildDir 'dist/editor-ui')
}
if ($BuildDir -like '*debug*') {
    throw "Refusing to package a Debug build ($BuildDir): the debug CRT is not redistributable. Use -BuildDir build/s1-release."
}
$exe = Join-Path $root (Join-Path $BuildDir 'src/shell/editor-ui.exe')
if (-not (Test-Path -LiteralPath $exe)) {
    throw "editor-ui.exe not found at $exe -- build first (cmake --build --preset s1)."
}
$windeployqt = 'C:/Qt/6.8.3/msvc2022_64/bin/windeployqt.exe'
if (-not (Test-Path -LiteralPath $windeployqt)) {
    throw "windeployqt not found at $windeployqt."
}

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $DistDir 'editor-ui.exe') -Force

# Default windeployqt layout (plugin subdirs beside the exe): Qt resolves
# those via the application directory out of the box. Do NOT pass
# --plugindir (a custom location needs qt.conf to be found at all).
& $windeployqt --release --qmldir (Join-Path $root 'src/shell/qml') `
    (Join-Path $DistDir 'editor-ui.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }

$ffbin = Join-Path $root 'third_party/ffmpeg/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin'
foreach ($dll in @('avcodec-62.dll','avformat-62.dll','avutil-60.dll','swscale-9.dll','swresample-6.dll','avfilter-11.dll')) {
    Copy-Item -LiteralPath (Join-Path $ffbin $dll) -Destination (Join-Path $DistDir $dll) -Force
}
# Headless QA support: windeployqt skips the offscreen platform plugin, but
# CI smoke (`--qml-smoke` with QT_QPA_PLATFORM=offscreen) needs it. Real
# desktop runs use qwindows.dll and never touch this file.
$offscreen = 'C:/Qt/6.8.3/msvc2022_64/plugins/platforms/qoffscreen.dll'
if (Test-Path -LiteralPath $offscreen) {
    Copy-Item -LiteralPath $offscreen -Destination (Join-Path $DistDir 'platforms/qoffscreen.dll') -Force
}
Copy-Item -LiteralPath (Join-Path $root 'third_party/ffmpeg/PINNED.md') -Destination (Join-Path $DistDir 'FFMPEG-PINNED.md') -Force
Copy-Item -LiteralPath (Join-Path $ffbin 'LICENSE.txt') -Destination (Join-Path $DistDir 'FFMPEG-LICENSE.txt') -Force -ErrorAction SilentlyContinue

Write-Host "Staged: $DistDir"
Get-ChildItem -LiteralPath $DistDir | Select-Object Name, Length | Format-Table -AutoSize | Out-String | Write-Host
