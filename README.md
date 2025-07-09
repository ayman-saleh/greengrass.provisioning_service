# AWS Greengrass Provisioning Service

A C++ service that automates the provisioning of AWS Greengrass devices by reading configuration from a database and setting up Greengrass Core software.

## Features

- **Automatic Provisioning**: Reads device configuration from SQLite database and provisions Greengrass
- **Internet Connectivity Check**: Validates connectivity before attempting provisioning
- **Status Reporting**: Writes status to a JSON file for monitoring
- **Idempotent**: Safe to run multiple times - detects if already provisioned
- **Systemd Integration**: Runs as a systemd service on Linux

## Prerequisites

- Linux system (Ubuntu 20.04+ or similar)
- C++17 compatible compiler
- CMake 3.16+
- Conan 2.0+ package manager
- Java Runtime Environment (for Greengrass)
- sudo privileges (for service installation)

## Building

1. Install Conan 2.0:
```bash
pip install conan
```

2. Install dependencies and build:
```bash
mkdir build && cd build
conan install .. --build=missing
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

3. Run tests:
```bash
make test
```

## Installation

1. Install the binary:
```bash
sudo make install
```

2. Create the database with device configurations (see Database Schema below)

3. Enable and start the service:
```bash
sudo systemctl enable greengrass-provisioning.service
sudo systemctl start greengrass-provisioning.service
```

## Usage

### Command Line
```bash
greengrass_provisioning_service -d /path/to/database.db -g /greengrass/v2 [-s /var/run/status.json] [-v]
```

Arguments:
- `-d, --database-path`: Path to SQLite database with device configurations (required)
- `-g, --greengrass-path`: Path where Greengrass will be installed (required)
- `-s, --status-file`: Path to status file (default: /var/run/greengrass-provisioning.status)
- `-v, --verbose`: Enable verbose logging

### As a Service

The service is configured via `/etc/systemd/system/greengrass-provisioning.service`. 
Edit this file to change database path or Greengrass installation directory.

## Database Schema

The SQLite database should have the following tables:

### device_config table
```sql
CREATE TABLE device_config (
    device_id TEXT PRIMARY KEY,
    thing_name TEXT NOT NULL,
    iot_endpoint TEXT NOT NULL,
    aws_region TEXT NOT NULL,
    root_ca_path TEXT NOT NULL,
    certificate_pem TEXT NOT NULL,
    private_key_pem TEXT NOT NULL,
    role_alias TEXT NOT NULL,
    role_alias_endpoint TEXT NOT NULL,
    nucleus_version TEXT,
    deployment_group TEXT,
    initial_components TEXT,
    proxy_url TEXT,
    mqtt_port INTEGER,
    custom_domain TEXT
);
```

### device_identifiers table (optional)
```sql
CREATE TABLE device_identifiers (
    device_id TEXT NOT NULL,
    mac_address TEXT,
    serial_number TEXT,
    FOREIGN KEY (device_id) REFERENCES device_config(device_id)
);
```

## Status File

The service writes a JSON status file with the following structure:
```json
{
    "status": "COMPLETED",
    "message": "Provisioning completed successfully",
    "timestamp": "2024-01-15T10:30:45Z",
    "progress_percentage": 100,
    "error_details": ""
}
```

Status values:
- `STARTING`: Service is initializing
- `CHECKING_PROVISIONING`: Checking if already provisioned
- `ALREADY_PROVISIONED`: Device is already provisioned
- `CHECKING_CONNECTIVITY`: Verifying internet connectivity
- `NO_CONNECTIVITY`: No internet connection available
- `READING_DATABASE`: Reading configuration from database
- `GENERATING_CONFIG`: Creating Greengrass configuration files
- `PROVISIONING`: Installing and configuring Greengrass
- `COMPLETED`: Provisioning successful
- `ERROR`: An error occurred (see error_details)

## Directory Structure

After provisioning, the Greengrass directory will contain:
```
/greengrass/v2/
├── config/
│   └── config.yaml
├── certs/
│   ├── <thing-name>.cert.pem
│   ├── <thing-name>.private.key
│   └── root.ca.pem
├── logs/
├── work/
├── packages/
├── deployments/
└── ggc-root/
```

## Logging

Logs are written to:
- Console output (when run manually)
- systemd journal (when run as service): `journalctl -u greengrass-provisioning`
- File: `/var/log/greengrass-provisioning.log`

## Security Considerations

- Private keys are written with 600 permissions (owner read/write only)
- The service requires root privileges to:
  - Create system users
  - Configure systemd services
  - Set file permissions
- Database should be protected with appropriate file permissions
- Consider encrypting sensitive data in the database

## Troubleshooting

1. **Service fails to start**: Check logs with `journalctl -u greengrass-provisioning -e`
2. **Database connection errors**: Verify database path and permissions
3. **No connectivity**: Ensure the device has internet access and can reach AWS endpoints
4. **Already provisioned**: Check `/greengrass/v2/config/config.yaml` exists
5. **Permission errors**: Ensure service runs as root or with appropriate sudo privileges

## Unit Tests

The project includes comprehensive unit tests for all major components using Google Test framework.

### Running Unit Tests

```bash
# Quick method - using the test runner script
./scripts/run-tests.sh

