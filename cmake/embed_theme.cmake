# Keep the editable default theme as the only source of the embedded copy.
set(theme_root "${PROJECT_SOURCE_DIR}/themes/default")
file(GLOB_RECURSE theme_files CONFIGURE_DEPENDS LIST_DIRECTORIES false
    RELATIVE "${theme_root}" "${theme_root}/*")
list(SORT theme_files)
set(MIZUKI_THEME_FILES "")
foreach(relative IN LISTS theme_files)
    set(source "${theme_root}/${relative}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    file(READ "${source}" content HEX)
    string(LENGTH "${content}" size)
    math(EXPR size "${size} / 2")
    string(REGEX REPLACE "(..)" "\\\\x\\1" content "${content}")
    string(HEX "${relative}" name)
    string(REGEX REPLACE "(..)" "\\\\x\\1" name "${name}")
    string(APPEND MIZUKI_THEME_FILES
        "    EmbeddedThemeFile{\"${name}\", std::string_view{\"${content}\", ${size}}},\n")
endforeach()
configure_file("${PROJECT_SOURCE_DIR}/cmake/embedded_theme.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_theme.hpp" @ONLY)
