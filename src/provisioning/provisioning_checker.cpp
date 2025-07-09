#include "provisioning/provisioning_checker.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <regex>

namespace greengrass {
namespace provisioning {

ProvisioningChecker::ProvisioningChecker(const std::string& greengrass_path)
    : greengrass_path_(greengrass_path) {
    // Set up expected paths
    config_path_ = greengrass_path_ / "config";
    certs_path_ = greengrass_path_ / "certs";
    ggc_root_path_ = greengrass_path_ / "ggc-root";
    
    spdlog::debug("ProvisioningChecker initialized with path: {}", greengrass_path);
}

ProvisioningStatus ProvisioningChecker::check_provisioning_status() {
    ProvisioningStatus status;
    
    spdlog::info("Checking Greengrass provisioning status at: {}", greengrass_path_.string());
    
    // Check if the main directory exists
    if (!std::filesystem::exists(greengrass_path_)) {
        status.is_provisioned = false;
        status.details = "Greengrass directory does not exist";
        spdlog::info("Greengrass directory does not exist");
        return status;
    }
    
    // Check for essential components
    bool has_config = check_config_exists();
    bool has_certs = check_certificates_exist();
    bool has_root = check_greengrass_root_exists();
    
    if (!has_config) {
        status.missing_components.push_back("config");
    }
    if (!has_certs) {
        status.missing_components.push_back("certificates");
    }
    if (!has_root) {
        status.missing_components.push_back("ggc-root");
    }
    
    // If all essential components exist, validate configuration
    if (has_config && has_certs && has_root) {
        if (validate_config_file()) {
            status.is_provisioned = true;
            status.thing_name = read_thing_name_from_config();
            status.greengrass_version = detect_greengrass_version();
            status.config_file_path = (config_path_ / "config.yaml").string();
            status.details = "Greengrass is fully provisioned";
            
            spdlog::info("Greengrass is already provisioned. Thing name: {}, Version: {}", 
                        status.thing_name, status.greengrass_version);
        } else {
            status.is_provisioned = false;
            status.details = "Configuration file is invalid or corrupted";
            spdlog::warn("Greengrass configuration file is invalid");
        }
    } else {
        status.is_provisioned = false;
        status.details = fmt::format("Missing components: {}", 
                                   fmt::join(status.missing_components, ", "));
        spdlog::info("Greengrass is not provisioned. {}", status.details);
    }
    
    return status;
}

bool ProvisioningChecker::check_config_exists() const {
    // Check for both YAML and JSON config files (Greengrass v2 uses YAML)
    auto yaml_config = config_path_ / "config.yaml";
    auto yml_config = config_path_ / "config.yml";
    auto json_config = config_path_ / "config.json";
    
    bool exists = std::filesystem::exists(yaml_config) || 
                  std::filesystem::exists(yml_config) || 
                  std::filesystem::exists(json_config);
    
    if (exists) {
        spdlog::debug("Configuration file found");
    } else {
        spdlog::debug("No configuration file found in {}", config_path_.string());
    }
    
    return exists;
}

bool ProvisioningChecker::check_certificates_exist() const {
    if (!std::filesystem::exists(certs_path_)) {
        spdlog::debug("Certificates directory does not exist");
        return false;
    }
    
    // Check if directory contains certificate files
    bool has_certs = check_directory_not_empty(certs_path_);
    
    if (has_certs) {
        // Look for common certificate file patterns
        bool found_cert = false;
        bool found_key = false;
        
        for (const auto& entry : std::filesystem::directory_iterator(certs_path_)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.find(".cert.pem") != std::string::npos || 
                    filename.find(".crt") != std::string::npos) {
                    found_cert = true;
                }
                if (filename.find(".private.key") != std::string::npos || 
                    filename.find(".key") != std::string::npos) {
                    found_key = true;
                }
            }
        }
        
        has_certs = found_cert && found_key;
        spdlog::debug("Certificates check - cert: {}, key: {}", found_cert, found_key);
    }
    
    return has_certs;
}

bool ProvisioningChecker::check_greengrass_root_exists() const {
    bool exists = std::filesystem::exists(ggc_root_path_) && 
                  std::filesystem::is_directory(ggc_root_path_);
    
    if (exists) {
        spdlog::debug("Greengrass root directory exists");
    } else {
        spdlog::debug("Greengrass root directory does not exist");
    }
    
    return exists;
}

