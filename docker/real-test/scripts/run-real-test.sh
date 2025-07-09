#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Load environment variables
if [ -f /workspace/.env ]; then
    log_info "Loading environment variables from .env file..."
    export $(cat /workspace/.env | grep -v '^#' | xargs)
else
    log_warning "No .env file found, using environment variables from Docker..."
fi

# Validate required environment variables
required_vars=(
    "AWS_DEFAULT_REGION"
    "IOT_THING_NAME"
    "IOT_ENDPOINT"
    "IOT_ROLE_ALIAS"
    "IOT_ROLE_ALIAS_ENDPOINT"
    "DEVICE_ID"
)

for var in "${required_vars[@]}"; do
    if [ -z "${!var}" ]; then
        log_error "Required environment variable $var is not set!"
        exit 1
    fi
done

log_info "=== Starting Greengrass Provisioning Test ==="
log_info "Thing Name: $IOT_THING_NAME"
log_info "Region: $AWS_DEFAULT_REGION"
log_info "Device ID: $DEVICE_ID"

# Step 1: Build the provisioning service
log_info "Building the provisioning service..."
cd /workspace
mkdir -p build
cd build
conan install .. --build=missing -s build_type=Release
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
log_success "Build completed successfully"

# Step 2: Create test database
log_info "Creating test database..."
/scripts/setup-database.sh
log_success "Database created successfully"

# Step 3: Run the provisioning service
log_info "Running provisioning service..."
cd /workspace
./build/src/greengrass_provisioning_service \
    --database-path "$DB_PATH" \
    --greengrass-path "$GREENGRASS_ROOT" \
    --status-file /var/run/greengrass-provisioning.status \
    $([ "$VERBOSE_LOGGING" = "true" ] && echo "--verbose")

# Check status
if [ -f /var/run/greengrass-provisioning.status ]; then
    status=$(jq -r '.status' /var/run/greengrass-provisioning.status)
    message=$(jq -r '.message' /var/run/greengrass-provisioning.status)
    
    log_info "Provisioning status: $status"
    log_info "Message: $message"
    
    if [ "$status" != "COMPLETED" ] && [ "$status" != "ALREADY_PROVISIONED" ]; then
        log_error "Provisioning failed!"
        if [ -f /var/run/greengrass-provisioning.status ]; then
            cat /var/run/greengrass-provisioning.status
        fi
        exit 1
    fi
else
    log_error "Status file not found!"
    exit 1
fi

log_success "Provisioning completed successfully!"

# Step 4: Verify Greengrass installation
log_info "Verifying Greengrass installation..."
/scripts/verify-provisioning.sh

# Step 5: Install and start Greengrass (if not already running)
log_info "Installing Greengrass as a system service..."
if [ -f "$GREENGRASS_ROOT/bin/greengrass.service" ]; then
    log_info "Greengrass service file found, installing..."
    sudo cp "$GREENGRASS_ROOT/bin/greengrass.service" /etc/systemd/system/
    sudo systemctl daemon-reload
    sudo systemctl enable greengrass.service
    
    # Start Greengrass
    log_info "Starting Greengrass service..."
    sudo systemctl start greengrass.service
    
    # Wait for Greengrass to start
    sleep 10
    
    # Check status
    if sudo systemctl is-active --quiet greengrass.service; then
        log_success "Greengrass service is running"
        sudo systemctl status greengrass.service --no-pager || true
    else
        log_error "Greengrass service failed to start"
        sudo journalctl -u greengrass.service --no-pager -n 50
        exit 1
    fi
else
    log_warning "Greengrass service file not found, running manually..."
    # Run Greengrass manually
    cd "$GREENGRASS_ROOT"
    sudo -E java -Droot="$GREENGRASS_ROOT" -jar "$GREENGRASS_ROOT/lib/Greengrass.jar" &
    GREENGRASS_PID=$!
    sleep 10
fi

# Step 6: Test Greengrass connectivity
log_info "Testing Greengrass connectivity..."
if [ -f "$GREENGRASS_ROOT/bin/greengrass-cli" ]; then
    # List components
    log_info "Listing installed components..."
    sudo "$GREENGRASS_ROOT/bin/greengrass-cli" component list || true
    
    # Check health
    log_info "Checking Greengrass health..."
    sudo "$GREENGRASS_ROOT/bin/greengrass-cli" get-health || true
fi

# Step 7: Cleanup (if requested)
if [ "$CLEANUP_ON_SUCCESS" = "true" ]; then
    log_info "Cleanup requested, removing AWS resources..."
    /scripts/cleanup-aws.sh
fi

log_success "=== Greengrass Provisioning Test Completed Successfully ==="
exit 0 