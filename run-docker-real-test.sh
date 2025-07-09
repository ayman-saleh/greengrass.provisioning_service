#!/bin/bash

# Wrapper script for running real AWS Greengrass provisioning test

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Print banner
echo -e "${BLUE}=================================${NC}"
echo -e "${BLUE}  Greengrass Real AWS Test Runner${NC}"
echo -e "${BLUE}=================================${NC}"
echo

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo -e "${RED}Error: Docker is not installed${NC}"
    exit 1
fi

# Check if Docker Compose is installed
if ! docker compose version &> /dev/null 2>&1; then
    echo -e "${RED}Error: Docker Compose is not installed${NC}"
    exit 1
fi

# Check for .env file
if [ ! -f ".env" ]; then
    echo -e "${YELLOW}Warning: No .env file found${NC}"
    echo "You can create one from the example:"
    echo "  cp docker/real-test/env.example .env"
    echo "  # Edit .env with your AWS configuration"
    echo
    echo "Or provide environment variables directly."
    echo
fi

# Parse command line arguments
INTERACTIVE=false
CLEANUP_FORCE=false
BUILD_ONLY=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -i|--interactive)
            INTERACTIVE=true
            shift
            ;;
        --cleanup)
            CLEANUP_FORCE=true
            shift
            ;;
        --build-only)
            BUILD_ONLY=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -i, --interactive    Run container interactively with bash"
            echo "  --cleanup           Force cleanup of AWS resources"
            echo "  --build-only        Build the container without running tests"
            echo "  -h, --help          Show this help message"
            echo ""
            echo "Environment variables are read from .env file or shell"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Create test output directory
mkdir -p test-output

# Build the container
echo -e "${BLUE}Building Docker container...${NC}"
docker compose -f docker-compose.real.yml build

if [ "$BUILD_ONLY" = true ]; then
    echo -e "${GREEN}Build completed successfully!${NC}"
    exit 0
fi

# Run the appropriate command
if [ "$CLEANUP_FORCE" = true ]; then
    echo -e "${YELLOW}Running cleanup...${NC}"
    docker compose -f docker-compose.real.yml run --rm greengrass-real-test /scripts/cleanup-aws.sh --force
elif [ "$INTERACTIVE" = true ]; then
    echo -e "${YELLOW}Starting interactive session...${NC}"
    echo -e "${YELLOW}Run '/scripts/run-real-test.sh' to start the test${NC}"
    docker compose -f docker-compose.real.yml run --rm greengrass-real-test /bin/bash
else
    echo -e "${BLUE}Running Greengrass provisioning test...${NC}"
    docker compose -f docker-compose.real.yml up --abort-on-container-exit
    EXIT_CODE=$?
    
    # Clean up containers
    docker compose -f docker-compose.real.yml down
    
    if [ $EXIT_CODE -eq 0 ]; then
        echo -e "${GREEN}Test completed successfully!${NC}"
        echo -e "Logs are available in: ./test-output/"
    else
        echo -e "${RED}Test failed with exit code: $EXIT_CODE${NC}"
        echo -e "Check logs in: ./test-output/"
        exit $EXIT_CODE
    fi
fi 