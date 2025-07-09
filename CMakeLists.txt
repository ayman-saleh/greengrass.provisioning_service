cmake_minimum_required(VERSION 3.16)
project(greengrass_provisioning_service VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Find required packages
find_package(CLI11 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)

# Library source files (without main.cpp)
set(LIB_SOURCES
    src/cli/argument_parser.cpp
    src/database/config_database.cpp
    src/networking/connectivity_checker.cpp
    src/provisioning/greengrass_provisioner.cpp
    src/provisioning/provisioning_checker.cpp
    src/config/config_generator.cpp
    src/status/status_reporter.cpp
)

# Create static library
add_library(greengrass_provisioning_lib STATIC ${LIB_SOURCES})

# Include directories for library
target_include_directories(greengrass_provisioning_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link libraries to static library
target_link_libraries(greengrass_provisioning_lib PUBLIC
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    fmt::fmt
    CURL::libcurl
    SQLite::SQLite3
)

# Create executable
add_executable(${PROJECT_NAME} src/main.cpp)

# Link the static library to the executable
target_link_libraries(${PROJECT_NAME} PRIVATE
    greengrass_provisioning_lib
)

# Set compiler warnings
if(MSVC)
    target_compile_options(greengrass_provisioning_lib PRIVATE /W4)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(greengrass_provisioning_lib PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install target
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Install systemd service file
install(FILES scripts/greengrass-provisioning.service
    DESTINATION /etc/systemd/system
    COMPONENT service
)

# Enable testing
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif() 