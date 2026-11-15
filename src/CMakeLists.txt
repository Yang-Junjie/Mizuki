
set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/parse/markdown.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/render/renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util/file.cpp
)

add_executable(mizuki ${SOURCE_FILES})

target_include_directories(mizuki PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(mizuki PRIVATE spdlog::spdlog md4c md4c-html efsw)