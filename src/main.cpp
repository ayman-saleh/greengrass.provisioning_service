#include <iostream>
#include <memory>
#include <fstream>
#include <algorithm>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <fmt/format.h>

#include "cli/argument_parser.h"
#include "status/status_reporter.h"
#include "provisioning/provisioning_checker.h"
#include "networking/connectivity_checker.h"
#include "database/config_database.h"
#include "config/config_generator.h"
#include "provisioning/greengrass_provisioner.h"

using namespace greengrass;

void setup_logging(bool verbose) {
    // Create console sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(verbose ? spdlog::level::debug : spdlog::level::info);
    
    // Create file sink
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "/var/log/greengrass-provisioning.log", 10 * 1024 * 1024, 3);
    file_sink->set_level(spdlog::level::debug);
    
    // Create multi-sink logger
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>("greengrass", sinks.begin(), sinks.end());
    
    // Register as default logger
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    
    if (verbose) {
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }
}

std::string get_device_identifier() {
    // Try to get device identifier (MAC address or serial number)
    // This is a simplified version - in production, you'd have more robust methods
    std::string identifier;
    
    // Try to get primary network interface MAC address
    std::ifstream mac_file("/sys/class/net/eth0/address");
    if (mac_file.is_open()) {
        std::getline(mac_file, identifier);
        mac_file.close();
        // Remove colons from MAC address
        identifier.erase(std::remove(identifier.begin(), identifier.end(), ':'), identifier.end());
        return identifier;
    }
    
    // Fall back to hostname
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    
    return "default-device";
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    cli::ArgumentParser parser;
    auto options = parser.parse(argc, argv);
    
    if (!options.has_value()) {
        std::cerr << parser.get_help_message() << std::endl;
        return 1;
    }
    
    // Setup logging
    setup_logging(options->verbose);
    
    spdlog::info("AWS Greengrass Provisioning Service starting...");
    spdlog::debug("Database path: {}", options->database_path);
    spdlog::debug("Greengrass path: {}", options->greengrass_path);
    spdlog::debug("Status file: {}", options->status_file);
    
    // Initialize status reporter
    auto status_reporter = std::make_unique<status::StatusReporter>(options->status_file);
    
    try {
        // Step 1: Check if already provisioned
        status_reporter->update_status(status::ServiceStatus::CHECKING_PROVISIONING);
        provisioning::ProvisioningChecker prov_checker(options->greengrass_path);
        auto prov_status = prov_checker.check_provisioning_status();
        
        if (prov_status.is_provisioned) {
            spdlog::info("Greengrass is already provisioned for thing: {}", prov_status.thing_name);
            status_reporter->update_status(status::ServiceStatus::ALREADY_PROVISIONED, 
                                         fmt::format("Already provisioned as {}", prov_status.thing_name));
            return 0;
        }
        
        // Step 2: Check internet connectivity
        status_reporter->update_status(status::ServiceStatus::CHECKING_CONNECTIVITY);
        networking::ConnectivityChecker conn_checker;
        auto conn_result = conn_checker.check_connectivity();
        
        if (!conn_result.is_connected) {
            spdlog::error("No internet connectivity: {}", conn_result.error_message);
            status_reporter->update_status(status::ServiceStatus::NO_CONNECTIVITY, 
                                         conn_result.error_message);
            return 2;
        }
        
        // Step 3: Read configuration from database
        status_reporter->update_status(status::ServiceStatus::READING_DATABASE);
        database::ConfigDatabase db(options->database_path);
        
        if (!db.connect()) {
            throw std::runtime_error(fmt::format("Failed to connect to database: {}", db.get_last_error()));
        }
        
        // Get device configuration
        std::string device_id = get_device_identifier();
        spdlog::info("Looking up configuration for device: {}", device_id);
        
        auto device_config = db.get_device_config_by_identifier(device_id);
        if (!device_config.has_value()) {
            // Try with default device ID
            device_config = db.get_device_config("default");
            if (!device_config.has_value()) {
                throw std::runtime_error("No device configuration found in database");
            }
        }
        
        spdlog::info("Found configuration for device: {} (Thing: {})", 
                    device_config->device_id, device_config->thing_name);
        
        // Set custom IoT endpoint if available
        if (!device_config->iot_endpoint.empty()) {
            conn_checker.set_iot_endpoint(fmt::format("https://{}", device_config->iot_endpoint));
        }
        
        // Step 4: Generate Greengrass configuration
        status_reporter->update_status(status::ServiceStatus::GENERATING_CONFIG);
        config::ConfigGenerator config_gen(options->greengrass_path);
        auto generated_config = config_gen.generate_config(device_config.value());
        
        if (!generated_config.success) {
            throw std::runtime_error(fmt::format("Failed to generate configuration: {}", 
                                               generated_config.error_message));
        }
        
        // Step 5: Provision Greengrass
        status_reporter->update_status(status::ServiceStatus::PROVISIONING);
        provisioning::GreengrassProvisioner provisioner(options->greengrass_path);
        
        // Set progress callback to update status
        provisioner.set_progress_callback(
            [&status_reporter](provisioning::ProvisioningStep step, int percentage, const std::string& message) {
                status_reporter->update_status(status::ServiceStatus::PROVISIONING, message, percentage);
            }
        );
        
        auto prov_result = provisioner.provision(device_config.value(), generated_config);
        
        if (!prov_result.success) {
            throw std::runtime_error(fmt::format("Provisioning failed: {}", prov_result.error_message));
        }
        
        // Success!
        status_reporter->update_status(status::ServiceStatus::COMPLETED, 
                                     "Greengrass provisioning completed successfully", 100);
        spdlog::info("Greengrass provisioning completed successfully!");
        
        db.disconnect();
        return 0;
        
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        status_reporter->report_error("Provisioning failed", e.what());
        return 1;
    } catch (...) {
        spdlog::error("Unknown fatal error occurred");
        status_reporter->report_error("Provisioning failed", "Unknown error");
        return 1;
    }
} 