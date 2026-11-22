include(FetchContent)
set(FETCHCONTENT_QUIET OFF CACHE BOOL "Show FetchContent download and population logs")

# Source releases carry the same dependency sources used for their binaries.
foreach(dependency spdlog md4c efsw httplib tomlplusplus)
    string(TOUPPER "${dependency}" dependency_upper)
    set(bundled_source "${PROJECT_SOURCE_DIR}/third_party/${dependency}")
    if(EXISTS "${bundled_source}/CMakeLists.txt" AND
       NOT FETCHCONTENT_SOURCE_DIR_${dependency_upper})
        set(FETCHCONTENT_SOURCE_DIR_${dependency_upper} "${bundled_source}")
    endif()
endforeach()

include(${CMAKE_CURRENT_LIST_DIR}/third_party_logging.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/third_party_md4c.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/third_party_efsw.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/third_party_httplib.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/third_party_tomlplusplus.cmake)
