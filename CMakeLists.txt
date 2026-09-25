cmake_minimum_required(VERSION 3.21)

project(MagicHomeController
    VERSION 1.1
    DESCRIPTION "Magic Home LED Controller"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

qt_add_executable(MagicHomeController WIN32
    app.rc
    icon.ico
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/SessionShutdownFilter.cpp
    src/SessionShutdownFilter.h
    src/WifiWorker.cpp
    src/WifiWorker.h
    src/ColorWheel.cpp
    src/ColorWheel.h
    src/ColorSmoother.cpp
    src/ColorSmoother.h
    src/ColorUtils.h
    src/EffectEngine.cpp
    src/EffectEngine.h
    src/AmbiLight.cpp
    src/AmbiLight.h
    src/ScreenBackend.h
    src/DxgiCapture.cpp
    src/DxgiCapture.h
    src/WgcCapture.cpp
    src/WgcCapture.h
    src/AmbiEditor.cpp
    src/AmbiEditor.h
    src/AppLog.cpp
    src/AppLog.h
    src/LogWindow.cpp
    src/LogWindow.h
    src/DeviceScanner.cpp
    src/DeviceScanner.h
    src/SettingsStore.cpp
    src/SettingsStore.h
    src/TrayPopup.cpp
    src/TrayPopup.h
    src/IconFactory.cpp
    src/IconFactory.h
    src/config.h
    src/protocol.h
)

target_link_libraries(MagicHomeController PRIVATE
    Qt6::Widgets
    Qt6::Network
)

target_compile_definitions(MagicHomeController PRIVATE
    QT_NO_KEYWORDS
    APP_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(MagicHomeController PRIVATE /W4 /permissive- /utf-8 /await)
    target_compile_definitions(MagicHomeController PRIVATE
        _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
    )

    # Windows.Graphics.Capture (WinRT) через сгенерированную C++/WinRT проекцию.
    target_include_directories(MagicHomeController PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

    # Интероп-функции WGC (CreateDirect3D11DeviceFromDXGIDevice и пр.) + активация WinRT.
    target_link_libraries(MagicHomeController PRIVATE WindowsApp)
endif()

set_target_properties(MagicHomeController PROPERTIES
    OUTPUT_NAME "Magic-Home-Controller"
    WIN32_EXECUTABLE TRUE
)

install(TARGETS MagicHomeController
    RUNTIME DESTINATION .
)

qt_generate_deploy_app_script(
    TARGET MagicHomeController
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})