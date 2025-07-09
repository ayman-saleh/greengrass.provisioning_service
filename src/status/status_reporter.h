#pragma once

#include <string>
#include <chrono>
#include <mutex>

namespace greengrass {
namespace status {

enum class ServiceStatus {
    STARTING,
    CHECKING_PROVISIONING,
    ALREADY_PROVISIONED,
    CHECKING_CONNECTIVITY,
    NO_CONNECTIVITY,
    READING_DATABASE,
    GENERATING_CONFIG,
    PROVISIONING,
    COMPLETED,
    ERROR
};

struct StatusInfo {
    ServiceStatus status;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    int progress_percentage = 0;
    std::string error_details;
};

class StatusReporter {
public:
    explicit StatusReporter(const std::string& status_file_path);
    ~StatusReporter() = default;
    
    // Update the current status
    void update_status(ServiceStatus status, const std::string& message = "", int progress = -1);
    
    // Report an error
    void report_error(const std::string& error_message, const std::string& details = "");
    
    // Get the current status
    StatusInfo get_current_status() const;
    
    // Convert status enum to string
    static std::string status_to_string(ServiceStatus status);
    
private:
    void write_status_file();
    
    std::string status_file_path_;
    StatusInfo current_status_;
    mutable std::mutex status_mutex_;
};

} // namespace status
} // namespace greengrass 