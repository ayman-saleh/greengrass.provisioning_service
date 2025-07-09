#include "config/config_generator.h"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <fstream>
#include <filesystem>

namespace greengrass {
namespace config {

ConfigGenerator::ConfigGenerator(const std::filesystem::path& greengrass_path)
    : greengrass_path_(greengrass_path) {
    // Set up paths
    config_path_ = greengrass_path_ / "config";
    certs_path_ = greengrass_path_ / "certs";
    logs_path_ = greengrass_path_ / "logs";
    work_path_ = greengrass_path_ / "work";
    
    spdlog::debug("ConfigGenerator initialized with path: {}", greengrass_path.string());
}

GeneratedConfig ConfigGenerator::generate_config(const database::DeviceConfig& device_config) {
    GeneratedConfig result;
    
    spdlog::info("Generating Greengrass configuration for device: {}", device_config.device_id);
    
    // Step 1: Create directory structure
    if (!create_directory_structure()) {
        result.error_message = last_error_;
        return result;
    }
    
    // Step 2: Write certificates
    if (!write_certificates(device_config)) {
        result.error_message = last_error_;
        return result;
    }
    
    // Step 3: Generate Greengrass v2 config
    if (!generate_greengrass_v2_config(device_config)) {
        result.error_message = last_error_;
        return result;
    }
    
    // Step 4: Validate configuration
    if (!validate_configuration()) {
        result.error_message = last_error_;
        return result;
    }
    
    // Set result paths
    result.config_file_path = config_path_ / "config.yaml";
    result.certificate_path = certs_path_ / fmt::format("{}.cert.pem", device_config.thing_name);
    result.private_key_path = certs_path_ / fmt::format("{}.private.key", device_config.thing_name);
    result.root_ca_path = certs_path_ / "root.ca.pem";
    result.success = true;
    
    spdlog::info("Successfully generated Greengrass configuration");
    return result;
}

bool ConfigGenerator::create_directory_structure() {
    try {
        // Create main directories
        std::filesystem::create_directories(greengrass_path_);
        std::filesystem::create_directories(config_path_);
        std::filesystem::create_directories(certs_path_);
        std::filesystem::create_directories(logs_path_);
        std::filesystem::create_directories(work_path_);
        
        // Create additional Greengrass v2 directories
        std::filesystem::create_directories(greengrass_path_ / "packages");
        std::filesystem::create_directories(greengrass_path_ / "deployments");
        std::filesystem::create_directories(greengrass_path_ / "ggc-root");
        
        // Set appropriate permissions
        std::filesystem::permissions(greengrass_path_, 
            std::filesystem::perms::owner_all | 
            std::filesystem::perms::group_read | std::filesystem::perms::group_exec);
        
        std::filesystem::permissions(certs_path_,
            std::filesystem::perms::owner_all | 
            std::filesystem::perms::group_read | std::filesystem::perms::group_exec);
        
        spdlog::debug("Created Greengrass directory structure");
        return true;
    } catch (const std::exception& e) {
        last_error_ = fmt::format("Failed to create directory structure: {}", e.what());
        spdlog::error(last_error_);
        return false;
    }
}

bool ConfigGenerator::write_certificates(const database::DeviceConfig& device_config) {
    try {
        // Write device certificate
        auto cert_path = certs_path_ / fmt::format("{}.cert.pem", device_config.thing_name);
        if (!write_file(cert_path, device_config.certificate_pem)) {
            return false;
        }
        set_file_permissions(cert_path, false);
        
        // Write private key
        auto key_path = certs_path_ / fmt::format("{}.private.key", device_config.thing_name);
        if (!write_file(key_path, device_config.private_key_pem)) {
            return false;
        }
        set_file_permissions(key_path, true);
        
        // Write root CA certificate
        auto root_ca_path = certs_path_ / "root.ca.pem";
        // If root_ca_path in config is a file path, read it; otherwise treat as content
        std::string root_ca_content;
        if (std::filesystem::exists(device_config.root_ca_path)) {
            std::ifstream root_ca_file(device_config.root_ca_path);
            root_ca_content = std::string((std::istreambuf_iterator<char>(root_ca_file)),
                                         std::istreambuf_iterator<char>());
        } else {
            root_ca_content = device_config.root_ca_path; // Assume it's the content itself
        }
        
        if (!write_file(root_ca_path, root_ca_content)) {
            return false;
        }
        set_file_permissions(root_ca_path, false);
        
        spdlog::debug("Successfully wrote certificates to {}", certs_path_.string());
        return true;
    } catch (const std::exception& e) {
        last_error_ = fmt::format("Failed to write certificates: {}", e.what());
        spdlog::error(last_error_);
        return false;
    }
}

bool ConfigGenerator::generate_greengrass_v2_config(const database::DeviceConfig& device_config) {
    try {
        std::string config_content = generate_yaml_config_content(device_config);
        auto config_file_path = config_path_ / "config.yaml";
        
        if (!write_file(config_file_path, config_content)) {
            return false;
        }
        
        set_file_permissions(config_file_path, false);
        
        spdlog::debug("Generated Greengrass v2 configuration file");
        return true;
    } catch (const std::exception& e) {
        last_error_ = fmt::format("Failed to generate config.yaml: {}", e.what());
        spdlog::error(last_error_);
        return false;
    }
}

bool ConfigGenerator::validate_configuration() const {
    // Check if all required files exist
    auto config_file = config_path_ / "config.yaml";
    if (!std::filesystem::exists(config_file)) {
        const_cast<std::string&>(last_error_) = "config.yaml does not exist";
        return false;
    }
    
    // Check if certificates exist
    bool certs_exist = false;
    for (const auto& entry : std::filesystem::directory_iterator(certs_path_)) {
        if (entry.path().extension() == ".pem" || entry.path().extension() == ".key") {
            certs_exist = true;
            break;
        }
    }
    
    if (!certs_exist) {
        const_cast<std::string&>(last_error_) = "No certificates found in certs directory";
        return false;
    }
    
    spdlog::debug("Configuration validation passed");
    return true;
}

std::string ConfigGenerator::get_last_error() const {
    return last_error_;
}

bool ConfigGenerator::write_file(const std::filesystem::path& path, const std::string& content) {
    try {
        std::ofstream file(path);
        if (!file.is_open()) {
            last_error_ = fmt::format("Failed to open file for writing: {}", path.string());
            spdlog::error(last_error_);
            return false;
        }
        
        file << content;
        file.close();
        
        spdlog::debug("Wrote file: {}", path.string());
        return true;
    } catch (const std::exception& e) {
        last_error_ = fmt::format("Failed to write file {}: {}", path.string(), e.what());
        spdlog::error(last_error_);
        return false;
    }
}

std::string ConfigGenerator::generate_yaml_config_content(const database::DeviceConfig& device_config) const {
    // Generate Greengrass v2 config.yaml content
    std::string config = fmt::format(R"yaml(---
system:
  certificateFilePath: "{}/certs/{}.cert.pem"
  privateKeyPath: "{}/certs/{}.private.key"
  rootCaPath: "{}/certs/root.ca.pem"
  rootpath: "{}"
  thingName: "{}"

services:
  aws.greengrass.Nucleus:
    version: "{}"
    configuration:
      awsRegion: "{}"
      iotRoleAlias: "{}"
      iotDataEndpoint: "{}"
      iotCredEndpoint: "{}"
)yaml",
        greengrass_path_.string(), device_config.thing_name,
        greengrass_path_.string(), device_config.thing_name,
        greengrass_path_.string(),
        greengrass_path_.string(),
        device_config.thing_name,
        device_config.nucleus_version.empty() ? "2.9.0" : device_config.nucleus_version,
        device_config.aws_region,
        device_config.role_alias,
        device_config.iot_endpoint,
        device_config.role_alias_endpoint
    );
    
