#!/bin/bash

# Build script for Greengrass Provisioning Service

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building Greengrass Provisioning Service...${NC}"

# Check for required tools
command -v cmake >/dev/null 2>&1 || { echo -e "${RED}CMake is required but not installed.${NC}" >&2; exit 1; }
command -v conan >/dev/null 2>&1 || { echo -e "${RED}Conan is required but not installed.${NC}" >&2; exit 1; }
command -v g++ >/dev/null 2>&1 || { echo -e "${RED}g++ is required but not installed.${NC}" >&2; exit 1; }

# Create build directory
mkdir -p build
cd build

# Run conan install
echo -e "${YELLOW}Installing dependencies with Conan...${NC}"
conan install .. --build=missing

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake

# Build
echo -e "${YELLOW}Building...${NC}"
make -j$(nproc)

echo -e "${GREEN}Build complete!${NC}"
echo -e "${GREEN}Binary located at: build/greengrass_provisioning_service${NC}"

# Ask if user wants to run tests
read -p "Run tests? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]
then
    echo -e "${YELLOW}Running tests...${NC}"
    make test
fi

# Ask if user wants to install
read -p "Install binary and service? (requires sudo) (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]
then
    echo -e "${YELLOW}Installing...${NC}"
    sudo make install
    echo -e "${GREEN}Installation complete!${NC}"
    echo -e "${GREEN}You can now enable the service with: sudo systemctl enable greengrass-provisioning.service${NC}"
fi 