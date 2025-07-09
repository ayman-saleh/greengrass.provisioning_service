# Docker Integration Tests

This directory contains Docker-based integration tests for the Greengrass Provisioning Service that simulate a complete provisioning cycle in an isolated environment.

## Overview

The integration tests use Docker Compose to create a complete test environment including:

- Mock AWS IoT endpoints
- Mock S3 storage (using MinIO)
- Test SQLite database with sample device configurations
- The provisioning service running in a controlled environment

## Test Components

### 1. Mock Services

- **Mock AWS IoT** (`mock-aws-iot`): Uses MockServer to simulate AWS IoT endpoints
  - Responds to connectivity checks
  - Provides mock credentials endpoint
  - Simulates device shadow operations

- **Mock S3** (`mock-s3`): Uses MinIO to simulate S3 for Greengrass artifacts
  - Stores mock Greengrass nucleus JAR
  - Provides artifact download capability

### 2. Test Database

The test database (`test-data/devices.db`) contains:
- Sample device configurations
- Test certificates and keys
- Device identifier mappings (MAC addresses)

### 3. Test Scripts

- **Bash Integration Test** (`test/run-integration-tests.sh`):
  - Validates binary availability
  - Checks database connectivity
  - Verifies mock service availability
  - Runs full provisioning cycle
  - Tests idempotency
  - Validates generated files

- **Python Integration Test** (`test/integration_tests.py`):
  - Comprehensive pytest-based tests
  - Status file monitoring
  - Certificate permission validation
  - Error handling tests

## Running the Tests

### Prerequisites

- Docker with Docker Compose plugin installed (Docker 20.10.0+)
- SQLite3 command-line tool (for database creation)
- jq (for JSON parsing in test results)

### Quick Start

From the project root directory:

```bash
./run-docker-tests.sh
```

This script will:
1. Build the test database
2. Build Docker images
3. Start mock services
4. Run integration tests
5. Display test results
6. Clean up containers

### Manual Testing

To run specific test scenarios:

```bash
# Build images
docker compose -f docker-compose.test.yml build

# Start mock services only
docker compose -f docker-compose.test.yml up -d mock-aws-iot mock-s3

# Run integration tests
docker compose -f docker-compose.test.yml run --rm greengrass-provisioning

# Run Python tests
docker compose -f docker-compose.test.yml run --rm greengrass-provisioning python3 -m pytest /test/integration_tests.py -v

# View logs
docker compose -f docker-compose.test.yml logs

# Clean up
docker compose -f docker-compose.test.yml down
```

## Test Scenarios

### 1. Full Provisioning Cycle
- Service starts with empty Greengrass directory
- Checks connectivity to AWS endpoints
- Reads device config from database
- Generates configuration files
- Creates certificates with proper permissions
- Completes provisioning successfully

### 2. Idempotency Test
- Runs provisioning twice
- Second run detects existing installation
- Returns "ALREADY_PROVISIONED" status
- No files are modified

### 3. Error Handling
- Tests with invalid database path
- Tests with unreachable endpoints
- Validates error reporting in status file

### 4. Status Monitoring
- Tracks progression through all states
- Validates status file format
- Checks progress percentage updates

## Test Data

### Sample Device Configuration

The test database includes a device with:
- Device ID: `test-device-001`
- Thing Name: `TestGreengrassThing`
- Mock certificates and keys
- IoT endpoint pointing to mock service

### Expected Results

After successful provisioning:
```
/greengrass/v2/
├── config/
│   └── config.yaml
├── certs/
│   ├── TestGreengrassThing.cert.pem
│   ├── TestGreengrassThing.private.key
│   └── root.ca.pem
└── [other Greengrass directories]
```

## Debugging

### View Test Results
```bash
cat docker/test-results/test-summary.json | jq '.'
```

### Check Status File
```bash
cat docker/test-results/provisioning.status | jq '.'
```

### Container Shell Access
```bash
docker compose -f docker-compose.test.yml run --rm greengrass-provisioning /bin/bash
```

### Mock Service Logs
```bash
docker compose -f docker-compose.test.yml logs mock-aws-iot
docker compose -f docker-compose.test.yml logs mock-s3
```

## Extending Tests

### Adding New Test Cases

1. Add test methods to `integration_tests.py`
2. Update `run-integration-tests.sh` for bash tests
3. Add new device configurations to `init-db.sql`

### Customizing Mock Responses

Edit `mock-aws/aws-iot-expectations.json` to add new mock endpoints or modify responses.

### Testing Different Scenarios

Modify environment variables in `docker-compose.test.yml`:
- `AWS_REGION`: Change target region
- `IOT_ENDPOINT`: Point to different mock endpoint
- `TEST_MODE`: Enable test-specific behavior

## CI/CD Integration

The Docker tests can be integrated into CI/CD pipelines:

```yaml
# Example GitHub Actions
- name: Run Docker Integration Tests
  run: |
    ./run-docker-tests.sh
  
- name: Upload Test Results
  if: always()
  uses: actions/upload-artifact@v2
  with:
    name: test-results
    path: docker/test-results/
```

## Troubleshooting

### Common Issues

1. **Port conflicts**: Ensure ports 8443 and 9000 are available
2. **Database creation fails**: Install sqlite3 package
3. **Permission errors**: Run with appropriate Docker permissions
4. **Build fails**: Check Docker daemon is running

### Clean State

To completely reset the test environment:
```bash
docker compose -f docker-compose.test.yml down -v
rm -rf docker/test-results/*
rm -f docker/test-data/devices.db
``` 