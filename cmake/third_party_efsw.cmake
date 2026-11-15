include(FetchContent)

FetchContent_Declare(
    efsw
    GIT_REPOSITORY https://github.com/SpartanJ/efsw.git
    GIT_TAG        1.7.3
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

# Keep efsw's generic option names local to this dependency. Build one static
# target without changing how any other dependency is linked.
block()
    # efsw supports C++17; keep its use of u8path out of C++20 deprecations.
    set(CMAKE_CXX_STANDARD 17)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_STATIC_LIBS OFF)
    set(BUILD_TEST_APP OFF)
    set(BUILD_AGENT OFF)
    set(EFSW_INSTALL OFF)
    FetchContent_MakeAvailable(efsw)
endblock()
