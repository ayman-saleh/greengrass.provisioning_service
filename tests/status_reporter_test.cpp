#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include "status/status_reporter.h"

using namespace greengrass::status;

class StatusReporterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory for test
        test_dir_ = std::filesystem::temp_directory_path() / "greengrass_test";
        std::filesystem::create_directories(test_dir_);
        status_file_ = test_dir_ / "test_status.json";
    }
    
    void TearDown() override {
        // Clean up test directory
        std::filesystem::remove_all(test_dir_);
    }
    
    nlohmann::json read_status_file() {
        std::ifstream file(status_file_);
        nlohmann::json status_json;
        file >> status_json;
        return status_json;
    }
    
    std::filesystem::path test_dir_;
    std::filesystem::path status_file_;
};

TEST_F(StatusReporterTest, CreateStatusFile) {
    StatusReporter reporter(status_file_.string());
    
    // Check that status file was created
    EXPECT_TRUE(std::filesystem::exists(status_file_));
    
    // Read and parse the status file
    auto status_json = read_status_file();
    
    // Verify initial status
    EXPECT_EQ(status_json["status"], "STARTING");
    EXPECT_EQ(status_json["progress_percentage"], 0);
    EXPECT_TRUE(status_json.contains("timestamp"));
}

TEST_F(StatusReporterTest, UpdateStatus) {
    StatusReporter reporter(status_file_.string());
    
    // Update status
    reporter.update_status(ServiceStatus::CHECKING_CONNECTIVITY, "Testing connectivity", 25);
    
    // Read and verify updated status
    auto status_json = read_status_file();
    
    EXPECT_EQ(status_json["status"], "CHECKING_CONNECTIVITY");
    EXPECT_EQ(status_json["message"], "Testing connectivity");
    EXPECT_EQ(status_json["progress_percentage"], 25);
    EXPECT_TRUE(status_json.contains("timestamp"));
}

TEST_F(StatusReporterTest, UpdateAllStatuses) {
    StatusReporter reporter(status_file_.string());
    
    // Test all status values
    struct StatusTest {
        ServiceStatus status;
        std::string expected_string;
        int progress;
    };
    
    std::vector<StatusTest> tests = {
        {ServiceStatus::STARTING, "STARTING", 0},
        {ServiceStatus::CHECKING_PROVISIONING, "CHECKING_PROVISIONING", 10},
        {ServiceStatus::ALREADY_PROVISIONED, "ALREADY_PROVISIONED", 100},
        {ServiceStatus::CHECKING_CONNECTIVITY, "CHECKING_CONNECTIVITY", 20},
        {ServiceStatus::NO_CONNECTIVITY, "NO_CONNECTIVITY", 100},
        {ServiceStatus::READING_DATABASE, "READING_DATABASE", 30},
        {ServiceStatus::GENERATING_CONFIG, "GENERATING_CONFIG", 50},
        {ServiceStatus::PROVISIONING, "PROVISIONING", 75},
        {ServiceStatus::COMPLETED, "COMPLETED", 100},
        {ServiceStatus::ERROR, "ERROR", 100}
    };
    
    for (const auto& test : tests) {
        reporter.update_status(test.status, "Test message", test.progress);
        auto status_json = read_status_file();
        EXPECT_EQ(status_json["status"], test.expected_string);
        EXPECT_EQ(status_json["progress_percentage"], test.progress);
    }
}

TEST_F(StatusReporterTest, ReportError) {
    StatusReporter reporter(status_file_.string());
    
    // Report an error
    reporter.report_error("Test error", "Error details");
    
    // Read and verify error status
    auto status_json = read_status_file();
    
    EXPECT_EQ(status_json["status"], "ERROR");
    EXPECT_EQ(status_json["message"], "Test error");
    EXPECT_EQ(status_json["error_details"], "Error details");
    // Progress percentage is not set by report_error, it keeps existing progress
    EXPECT_EQ(status_json["progress_percentage"], 0);  // Initial progress from constructor
}

TEST_F(StatusReporterTest, ReportErrorEmptyDetails) {
    StatusReporter reporter(status_file_.string());
    
    // Report an error with empty details
    reporter.report_error("Simple error", "");
    
    auto status_json = read_status_file();
    
    EXPECT_EQ(status_json["status"], "ERROR");
    EXPECT_EQ(status_json["message"], "Simple error");
    // Empty error_details won't be included in JSON
    EXPECT_FALSE(status_json.contains("error_details"));
}

TEST_F(StatusReporterTest, StatusToString) {
    EXPECT_EQ(StatusReporter::status_to_string(ServiceStatus::STARTING), "STARTING");
    EXPECT_EQ(StatusReporter::status_to_string(ServiceStatus::CHECKING_PROVISIONING), "CHECKING_PROVISIONING");
    EXPECT_EQ(StatusReporter::status_to_string(ServiceStatus::ALREADY_PROVISIONED), "ALREADY_PROVISIONED");
    EXPECT_EQ(StatusReporter::status_to_string(ServiceStatus::CHECKING_CONNECTIVITY), "CHECKING_CONNECTIVITY");
    EXPECT_EQ(StatusReporter::status_to_string(ServiceStatus::NO_CONNECTIVITY), "NO_CONNECTIVITY");
    EXPECT_EQ(StatusReporter::status_to_string(ServiceStatus::READING_DATABASE), "READING_DATABASE");
    EXPECT_EQ(StatusReporter::status_to_string(ServiceStatus::GENERATING_CONFIG), "GENERATING_CONFIG");
    EXPECT_EQ(StatusReporter::status_to_string(ServiceStatus::PROVISIONING), "PROVISIONING");
    EXPECT_EQ(StatusReporter::status_to_string(ServiceStatus::COMPLETED), "COMPLETED");
    EXPECT_EQ(StatusReporter::status_to_string(ServiceStatus::ERROR), "ERROR");
}

