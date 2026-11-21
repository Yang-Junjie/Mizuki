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

add_executable(mizuki_output_transaction_tests
    output_transaction_test.cpp
    "${PROJECT_SOURCE_DIR}/src/build/output_transaction.cpp"
    "${PROJECT_SOURCE_DIR}/src/util/file.cpp")
target_include_directories(mizuki_output_transaction_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(mizuki_output_transaction_tests PRIVATE spdlog::spdlog)
add_test(NAME mizuki.output_transaction COMMAND mizuki_output_transaction_tests)

set_tests_properties(mizuki.builder mizuki.site mizuki.output_transaction PROPERTIES TIMEOUT 60)
set_tests_properties(mizuki.dev PROPERTIES TIMEOUT 120)
set_tests_properties(mizuki.builder mizuki.site mizuki.dev PROPERTIES ENVIRONMENT "PYTHONUTF8=1")