# Or manually with CMake
mkdir build-tests
cd build-tests
conan install .. --build=missing -s build_type=Debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
make -j$(nproc)
ctest --output-on-failure
```

### Test Coverage

Unit tests are provided for the following components:
- **ArgumentParser**: Command-line argument parsing and validation
- **ConfigDatabase**: SQLite database operations and device configuration retrieval
- **ConnectivityChecker**: Network connectivity and DNS resolution tests
- **ProvisioningChecker**: Greengrass installation detection and validation
- **ConfigGenerator**: Configuration file generation and certificate handling
- **StatusReporter**: Status file writing and atomic updates

### Test Structure

```
tests/
├── test_main.cpp                    # Test runner main
├── argument_parser_test.cpp         # CLI argument parsing tests
├── config_database_test.cpp         # Database operations tests
├── connectivity_checker_test.cpp    # Network connectivity tests
├── provisioning_checker_test.cpp    # Provisioning detection tests
├── config_generator_test.cpp        # Config generation tests
├── status_reporter_test.cpp         # Status reporting tests
└── CMakeLists.txt                  # Test build configuration
```

### Coverage Report

If `lcov` and `genhtml` are installed, the test runner script will automatically generate a code coverage report:

```bash
./scripts/run-tests.sh
# Coverage report will be generated in build-tests/coverage_report/index.html
```

### Current Test Status

- Total: 94 tests across 7 test suites
- Status: All tests passing ✓
- Coverage: 37.1% line coverage, 50.1% function coverage
- Build: Fully integrated with CMake and Conan 2.0

## Docker Integration Tests

The project includes comprehensive Docker-based integration tests that simulate the complete provisioning cycle in an isolated environment.

### Running Docker Tests

```bash
# Quick start - run all integration tests
./run-docker-tests.sh

# Or manually with docker compose
docker compose -f docker-compose.test.yml build
docker compose -f docker-compose.test.yml run --rm greengrass-provisioning
```

### Test Environment

The Docker tests include:
- Mock AWS IoT endpoints (using MockServer)
- Mock S3 storage (using MinIO)
- Test SQLite database with sample configurations
- Automated test scripts (Bash and Python/pytest)

### Test Coverage

- Full provisioning cycle from scratch
- Idempotency verification
- Error handling scenarios
- Status file monitoring
- Certificate permission validation
- Network connectivity checks

See [docker/README.md](docker/README.md) for detailed documentation on the Docker integration tests.

## Real AWS Environment Testing

For testing with real AWS resources, use the dedicated real test environment:

### Prerequisites

- Pre-created IoT Thing with certificates and policies
- IAM Role and Role Alias for Greengrass
- AWS credentials with appropriate permissions

### Quick Start

```bash
# Copy and configure environment file
cp docker/real-test/env.example .env
# Edit .env with your AWS configuration

# Run the real AWS test
./run-docker-real-test.sh

# Interactive debugging
./run-docker-real-test.sh --interactive

# Force cleanup
./run-docker-real-test.sh --cleanup
```

### Features

- Validates provisioning with real AWS IoT Core
- Tests actual Greengrass Core Device registration
- Verifies connectivity to AWS services
- Automatic cleanup of AWS resources
- Support for CI/CD integration

See [docker/real-test/README.md](docker/real-test/README.md) for detailed configuration and usage instructions.

## License

This project is proprietary software. All rights reserved. 