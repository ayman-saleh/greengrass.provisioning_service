#!/bin/bash
set -e

# Colors for output
BLUE='\033[0;34m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}[DB-SETUP]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[DB-SETUP]${NC} $1"
}

log_error() {
    echo -e "${RED}[DB-SETUP]${NC} $1"
}

# Function to handle certificate content
get_certificate_content() {
    local env_var_path="$1"
    local env_var_base64="$2"
    
    # Check if path variable is set
    if [ -n "${!env_var_path}" ] && [ -f "${!env_var_path}" ]; then
        cat "${!env_var_path}"
    # Check if base64 content is set
    elif [ -n "${!env_var_base64}" ]; then
        echo "${!env_var_base64}" | base64 -d
    else
        echo ""
    fi
}

log_info "Setting up test database at $DB_PATH"

# Remove existing database if it exists
if [ -f "$DB_PATH" ]; then
    log_info "Removing existing database..."
    rm -f "$DB_PATH"
fi

# Get certificate contents
CERT_CONTENT=$(get_certificate_content "DEVICE_CERTIFICATE_PEM_PATH" "DEVICE_CERTIFICATE_PEM_BASE64")
KEY_CONTENT=$(get_certificate_content "DEVICE_PRIVATE_KEY_PATH" "DEVICE_PRIVATE_KEY_BASE64")
CA_CONTENT=$(get_certificate_content "ROOT_CA_PATH" "ROOT_CA_BASE64")

# Validate certificate content
if [ -z "$CERT_CONTENT" ]; then
    log_error "Device certificate not found! Set DEVICE_CERTIFICATE_PEM_PATH or DEVICE_CERTIFICATE_PEM_BASE64"
    exit 1
fi

if [ -z "$KEY_CONTENT" ]; then
    log_error "Private key not found! Set DEVICE_PRIVATE_KEY_PATH or DEVICE_PRIVATE_KEY_BASE64"
    exit 1
fi

if [ -z "$CA_CONTENT" ]; then
    log_error "Root CA not found! Set ROOT_CA_PATH or ROOT_CA_BASE64"
    exit 1
fi

# Create database and tables
log_info "Creating database schema..."
sqlite3 "$DB_PATH" <<EOF
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

CREATE TABLE device_identifiers (
    device_id TEXT NOT NULL,
    mac_address TEXT,
    serial_number TEXT,
    FOREIGN KEY (device_id) REFERENCES device_config(device_id)
);
EOF

# Prepare SQL values
NUCLEUS_VERSION=${GREENGRASS_NUCLEUS_VERSION:-"2.12.1"}
DEPLOYMENT_GROUP=${GREENGRASS_DEPLOYMENT_GROUP:-""}
INITIAL_COMPONENTS=${GREENGRASS_INITIAL_COMPONENTS:-""}
PROXY_URL=${PROXY_URL:-""}
MQTT_PORT=${MQTT_PORT:-"8883"}
CUSTOM_DOMAIN=${CUSTOM_DOMAIN:-""}

# Insert device configuration
log_info "Inserting device configuration..."
cat > /tmp/insert_device.sql <<EOF
INSERT INTO device_config (
    device_id, thing_name, iot_endpoint, aws_region,
    root_ca_path, certificate_pem, private_key_pem,
    role_alias, role_alias_endpoint, nucleus_version,
    deployment_group, initial_components, proxy_url,
    mqtt_port, custom_domain
) VALUES (
    '${DEVICE_ID}',
    '${IOT_THING_NAME}',
    '${IOT_ENDPOINT}',
    '${AWS_DEFAULT_REGION}',
    '${CA_CONTENT//\'/\'\'}',
    '${CERT_CONTENT//\'/\'\'}',
    '${KEY_CONTENT//\'/\'\'}',
    '${IOT_ROLE_ALIAS}',
    '${IOT_ROLE_ALIAS_ENDPOINT}',
    '${NUCLEUS_VERSION}',
    '${DEPLOYMENT_GROUP}',
    '${INITIAL_COMPONENTS}',
    '${PROXY_URL}',
    ${MQTT_PORT},
    '${CUSTOM_DOMAIN}'
);
EOF

sqlite3 "$DB_PATH" < /tmp/insert_device.sql
rm -f /tmp/insert_device.sql

# Insert device identifiers if provided
if [ -n "$DEVICE_MAC_ADDRESS" ] || [ -n "$DEVICE_SERIAL_NUMBER" ]; then
    log_info "Inserting device identifiers..."
    
    if [ -n "$DEVICE_MAC_ADDRESS" ]; then
        sqlite3 "$DB_PATH" <<EOF
INSERT INTO device_identifiers (device_id, mac_address)
VALUES ('${DEVICE_ID}', '${DEVICE_MAC_ADDRESS}');
EOF
    fi
    
    if [ -n "$DEVICE_SERIAL_NUMBER" ]; then
        sqlite3 "$DB_PATH" <<EOF
INSERT INTO device_identifiers (device_id, serial_number)
VALUES ('${DEVICE_ID}', '${DEVICE_SERIAL_NUMBER}');
EOF
    fi
fi

# Verify database
log_info "Verifying database..."
DEVICE_COUNT=$(sqlite3 "$DB_PATH" "SELECT COUNT(*) FROM device_config;")
log_success "Database created with $DEVICE_COUNT device(s)"

# Show device info
sqlite3 "$DB_PATH" <<EOF
.mode column
.headers on
SELECT device_id, thing_name, iot_endpoint, aws_region
FROM device_config;
EOF 