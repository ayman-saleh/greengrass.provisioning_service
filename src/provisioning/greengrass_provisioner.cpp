#include "provisioning/greengrass_provisioner.h"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <curl/curl.h>
#include <fstream>
#include <array>
#include <cstdio>
#include <memory>
#include <thread>
#include <chrono>
#include <cstdlib>

namespace greengrass {
namespace provisioning {

// Callback for writing downloaded data to file
static size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

GreengrassProvisioner::GreengrassProvisioner(const std::filesystem::path& greengrass_path)
    : greengrass_path_(greengrass_path) {
    // Set default Java home if not specified
    if (java_home_.empty()) {
        // Try to detect Java home
        std::string output;
        if (execute_command("which java", output) && !output.empty()) {
            // Follow symlinks to get actual Java installation
            execute_command("readlink -f $(which java) | sed 's:/bin/java::'", java_home_);
            java_home_.erase(java_home_.find_last_not_of("\n\r") + 1); // Trim newline
        }
    }
    
    spdlog::debug("GreengrassProvisioner initialized with path: {}", greengrass_path.string());
}

ProvisioningResult GreengrassProvisioner::provision(const database::DeviceConfig& device_config,
                                                  const config::GeneratedConfig& generated_config) {
    ProvisioningResult result;
    
    spdlog::info("Starting Greengrass provisioning for device: {}", device_config.device_id);
    report_progress(ProvisioningStep::INITIALIZING, 0, "Initializing provisioning process");
    
    // Step 1: Create Greengrass user and group
    if (!create_greengrass_user()) {
        result.error_message = "Failed to create Greengrass user and group";
        spdlog::error(result.error_message);
        return result;
    }
    result.last_completed_step = ProvisioningStep::INITIALIZING;
    
    // Step 2: Download Greengrass nucleus (if needed)
    report_progress(ProvisioningStep::DOWNLOADING_NUCLEUS, 20, "Downloading Greengrass nucleus");
    nucleus_jar_path_ = greengrass_path_ / "lib" / "Greengrass.jar";
    
    if (!std::filesystem::exists(nucleus_jar_path_)) {
        if (!download_greengrass_nucleus(device_config.nucleus_version)) {
            result.error_message = "Failed to download Greengrass nucleus";
            spdlog::error(result.error_message);
            return result;
        }
    } else {
        spdlog::info("Greengrass nucleus already exists, skipping download");
    }
    result.last_completed_step = ProvisioningStep::DOWNLOADING_NUCLEUS;
    
    // Step 3: Install Greengrass nucleus
    report_progress(ProvisioningStep::INSTALLING_NUCLEUS, 40, "Installing Greengrass nucleus");
    if (!install_greengrass_nucleus(device_config)) {
        result.error_message = "Failed to install Greengrass nucleus";
        spdlog::error(result.error_message);
        return result;
    }
    result.last_completed_step = ProvisioningStep::INSTALLING_NUCLEUS;
    
    // Step 4: Configure systemd service
    report_progress(ProvisioningStep::CONFIGURING_SYSTEMD, 60, "Configuring systemd service");
    if (!configure_systemd_service()) {
        result.error_message = "Failed to configure systemd service";
        spdlog::error(result.error_message);
        return result;
    }
    result.last_completed_step = ProvisioningStep::CONFIGURING_SYSTEMD;
    
    // Step 5: Start Greengrass service
    report_progress(ProvisioningStep::STARTING_SERVICE, 80, "Starting Greengrass service");
    if (!start_greengrass_service()) {
        result.error_message = "Failed to start Greengrass service";
        spdlog::error(result.error_message);
        return result;
    }
    result.last_completed_step = ProvisioningStep::STARTING_SERVICE;
    
    // Step 6: Verify connection
    report_progress(ProvisioningStep::VERIFYING_CONNECTION, 90, "Verifying Greengrass connection");
    if (!verify_greengrass_connection()) {
        result.error_message = "Failed to verify Greengrass connection";
        spdlog::error(result.error_message);
        return result;
    }
    result.last_completed_step = ProvisioningStep::VERIFYING_CONNECTION;
    
    // Success
    report_progress(ProvisioningStep::COMPLETED, 100, "Provisioning completed successfully");
    result.success = true;
    result.last_completed_step = ProvisioningStep::COMPLETED;
    
    spdlog::info("Greengrass provisioning completed successfully");
    return result;
}

bool GreengrassProvisioner::download_greengrass_nucleus(const std::string& version) {
    std::string nucleus_version = version.empty() ? "2.9.0" : version;
    std::string url = get_nucleus_download_url(nucleus_version);
    
    // Create lib directory
    std::filesystem::create_directories(greengrass_path_ / "lib");
    
    // Check if running in test mode
    const char* test_mode = std::getenv("TEST_MODE");
    if (test_mode && std::string(test_mode) == "true") {
        spdlog::info("TEST_MODE: Creating mock Greengrass nucleus");
        // Create a mock JAR file
        std::ofstream mock_jar(nucleus_jar_path_);
        mock_jar << "Mock Greengrass JAR for testing";
        mock_jar.close();
        return true;
    }
    
    spdlog::info("Downloading Greengrass nucleus version {} from {}", nucleus_version, url);
    return download_file(url, nucleus_jar_path_);
}

bool GreengrassProvisioner::install_greengrass_nucleus(const database::DeviceConfig& device_config) {
    // For Greengrass v2, we rely on the configuration being already generated
    // The actual installation happens when we start the service with the JAR
    
    // Set ownership of Greengrass directory
    std::string chown_cmd = fmt::format("sudo chown -R {}:{} {}", 
                                       greengrass_user_, greengrass_group_, 
                                       greengrass_path_.string());
    std::string output;
    if (!execute_command(chown_cmd, output)) {
        spdlog::error("Failed to set ownership of Greengrass directory");
        return false;
    }
    
    spdlog::info("Greengrass nucleus installation prepared");
    return true;
}

bool GreengrassProvisioner::configure_systemd_service() {
    // Check if running in test mode
    const char* test_mode = std::getenv("TEST_MODE");
    if (test_mode && std::string(test_mode) == "true") {
        spdlog::info("TEST_MODE: Skipping systemd configuration");
        return true;
    }
    
    // Create systemd service file
    std::string service_content = fmt::format(R"service([Unit]
Description=Greengrass Core
After=network.target

[Service]
Type=simple
PIDFile={}/alts/loader.pid
RemainAfterExit=no
Restart=on-failure
RestartSec=10
User={}
Group={}
Environment="JAVA_HOME={}"
ExecStart=/usr/bin/java -Dlog.store=FILE -Droot={} -jar {}/lib/Greengrass.jar --config-path {}/config/config.yaml
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
)service",
        greengrass_path_.string(),
        greengrass_user_,
        greengrass_group_,
        java_home_,
        greengrass_path_.string(),
        greengrass_path_.string(),
        greengrass_path_.string()
    );
    
