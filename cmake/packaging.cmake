include(GNUInstallDirs)

set(distribution_docs "${CMAKE_INSTALL_DATADIR}/mizuki")
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/README.md"
              "${PROJECT_SOURCE_DIR}/CHANGELOG.md" "${PROJECT_SOURCE_DIR}/CONTRIBUTING.md"
              "${PROJECT_SOURCE_DIR}/RELEASING.md"
    DESTINATION "${distribution_docs}" COMPONENT mizuki)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/docs/"
    DESTINATION "${distribution_docs}/docs" COMPONENT mizuki)

set(CPACK_SOURCE_INSTALLED_DIRECTORIES "${PROJECT_SOURCE_DIR};/")
foreach(dependency spdlog md4c efsw httplib tomlplusplus json inja)
    FetchContent_GetProperties(${dependency} SOURCE_DIR dependency_source)
    if(NOT IS_DIRECTORY "${dependency_source}")
        message(FATAL_ERROR "Missing source for distribution dependency: ${dependency}")
    endif()
    if(EXISTS "${dependency_source}/LICENSE")
        set(dependency_license "${dependency_source}/LICENSE")
    elseif(EXISTS "${dependency_source}/LICENSE.md")
        set(dependency_license "${dependency_source}/LICENSE.md")
    elseif(EXISTS "${dependency_source}/LICENSE.MIT")
        set(dependency_license "${dependency_source}/LICENSE.MIT")
    else()
        message(FATAL_ERROR "Missing license for distribution dependency: ${dependency}")
    endif()
    install(FILES "${dependency_license}"
        DESTINATION "${distribution_docs}/licenses/${dependency}" COMPONENT mizuki)

    install(DIRECTORY "${dependency_source}/"
        DESTINATION "third_party/${dependency}" COMPONENT mizuki_sources EXCLUDE_FROM_ALL
        PATTERN ".git" EXCLUDE PATTERN "__pycache__" EXCLUDE PATTERN "*.pyc" EXCLUDE)
    if(dependency STREQUAL "spdlog")
        # fmt's bundled headers contain its complete license in the first comment.
        file(READ "${dependency_source}/include/spdlog/fmt/bundled/format.h" fmt_header)
        string(FIND "${fmt_header}" "*/" fmt_license_end)
        if(NOT fmt_header MATCHES "^/[*]" OR fmt_license_end LESS 0)
            message(FATAL_ERROR "Cannot extract the bundled fmt license")
        endif()
        math(EXPR fmt_license_length "${fmt_license_end} + 2")
        string(SUBSTRING "${fmt_header}" 0 ${fmt_license_length} fmt_license)
        file(WRITE "${PROJECT_BINARY_DIR}/licenses/fmt-LICENSE" "${fmt_license}\n")
        install(FILES "${PROJECT_BINARY_DIR}/licenses/fmt-LICENSE"
            DESTINATION "${distribution_docs}/licenses/fmt" RENAME LICENSE COMPONENT mizuki)
    endif()
endforeach()

string(TOLOWER "${CMAKE_SYSTEM_NAME}" distribution_platform)
set(CPACK_PACKAGE_NAME mizuki)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A static blog generator with live preview")
set(CPACK_PACKAGE_FILE_NAME "mizuki-${PROJECT_VERSION}-${distribution_platform}-x86_64")
set(CPACK_PACKAGE_DIRECTORY "${PROJECT_BINARY_DIR}/packages")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_VERBATIM_VARIABLES ON)
# Install just our component; dependencies may define their own install rules.
set(CPACK_INSTALL_CMAKE_PROJECTS "${PROJECT_BINARY_DIR};${PROJECT_NAME};mizuki;/")
if(WIN32)
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
endif()

set(CPACK_SOURCE_GENERATOR TGZ)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "mizuki-${PROJECT_VERSION}-source")
set(CPACK_SOURCE_INSTALL_CMAKE_PROJECTS "${PROJECT_BINARY_DIR};${PROJECT_NAME};mizuki_sources;/")
set(CPACK_SOURCE_IGNORE_FILES
    "/[.]git(/|$)"
    "/__pycache__(/|$)"
    "[.]pyc$"
    "^${PROJECT_BINARY_DIR}(/|$)"
    "^${PROJECT_SOURCE_DIR}/build(/|$)"
    "^${PROJECT_SOURCE_DIR}/dist(/|$)"
    "^${PROJECT_SOURCE_DIR}/third_party(/|$)"
    "^${PROJECT_SOURCE_DIR}/[.]cache(/|$)"
    "^${PROJECT_SOURCE_DIR}/CMakeUserPresets[.]json$")

include(CPack)
