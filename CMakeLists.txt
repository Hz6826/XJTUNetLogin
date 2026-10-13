cmake_minimum_required(VERSION 3.21)

project(XJTUNetLogin VERSION 0.1.0 LANGUAGES CXX)

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
)

# ---------- Windows 开发时拷贝 Qt 运行时 DLL ----------
if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()

    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()

    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET XJTUNetLogin POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:XJTUNetLogin>/plugins/platforms/")
        add_custom_command(TARGET XJTUNetLogin POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:XJTUNetLogin>/plugins/platforms/")
    endif ()

    foreach (QT_LIB Core Gui Widgets Network)
        add_custom_command(TARGET XJTUNetLogin POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:XJTUNetLogin>")
    endforeach ()
endif ()
