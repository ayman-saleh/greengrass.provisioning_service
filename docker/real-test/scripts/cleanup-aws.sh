#!/bin/bash
set -e

# Colors for output
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}[CLEANUP]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[CLEANUP]${NC} $1"
}

log_error() {
    echo -e "${RED}[CLEANUP]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[CLEANUP]${NC} $1"
}

# Check if cleanup should be performed
if [ "$CLEANUP_ON_SUCCESS" != "true" ] && [ "$1" != "--force" ]; then
    log_warning "Cleanup not requested (CLEANUP_ON_SUCCESS=$CLEANUP_ON_SUCCESS)"
    log_info "To force cleanup, run: $0 --force"
    exit 0
fi

log_info "=== Starting AWS Resource Cleanup ==="

# Stop Greengrass service if running
log_info "Stopping Greengrass service..."
if systemctl is-active --quiet greengrass.service 2>/dev/null; then
    sudo systemctl stop greengrass.service || true
    sudo systemctl disable greengrass.service || true
    log_success "Greengrass service stopped"
else
    log_info "Greengrass service not running"
fi

# Kill any manual Greengrass process
if [ -n "$GREENGRASS_PID" ]; then
    log_info "Stopping manual Greengrass process..."
    sudo kill -TERM $GREENGRASS_PID 2>/dev/null || true
    sleep 5
fi

# Delete Greengrass Core Device from AWS IoT
log_info "Deleting Greengrass Core Device from AWS..."
if [ -n "$IOT_THING_NAME" ]; then
    # Get the core device status
    CORE_DEVICE_STATUS=$(aws greengrassv2 get-core-device \
        --core-device-thing-name "$IOT_THING_NAME" \
        --region "$AWS_DEFAULT_REGION" \
        --query 'status' \
        --output text 2>/dev/null || echo "NOT_FOUND")
    
    if [ "$CORE_DEVICE_STATUS" != "NOT_FOUND" ]; then
        log_info "Found Greengrass Core Device: $IOT_THING_NAME (Status: $CORE_DEVICE_STATUS)"
        
        # Delete the core device
        log_info "Deleting Greengrass Core Device..."
        aws greengrassv2 delete-core-device \
            --core-device-thing-name "$IOT_THING_NAME" \
            --region "$AWS_DEFAULT_REGION" || {
                log_error "Failed to delete core device"
            }
        
        # Wait for deletion to complete
        log_info "Waiting for core device deletion to complete..."
        for i in {1..30}; do
            CORE_DEVICE_STATUS=$(aws greengrassv2 get-core-device \
                --core-device-thing-name "$IOT_THING_NAME" \
                --region "$AWS_DEFAULT_REGION" \
                --query 'status' \
                --output text 2>/dev/null || echo "NOT_FOUND")
            
            if [ "$CORE_DEVICE_STATUS" = "NOT_FOUND" ]; then
                log_success "Core device deleted successfully"
                break
            fi
            
            log_info "Core device status: $CORE_DEVICE_STATUS (attempt $i/30)"
            sleep 2
        done
    else
        log_info "No Greengrass Core Device found for $IOT_THING_NAME"
    fi
fi

# Clean up local Greengrass files
log_info "Cleaning up local Greengrass files..."
if [ -d "$GREENGRASS_ROOT" ]; then
    # Remove sensitive files first
    sudo rm -rf "$GREENGRASS_ROOT/config" 2>/dev/null || true
    sudo rm -rf "$GREENGRASS_ROOT/certs" 2>/dev/null || true
    sudo rm -rf "$GREENGRASS_ROOT/logs" 2>/dev/null || true
    sudo rm -rf "$GREENGRASS_ROOT/work" 2>/dev/null || true
    
    # Remove deployment artifacts
    sudo rm -rf "$GREENGRASS_ROOT/deployments" 2>/dev/null || true
    sudo rm -rf "$GREENGRASS_ROOT/packages" 2>/dev/null || true
    
    # Remove the entire Greengrass directory
    sudo rm -rf "$GREENGRASS_ROOT" 2>/dev/null || true
    
    log_success "Greengrass files removed"
else
    log_info "No Greengrass files to clean up"
fi

# Remove systemd service file
if [ -f /etc/systemd/system/greengrass.service ]; then
    log_info "Removing systemd service file..."
    sudo rm -f /etc/systemd/system/greengrass.service
    sudo systemctl daemon-reload
    log_success "Systemd service file removed"
fi

# Remove status file
if [ -f /var/run/greengrass-provisioning.status ]; then
    sudo rm -f /var/run/greengrass-provisioning.status
fi

# Remove test database
if [ -f "$DB_PATH" ]; then
    rm -f "$DB_PATH"
    log_success "Test database removed"
fi

log_success "=== AWS Resource Cleanup Completed ==="

# Note about IoT Thing and certificates
log_warning "Note: This script does not delete the IoT Thing, certificates, or policies."
log_warning "These resources should be managed separately as they may be shared or pre-existing."
log_info "To fully clean up AWS resources, you may need to:"
log_info "  - Delete the IoT Thing: $IOT_THING_NAME"
log_info "  - Deactivate and delete associated certificates"
log_info "  - Detach and delete IoT policies"
log_info "  - Delete the IAM role and role alias if created specifically for this test" 