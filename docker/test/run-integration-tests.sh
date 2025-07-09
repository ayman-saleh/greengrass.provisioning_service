#!/bin/bash

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}Starting Greengrass Provisioning Integration Tests${NC}"

# Test environment variables
export TEST_DATABASE="/opt/greengrass/config/devices.db"
export TEST_GREENGRASS_PATH="/greengrass/v2"
export TEST_STATUS_FILE="/test-results/provisioning.status"

# Create test results directory
mkdir -p /test-results

# Function to check service status
check_status() {
    if [ -f "$TEST_STATUS_FILE" ]; then
        cat "$TEST_STATUS_FILE" | jq -r '.status'
    else
        echo "NO_FILE"
    fi
}

# Function to wait for status
wait_for_status() {
    local expected_status=$1
    local timeout=$2
    local elapsed=0
    
    echo -e "${YELLOW}Waiting for status: $expected_status${NC}"
    
    while [ $elapsed -lt $timeout ]; do
        current_status=$(check_status)
        if [ "$current_status" = "$expected_status" ]; then
            echo -e "${GREEN}Status reached: $expected_status${NC}"
            return 0
        fi
        
        if [ "$current_status" = "ERROR" ]; then
            echo -e "${RED}Error status detected!${NC}"
            cat "$TEST_STATUS_FILE" | jq '.'
            return 1
        fi
        
        sleep 1
        elapsed=$((elapsed + 1))
    done
    
    echo -e "${RED}Timeout waiting for status: $expected_status${NC}"
    echo "Current status: $(check_status)"
    return 1
}

# Test 1: Check binary exists and shows help
echo -e "\n${YELLOW}Test 1: Binary availability${NC}"
if /usr/local/bin/greengrass_provisioning_service --help > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Binary is available and responds to --help${NC}"
else
    echo -e "${RED}✗ Binary not found or --help failed${NC}"
    exit 1
fi

# Test 2: Check database connectivity
echo -e "\n${YELLOW}Test 2: Database connectivity${NC}"
if sqlite3 "$TEST_DATABASE" "SELECT COUNT(*) FROM device_config;" > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Database is accessible${NC}"
    device_count=$(sqlite3 "$TEST_DATABASE" "SELECT COUNT(*) FROM device_config;")
    echo "  Found $device_count device configurations"
else
    echo -e "${RED}✗ Cannot access database${NC}"
    exit 1
fi

# Test 3: Check network connectivity to mock services
echo -e "\n${YELLOW}Test 3: Mock service connectivity${NC}"

# Wait for mock services to be ready
sleep 5

if nc -z mock-aws-iot 1080; then
    echo -e "${GREEN}✓ Mock AWS IoT is reachable${NC}"
else
    echo -e "${RED}✗ Cannot reach Mock AWS IoT${NC}"
    exit 1
fi

if nc -z mock-s3 9000; then
    echo -e "${GREEN}✓ Mock S3 is reachable${NC}"
else
    echo -e "${RED}✗ Cannot reach Mock S3${NC}"
    exit 1
fi

# Test 4: Run provisioning service
echo -e "\n${YELLOW}Test 4: Running provisioning service${NC}"

# Clean any previous runs
rm -rf "$TEST_GREENGRASS_PATH"/*
rm -f "$TEST_STATUS_FILE"

# Run the provisioning service
echo "Executing: greengrass_provisioning_service -d $TEST_DATABASE -g $TEST_GREENGRASS_PATH -s $TEST_STATUS_FILE -v"
/usr/local/bin/greengrass_provisioning_service \
    -d "$TEST_DATABASE" \
    -g "$TEST_GREENGRASS_PATH" \
    -s "$TEST_STATUS_FILE" \
    -v &

PROVISIONING_PID=$!

# Monitor the provisioning process
echo -e "${YELLOW}Monitoring provisioning process (PID: $PROVISIONING_PID)${NC}"

# Check various states
if wait_for_status "CHECKING_PROVISIONING" 10; then
    echo -e "${GREEN}✓ Provisioning check started${NC}"
fi

if wait_for_status "CHECKING_CONNECTIVITY" 20; then
    echo -e "${GREEN}✓ Connectivity check started${NC}"
fi

if wait_for_status "READING_DATABASE" 30; then
    echo -e "${GREEN}✓ Database reading started${NC}"
fi

if wait_for_status "GENERATING_CONFIG" 40; then
    echo -e "${GREEN}✓ Config generation started${NC}"
fi

# Wait for completion or error
wait_for_status "COMPLETED" 120 || wait_for_status "ALREADY_PROVISIONED" 120

# Wait for process to finish
wait $PROVISIONING_PID
PROVISIONING_EXIT_CODE=$?

echo -e "\n${YELLOW}Provisioning process finished with exit code: $PROVISIONING_EXIT_CODE${NC}"

# Test 5: Verify results
echo -e "\n${YELLOW}Test 5: Verifying provisioning results${NC}"

# Check final status
final_status=$(check_status)
echo "Final status: $final_status"

if [ "$final_status" = "COMPLETED" ] || [ "$final_status" = "ALREADY_PROVISIONED" ]; then
    echo -e "${GREEN}✓ Provisioning completed successfully${NC}"
else
    echo -e "${RED}✗ Provisioning did not complete successfully${NC}"
    cat "$TEST_STATUS_FILE" | jq '.'
    exit 1
fi

# Check if configuration files were created
if [ -f "$TEST_GREENGRASS_PATH/config/config.yaml" ]; then
    echo -e "${GREEN}✓ Config file created${NC}"
else
    echo -e "${RED}✗ Config file not found${NC}"
    exit 1
fi

# Check if certificates were created
if ls "$TEST_GREENGRASS_PATH/certs"/*.pem > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Certificates created${NC}"
    ls -la "$TEST_GREENGRASS_PATH/certs/"
else
    echo -e "${RED}✗ Certificates not found${NC}"
    exit 1
fi

# Test 6: Run provisioning again to test idempotency
echo -e "\n${YELLOW}Test 6: Testing idempotency${NC}"

rm -f "$TEST_STATUS_FILE"

/usr/local/bin/greengrass_provisioning_service \
    -d "$TEST_DATABASE" \
    -g "$TEST_GREENGRASS_PATH" \
    -s "$TEST_STATUS_FILE" \
    -v

second_run_exit_code=$?
second_run_status=$(check_status)

if [ "$second_run_status" = "ALREADY_PROVISIONED" ] && [ $second_run_exit_code -eq 0 ]; then
    echo -e "${GREEN}✓ Idempotency check passed${NC}"
else
    echo -e "${RED}✗ Idempotency check failed${NC}"
    echo "Expected status: ALREADY_PROVISIONED, got: $second_run_status"
    exit 1
fi

# Save test results
echo -e "\n${YELLOW}Saving test results${NC}"
cat > /test-results/test-summary.json <<EOF
{
  "test_run": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "tests_passed": 6,
  "tests_failed": 0,
  "provisioning_exit_code": $PROVISIONING_EXIT_CODE,
  "final_status": "$final_status",
  "idempotency_check": "passed",
  "files_created": {
    "config": $([ -f "$TEST_GREENGRASS_PATH/config/config.yaml" ] && echo "true" || echo "false"),
    "certificates": $(ls "$TEST_GREENGRASS_PATH/certs"/*.pem 2>/dev/null | wc -l)
  }
}
EOF

echo -e "\n${GREEN}All integration tests passed!${NC}"
exit 0 