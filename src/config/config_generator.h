#pragma once

#include <string>
#include <filesystem>
#include "database/config_database.h"

namespace greengrass {
namespace config {

struct GeneratedConfig {
    std::filesystem::path config_file_path;
    std::filesystem::path certificate_path;
    std::filesystem::path private_key_path;
    std::filesystem::path root_ca_path;
    bool success = false;
    std::string error_message;
};

class ConfigGenerator {
public:
    ConfigGenerator(const std::filesystem::path& greengrass_path);
    ~ConfigGenerator() = default;
    
    // Generate Greengrass configuration from device config
    GeneratedConfig generate_config(const database::DeviceConfig& device_config);
    
    // Create directory structure for Greengrass
    bool create_directory_structure();
    
    // Write certificate files
    bool write_certificates(const database::DeviceConfig& device_config);
    
    // Generate Greengrass v2 config.yaml
    bool generate_greengrass_v2_config(const database::DeviceConfig& device_config);
    
    // Validate generated configuration
    bool validate_configuration() const;
    
    // Get the last error message
    std::string get_last_error() const;
    
private:
    std::filesystem::path greengrass_path_;
    std::filesystem::path config_path_;
    std::filesystem::path certs_path_;
    std::filesystem::path logs_path_;
    std::filesystem::path work_path_;
    std::string last_error_;
    
    // Helper methods
    bool write_file(const std::filesystem::path& path, const std::string& content);
    std::string generate_yaml_config_content(const database::DeviceConfig& device_config) const;
    bool set_file_permissions(const std::filesystem::path& path, bool is_private_key = false);
};

} // namespace config
} // namespace greengrass 