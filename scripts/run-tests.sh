#!/bin/bash
# Script to build and run unit tests for Greengrass Provisioning Service

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo -e "${GREEN}Building and running unit tests for Greengrass Provisioning Service${NC}"
echo "=========================================="

# Change to project root
cd "$PROJECT_ROOT"

# Create build directory
BUILD_DIR="build-tests"
if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Cleaning existing build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake using Conan
echo -e "${GREEN}Installing dependencies with Conan...${NC}"
conan install .. --build=missing -s build_type=Debug

# Build the project
echo -e "${GREEN}Building project with CMake...${NC}"
# Check CMake version to use presets if available
CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d'.' -f1)
CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d'.' -f2)

if [ "$CMAKE_MAJOR" -gt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -ge 23 ]); then
    echo "Using CMake presets (CMake $CMAKE_VERSION)"
    cmake --preset conan-debug
else
    echo "Using traditional CMake configuration (CMake $CMAKE_VERSION)"
    cmake .. -G "Unix Makefiles" \
        -DCMAKE_TOOLCHAIN_FILE=build/Debug/generators/conan_toolchain.cmake \
        -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_TESTS=ON
fi

cmake --build . -j$(nproc)

# Run tests
echo -e "${GREEN}Running unit tests...${NC}"
echo "=========================================="

# Run tests with colored output
if command -v ctest &> /dev/null; then
    ctest --output-on-failure --verbose
    TEST_RESULT=$?
else
    # Fallback to direct execution
    ./tests/greengrass_provisioning_tests
    TEST_RESULT=$?
fi

# Report results
echo "=========================================="
if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi

# Optional: Generate coverage report if lcov is installed
if command -v lcov &> /dev/null && command -v genhtml &> /dev/null; then
    echo -e "${YELLOW}Generating coverage report...${NC}"
    
    # Add coverage flags and rebuild
    cd "$PROJECT_ROOT"
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    conan install .. --build=missing -s build_type=Debug
    
    if [ "$CMAKE_MAJOR" -gt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -ge 23 ]); then
        # For CMake >= 3.23, modify the preset to add coverage flags
        # This would require modifying the CMakeUserPresets.json
        cmake --preset conan-debug -DCMAKE_CXX_FLAGS="--coverage" -DCMAKE_EXE_LINKER_FLAGS="--coverage"
    else
        cmake .. -G "Unix Makefiles" \
            -DCMAKE_TOOLCHAIN_FILE=build/Debug/generators/conan_toolchain.cmake \
            -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
            -DCMAKE_BUILD_TYPE=Debug \
            -DBUILD_TESTS=ON \
            -DCMAKE_CXX_FLAGS="--coverage" \
            -DCMAKE_EXE_LINKER_FLAGS="--coverage"
    fi
    
    cmake --build . -j$(nproc)
    
    # Run tests again for coverage
    ./tests/greengrass_provisioning_tests
    
    # Generate coverage report
    lcov --capture --directory . --output-file coverage.info
    lcov --remove coverage.info '/usr/*' '*/tests/*' '*/conan/*' --output-file coverage.info
    genhtml coverage.info --output-directory coverage_report
    
    echo -e "${GREEN}Coverage report generated in: $BUILD_DIR/coverage_report/index.html${NC}"
fi 