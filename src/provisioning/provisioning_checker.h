#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace greengrass {
namespace provisioning {

struct ProvisioningStatus {
    bool is_provisioned = false;
    std::string greengrass_version;
    std::string thing_name;
    std::string config_file_path;
    std::vector<std::string> missing_components;
    std::string details;
};

class ProvisioningChecker {
public:
    explicit ProvisioningChecker(const std::string& greengrass_path);
    ~ProvisioningChecker() = default;
    
    // Check if Greengrass is already provisioned
    ProvisioningStatus check_provisioning_status();
    
    // Check if specific Greengrass components exist
    bool check_config_exists() const;
    bool check_certificates_exist() const;
    bool check_greengrass_root_exists() const;
    
    // Validate configuration file
    bool validate_config_file() const;
    
private:
    std::filesystem::path greengrass_path_;
    std::filesystem::path config_path_;
    std::filesystem::path certs_path_;
    std::filesystem::path ggc_root_path_;
    
    // Helper methods
    bool check_directory_not_empty(const std::filesystem::path& dir) const;
    std::string read_thing_name_from_config() const;
    std::string detect_greengrass_version() const;
};

} // namespace provisioning
} // namespace greengrass 