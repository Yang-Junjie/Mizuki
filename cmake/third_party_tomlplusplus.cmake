include(FetchContent)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS 3.4
)
block()
    set(TOMLPLUSPLUS_BUILD_TESTS OFF)
    set(TOMLPLUSPLUS_BUILD_EXAMPLES OFF)
    FetchContent_MakeAvailable(tomlplusplus)
endblock()
