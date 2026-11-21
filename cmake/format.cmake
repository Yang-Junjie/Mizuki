find_program(MIZUKI_CLANG_FORMAT NAMES clang-format-18 clang-format)
if(MIZUKI_CLANG_FORMAT)
    file(GLOB_RECURSE MIZUKI_FORMAT_SOURCES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp"
        "${PROJECT_SOURCE_DIR}/tests/*.hpp")
    add_custom_target(format
        COMMAND "${MIZUKI_CLANG_FORMAT}" -i ${MIZUKI_FORMAT_SOURCES}
        COMMENT "Formatting project C++ sources"
        VERBATIM)
    add_custom_target(check-format
        COMMAND "${MIZUKI_CLANG_FORMAT}" --dry-run --Werror ${MIZUKI_FORMAT_SOURCES}
        COMMENT "Checking project C++ formatting"
        VERBATIM)
else()
    message(STATUS "clang-format not found; format and check-format targets are unavailable")
endif()
