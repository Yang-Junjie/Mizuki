include(FetchContent)

FetchContent_Declare(json
    URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.12.0
    URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187)
FetchContent_Declare(inja
    URL https://codeload.github.com/pantor/inja/tar.gz/refs/tags/v3.5.0
    URL_HASH SHA256=a5f0266673c59028eab6ceeddd8b862c70abfeb32fb7a5387c16bf46f3269ab2)
block()
    set(JSON_BuildTests OFF)
    set(INJA_USE_EMBEDDED_JSON OFF CACHE BOOL "Use the pinned JSON dependency" FORCE)
    set(INJA_BUILD_TESTS OFF CACHE BOOL "Build Inja tests" FORCE)
    set(INJA_BUILD_BENCHMARK OFF CACHE BOOL "Build Inja benchmarks" FORCE)
    set(INJA_INSTALL OFF CACHE BOOL "Install Inja" FORCE)
    set(INJA_EXPORT OFF CACHE BOOL "Export Inja targets" FORCE)
    FetchContent_MakeAvailable(json inja)
endblock()
