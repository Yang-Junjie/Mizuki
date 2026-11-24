cmake_minimum_required(VERSION 3.28)
project(Mizuki VERSION 0.2.1 LANGUAGES C CXX)

set(MIZUKI_RELEASE_TAG "" CACHE STRING "Release tag to check against the project version")
if(MIZUKI_RELEASE_TAG AND NOT MIZUKI_RELEASE_TAG STREQUAL "v${PROJECT_VERSION}")
    message(FATAL_ERROR "Release tag '${MIZUKI_RELEASE_TAG}' must be 'v${PROJECT_VERSION}'")
endif()

option(MIZUKI_BUILD_DISTRIBUTION "Build binary and source release archives" OFF)
if(MIZUKI_BUILD_DISTRIBUTION)
    if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
        message(FATAL_ERROR "Distribution builds require a separate build directory")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
       NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
        message(FATAL_ERROR "Distribution builds currently support x86_64 only")
    endif()
    if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU") AND
       NOT (WIN32 AND MSVC))
        message(FATAL_ERROR "Distribution builds require Linux/GCC or Windows/MSVC (including ClangCL)")
    endif()
    set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(cmake/project_options.cmake)
include(cmake/dependencies.cmake)
include(cmake/format.cmake)

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(MIZUKI_BUILD_DISTRIBUTION)
    include(cmake/packaging.cmake)
endif()