TEST_F(StatusReporterTest, EmptyStatusMessage) {
    StatusReporter reporter(status_file_.string());
    
    // Update with empty message
    reporter.update_status(ServiceStatus::PROVISIONING, "", 50);
    
    auto status_json = read_status_file();
    
    EXPECT_EQ(status_json["status"], "PROVISIONING");
    // Empty message triggers default message for PROVISIONING status
    EXPECT_EQ(status_json["message"], "Provisioning Greengrass device");
}

TEST_F(StatusReporterTest, ProgressBoundaries) {
    StatusReporter reporter(status_file_.string());
    
    // Test progress at boundaries
    reporter.update_status(ServiceStatus::CHECKING_CONNECTIVITY, "0% progress", 0);
    auto status_json = read_status_file();
    EXPECT_EQ(status_json["progress_percentage"], 0);
    
    reporter.update_status(ServiceStatus::PROVISIONING, "100% progress", 100);
    status_json = read_status_file();
    EXPECT_EQ(status_json["progress_percentage"], 100);
    
    // Test negative progress (should be clamped)
    reporter.update_status(ServiceStatus::PROVISIONING, "Negative progress", -10);
    status_json = read_status_file();
    EXPECT_GE(status_json["progress_percentage"], 0);
    
    // Test over 100 progress (should be clamped)
    reporter.update_status(ServiceStatus::PROVISIONING, "Over progress", 150);
    status_json = read_status_file();
    EXPECT_LE(status_json["progress_percentage"], 100);
}

TEST_F(StatusReporterTest, LongMessages) {
    StatusReporter reporter(status_file_.string());
    
    // Test with a very long message
    std::string long_message(1000, 'A');
    reporter.update_status(ServiceStatus::PROVISIONING, long_message, 50);
    
    auto status_json = read_status_file();
    
    EXPECT_EQ(status_json["status"], "PROVISIONING");
    EXPECT_EQ(status_json["message"], long_message);
}

TEST_F(StatusReporterTest, SpecialCharactersInMessage) {
    StatusReporter reporter(status_file_.string());
    
    // Test with special characters
    std::string special_message = "Test with \"quotes\" and \nnewlines\t and tabs";
    reporter.update_status(ServiceStatus::PROVISIONING, special_message, 50);
    
    auto status_json = read_status_file();
    
    EXPECT_EQ(status_json["message"], special_message);
}

TEST_F(StatusReporterTest, NonExistentDirectory) {
    // Test creating status file in non-existent directory
    auto non_existent = test_dir_ / "non_existent" / "path" / "status.json";
    
    StatusReporter reporter(non_existent.string());
    
    // Should create the directory structure
    EXPECT_TRUE(std::filesystem::exists(non_existent));
}

TEST_F(StatusReporterTest, AtomicFileWrite) {
    StatusReporter reporter(status_file_.string());
    
    // Initial status
    reporter.update_status(ServiceStatus::STARTING, "Initial", 0);
    
    // Simulate rapid updates to test atomic writes
    for (int i = 0; i < 10; ++i) {
        reporter.update_status(ServiceStatus::PROVISIONING, 
                             "Rapid update " + std::to_string(i), i * 10);
    }
    
    // Final read should have the last update
    auto status_json = read_status_file();
    EXPECT_EQ(status_json["message"], "Rapid update 9");
    EXPECT_EQ(status_json["progress_percentage"], 90);
}

TEST_F(StatusReporterTest, TimestampFormat) {
    StatusReporter reporter(status_file_.string());
    
    reporter.update_status(ServiceStatus::PROVISIONING, "Test", 50);
    
    auto status_json = read_status_file();
    
    // Check that timestamp exists and is a string
    EXPECT_TRUE(status_json.contains("timestamp"));
    EXPECT_TRUE(status_json["timestamp"].is_string());
    
    // Check timestamp format (ISO 8601)
    std::string timestamp = status_json["timestamp"];
    EXPECT_NE(timestamp.find('T'), std::string::npos);  // Contains 'T' separator
    EXPECT_NE(timestamp.find('Z'), std::string::npos);  // Contains 'Z' for UTC
}

TEST_F(StatusReporterTest, MultipleConcurrentReporters) {
    // Test that multiple reporters can update the same file
    auto reporter1 = std::make_unique<StatusReporter>(status_file_.string());
    auto reporter2 = std::make_unique<StatusReporter>(status_file_.string());
    
    reporter1->update_status(ServiceStatus::CHECKING_CONNECTIVITY, "From reporter 1", 25);
    reporter2->update_status(ServiceStatus::PROVISIONING, "From reporter 2", 50);
    
    // The last write should win
    auto status_json = read_status_file();
    EXPECT_EQ(status_json["status"], "PROVISIONING");
    EXPECT_EQ(status_json["message"], "From reporter 2");
}

TEST_F(StatusReporterTest, DestructorBehavior) {
    {
        StatusReporter reporter(status_file_.string());
        reporter.update_status(ServiceStatus::PROVISIONING, "Before destruction", 75);
    }  // Reporter goes out of scope
    
    // File should still exist and be readable after destruction
    EXPECT_TRUE(std::filesystem::exists(status_file_));
    
    auto status_json = read_status_file();
    EXPECT_EQ(status_json["status"], "PROVISIONING");
    EXPECT_EQ(status_json["message"], "Before destruction");
} 