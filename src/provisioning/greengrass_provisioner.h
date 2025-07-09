#pragma once

#include <string>
#include <memory>
#include <functional>
#include "database/config_database.h"
#include "config/config_generator.h"

namespace greengrass {
namespace provisioning {

enum class ProvisioningStep {
    INITIALIZING,
    DOWNLOADING_NUCLEUS,
    INSTALLING_NUCLEUS,
    CONFIGURING_SYSTEMD,
    STARTING_SERVICE,
    VERIFYING_CONNECTION,
    COMPLETED
};

struct ProvisioningResult {
    bool success = false;
    ProvisioningStep last_completed_step = ProvisioningStep::INITIALIZING;
    std::string error_message;
    std::string greengrass_service_name = "greengrass";
};

class GreengrassProvisioner {
public:
    explicit GreengrassProvisioner(const std::filesystem::path& greengrass_path);
    ~GreengrassProvisioner() = default;
    
    // Main provisioning method
    ProvisioningResult provision(const database::DeviceConfig& device_config,
                               const config::GeneratedConfig& generated_config);
    
    // Individual provisioning steps
    bool download_greengrass_nucleus(const std::string& version);
    bool install_greengrass_nucleus(const database::DeviceConfig& device_config);
    bool configure_systemd_service();
    bool start_greengrass_service();
    bool verify_greengrass_connection();
    
    // Set progress callback
    using ProgressCallback = std::function<void(ProvisioningStep step, int percentage, const std::string& message)>;
    void set_progress_callback(ProgressCallback callback);
    
    // Configuration setters
    void set_java_home(const std::string& java_home);
    void set_greengrass_user(const std::string& user);
    void set_greengrass_group(const std::string& group);
    
private:
    std::filesystem::path greengrass_path_;
    std::filesystem::path nucleus_jar_path_;
    std::string java_home_;
    std::string greengrass_user_ = "ggc_user";
    std::string greengrass_group_ = "ggc_group";
    ProgressCallback progress_callback_;
    
    // Helper methods
    bool execute_command(const std::string& command, std::string& output);
    bool create_greengrass_user();
    bool download_file(const std::string& url, const std::filesystem::path& destination);
    std::string get_nucleus_download_url(const std::string& version) const;
    void report_progress(ProvisioningStep step, int percentage, const std::string& message);
};

} // namespace provisioning
} // namespace greengrass 