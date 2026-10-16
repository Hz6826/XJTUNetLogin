# 由 CMakeLists.txt 通过 `cmake -P` 调用
# 输入：
#   SRC - 源码根目录（含 .git）
#   OUT - 输出的 .cmake 脚本路径
# 内容不变时不写入，避免更新 mtime 触发下游重编

find_package(Git QUIET)

set(GIT_HASH_SHORT "unknown")
set(GIT_BRANCH "unknown")

if (GIT_FOUND AND EXISTS "${SRC}/.git")
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${SRC}
            OUTPUT_VARIABLE GIT_HASH_SHORT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
            WORKING_DIRECTORY ${SRC}
            OUTPUT_VARIABLE GIT_BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )
endif ()

# detached HEAD 时 rev-parse --abbrev-ref 会返回 "HEAD"
if (GIT_BRANCH STREQUAL "HEAD")
    set(GIT_BRANCH "detached@${GIT_HASH_SHORT}")
endif ()

string(CONCAT NEW_CONTENT
        "set(GIT_HASH_SHORT \"${GIT_HASH_SHORT}\")\n"
        "set(GIT_BRANCH \"${GIT_BRANCH}\")\n"
)

if (EXISTS "${OUT}")
    file(READ "${OUT}" OLD_CONTENT)
    if (OLD_CONTENT STREQUAL NEW_CONTENT)
        return()
    endif ()
endif ()

file(WRITE "${OUT}" "${NEW_CONTENT}")
