find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(NAME mizuki.builder
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_builder.py"
            "$<TARGET_FILE:mizuki>")

add_test(NAME mizuki.site
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_site.py"
            "$<TARGET_FILE:mizuki>")
add_test(NAME mizuki.dev
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_dev.py"
            "$<TARGET_FILE:mizuki>")

add_test(NAME mizuki.publication
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_publication.py"
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

set_tests_properties(mizuki.builder mizuki.site mizuki.publication mizuki.output_transaction mizuki.core PROPERTIES TIMEOUT 60)
set_tests_properties(mizuki.dev PROPERTIES TIMEOUT 120)
set_tests_properties(mizuki.builder mizuki.site mizuki.dev mizuki.publication PROPERTIES ENVIRONMENT "PYTHONUTF8=1")
