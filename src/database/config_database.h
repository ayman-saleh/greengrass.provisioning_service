#pragma once

#include <string>
#include <memory>
#include <optional>
#include <vector>

// Forward declaration
struct sqlite3;

namespace greengrass {
namespace database {

struct DeviceConfig {
    std::string device_id;
    std::string thing_name;
    std::string iot_endpoint;
    std::string aws_region;
    std::string root_ca_path;
    std::string certificate_pem;
    std::string private_key_pem;
    std::string role_alias;
    std::string role_alias_endpoint;
    
    // Additional Greengrass-specific config
    std::string nucleus_version;
    std::string deployment_group;
    std::vector<std::string> initial_components;
    
    // Optional fields
    std::optional<std::string> proxy_url;
    std::optional<int> mqtt_port;
    std::optional<std::string> custom_domain;
};

class ConfigDatabase {
public:
    explicit ConfigDatabase(const std::string& database_path);
    ~ConfigDatabase();
    
    // Connect to the database
    bool connect();
    
    // Disconnect from the database
    void disconnect();
    
    // Check if connected
    bool is_connected() const;
    
    // Read device configuration by device ID
    std::optional<DeviceConfig> get_device_config(const std::string& device_id);
    
    // Read device configuration by MAC address or serial number
    std::optional<DeviceConfig> get_device_config_by_identifier(const std::string& identifier);
    
    // List all device IDs in the database
    std::vector<std::string> list_device_ids();
    
    // Get the last error message
    std::string get_last_error() const;
    
private:
    std::string database_path_;
    sqlite3* db_ = nullptr;
    std::string last_error_;
    
    // Helper methods
    bool execute_query(const std::string& query);
    std::optional<DeviceConfig> parse_device_config_from_query(const std::string& query);
};

} // namespace database
} // namespace greengrass 