@echo off
setlocal

set "QT_ROOT=C:\Qt\6.8.3\msvc2022_64"

echo --- lupdate (scan sources) ---
"%QT_ROOT%\bin\lupdate.exe" src -source-language en -target-language ru -ts "translations\magic_home_controller_ru.ts" || exit /b 1

echo.
echo --- lrelease (compile .qm) ---
"%QT_ROOT%\bin\lrelease.exe" "translations\magic_home_controller_ru.ts" -qm "translations\magic_home_controller_ru.qm" || exit /b 1

echo.
echo Translations updated. Edit translations\magic_home_controller_ru.ts in Qt Linguist, then re-run lrelease or this script.
endlocal