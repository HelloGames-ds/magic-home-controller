@echo off
setlocal

set "VSDEVCMD=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "QT_ROOT=C:\Qt\6.8.3\msvc2022_64"

call "%VSDEVCMD%" -arch=x64 -host_arch=x64 || exit /b 1

if exist build-release rmdir /s /q build-release
if exist dist-cpp rmdir /s /q dist-cpp

"%CMAKE%" -S . -B build-release -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%QT_ROOT%" ^
    -DCMAKE_MAKE_PROGRAM="%NINJA%" || exit /b 1
"%CMAKE%" --build build-release --parallel || exit /b 1

mkdir dist-cpp
copy /y "build-release\Magic-Home-Controller.exe" "dist-cpp\Magic-Home-Controller.exe" >nul || exit /b 1
"%QT_ROOT%\bin\windeployqt.exe" --release --compiler-runtime "dist-cpp\Magic-Home-Controller.exe" || exit /b 1

if not exist "dist-cpp\translations" mkdir "dist-cpp\translations"
copy /y "translations\magic_home_controller_ru.qm" "dist-cpp\translations\magic_home_controller_ru.qm" >nul || exit /b 1
if not exist "dist-cpp\translations\qtbase_ru.qm" copy /y "C:\Qt\6.8.3\msvc2022_64\translations\qtbase_ru.qm" "dist-cpp\translations\qtbase_ru.qm" >nul 2>&1

echo Release package: %CD%\dist-cpp\Magic-Home-Controller.exe
endlocal