find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(NAME mizuki.cli
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_cli.py"
            "$<TARGET_FILE:mizuki>")
set_tests_properties(mizuki.cli PROPERTIES TIMEOUT 30
    ENVIRONMENT "PYTHONUTF8=1;MIZUKI_EXPECTED_VERSION=${PROJECT_VERSION}")

add_test(NAME mizuki.builder
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_builder.py"
            "$<TARGET_FILE:mizuki>")

add_test(NAME mizuki.site
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_site.py"
            "$<TARGET_FILE:mizuki>")
add_test(NAME mizuki.markdown_features
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_markdown_features.py"
            "$<TARGET_FILE:mizuki>")
add_test(NAME mizuki.reading
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_reading.py"
            "$<TARGET_FILE:mizuki>")
add_test(NAME mizuki.theme
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_theme.py"
            "$<TARGET_FILE:mizuki>")
add_test(NAME mizuki.theme_data
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_theme_data.py"
            "$<TARGET_FILE:mizuki>")
add_test(NAME mizuki.front_matter
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_front_matter.py"
            "$<TARGET_FILE:mizuki>")
add_test(NAME mizuki.docs
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_docs.py"
            "$<TARGET_FILE:mizuki>")
add_test(NAME mizuki.dev
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_dev.py"
            "$<TARGET_FILE:mizuki>")

add_test(NAME mizuki.publication
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_publication.py"
            "$<TARGET_FILE:mizuki>")

add_test(NAME mizuki.listings
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_listings.py"
            "$<TARGET_FILE:mizuki>")

add_test(NAME mizuki.scaffold
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_scaffold.py"
            "$<TARGET_FILE:mizuki>")

add_executable(mizuki_output_transaction_tests
    output_transaction_test.cpp)
target_link_libraries(mizuki_output_transaction_tests PRIVATE mizuki_core)
mizuki_configure_target(mizuki_output_transaction_tests)
add_test(NAME mizuki.output_transaction COMMAND mizuki_output_transaction_tests)

add_executable(mizuki_core_tests core_test.cpp)
target_link_libraries(mizuki_core_tests PRIVATE mizuki_core)
mizuki_configure_target(mizuki_core_tests)
add_test(NAME mizuki.core COMMAND mizuki_core_tests)

set_tests_properties(mizuki.builder mizuki.site mizuki.markdown_features mizuki.reading mizuki.theme mizuki.theme_data mizuki.front_matter mizuki.docs mizuki.publication mizuki.listings mizuki.scaffold mizuki.output_transaction mizuki.core PROPERTIES TIMEOUT 60)
set_tests_properties(mizuki.dev PROPERTIES TIMEOUT 120)
set_tests_properties(mizuki.builder mizuki.site mizuki.markdown_features mizuki.reading mizuki.theme mizuki.theme_data mizuki.front_matter mizuki.docs mizuki.dev mizuki.publication mizuki.listings mizuki.scaffold PROPERTIES ENVIRONMENT "PYTHONUTF8=1")

if(MIZUKI_BUILD_DISTRIBUTION)
    find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
    add_test(NAME mizuki.package
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_package.py"
                --build-dir "${PROJECT_BINARY_DIR}" --version "${PROJECT_VERSION}"
                --config "$<CONFIG>")
    set_tests_properties(mizuki.package PROPERTIES TIMEOUT 90
        RESOURCE_LOCK mizuki.packaging ENVIRONMENT "PYTHONUTF8=1")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME mizuki.source_package
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_package.py"
                    --build-dir "${PROJECT_BINARY_DIR}" --version "${PROJECT_VERSION}"
                    --config "$<CONFIG>" --source)
        set_tests_properties(mizuki.source_package PROPERTIES TIMEOUT 600
            RESOURCE_LOCK mizuki.packaging ENVIRONMENT "PYTHONUTF8=1")
    endif()
endif()
