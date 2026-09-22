@echo off
setlocal

set "VSDEVCMD=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJA=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "QT_ROOT=C:\Qt\6.8.3\msvc2022_64"

if not exist "%VSDEVCMD%" (
    echo Visual Studio developer environment not found.
    exit /b 1
)
if not exist "%QT_ROOT%\bin\Qt6Widgets.dll" (
    echo Qt 6.8.3 MSVC installation not found.
    exit /b 1
)

call "%VSDEVCMD%" -arch=x64 -host_arch=x64 || exit /b 1

if exist build-cpp rmdir /s /q build-cpp
"%CMAKE%" -S . -B build-cpp -G Ninja ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_PREFIX_PATH="%QT_ROOT%" ^
    -DCMAKE_MAKE_PROGRAM="%NINJA%" || exit /b 1

"%CMAKE%" --build build-cpp --parallel || exit /b 1

"%QT_ROOT%\bin\windeployqt.exe" --debug --no-translations --compiler-runtime "build-cpp\Magic-Home-Controller.exe" || exit /b 1

echo Debug package: %CD%\build-cpp\Magic-Home-Controller.exe
endlocal
