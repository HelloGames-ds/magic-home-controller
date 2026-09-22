@echo off
setlocal

call build-release.cmd || exit /b 1

set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo Inno Setup not found: %ISCC%
    exit /b 1
)

if exist installer-output rmdir /s /q installer-output
"%ISCC%" installer.iss || exit /b 1

echo Installer: %CD%\installer-output\Magic-Home-Controller-Setup-1.0-x64.exe
endlocal
