cmake_minimum_required(VERSION 3.21)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

project(XJTUNetLogin VERSION 0.5.0 LANGUAGES CXX)

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

# ---------- 可执行目标 ----------
qt_add_executable(XJTUNetLogin
        src/main.cpp
        src/net/SrunCrypto.cpp
        src/net/SrunClient.cpp
        src/config/Config.cpp
        src/ui/MainWindow.cpp
        src/ui/TrayController.cpp
        resources/resources.qrc
        src/platform/AutoStart.cpp
        src/ui/ThemeUtils.h
        src/ui/MainWindowUi.cpp
        src/ui/MainWindowLog.cpp
        src/ui/MainWindowConfig.cpp
        src/ui/MainWindowAuth.cpp
        src/ui/MainWindowInfo.cpp
        src/net/SrunHttpUtils.cpp
)

# ---------- 生成 AppInfo.h（含 Git 信息） ----------
set(GIT_VERSION_FILE  ${CMAKE_BINARY_DIR}/generated/git_version.cmake)
set(APPINFO_GENERATED ${CMAKE_BINARY_DIR}/generated/AppInfo.h)

# 每次都跑一次 git 检查，但只有内容变化时才会写文件
add_custom_target(git_version ALL
        COMMAND ${CMAKE_COMMAND}
        -D SRC=${CMAKE_SOURCE_DIR}
        -D OUT=${GIT_VERSION_FILE}
        -P ${CMAKE_SOURCE_DIR}/cmake/GetGitVersion.cmake
        BYPRODUCTS ${GIT_VERSION_FILE}
        COMMENT "Checking git version"
        VERBATIM
)

# AppInfo.h 依赖 git_version.cmake（文件依赖，不是 target 依赖）
# → git_version.cmake 没变时，AppInfo.h 也不会重新生成
add_custom_command(
        OUTPUT ${APPINFO_GENERATED}
        COMMAND ${CMAKE_COMMAND}
        -D SRC=${CMAKE_SOURCE_DIR}
        -D OUT=${APPINFO_GENERATED}
        -D VERSION=${PROJECT_VERSION}
        -D NAME=${PROJECT_NAME}
        -D GIT_VERSION_FILE=${GIT_VERSION_FILE}
        -D BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateAppInfo.cmake
        DEPENDS
        ${CMAKE_SOURCE_DIR}/src/AppInfo.h.in
        ${CMAKE_SOURCE_DIR}/cmake/GenerateAppInfo.cmake
        ${GIT_VERSION_FILE}
        COMMENT "Generating AppInfo.h"
        VERBATIM
)

add_custom_target(appinfo DEPENDS ${APPINFO_GENERATED})
add_dependencies(XJTUNetLogin appinfo)

if (WIN32)
    target_sources(XJTUNetLogin PRIVATE
            resources/windows/app.rc
    )
endif()

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
