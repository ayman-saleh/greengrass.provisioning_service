# Find GTest package
find_package(GTest REQUIRED)

# Test sources
set(TEST_SOURCES
    test_main.cpp
    argument_parser_test.cpp
    config_database_test.cpp
    connectivity_checker_test.cpp
    provisioning_checker_test.cpp
    config_generator_test.cpp
    status_reporter_test.cpp
)

# Create test executable
add_executable(greengrass_provisioning_tests ${TEST_SOURCES})

# Include directories
target_include_directories(greengrass_provisioning_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(greengrass_provisioning_tests PRIVATE
    greengrass_provisioning_lib  # Link against our static library
    GTest::gtest                 # Only gtest, not gtest_main
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    fmt::fmt
    CURL::libcurl
    SQLite::SQLite3
)

# Add test
enable_testing()
add_test(NAME greengrass_provisioning_tests COMMAND greengrass_provisioning_tests)

# Add custom target to run tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS greengrass_provisioning_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running unit tests..."
) 