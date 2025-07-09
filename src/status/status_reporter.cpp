#include "status/status_reporter.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace greengrass {
namespace status {

StatusReporter::StatusReporter(const std::string& status_file_path)
    : status_file_path_(status_file_path) {
    // Initialize with starting status
    current_status_.status = ServiceStatus::STARTING;
    current_status_.message = "Service is starting";
    current_status_.timestamp = std::chrono::system_clock::now();
    current_status_.progress_percentage = 0;
    
    // Ensure the directory exists
    std::filesystem::path status_path(status_file_path_);
    std::filesystem::create_directories(status_path.parent_path());
    
    // Write initial status
    write_status_file();
}

void StatusReporter::update_status(ServiceStatus status, const std::string& message, int progress) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    current_status_.status = status;
    current_status_.timestamp = std::chrono::system_clock::now();
    
    if (!message.empty()) {
        current_status_.message = message;
    } else {
        // Provide default messages for each status
        switch (status) {
            case ServiceStatus::STARTING:
                current_status_.message = "Service is starting";
                break;
            case ServiceStatus::CHECKING_PROVISIONING:
                current_status_.message = "Checking if Greengrass is already provisioned";
                break;
            case ServiceStatus::ALREADY_PROVISIONED:
                current_status_.message = "Greengrass is already provisioned";
                break;
            case ServiceStatus::CHECKING_CONNECTIVITY:
                current_status_.message = "Checking internet connectivity";
                break;
            case ServiceStatus::NO_CONNECTIVITY:
                current_status_.message = "No internet connectivity available";
                break;
            case ServiceStatus::READING_DATABASE:
                current_status_.message = "Reading configuration from database";
                break;
            case ServiceStatus::GENERATING_CONFIG:
                current_status_.message = "Generating Greengrass configuration";
                break;
            case ServiceStatus::PROVISIONING:
                current_status_.message = "Provisioning Greengrass device";
                break;
            case ServiceStatus::COMPLETED:
                current_status_.message = "Provisioning completed successfully";
                break;
            case ServiceStatus::ERROR:
                current_status_.message = "An error occurred during provisioning";
                break;
        }
    }
    
    if (progress >= 0 && progress <= 100) {
        current_status_.progress_percentage = progress;
    } else {
        // Auto-calculate progress based on status
        switch (status) {
            case ServiceStatus::STARTING:
                current_status_.progress_percentage = 5;
                break;
            case ServiceStatus::CHECKING_PROVISIONING:
                current_status_.progress_percentage = 10;
                break;
            case ServiceStatus::ALREADY_PROVISIONED:
            case ServiceStatus::COMPLETED:
                current_status_.progress_percentage = 100;
                break;
            case ServiceStatus::CHECKING_CONNECTIVITY:
                current_status_.progress_percentage = 20;
                break;
            case ServiceStatus::NO_CONNECTIVITY:
                current_status_.progress_percentage = 20;
                break;
            case ServiceStatus::READING_DATABASE:
                current_status_.progress_percentage = 40;
                break;
            case ServiceStatus::GENERATING_CONFIG:
                current_status_.progress_percentage = 60;
                break;
            case ServiceStatus::PROVISIONING:
                current_status_.progress_percentage = 80;
                break;
            case ServiceStatus::ERROR:
                // Keep existing progress
                break;
        }
    }
    
    // Clear error details if not in error state
    if (status != ServiceStatus::ERROR) {
        current_status_.error_details.clear();
    }
    
    write_status_file();
    spdlog::info("Status updated: {} - {}", status_to_string(status), current_status_.message);
}

void StatusReporter::report_error(const std::string& error_message, const std::string& details) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    current_status_.status = ServiceStatus::ERROR;
    current_status_.message = error_message;
    current_status_.error_details = details;
    current_status_.timestamp = std::chrono::system_clock::now();
    
    write_status_file();
    spdlog::error("Error reported: {} - {}", error_message, details);
}

StatusInfo StatusReporter::get_current_status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return current_status_;
}

std::string StatusReporter::status_to_string(ServiceStatus status) {
    switch (status) {
        case ServiceStatus::STARTING:
            return "STARTING";
        case ServiceStatus::CHECKING_PROVISIONING:
            return "CHECKING_PROVISIONING";
        case ServiceStatus::ALREADY_PROVISIONED:
            return "ALREADY_PROVISIONED";
        case ServiceStatus::CHECKING_CONNECTIVITY:
            return "CHECKING_CONNECTIVITY";
        case ServiceStatus::NO_CONNECTIVITY:
            return "NO_CONNECTIVITY";
        case ServiceStatus::READING_DATABASE:
            return "READING_DATABASE";
        case ServiceStatus::GENERATING_CONFIG:
            return "GENERATING_CONFIG";
        case ServiceStatus::PROVISIONING:
            return "PROVISIONING";
        case ServiceStatus::COMPLETED:
            return "COMPLETED";
        case ServiceStatus::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

void StatusReporter::write_status_file() {
    try {
        // Convert timestamp to ISO 8601 string
        auto time_t = std::chrono::system_clock::to_time_t(current_status_.timestamp);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        
        // Create JSON object
        nlohmann::json status_json = {
            {"status", status_to_string(current_status_.status)},
            {"message", current_status_.message},
            {"timestamp", ss.str()},
            {"progress_percentage", current_status_.progress_percentage}
        };
        
        if (!current_status_.error_details.empty()) {
            status_json["error_details"] = current_status_.error_details;
        }
        
        // Write to file atomically
        std::string temp_file = status_file_path_ + ".tmp";
        std::ofstream file(temp_file);
        if (file.is_open()) {
            file << status_json.dump(4) << std::endl;
            file.close();
            
            // Atomic rename
            std::filesystem::rename(temp_file, status_file_path_);
            
            // Set permissions to be readable by monitoring services
            std::filesystem::permissions(status_file_path_,
                std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write |
                std::filesystem::perms::group_read |
                std::filesystem::perms::others_read);
        } else {
            spdlog::error("Failed to open status file for writing: {}", temp_file);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to write status file: {}", e.what());
    }
}

} // namespace status
} // namespace greengrass 