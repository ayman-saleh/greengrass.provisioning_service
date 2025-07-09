#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace greengrass {
namespace networking {

struct ConnectivityResult {
    bool is_connected = false;
    bool dns_works = false;
    bool https_works = false;
    std::string error_message;
    std::chrono::milliseconds latency{0};
    std::vector<std::string> tested_endpoints;
};

class ConnectivityChecker {
public:
    ConnectivityChecker();
    ~ConnectivityChecker() = default;
    
    // Check overall connectivity
    ConnectivityResult check_connectivity();
    
    // Check specific connectivity aspects
    bool check_dns_resolution(const std::string& hostname);
    bool check_https_endpoint(const std::string& url);
    bool check_aws_iot_endpoints();
    
    // Set custom AWS IoT endpoint for testing
    void set_iot_endpoint(const std::string& endpoint);
    
    // Set timeout for connectivity checks
    void set_timeout_seconds(int timeout);
    
private:
    std::vector<std::string> aws_endpoints_;
    std::string custom_iot_endpoint_;
    int timeout_seconds_ = 10;
    
    // Helper methods
    bool perform_curl_request(const std::string& url);
    std::chrono::milliseconds measure_latency(const std::string& url);
};

} // namespace networking
} // namespace greengrass 