include(FetchContent)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.17.0
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    FIND_PACKAGE_ARGS
)

block()
    set(SPDLOG_BUILD_SHARED OFF)
    set(SPDLOG_FMT_EXTERNAL OFF)
    set(SPDLOG_BUILD_EXAMPLE OFF)
    set(SPDLOG_BUILD_TESTS OFF)
    FetchContent_MakeAvailable(spdlog)
endblock()
