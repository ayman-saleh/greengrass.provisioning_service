#!/bin/bash
set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}[VERIFY]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[VERIFY]${NC} ✓ $1"
}

log_error() {
    echo -e "${RED}[VERIFY]${NC} ✗ $1"
}

log_warning() {
    echo -e "${YELLOW}[VERIFY]${NC} ! $1"
}

ERRORS=0

# Function to check file/directory exists
check_exists() {
    local path="$1"
    local description="$2"
    
    if [ -e "$path" ]; then
        log_success "$description exists: $path"
        return 0
    else
        log_error "$description missing: $path"
        ERRORS=$((ERRORS + 1))
        return 1
    fi
}

# Function to check file permissions
check_permissions() {
    local path="$1"
    local expected_perms="$2"
    local description="$3"
    
    if [ ! -e "$path" ]; then
        log_error "$description does not exist: $path"
        ERRORS=$((ERRORS + 1))
        return 1
    fi
    
    actual_perms=$(stat -c "%a" "$path")
    if [ "$actual_perms" = "$expected_perms" ]; then
        log_success "$description has correct permissions ($expected_perms): $path"
        return 0
    else
        log_error "$description has incorrect permissions (expected: $expected_perms, actual: $actual_perms): $path"
        ERRORS=$((ERRORS + 1))
        return 1
    fi
}

# Function to check file content
check_file_contains() {
    local file="$1"
    local search_string="$2"
    local description="$3"
    
    if [ ! -f "$file" ]; then
        log_error "File does not exist: $file"
        ERRORS=$((ERRORS + 1))
        return 1
    fi
    
    if grep -q "$search_string" "$file"; then
        log_success "$description found in $file"
        return 0
    else
        log_error "$description not found in $file"
        ERRORS=$((ERRORS + 1))
        return 1
    fi
}

log_info "=== Verifying Greengrass Provisioning ==="

# Check directory structure
log_info "Checking directory structure..."
check_exists "$GREENGRASS_ROOT" "Greengrass root directory"
check_exists "$GREENGRASS_ROOT/config" "Config directory"
check_exists "$GREENGRASS_ROOT/certs" "Certificates directory"
check_exists "$GREENGRASS_ROOT/logs" "Logs directory"
check_exists "$GREENGRASS_ROOT/work" "Work directory"
check_exists "$GREENGRASS_ROOT/packages" "Packages directory"
check_exists "$GREENGRASS_ROOT/deployments" "Deployments directory"
check_exists "$GREENGRASS_ROOT/ggc-root" "GGC root directory"

# Check configuration file
log_info "Checking configuration file..."
CONFIG_FILE="$GREENGRASS_ROOT/config/config.yaml"
if check_exists "$CONFIG_FILE" "Configuration file"; then
    # Verify configuration content
    check_file_contains "$CONFIG_FILE" "thingName: \"$IOT_THING_NAME\"" "Thing name"
    check_file_contains "$CONFIG_FILE" "iotDataEndpoint: \"$IOT_ENDPOINT\"" "IoT endpoint"
    check_file_contains "$CONFIG_FILE" "awsRegion: \"$AWS_DEFAULT_REGION\"" "AWS region"
    check_file_contains "$CONFIG_FILE" "iotRoleAlias: \"$IOT_ROLE_ALIAS\"" "Role alias"
    
    # Check if it's YAML format
    if command -v yq &> /dev/null; then
        log_info "Validating YAML syntax..."
        if yq eval '.' "$CONFIG_FILE" > /dev/null 2>&1; then
            log_success "Valid YAML configuration"
        else
            log_error "Invalid YAML syntax in configuration file"
            ERRORS=$((ERRORS + 1))
        fi
    fi
fi

# Check certificates
log_info "Checking certificates..."
CERT_FILE="$GREENGRASS_ROOT/certs/$IOT_THING_NAME.cert.pem"
KEY_FILE="$GREENGRASS_ROOT/certs/$IOT_THING_NAME.private.key"
CA_FILE="$GREENGRASS_ROOT/certs/root.ca.pem"

