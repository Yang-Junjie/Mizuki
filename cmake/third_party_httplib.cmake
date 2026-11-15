include(FetchContent)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.66.0
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

block()
    set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF)
    set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF)
    set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF)
    set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF)
    set(HTTPLIB_INSTALL OFF)
    set(HTTPLIB_TEST OFF)
    FetchContent_MakeAvailable(httplib)
endblock()
