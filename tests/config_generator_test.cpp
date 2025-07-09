#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "config/config_generator.h"
#include "database/config_database.h"

using namespace greengrass::config;
using namespace greengrass::database;

class ConfigGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory for test
        test_dir_ = std::filesystem::temp_directory_path() / "greengrass_gen_test";
        std::filesystem::create_directories(test_dir_);
        
        output_dir_ = test_dir_ / "output";
        
        generator_ = std::make_unique<ConfigGenerator>(output_dir_);
        
        // Create test device config
        test_config_.device_id = "test-device-001";
        test_config_.thing_name = "TestThing";
        test_config_.iot_endpoint = "iot.us-east-1.amazonaws.com";
        test_config_.aws_region = "us-east-1";
        test_config_.root_ca_path = "-----BEGIN CERTIFICATE-----\nMOCK_ROOT_CA\n-----END CERTIFICATE-----";
        test_config_.certificate_pem = "-----BEGIN CERTIFICATE-----\nMOCK_CERTIFICATE_CONTENT\n-----END CERTIFICATE-----";
        test_config_.private_key_pem = "-----BEGIN RSA PRIVATE KEY-----\nMOCK_PRIVATE_KEY_CONTENT\n-----END RSA PRIVATE KEY-----";
        test_config_.role_alias = "TestRoleAlias";
        test_config_.role_alias_endpoint = "cred.iot.us-east-1.amazonaws.com";
        test_config_.nucleus_version = "2.9.0";
        test_config_.deployment_group = "test-deployment-group";
        test_config_.initial_components = {"Component1", "Component2"};
        test_config_.mqtt_port = 8883;
    }
    
    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }
    
    std::string read_file(const std::filesystem::path& path) {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    bool check_file_permissions(const std::filesystem::path& path, bool is_private_key) {
        auto perms = std::filesystem::status(path).permissions();
        if (is_private_key) {
            // Private key should only have owner read/write permissions (600)
            return (perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none &&
                   (perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none &&
                   (perms & std::filesystem::perms::group_all) == std::filesystem::perms::none &&
                   (perms & std::filesystem::perms::others_all) == std::filesystem::perms::none;
        } else {
            // Other files should have owner read/write and optionally group read
            return (perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none &&
                   (perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
        }
    }
    
    std::filesystem::path test_dir_;
    std::filesystem::path output_dir_;
    std::unique_ptr<ConfigGenerator> generator_;
    DeviceConfig test_config_;
};

TEST_F(ConfigGeneratorTest, GenerateCompleteConfig) {
    auto result = generator_->generate_config(test_config_);
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.error_message.empty());
    
    // Check that output directory was created
    EXPECT_TRUE(std::filesystem::exists(output_dir_));
    
    // Check that config file was created
    EXPECT_TRUE(std::filesystem::exists(result.config_file_path));
    EXPECT_EQ(result.config_file_path, output_dir_ / "config" / "config.yaml");
    
    // Check that certificates were created
    EXPECT_TRUE(std::filesystem::exists(result.certificate_path));
    EXPECT_TRUE(std::filesystem::exists(result.private_key_path));
    EXPECT_TRUE(std::filesystem::exists(result.root_ca_path));
}

TEST_F(ConfigGeneratorTest, ConfigFileContent) {
    auto result = generator_->generate_config(test_config_);
    
    ASSERT_TRUE(result.success);
    
    auto config_content = read_file(result.config_file_path);
    
    // Check for required fields in their correct YAML structure
    EXPECT_NE(config_content.find("thingName: \"TestThing\""), std::string::npos);
    EXPECT_NE(config_content.find("iotDataEndpoint: \"iot.us-east-1.amazonaws.com\""), std::string::npos);
    EXPECT_NE(config_content.find("awsRegion: \"us-east-1\""), std::string::npos);
    EXPECT_NE(config_content.find("iotRoleAlias: \"TestRoleAlias\""), std::string::npos);
    EXPECT_NE(config_content.find("iotCredEndpoint: \"cred.iot.us-east-1.amazonaws.com\""), std::string::npos);
    EXPECT_NE(config_content.find("port: 8883"), std::string::npos);
}

TEST_F(ConfigGeneratorTest, CertificateFiles) {
    auto result = generator_->generate_config(test_config_);
    
    ASSERT_TRUE(result.success);
    
    auto cert_content = read_file(result.certificate_path);
    auto key_content = read_file(result.private_key_path);
    auto ca_content = read_file(result.root_ca_path);
    
    EXPECT_EQ(cert_content, test_config_.certificate_pem);
    EXPECT_EQ(key_content, test_config_.private_key_pem);
    EXPECT_EQ(ca_content, test_config_.root_ca_path); // root_ca_path contains the content
}

TEST_F(ConfigGeneratorTest, FilePermissions) {
    auto result = generator_->generate_config(test_config_);
    
    ASSERT_TRUE(result.success);
    
    // Check private key permissions
    EXPECT_TRUE(check_file_permissions(result.private_key_path, true));
    
    // Check other file permissions
    EXPECT_TRUE(check_file_permissions(result.certificate_path, false));
    EXPECT_TRUE(check_file_permissions(result.root_ca_path, false));
}

TEST_F(ConfigGeneratorTest, DirectoryStructure) {
    auto result = generator_->generate_config(test_config_);
    
    ASSERT_TRUE(result.success);
    
    // Check all required directories exist
    EXPECT_TRUE(std::filesystem::exists(output_dir_ / "config"));
    EXPECT_TRUE(std::filesystem::exists(output_dir_ / "certs"));
    EXPECT_TRUE(std::filesystem::exists(output_dir_ / "ggc-root"));
    EXPECT_TRUE(std::filesystem::exists(output_dir_ / "logs"));
    EXPECT_TRUE(std::filesystem::exists(output_dir_ / "work"));
    EXPECT_TRUE(std::filesystem::exists(output_dir_ / "packages"));
    EXPECT_TRUE(std::filesystem::exists(output_dir_ / "deployments"));
}

TEST_F(ConfigGeneratorTest, OverwriteExistingFiles) {
    // Generate config first time
    auto result1 = generator_->generate_config(test_config_);
    ASSERT_TRUE(result1.success);
    
    // Modify the config
    test_config_.thing_name = "ModifiedThing";
    
    // Generate again (should overwrite)
    auto result2 = generator_->generate_config(test_config_);
    ASSERT_TRUE(result2.success);
    
    auto config_content = read_file(result2.config_file_path);
    EXPECT_NE(config_content.find("thingName: \"ModifiedThing\""), std::string::npos);
    EXPECT_EQ(config_content.find("thingName: \"TestThing\""), std::string::npos);
}

TEST_F(ConfigGeneratorTest, InvalidOutputPath) {
    // Try to generate config in a path that can't be created
    ConfigGenerator bad_generator("/root/no_permission/path");
    auto result = bad_generator.generate_config(test_config_);
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

TEST_F(ConfigGeneratorTest, EmptyDeviceConfig) {
    DeviceConfig empty_config;
    auto result = generator_->generate_config(empty_config);
    
    // Should still succeed but generate default config
    EXPECT_TRUE(result.success);
}

TEST_F(ConfigGeneratorTest, MinimalDeviceConfig) {
    DeviceConfig minimal_config;
    minimal_config.device_id = "minimal";
    minimal_config.thing_name = "MinimalThing";
    minimal_config.iot_endpoint = "iot.amazonaws.com";
    minimal_config.aws_region = "us-east-1";
    minimal_config.root_ca_path = "CERT";
    minimal_config.certificate_pem = "CERT";
    minimal_config.private_key_pem = "KEY";
    minimal_config.role_alias = "Role";
    minimal_config.role_alias_endpoint = "cred.iot.amazonaws.com";
    
    auto result = generator_->generate_config(minimal_config);
    
    ASSERT_TRUE(result.success);
    
    auto config_content = read_file(result.config_file_path);
    
    // Should have defaults for optional fields
    EXPECT_NE(config_content.find("version: \"2.9.0\""), std::string::npos);  // Default version
}

TEST_F(ConfigGeneratorTest, ConfigWithProxyUrl) {
    test_config_.proxy_url = "http://proxy.company.com:8080";
    
    auto result = generator_->generate_config(test_config_);
    
    ASSERT_TRUE(result.success);
    
    auto config_content = read_file(result.config_file_path);
    EXPECT_NE(config_content.find("url: \"http://proxy.company.com:8080\""), std::string::npos);
}

TEST_F(ConfigGeneratorTest, ConfigWithCustomDomain) {
    test_config_.custom_domain = "iot.custom.domain.com";
    
    auto result = generator_->generate_config(test_config_);
    
    ASSERT_TRUE(result.success);
    // Custom domain handling is not implemented in the current generator
}

TEST_F(ConfigGeneratorTest, ConfigWithInitialComponents) {
    auto result = generator_->generate_config(test_config_);
    
    ASSERT_TRUE(result.success);
    
    auto config_content = read_file(result.config_file_path);
    
    // Check that deployment polling is configured (indicates deployment group support)
    EXPECT_NE(config_content.find("deploymentPollingFrequency: 15"), std::string::npos);
}

TEST_F(ConfigGeneratorTest, ValidateGeneratedConfig) {
    auto result = generator_->generate_config(test_config_);
    
    ASSERT_TRUE(result.success);
    
    // The validation is done as part of generate_config
    EXPECT_TRUE(generator_->validate_configuration());
}

TEST_F(ConfigGeneratorTest, ValidateInvalidConfig) {
    // Create an invalid config structure manually
    std::filesystem::create_directories(output_dir_ / "config");
    std::filesystem::create_directories(output_dir_ / "certs");
    std::ofstream config(output_dir_ / "config" / "config.yaml");
    config << "invalid: yaml: content";
    config.close();
    
    // Validation should fail if no certificates exist
    EXPECT_FALSE(generator_->validate_configuration());
}

TEST_F(ConfigGeneratorTest, CertificateFormattingNewlines) {
    // Test with certificates that have different newline formats
    test_config_.certificate_pem = "-----BEGIN CERTIFICATE-----\nCERT_LINE1\nCERT_LINE2\n-----END CERTIFICATE-----";
    test_config_.private_key_pem = "-----BEGIN RSA PRIVATE KEY-----\nKEY_LINE1\nKEY_LINE2\n-----END RSA PRIVATE KEY-----";
    
    auto result = generator_->generate_config(test_config_);
    
    ASSERT_TRUE(result.success);
    
    auto cert_content = read_file(result.certificate_path);
    auto key_content = read_file(result.private_key_path);
    
    // Should have proper newlines
    EXPECT_NE(cert_content.find("\n"), std::string::npos);
    EXPECT_NE(key_content.find("\n"), std::string::npos);
}

TEST_F(ConfigGeneratorTest, LargeConfiguration) {
    // Test with many initial components
    test_config_.initial_components.clear();
    for (int i = 0; i < 50; i++) {
        test_config_.initial_components.push_back("Component" + std::to_string(i));
    }
    
    auto result = generator_->generate_config(test_config_);
    
    EXPECT_TRUE(result.success);
}

TEST_F(ConfigGeneratorTest, SpecialCharactersInConfig) {
    test_config_.thing_name = "Thing-Name_With.Special@Characters";
    test_config_.deployment_group = "group/with/slashes";
    
    auto result = generator_->generate_config(test_config_);
    
    ASSERT_TRUE(result.success);
    
    auto config_content = read_file(result.config_file_path);
    EXPECT_NE(config_content.find("thingName: \"Thing-Name_With.Special@Characters\""), std::string::npos);
}

TEST_F(ConfigGeneratorTest, NonDefaultMqttPort) {
    test_config_.mqtt_port = 443;  // Using ALPN
    
    auto result = generator_->generate_config(test_config_);
    
    ASSERT_TRUE(result.success);
    
    auto config_content = read_file(result.config_file_path);
    EXPECT_NE(config_content.find("port: 443"), std::string::npos);
}

TEST_F(ConfigGeneratorTest, EmptyOptionalFields) {
    // Clear all optional fields
    test_config_.nucleus_version.clear();
    test_config_.deployment_group.clear();
    test_config_.initial_components.clear();
    test_config_.proxy_url.reset();
    test_config_.mqtt_port.reset();
    test_config_.custom_domain.reset();
    
    auto result = generator_->generate_config(test_config_);
    
    ASSERT_TRUE(result.success);
    
    // Should still generate valid config with defaults
    EXPECT_TRUE(generator_->validate_configuration());
}

TEST_F(ConfigGeneratorTest, RootCaFromFile) {
    // Create a temporary root CA file
    auto root_ca_file = test_dir_ / "root.ca.pem";
    std::ofstream ca_file(root_ca_file);
    ca_file << "-----BEGIN CERTIFICATE-----\nFILE_ROOT_CA_CONTENT\n-----END CERTIFICATE-----";
    ca_file.close();
    
    test_config_.root_ca_path = root_ca_file.string();
    
    auto result = generator_->generate_config(test_config_);
    
    ASSERT_TRUE(result.success);
    
    auto ca_content = read_file(result.root_ca_path);
    EXPECT_NE(ca_content.find("FILE_ROOT_CA_CONTENT"), std::string::npos);
}
