# Real AWS Greengrass Provisioning Test

This Docker-based test runs the Greengrass provisioning service with real AWS resources to validate the complete provisioning workflow.

## Prerequisites

### AWS Resources Required

Before running this test, you must have the following AWS resources already created:

1. **IoT Thing** - A thing registered in AWS IoT Core
2. **Device Certificate** - X.509 certificate for the IoT Thing
3. **IoT Policy** - Policy attached to the certificate with appropriate permissions
4. **IAM Role** - For Greengrass token exchange
5. **Role Alias** - IoT Role Alias for credential provider

### Required IAM Permissions

The AWS credentials used must have permissions to:
- Read IoT Thing details
- Create/Delete Greengrass Core Devices
- Assume the Greengrass Token Exchange Role

### Local Requirements

- Docker with Docker Compose plugin (Docker 20.10.0+)
- AWS CLI configured (optional, if not providing credentials via environment)
- Certificate files or their base64-encoded content

## Configuration

### Method 1: Using .env File

1. Copy the example environment file:
```bash
cp docker/real-test/env.example .env
```

2. Edit `.env` with your AWS configuration:
```bash
# AWS Credentials
AWS_ACCESS_KEY_ID=your-access-key
AWS_SECRET_ACCESS_KEY=your-secret-key
AWS_DEFAULT_REGION=us-east-1

# IoT Thing Configuration
IOT_THING_NAME=my-greengrass-device
IOT_ENDPOINT=xxxxx.iot.us-east-1.amazonaws.com
IOT_ROLE_ALIAS=GreengrassCoreTokenExchangeRoleAlias
IOT_ROLE_ALIAS_ENDPOINT=xxxxx.credentials.iot.us-east-1.amazonaws.com

# Device Configuration
DEVICE_ID=my-device-001
```

### Method 2: Using Certificate Files

If you have certificate files locally:

1. Set the certificate paths in `.env`:
```bash
DEVICE_CERTIFICATE_PEM_PATH=/path/to/device.cert.pem
DEVICE_PRIVATE_KEY_PATH=/path/to/device.private.key
ROOT_CA_PATH=/path/to/AmazonRootCA1.pem
```

2. Mount the certificate directory:
```bash
CERTIFICATE_MOUNT_PATH=/path/to/certificates docker compose -f docker-compose.real.yml up
```

### Method 3: Using Base64-Encoded Certificates

For CI/CD environments, you can provide certificates as base64-encoded strings:

```bash
# Encode certificates
DEVICE_CERTIFICATE_PEM_BASE64=$(base64 -w0 < device.cert.pem)
DEVICE_PRIVATE_KEY_BASE64=$(base64 -w0 < device.private.key)
ROOT_CA_BASE64=$(base64 -w0 < AmazonRootCA1.pem)
```

## Running the Test

### Basic Run

```bash
# Run with default settings
docker compose -f docker-compose.real.yml up --build

# Run in detached mode
docker compose -f docker-compose.real.yml up --build -d

# View logs
docker compose -f docker-compose.real.yml logs -f
```

### Interactive Debugging

```bash
# Start container with shell access
docker compose -f docker-compose.real.yml run --rm greengrass-real-test /bin/bash

# Inside container, run test manually
/scripts/run-real-test.sh
```

### Cleanup Options

By default, the test will clean up AWS resources on success. Control this behavior with:

```bash
# Keep resources after successful test
CLEANUP_ON_SUCCESS=false docker compose -f docker-compose.real.yml up

# Clean up even on failure
CLEANUP_ON_FAILURE=true docker compose -f docker-compose.real.yml up
```

## Test Workflow

1. **Build** - Compiles the provisioning service
2. **Database Setup** - Creates SQLite database with device configuration
3. **Provisioning** - Runs the provisioning service
4. **Verification** - Checks installation files and permissions
5. **Greengrass Start** - Attempts to start Greengrass service
6. **Connectivity Test** - Verifies connection to AWS IoT
7. **Cleanup** - Removes Greengrass Core Device from AWS

## Output and Logs

Test outputs are saved to `./test-output/` directory:
- `provisioning.log` - Provisioning service output
- `greengrass.log` - Greengrass runtime logs
- `test-summary.json` - Test results summary

## Troubleshooting

### Common Issues

1. **Certificate Not Found**
   - Ensure certificate paths are correct
   - Check file permissions
   - Verify base64 encoding if using encoded format

2. **Connection Failed**
   - Verify IoT endpoint is correct
   - Check certificate is activated in AWS IoT
   - Ensure IoT policy allows required actions

3. **Provisioning Already Exists**
   - Run cleanup manually: `docker compose -f docker-compose.real.yml run --rm greengrass-real-test /scripts/cleanup-aws.sh --force`
   - Or set a different IOT_THING_NAME

4. **Permission Denied**
   - Ensure Docker is running with proper permissions
   - Container needs privileged mode for systemd

### Debug Mode

Enable verbose logging:
```bash
VERBOSE_LOGGING=true docker compose -f docker-compose.real.yml up
```

### Manual Cleanup

If automatic cleanup fails:

```bash
# Delete Greengrass Core Device
aws greengrassv2 delete-core-device \
    --core-device-thing-name YOUR_THING_NAME \
    --region YOUR_REGION

# Clean local files
docker compose -f docker-compose.real.yml run --rm greengrass-real-test \
    bash -c "rm -rf /greengrass/v2"
```

## Security Notes

- Never commit `.env` file with real credentials
- Use AWS IAM roles when possible
- Rotate certificates regularly
- Limit IAM permissions to minimum required
- Consider using AWS Secrets Manager for credentials in production

## CI/CD Integration

Example GitHub Actions workflow:

```yaml
- name: Run Greengrass Test
  env:
    AWS_ACCESS_KEY_ID: ${{ secrets.AWS_ACCESS_KEY_ID }}
    AWS_SECRET_ACCESS_KEY: ${{ secrets.AWS_SECRET_ACCESS_KEY }}
    IOT_THING_NAME: ci-test-device-${{ github.run_id }}
    DEVICE_CERTIFICATE_PEM_BASE64: ${{ secrets.DEVICE_CERT_BASE64 }}
    DEVICE_PRIVATE_KEY_BASE64: ${{ secrets.DEVICE_KEY_BASE64 }}
  run: |
    docker compose -f docker-compose.real.yml up --build
``` 