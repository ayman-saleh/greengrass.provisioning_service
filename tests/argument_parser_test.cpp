#include <gtest/gtest.h>
#include <vector>
#include <string>
#include "cli/argument_parser.h"
#include <filesystem>
#include <fstream>

using namespace greengrass::cli;

class ArgumentParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<ArgumentParser>();
        
        // Create test directory
        test_dir_ = std::filesystem::temp_directory_path() / "greengrass_arg_test";
        std::filesystem::create_directories(test_dir_);
        
        // Create test database file
        test_db_path_ = test_dir_ / "test.db";
        std::ofstream db_file(test_db_path_);
        db_file << "test database content";
        db_file.close();
        
        // Create test greengrass directory
        test_gg_path_ = test_dir_ / "greengrass";
        std::filesystem::create_directories(test_gg_path_);
    }
    
    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }
    
    std::unique_ptr<ArgumentParser> parser;
    std::filesystem::path test_dir_;
    std::filesystem::path test_db_path_;
    std::filesystem::path test_gg_path_;
    
    // Helper to convert string args to argc/argv format
    std::pair<int, std::vector<char*>> make_args(const std::vector<std::string>& args) {
        arg_strings = args;
        arg_pointers.clear();
        
        for (auto& arg : arg_strings) {
            arg_pointers.push_back(const_cast<char*>(arg.c_str()));
        }
        
        return {static_cast<int>(arg_pointers.size()), arg_pointers};
    }
    
private:
    std::vector<std::string> arg_strings;
    std::vector<char*> arg_pointers;
};

TEST_F(ArgumentParserTest, ParseValidArguments) {
    auto [argc, argv] = make_args({"prog", 
                                   "--database-path", test_db_path_.string(),
                                   "--greengrass-path", test_gg_path_.string()});
    
    auto result = parser->parse(argc, argv.data());
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->database_path, test_db_path_.string());
    EXPECT_EQ(result->greengrass_path, test_gg_path_.string());
    EXPECT_EQ(result->status_file, "/var/run/greengrass-provisioning.status");
    EXPECT_FALSE(result->verbose);
}

TEST_F(ArgumentParserTest, ParseWithAllOptions) {
    auto status_file = test_dir_ / "custom.status";
    
    auto [argc, argv] = make_args({"prog", 
                                   "--database-path", test_db_path_.string(),
                                   "--greengrass-path", test_gg_path_.string(),
                                   "--status-file", status_file.string(),
                                   "--verbose"});
    
    auto result = parser->parse(argc, argv.data());
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->database_path, test_db_path_.string());
    EXPECT_EQ(result->greengrass_path, test_gg_path_.string());
    EXPECT_EQ(result->status_file, status_file.string());
    EXPECT_TRUE(result->verbose);
}

TEST_F(ArgumentParserTest, ParseMissingRequiredArguments) {
    // Missing greengrass-path
    auto [argc, argv] = make_args({"prog", 
                                   "--database-path", test_db_path_.string()});
    
    auto result = parser->parse(argc, argv.data());
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(ArgumentParserTest, ParseEmptyArguments) {
    auto [argc, argv] = make_args({"greengrass_provisioning_service"});
    
    auto result = parser->parse(argc, argv.data());
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(ArgumentParserTest, ParseHelpFlag) {
    auto [argc, argv] = make_args({
        "greengrass_provisioning_service",
        "--help"
    });
    
    auto result = parser->parse(argc, argv.data());
    
    // Help should cause parse to return nullopt
    EXPECT_FALSE(result.has_value());
}

TEST_F(ArgumentParserTest, ParseShortFormArguments) {
    auto [argc, argv] = make_args({"prog", 
                                   "-d", test_db_path_.string(),
                                   "-g", test_gg_path_.string(),
                                   "-v"});
    
    auto result = parser->parse(argc, argv.data());
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->database_path, test_db_path_.string());
    EXPECT_EQ(result->greengrass_path, test_gg_path_.string());
    EXPECT_TRUE(result->verbose);
}

TEST_F(ArgumentParserTest, GetHelpMessage) {
    std::string help = parser->get_help_message();
    
    // Check that help message contains key information
    EXPECT_NE(help.find("Greengrass"), std::string::npos);
    EXPECT_NE(help.find("--database-path"), std::string::npos);
    EXPECT_NE(help.find("--greengrass-path"), std::string::npos);
    EXPECT_NE(help.find("--status-file"), std::string::npos);
    EXPECT_NE(help.find("--verbose"), std::string::npos);
}

TEST_F(ArgumentParserTest, ParseInvalidOption) {
    auto [argc, argv] = make_args({"prog", 
                                   "--database-path", test_db_path_.string(),
                                   "--greengrass-path", test_gg_path_.string(),
                                   "--invalid-option", "value"});
    
    auto result = parser->parse(argc, argv.data());
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(ArgumentParserTest, DefaultStatusFile) {
    auto [argc, argv] = make_args({"prog", 
                                   "-d", test_db_path_.string(),
                                   "-g", test_gg_path_.string()});
    
    auto result = parser->parse(argc, argv.data());
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->status_file, "/var/run/greengrass-provisioning.status");
}

TEST_F(ArgumentParserTest, VerboseFlagVariations) {
    // Test --verbose
    auto [argc1, argv1] = make_args({"prog", 
                                     "-d", test_db_path_.string(),
                                     "-g", test_gg_path_.string(),
                                     "--verbose"});
    
    auto result1 = parser->parse(argc1, argv1.data());
    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result1->verbose);
    
    // Test -v
    auto [argc2, argv2] = make_args({"prog", 
                                     "-d", test_db_path_.string(),
                                     "-g", test_gg_path_.string(),
                                     "-v"});
    
    auto result2 = parser->parse(argc2, argv2.data());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result2->verbose);
}

TEST_F(ArgumentParserTest, NonExistentDatabaseFile) {
    auto [argc, argv] = make_args({"prog", 
                                   "--database-path", "/non/existent/file.db",
                                   "--greengrass-path", test_gg_path_.string()});
    
    auto result = parser->parse(argc, argv.data());
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(ArgumentParserTest, NonExistentGreengrassDirectory) {
    auto [argc, argv] = make_args({"prog", 
                                   "--database-path", test_db_path_.string(),
                                   "--greengrass-path", "/non/existent/directory"});
    
    auto result = parser->parse(argc, argv.data());
    
    EXPECT_FALSE(result.has_value());
}
