#!/bin/bash

# Script to run Docker integration tests for Greengrass Provisioning Service

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== Greengrass Provisioning Service - Docker Integration Tests ===${NC}"

# Check Docker is installed
if ! command -v docker &> /dev/null; then
    echo -e "${RED}Docker is not installed. Please install Docker first.${NC}"
    exit 1
fi

# Check Docker Compose is installed
if ! command -v docker compose &> /dev/null; then
    echo -e "${RED}Docker Compose is not installed. Please install Docker Compose first.${NC}"
    exit 1
fi

# Clean up previous test results
echo -e "${YELLOW}Cleaning up previous test results...${NC}"
rm -rf docker/test-results/*
mkdir -p docker/test-results

# Build test database
echo -e "${YELLOW}Creating test database...${NC}"
if [ ! -f "docker/test-data/devices.db" ]; then
    sqlite3 docker/test-data/devices.db < docker/test-data/init-db.sql
    echo -e "${GREEN}✓ Test database created${NC}"
else
    echo -e "${GREEN}✓ Test database already exists${NC}"
fi

# Create mock Greengrass JAR (placeholder)
echo -e "${YELLOW}Creating mock Greengrass artifacts...${NC}"
mkdir -p docker/mock-aws/s3-data/greengrass
echo "Mock Greengrass JAR" > docker/mock-aws/s3-data/greengrass/greengrass-2.9.0.zip

# Build Docker images
echo -e "${YELLOW}Building Docker images...${NC}"
docker compose -f docker-compose.test.yml build

# Start services
echo -e "${YELLOW}Starting test services...${NC}"
docker compose -f docker-compose.test.yml up -d mock-aws-iot mock-s3

# Wait for services to be ready
echo -e "${YELLOW}Waiting for mock services to be ready...${NC}"
sleep 10

# Run integration tests
echo -e "${YELLOW}Running integration tests...${NC}"
docker compose -f docker-compose.test.yml run --rm greengrass-provisioning

# Check test results
if [ -f "docker/test-results/test-summary.json" ]; then
    echo -e "\n${GREEN}Test Summary:${NC}"
    cat docker/test-results/test-summary.json | jq '.'
    
    # Check if all tests passed
    tests_failed=$(cat docker/test-results/test-summary.json | jq -r '.tests_failed')
    if [ "$tests_failed" = "0" ]; then
        echo -e "\n${GREEN}✓ All integration tests passed!${NC}"
        exit_code=0
    else
        echo -e "\n${RED}✗ Some tests failed!${NC}"
        exit_code=1
    fi
else
    echo -e "\n${RED}✗ No test results found!${NC}"
    exit_code=1
fi

# Clean up
echo -e "\n${YELLOW}Cleaning up Docker containers...${NC}"
docker compose -f docker-compose.test.yml down

# Show logs if tests failed
if [ $exit_code -ne 0 ]; then
    echo -e "\n${YELLOW}Container logs:${NC}"
    docker compose -f docker-compose.test.yml logs
fi

exit $exit_code 