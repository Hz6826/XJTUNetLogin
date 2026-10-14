cmake_minimum_required(VERSION 3.21)

project(XJTUNetLogin VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---------- 查找 Qt 模块 ----------
find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        Network
        Svg
)

qt_standard_project_setup()

configure_file(
        ${CMAKE_SOURCE_DIR}/src/AppInfo.h.in
        ${CMAKE_BINARY_DIR}/generated/AppInfo.h
        @ONLY
)

# ---------- 可执行目标 ----------
qt_add_executable(XJTUNetLogin
        src/main.cpp
        src/net/SrunCrypto.cpp
        src/net/SrunClient.cpp
        src/config/Config.cpp
        src/ui/MainWindow.cpp
        src/ui/MainWindow.h
        resources/resources.qrc
        # 后续逐步加入：
        # src/app/Application.cpp
        # src/app/TrayController.cpp
        # src/core/AutoLoginService.cpp
        # src/platform/WindowsPlatformHelper.cpp
)

# 让源文件可以用 "net/SrunClient.h" 这种相对路径 include
target_include_directories(XJTUNetLogin PRIVATE
        src
        ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(XJTUNetLogin PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Svg
)

# ---------- Windows 开发时拷贝 Qt 运行时 DLL ----------
if (WIN32)
    find_program(WINDEPLOYQT_EXE windeployqt
            HINTS "${QT_INSTALL_PATH}/bin")

    if (WINDEPLOYQT_EXE)
        add_custom_command(TARGET XJTUNetLogin POST_BUILD
                COMMAND "${WINDEPLOYQT_EXE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:XJTUNetLogin>"
                COMMENT "Running windeployqt to copy Qt runtime")
    else ()
        message(WARNING "windeployqt not found, runtime DLLs may not be copied")
    endif ()
endif ()
