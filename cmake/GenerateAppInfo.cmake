# 输入：
#   SRC              - 源码根目录
#   OUT              - 输出的 AppInfo.h
#   VERSION          - 项目版本
#   NAME             - 项目名
#   GIT_VERSION_FILE - 由 GetGitVersion.cmake 生成的脚本
#   BUILD_TYPE       - CMake 的 CMAKE_BUILD_TYPE（可能为空）

set(GIT_HASH_SHORT "unknown")
set(GIT_BRANCH "unknown")
if (EXISTS "${GIT_VERSION_FILE}")
    include("${GIT_VERSION_FILE}")
endif ()

set(PROJECT_VERSION "${VERSION}")
set(PROJECT_NAME    "${NAME}")

# 单配置生成器（Ninja / Makefile）直接读 CMAKE_BUILD_TYPE；
# 多配置生成器（VS / Xcode）配置阶段为空，用 "Multi-Config" 占位
if ("${BUILD_TYPE}" STREQUAL "")
    set(BUILD_TYPE "Multi-Config")
endif ()

# 是否 Debug
if (BUILD_TYPE STREQUAL "Debug")
    set(IS_DEBUG_BUILD "true")
else ()
    set(IS_DEBUG_BUILD "false")
endif ()

configure_file(
        "${SRC}/src/AppInfo.h.in"
        "${OUT}"
        @ONLY
)
