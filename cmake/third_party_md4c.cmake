include(FetchContent)

FetchContent_Declare(
    md4c
    GIT_REPOSITORY https://github.com/mity/md4c.git
    GIT_TAG        v0.6.0
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    FIND_PACKAGE_ARGS 0.6
)

FetchContent_MakeAvailable(md4c)