check_exists "$CERT_FILE" "Device certificate"
check_exists "$KEY_FILE" "Private key"
check_exists "$CA_FILE" "Root CA certificate"

# Check certificate permissions
check_permissions "$KEY_FILE" "600" "Private key"
check_permissions "$CERT_FILE" "644" "Device certificate"
check_permissions "$CA_FILE" "644" "Root CA certificate"

# Verify certificate validity
if [ -f "$CERT_FILE" ]; then
    log_info "Checking certificate validity..."
    if openssl x509 -in "$CERT_FILE" -noout -checkend 0; then
        log_success "Certificate is valid and not expired"
        
        # Extract certificate details
        CERT_CN=$(openssl x509 -in "$CERT_FILE" -noout -subject | sed -n 's/.*CN = \(.*\)/\1/p' | cut -d',' -f1)
        CERT_EXPIRES=$(openssl x509 -in "$CERT_FILE" -noout -enddate | cut -d'=' -f2)
        log_info "Certificate CN: $CERT_CN"
        log_info "Certificate expires: $CERT_EXPIRES"
    else
        log_error "Certificate is expired or invalid"
        ERRORS=$((ERRORS + 1))
    fi
fi

# Check Greengrass software installation
log_info "Checking Greengrass software..."
if [ -f "$GREENGRASS_ROOT/lib/Greengrass.jar" ]; then
    log_success "Greengrass nucleus JAR found"
    
    # Check if we can determine the version
    if [ -f "$GREENGRASS_ROOT/recipe/aws.greengrass.Nucleus.json" ]; then
        NUCLEUS_VERSION=$(jq -r '.componentVersion' "$GREENGRASS_ROOT/recipe/aws.greengrass.Nucleus.json" 2>/dev/null || echo "Unknown")
        log_info "Greengrass Nucleus version: $NUCLEUS_VERSION"
    fi
else
    log_warning "Greengrass nucleus JAR not found (may not be installed yet)"
fi

# Check Greengrass CLI
if [ -f "$GREENGRASS_ROOT/bin/greengrass-cli" ]; then
    log_success "Greengrass CLI found"
    check_permissions "$GREENGRASS_ROOT/bin/greengrass-cli" "755" "Greengrass CLI"
else
    log_warning "Greengrass CLI not found (may be installed later)"
fi

# Check systemd service file
if [ -f "$GREENGRASS_ROOT/bin/greengrass.service" ]; then
    log_success "Systemd service file found"
else
    log_warning "Systemd service file not found (may be created during installation)"
fi

# Check status file
STATUS_FILE="/var/run/greengrass-provisioning.status"
if [ -f "$STATUS_FILE" ]; then
    log_success "Status file exists"
    
    # Parse status
    STATUS=$(jq -r '.status' "$STATUS_FILE" 2>/dev/null || echo "UNKNOWN")
    MESSAGE=$(jq -r '.message' "$STATUS_FILE" 2>/dev/null || echo "No message")
    PROGRESS=$(jq -r '.progress_percentage' "$STATUS_FILE" 2>/dev/null || echo "0")
    
    log_info "Provisioning status: $STATUS"
    log_info "Message: $MESSAGE"
    log_info "Progress: $PROGRESS%"
    
    if [ "$STATUS" = "COMPLETED" ] || [ "$STATUS" = "ALREADY_PROVISIONED" ]; then
        log_success "Provisioning completed successfully"
    else
        log_error "Provisioning not completed (status: $STATUS)"
        ERRORS=$((ERRORS + 1))
    fi
else
    log_error "Status file not found"
    ERRORS=$((ERRORS + 1))
fi

# Summary
echo
if [ $ERRORS -eq 0 ]; then
    log_success "=== All verification checks passed! ==="
    exit 0
else
    log_error "=== Verification failed with $ERRORS error(s) ==="
    exit 1
fi 