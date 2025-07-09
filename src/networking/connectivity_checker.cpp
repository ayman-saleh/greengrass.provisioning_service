#include "networking/connectivity_checker.h"
#include <curl/curl.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdlib> // Required for std::getenv

namespace greengrass {
namespace networking {

// Callback function for CURL to handle response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    // We don't need to store the response, just return the size
    return size * nmemb;
}

ConnectivityChecker::ConnectivityChecker() {
    // Initialize default AWS endpoints
    aws_endpoints_ = {
        "https://iot.us-east-1.amazonaws.com",
        "https://iot.us-west-2.amazonaws.com", 
        "https://greengrass.us-east-1.amazonaws.com",
        "https://www.amazontrust.com"
    };
    
    // Check if running in test mode
    const char* test_mode = std::getenv("TEST_MODE");
    const char* iot_endpoint = std::getenv("IOT_ENDPOINT");
    
    if (test_mode && std::string(test_mode) == "true" && iot_endpoint) {
        // In test mode, use mock endpoints
        spdlog::info("Running in TEST_MODE, using mock endpoint: {}", iot_endpoint);
        aws_endpoints_.clear();
        aws_endpoints_.push_back(fmt::format("http://{}", iot_endpoint));
        custom_iot_endpoint_ = fmt::format("http://{}", iot_endpoint);
    }
    
    // Initialize CURL globally
    curl_global_init(CURL_GLOBAL_ALL);
}

ConnectivityResult ConnectivityChecker::check_connectivity() {
    ConnectivityResult result;
    
    spdlog::info("Starting connectivity check...");
    
    // Step 1: Check DNS resolution
    spdlog::debug("Checking DNS resolution...");
    result.dns_works = check_dns_resolution("amazonaws.com");
    if (!result.dns_works) {
        result.error_message = "DNS resolution failed";
        spdlog::error("DNS resolution check failed");
        return result;
    }
    
    // Step 2: Check HTTPS connectivity to AWS
    spdlog::debug("Checking HTTPS connectivity...");
    std::string test_url = "https://www.amazontrust.com";
    result.https_works = check_https_endpoint(test_url);
    if (!result.https_works) {
        result.error_message = "HTTPS connectivity check failed";
        spdlog::error("HTTPS connectivity check failed");
        return result;
    }
    
    // Step 3: Measure latency
    result.latency = measure_latency(test_url);
    spdlog::debug("Latency to {}: {}ms", test_url, result.latency.count());
    
    // Step 4: Check AWS IoT endpoints
    spdlog::debug("Checking AWS IoT endpoints...");
    if (!custom_iot_endpoint_.empty()) {
        result.tested_endpoints.push_back(custom_iot_endpoint_);
        if (!check_https_endpoint(custom_iot_endpoint_)) {
            result.error_message = "Failed to connect to custom IoT endpoint";
            spdlog::error("Failed to connect to custom IoT endpoint: {}", custom_iot_endpoint_);
            return result;
        }
    } else {
        // Test general AWS IoT connectivity
        bool any_endpoint_works = false;
        for (const auto& endpoint : aws_endpoints_) {
            result.tested_endpoints.push_back(endpoint);
            if (check_https_endpoint(endpoint)) {
                any_endpoint_works = true;
                spdlog::debug("Successfully connected to: {}", endpoint);
                break;
            }
        }
        
        if (!any_endpoint_works) {
            result.error_message = "Failed to connect to any AWS IoT endpoint";
            spdlog::error("Failed to connect to any AWS IoT endpoint");
            return result;
        }
    }
    
    result.is_connected = true;
    spdlog::info("Connectivity check passed. Latency: {}ms", result.latency.count());
    return result;
}

bool ConnectivityChecker::check_dns_resolution(const std::string& hostname) {
    struct hostent* host_entry = gethostbyname(hostname.c_str());
    
    if (host_entry == nullptr) {
        spdlog::debug("Failed to resolve hostname: {}", hostname);
        return false;
    }
    
    // Convert the first IP address to string for logging
    struct in_addr addr;
    addr.s_addr = *((unsigned long*)host_entry->h_addr_list[0]);
    spdlog::debug("Resolved {} to {}", hostname, inet_ntoa(addr));
    
    return true;
}

bool ConnectivityChecker::check_https_endpoint(const std::string& url) {
    return perform_curl_request(url);
}

bool ConnectivityChecker::check_aws_iot_endpoints() {
    if (!custom_iot_endpoint_.empty()) {
        return check_https_endpoint(custom_iot_endpoint_);
    }
    
    // Check if at least one AWS endpoint is reachable
    for (const auto& endpoint : aws_endpoints_) {
        if (check_https_endpoint(endpoint)) {
            return true;
        }
    }
    
    return false;
}

void ConnectivityChecker::set_iot_endpoint(const std::string& endpoint) {
    custom_iot_endpoint_ = endpoint;
    spdlog::debug("Custom IoT endpoint set to: {}", endpoint);
}

void ConnectivityChecker::set_timeout_seconds(int timeout) {
    timeout_seconds_ = timeout;
    spdlog::debug("Connectivity check timeout set to: {} seconds", timeout);
}

bool ConnectivityChecker::perform_curl_request(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        spdlog::error("Failed to initialize CURL");
        return false;
    }
    
    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds_);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout_seconds_ / 2);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Perform only HEAD request to minimize data transfer
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    
    // Enable verbose output in debug mode
    if (spdlog::get_level() == spdlog::level::debug) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }
    
    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    
    bool success = false;
    if (res == CURLE_OK) {
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        // Consider 2xx and 3xx responses as successful
        if (response_code >= 200 && response_code < 400) {
            success = true;
            spdlog::debug("Successfully connected to {} (HTTP {})", url, response_code);
        } else {
            spdlog::debug("HTTP request to {} returned status: {}", url, response_code);
        }
    } else {
        spdlog::debug("CURL request to {} failed: {}", url, curl_easy_strerror(res));
    }
    
    curl_easy_cleanup(curl);
    return success;
}

std::chrono::milliseconds ConnectivityChecker::measure_latency(const std::string& url) {
    auto start = std::chrono::steady_clock::now();
    
    bool success = perform_curl_request(url);
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    if (!success) {
        // Return max value if request failed
        return std::chrono::milliseconds::max();
    }
    
    return duration;
}

} // namespace networking
} // namespace greengrass 