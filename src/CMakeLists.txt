include("${PROJECT_SOURCE_DIR}/cmake/embed_theme.cmake")

# Build logic is reusable without bringing in HTTP or file watching.
add_library(mizuki_core STATIC
    build/builder.cpp
    build/links.cpp
    build/output_transaction.cpp
    build/paths.cpp
    build/plan.cpp
    config/config.cpp
    parse/front_matter.cpp
    parse/markdown.cpp
    render/article_list.cpp
    render/html_links.cpp
    render/navigation.cpp
    render/pagination.cpp
    render/publication.cpp
    render/template.cpp
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
    spdlog::spdlog md4c md4c-html tomlplusplus::tomlplusplus)

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

foreach(target mizuki_core mizuki_dev mizuki)
    mizuki_configure_target(${target})
endforeach()

include(GNUInstallDirs)
install(TARGETS mizuki RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT mizuki)
