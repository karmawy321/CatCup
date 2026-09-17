@echo off
set "ROOT=%~dp0"
set "DIST_DIR=%ROOT%build\s1-release\dist\editor-ui"
set "RELEASE_EXE=%DIST_DIR%\editor-ui.exe"
set "DEBUG_EXE=%ROOT%build\s1-debug\src\shell\editor-ui.exe"

if exist "%RELEASE_EXE%" (
    start "" /D "%DIST_DIR%" "%RELEASE_EXE%"
    exit /b 0
)

if exist "%DEBUG_EXE%" (
    set "PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH%"
    start "" "%DEBUG_EXE%"
    exit /b 0
)

echo No editor executable found. Please build or deploy the application.
pause