bool ProvisioningChecker::validate_config_file() const {
    // Try to find and validate the config file
    std::vector<std::filesystem::path> possible_configs = {
        config_path_ / "config.yaml",
        config_path_ / "config.yml",
        config_path_ / "config.json"
    };
    
    for (const auto& config_file : possible_configs) {
        if (std::filesystem::exists(config_file)) {
            try {
                std::ifstream file(config_file);
                if (!file.is_open()) {
                    continue;
                }
                
                // Basic validation - check if file is not empty and contains expected content
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                
                if (content.empty()) {
                    spdlog::warn("Configuration file is empty: {}", config_file.string());
                    return false;
                }
                
                // For YAML files, check for basic structure
                if (config_file.extension() == ".yaml" || config_file.extension() == ".yml") {
                    // Check for essential Greengrass v2 configuration keys
                    bool has_system = content.find("system:") != std::string::npos;
                    bool has_services = content.find("services:") != std::string::npos;
                    
                    if (has_system && has_services) {
                        spdlog::debug("Valid YAML configuration found");
                        return true;
                    }
                }
                // For JSON files
                else if (config_file.extension() == ".json") {
                    try {
                        auto json = nlohmann::json::parse(content);
                        // Basic check for required fields
                        if (json.contains("coreThing") || json.contains("system")) {
                            spdlog::debug("Valid JSON configuration found");
                            return true;
                        }
                    } catch (const nlohmann::json::exception& e) {
                        spdlog::warn("Invalid JSON in config file: {}", e.what());
                        return false;
                    }
                }
            } catch (const std::exception& e) {
                spdlog::error("Error validating config file {}: {}", 
                            config_file.string(), e.what());
                return false;
            }
        }
    }
    
    return false;
}

bool ProvisioningChecker::check_directory_not_empty(const std::filesystem::path& dir) const {
    try {
        return std::filesystem::exists(dir) && 
               !std::filesystem::is_empty(dir);
    } catch (const std::exception& e) {
        spdlog::error("Error checking directory {}: {}", dir.string(), e.what());
        return false;
    }
}

std::string ProvisioningChecker::read_thing_name_from_config() const {
    std::vector<std::filesystem::path> possible_configs = {
        config_path_ / "config.yaml",
        config_path_ / "config.yml",
        config_path_ / "config.json"
    };
    
    for (const auto& config_file : possible_configs) {
        if (std::filesystem::exists(config_file)) {
            try {
                std::ifstream file(config_file);
                if (!file.is_open()) {
                    continue;
                }
                
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                
                // For YAML files, use regex to find thingName
                if (config_file.extension() == ".yaml" || config_file.extension() == ".yml") {
                    std::regex thing_name_regex(R"(thingName:\s*([^\s\n]+))");
                    std::smatch match;
                    if (std::regex_search(content, match, thing_name_regex)) {
                        return match[1].str();
                    }
                }
                // For JSON files
                else if (config_file.extension() == ".json") {
                    try {
                        auto json = nlohmann::json::parse(content);
                        if (json.contains("coreThing") && json["coreThing"].contains("thingName")) {
                            return json["coreThing"]["thingName"];
                        }
                        if (json.contains("system") && json["system"].contains("thingName")) {
                            return json["system"]["thingName"];
                        }
                    } catch (const nlohmann::json::exception& e) {
                        spdlog::debug("Error parsing JSON for thing name: {}", e.what());
                    }
                }
            } catch (const std::exception& e) {
                spdlog::error("Error reading thing name from config: {}", e.what());
            }
        }
    }
    
    return "unknown";
}

std::string ProvisioningChecker::detect_greengrass_version() const {
    // Check for version indicators
    // Greengrass v2 typically has a 'recipe' directory
    if (std::filesystem::exists(greengrass_path_ / "recipes")) {
        return "v2.x";
    }
    
    // Check config file format - v2 uses YAML
    if (std::filesystem::exists(config_path_ / "config.yaml") || 
        std::filesystem::exists(config_path_ / "config.yml")) {
        return "v2.x";
    }
    
    // v1 uses JSON config
    if (std::filesystem::exists(config_path_ / "config.json")) {
        return "v1.x";
    }
    
    return "unknown";
}

} // namespace provisioning
} // namespace greengrass 