    // Add MQTT configuration if custom port is specified
    if (device_config.mqtt_port.has_value()) {
        config += fmt::format(R"yaml(      mqtt:
        port: {}
)yaml", device_config.mqtt_port.value());
    }
    
    // Add proxy configuration if specified
    if (device_config.proxy_url.has_value()) {
        config += fmt::format(R"yaml(      networkProxy:
        proxy:
          url: "{}"
)yaml", device_config.proxy_url.value());
    }
    
    // Add logging configuration
    config += R"yaml(      logging:
        level: "INFO"
        fileSizeKB: 1024
        totalLogsSizeKB: 25600
        format: "JSON"
)yaml";
    
    // Add deployment configuration
    if (!device_config.deployment_group.empty()) {
        config += fmt::format(R"yaml(      deploymentPollingFrequency: 15
      componentStoreMaxSizeBytes: 10737418240
      deploymentStatusKeepAliveFrequency: 60
)yaml");
    }
    
    return config;
}

bool ConfigGenerator::set_file_permissions(const std::filesystem::path& path, bool is_private_key) {
    try {
        if (is_private_key) {
            // Private key should only be readable by owner
            std::filesystem::permissions(path,
                std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                std::filesystem::perm_options::replace);
        } else {
            // Other files can be read by owner and group
            std::filesystem::permissions(path,
                std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
                std::filesystem::perms::group_read,
                std::filesystem::perm_options::replace);
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::warn("Failed to set permissions for {}: {}", path.string(), e.what());
        return true; // Non-fatal error
    }
}

} // namespace config
} // namespace greengrass 