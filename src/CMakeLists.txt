include("${PROJECT_SOURCE_DIR}/cmake/embed_theme.cmake")

# Invalidate render entries when the renderer or its pinned dependencies change,
# including local builds that share a release version.
file(GLOB_RECURSE cache_inputs CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/parse/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/parse/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/render/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/render/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/util/*.hpp")
list(APPEND cache_inputs
    "${CMAKE_CURRENT_SOURCE_DIR}/build/builder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/context.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/incremental.cpp"
    "${PROJECT_SOURCE_DIR}/cmake/third_party_md4c.cmake"
    "${PROJECT_SOURCE_DIR}/cmake/third_party_templates.cmake")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${cache_inputs})
set(cache_identity "${PROJECT_VERSION};${CMAKE_SYSTEM_NAME};${CMAKE_CXX_COMPILER_ID};${CMAKE_CXX_COMPILER_VERSION}")
foreach(input IN LISTS cache_inputs)
    file(SHA256 "${input}" digest)
    string(APPEND cache_identity "${digest}")
endforeach()
string(SHA256 MIZUKI_RENDER_CACHE_VERSION "${cache_identity}")
configure_file(build/cache_version.hpp.in generated/build/cache_version.hpp @ONLY)

# Build logic is reusable without bringing in HTTP or file watching.
add_library(mizuki_core STATIC
    build/artifact_cache.cpp
    build/builder.cpp
    build/context.cpp
    build/image_jpeg.cpp
    build/image_png.cpp
    build/image_webp.cpp
    build/images.cpp
    build/incremental.cpp
    build/links.cpp
    build/output_transaction.cpp
    build/paths.cpp
    build/plan.cpp
    config/config.cpp
    config/values.cpp
    parse/front_matter.cpp
    parse/headings.cpp
    parse/markdown.cpp
    render/html_links.cpp
    render/publication.cpp
    render/theme.cpp
    scaffold/content.cpp
    scaffold/files.cpp
    scaffold/site.cpp
    util/date.cpp
    util/file.cpp
    util/html.cpp
    util/path.cpp
    util/url.cpp
)
target_include_directories(mizuki_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(mizuki_core PRIVATE
    spdlog::spdlog md4c md4c-html tomlplusplus::tomlplusplus pantor::inja
    mizuki_jpeg webp libwebpmux mizuki_sha256
    PUBLIC nlohmann_json::nlohmann_json)

add_library(mizuki_dev STATIC
    dev/file_watcher.cpp
    dev/live_reload.cpp
    dev/preview.cpp
    dev/server.cpp
    dev/watch.cpp
)
target_link_libraries(mizuki_dev PUBLIC mizuki_core PRIVATE
    spdlog::spdlog efsw httplib::httplib)

add_executable(mizuki main.cpp cli/commands.cpp)
target_link_libraries(mizuki PRIVATE mizuki_dev spdlog::spdlog)
configure_file(cli/version.hpp.in generated/cli/version.hpp @ONLY)
target_include_directories(mizuki PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(MIZUKI_BUILD_DISTRIBUTION AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(mizuki PRIVATE -static-libstdc++ -static-libgcc)
endif()

foreach(target mizuki_core mizuki_dev mizuki)
    mizuki_configure_target(${target})
endforeach()

include(GNUInstallDirs)
install(TARGETS mizuki RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT mizuki)