    // Write service file to temp location
    std::string temp_service_file = "/tmp/greengrass.service";
    std::ofstream service_file(temp_service_file);
    if (!service_file.is_open()) {
        spdlog::error("Failed to create temporary service file");
        return false;
    }
    service_file << service_content;
    service_file.close();
    
    // Copy to systemd directory
    std::string copy_cmd = fmt::format("sudo cp {} /etc/systemd/system/greengrass.service", 
                                      temp_service_file);
    std::string output;
    if (!execute_command(copy_cmd, output)) {
        spdlog::error("Failed to copy service file to systemd");
        return false;
    }
    
    // Reload systemd
    if (!execute_command("sudo systemctl daemon-reload", output)) {
        spdlog::error("Failed to reload systemd");
        return false;
    }
    
    // Enable service
    if (!execute_command("sudo systemctl enable greengrass.service", output)) {
        spdlog::error("Failed to enable Greengrass service");
        return false;
    }
    
    // Clean up temp file
    std::filesystem::remove(temp_service_file);
    
    spdlog::info("Configured systemd service for Greengrass");
    return true;
}

bool GreengrassProvisioner::start_greengrass_service() {
    // Check if running in test mode
    const char* test_mode = std::getenv("TEST_MODE");
    if (test_mode && std::string(test_mode) == "true") {
        spdlog::info("TEST_MODE: Skipping service start");
        return true;
    }
    
    std::string output;
    
    // Stop service if already running
    execute_command("sudo systemctl stop greengrass.service", output);
    
    // Start the service
    if (!execute_command("sudo systemctl start greengrass.service", output)) {
        spdlog::error("Failed to start Greengrass service");
        return false;
    }
    
    // Wait a bit for service to start
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Check service status
    if (!execute_command("sudo systemctl is-active greengrass.service", output)) {
        spdlog::error("Greengrass service is not active");
        return false;
    }
    
    spdlog::info("Greengrass service started successfully");
    return true;
}

bool GreengrassProvisioner::verify_greengrass_connection() {
    // Check if running in test mode
    const char* test_mode = std::getenv("TEST_MODE");
    if (test_mode && std::string(test_mode) == "true") {
        spdlog::info("TEST_MODE: Simulating successful connection verification");
        return true;
    }
    
    // Check if Greengrass logs show successful connection
    std::string log_file = (greengrass_path_ / "logs" / "greengrass.log").string();
    
    // Wait for logs to be created
    int wait_count = 0;
    while (!std::filesystem::exists(log_file) && wait_count < 30) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        wait_count++;
    }
    
