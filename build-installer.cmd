@echo off
setlocal

call build-release.cmd || exit /b 1

set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo Inno Setup not found: %ISCC%
    exit /b 1
)

set "APPVER=%~1"
if not defined APPVER set "APPVER=1.1"
set "APPVER=%APPVER:v=%"

if exist installer-output rmdir /s /q installer-output
"%ISCC%" /DAppVersion=%APPVER% installer.iss || exit /b 1

for %%f in (installer-output\*.exe) do echo Installer: %CD%\%%f
endlocal
