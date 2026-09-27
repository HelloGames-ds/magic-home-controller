cmake_minimum_required(VERSION 3.21)

project(MagicHomeController
    VERSION 1.2
    DESCRIPTION "Magic Home LED Controller"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# The official Qt 6 binaries link against the release MSVC runtime (/MD). A Debug
# build would default to /MDd, and the two runtimes keep separate heaps, so every
# allocation that crosses the Qt boundary corrupts the heap. Force /MD everywhere.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set(CMAKE_MSVC_RUNTIME_LIBRARY_${_cfg} "MultiThreadedDLL")
    endforeach()
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

# Ресурс версии собирается из шаблона, иначе exe продолжает показывать
# захардкоженную версию и Windows не знает, что установлена новая сборка.
# Объявление должно идти до qt_add_executable: список источников вычисляется
# сразу, и пустая переменная молча убрала бы ресурс из сборки.
set(APP_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/icon.ico")
set(APP_VERSION_RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
configure_file(app.rc.in "${APP_VERSION_RESOURCE}" @ONLY)

qt_add_executable(MagicHomeController WIN32
    ${APP_VERSION_RESOURCE}
    icon.ico
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/SystemSessionFilter.cpp
    src/SystemSessionFilter.h
    src/UpdateChecker.cpp
    src/UpdateChecker.h
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
    src/CaptureZone.cpp
    src/CaptureZone.h
    src/MaskedSampling.h
    src/ScreenBackend.cpp
    src/ScreenBackend.h
    src/DxgiCapture.cpp
    src/DxgiCapture.h
    src/WgcCapture.cpp
    src/WgcCapture.h
    src/AmbiEditor.cpp
    src/AmbiEditor.h
    src/AmbiCanvas.cpp
    src/AmbiCanvas.h
    src/EditorIcons.cpp
    src/EditorIcons.h
    src/CurveEditor.cpp
  src/CurveEditor.h
  src/CrashLog.cpp
  src/CrashLog.h
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

    # Уведомления о блокировке сессии и сне (WTSRegisterSessionNotification).
    target_link_libraries(MagicHomeController PRIVATE Wtsapi32)

    # Символы для crash.log: без /DEBUG в Release .pdb не создаётся и стек
    # падения состоит из одних адресов. Включаются опцией: в обычной сборке они
    # не нужны и добавляют ~6 МБ к установщику.
    option(MHC_SYMBOLS "Embed debug symbols into Release for symbolised crash.log" OFF)
    if(MHC_SYMBOLS)
        target_compile_options(MagicHomeController PRIVATE $<$<CONFIG:Release>:/DEBUG:FULL>)
        target_link_options(MagicHomeController PRIVATE $<$<CONFIG:Release>:/DEBUG:FULL>)
    endif()
    target_link_libraries(MagicHomeController PRIVATE dbghelp)
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