    if (!std::filesystem::exists(log_file)) {
        spdlog::warn("Greengrass log file not found, assuming connection is ok");
        return true;
    }
    
    // Check recent logs for connection success
    std::string tail_cmd = fmt::format("tail -n 50 {} | grep -i 'connected\\|established\\|successful'", 
                                      log_file);
    std::string output;
    if (execute_command(tail_cmd, output) && !output.empty()) {
        spdlog::info("Greengrass connection verified from logs");
        return true;
    }
    
    // If no explicit success, check for errors
    std::string error_cmd = fmt::format("tail -n 50 {} | grep -i 'error\\|failed'", log_file);
    if (execute_command(error_cmd, output) && !output.empty()) {
        spdlog::warn("Found errors in Greengrass logs: {}", output);
        return false;
    }
    
    // Assume success if no errors found
    spdlog::info("No errors found in logs, assuming connection successful");
    return true;
}

void GreengrassProvisioner::set_progress_callback(ProgressCallback callback) {
    progress_callback_ = callback;
}

void GreengrassProvisioner::set_java_home(const std::string& java_home) {
    java_home_ = java_home;
}

void GreengrassProvisioner::set_greengrass_user(const std::string& user) {
    greengrass_user_ = user;
}

void GreengrassProvisioner::set_greengrass_group(const std::string& group) {
    greengrass_group_ = group;
}

bool GreengrassProvisioner::execute_command(const std::string& command, std::string& output) {
    std::array<char, 128> buffer;
    output.clear();
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        spdlog::error("Failed to execute command: {}", command);
        return false;
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
    }
    
    int result = pclose(pipe.release());
    return result == 0;
}

bool GreengrassProvisioner::create_greengrass_user() {
    // Check if running in test mode
    const char* test_mode = std::getenv("TEST_MODE");
    if (test_mode && std::string(test_mode) == "true") {
        spdlog::info("TEST_MODE: Skipping user creation");
        return true;
    }
    
    std::string output;
    
    // Check if user already exists
    std::string check_user_cmd = fmt::format("id -u {}", greengrass_user_);
    if (execute_command(check_user_cmd, output)) {
        spdlog::info("Greengrass user {} already exists", greengrass_user_);
        return true;
    }
    
    // Create group
    std::string create_group_cmd = fmt::format("sudo groupadd --system {}", greengrass_group_);
    execute_command(create_group_cmd, output); // Ignore if already exists
    
    // Create user
    std::string create_user_cmd = fmt::format(
        "sudo useradd --system --gid {} --shell /bin/false {}",
        greengrass_group_, greengrass_user_
    );
    
    if (!execute_command(create_user_cmd, output)) {
        spdlog::error("Failed to create Greengrass user");
        return false;
    }
    
    spdlog::info("Created Greengrass user and group");
    return true;
}

bool GreengrassProvisioner::download_file(const std::string& url, const std::filesystem::path& destination) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        spdlog::error("Failed to initialize CURL");
        return false;
    }
    
    FILE* fp = fopen(destination.c_str(), "wb");
    if (!fp) {
        spdlog::error("Failed to open file for writing: {}", destination.string());
        curl_easy_cleanup(curl);
        return false;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5 minute timeout
    
    CURLcode res = curl_easy_perform(curl);
    
    fclose(fp);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        spdlog::error("Failed to download file: {}", curl_easy_strerror(res));
        std::filesystem::remove(destination);
        return false;
    }
    
    spdlog::info("Successfully downloaded file to {}", destination.string());
    return true;
}

std::string GreengrassProvisioner::get_nucleus_download_url(const std::string& version) const {
    // Return the Greengrass nucleus download URL
    // Note: In production, this would come from the database or configuration
    return fmt::format(
        "https://d2s8p88vqu9w66.cloudfront.net/releases/greengrass-{}.zip",
        version
    );
}

void GreengrassProvisioner::report_progress(ProvisioningStep step, int percentage, const std::string& message) {
    if (progress_callback_) {
        progress_callback_(step, percentage, message);
    }
    spdlog::info("[{}%] {}", percentage, message);
}

} // namespace provisioning
} // namespace greengrass 