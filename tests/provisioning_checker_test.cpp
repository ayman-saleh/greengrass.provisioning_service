#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "provisioning/provisioning_checker.h"

using namespace greengrass::provisioning;

class ProvisioningCheckerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory for test
        test_dir_ = std::filesystem::temp_directory_path() / "greengrass_prov_test";
        std::filesystem::create_directories(test_dir_);
        
        greengrass_path_ = test_dir_ / "greengrass";
        std::filesystem::create_directories(greengrass_path_);
        
        checker_ = std::make_unique<ProvisioningChecker>(greengrass_path_.string());
    }
    
    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }
    
    void create_minimal_greengrass_setup() {
        // Create directories
        std::filesystem::create_directories(greengrass_path_ / "config");
        std::filesystem::create_directories(greengrass_path_ / "certs");
        std::filesystem::create_directories(greengrass_path_ / "ggc-root");
        
        // Create config file
        std::ofstream config_file(greengrass_path_ / "config" / "config.yaml");
        config_file << "---\n";
        config_file << "system:\n";
        config_file << "  thingName: TestThing\n";
        config_file << "services:\n";
        config_file << "  aws.greengrass.Nucleus:\n";
        config_file << "    version: 2.9.0\n";
        config_file.close();
        
        // Create certificates
        std::ofstream cert_file(greengrass_path_ / "certs" / "device.cert.pem");
        cert_file << "-----BEGIN CERTIFICATE-----\nMOCK_CERT\n-----END CERTIFICATE-----\n";
        cert_file.close();
        
        std::ofstream key_file(greengrass_path_ / "certs" / "device.private.key");
        key_file << "-----BEGIN RSA PRIVATE KEY-----\nMOCK_KEY\n-----END RSA PRIVATE KEY-----\n";
        key_file.close();
    }
    
    void create_greengrass_v2_setup() {
        create_minimal_greengrass_setup();
        
        // Add v2 specific directories
        std::filesystem::create_directories(greengrass_path_ / "recipes");
        std::filesystem::create_directories(greengrass_path_ / "packages");
        std::filesystem::create_directories(greengrass_path_ / "deployments");
    }
    
    void create_greengrass_v1_setup() {
        // Create directories
        std::filesystem::create_directories(greengrass_path_ / "config");
        std::filesystem::create_directories(greengrass_path_ / "certs");
        std::filesystem::create_directories(greengrass_path_ / "ggc-root");
        
        // Create JSON config for v1
        std::ofstream config_file(greengrass_path_ / "config" / "config.json");
        config_file << "{\n";
        config_file << "  \"coreThing\": {\n";
        config_file << "    \"thingName\": \"TestThingV1\"\n";
        config_file << "  }\n";
        config_file << "}\n";
        config_file.close();
        
        // Create certificates
        std::ofstream cert_file(greengrass_path_ / "certs" / "device.cert.pem");
        cert_file << "-----BEGIN CERTIFICATE-----\nMOCK_CERT\n-----END CERTIFICATE-----\n";
        cert_file.close();
        
        std::ofstream key_file(greengrass_path_ / "certs" / "device.private.key");
        key_file << "-----BEGIN RSA PRIVATE KEY-----\nMOCK_KEY\n-----END RSA PRIVATE KEY-----\n";
        key_file.close();
    }
    
    std::filesystem::path test_dir_;
    std::filesystem::path greengrass_path_;
    std::unique_ptr<ProvisioningChecker> checker_;
};

TEST_F(ProvisioningCheckerTest, CheckNonExistentDirectory) {
    auto non_existent_path = test_dir_ / "non_existent";
    ProvisioningChecker checker(non_existent_path.string());
    
    auto status = checker.check_provisioning_status();
    
    EXPECT_FALSE(status.is_provisioned);
    EXPECT_EQ(status.details, "Greengrass directory does not exist");
}

TEST_F(ProvisioningCheckerTest, CheckEmptyDirectory) {
    auto status = checker_->check_provisioning_status();
    
    EXPECT_FALSE(status.is_provisioned);
    EXPECT_FALSE(status.missing_components.empty());
    EXPECT_NE(std::find(status.missing_components.begin(), 
                        status.missing_components.end(), 
                        "config"), 
              status.missing_components.end());
}

TEST_F(ProvisioningCheckerTest, CheckFullyProvisioned) {
    create_minimal_greengrass_setup();
    
    auto status = checker_->check_provisioning_status();
    
    EXPECT_TRUE(status.is_provisioned);
    EXPECT_EQ(status.thing_name, "TestThing");
    EXPECT_EQ(status.details, "Greengrass is fully provisioned");
}

TEST_F(ProvisioningCheckerTest, CheckConfigExists) {
    // No config initially
    EXPECT_FALSE(checker_->check_config_exists());
    
    // Create config directory and file
    std::filesystem::create_directories(greengrass_path_ / "config");
    std::ofstream config(greengrass_path_ / "config" / "config.yaml");
    config << "test";
    config.close();
    
    EXPECT_TRUE(checker_->check_config_exists());
}

TEST_F(ProvisioningCheckerTest, CheckConfigFormats) {
    std::filesystem::create_directories(greengrass_path_ / "config");
    
    // Test YAML format
    {
        std::ofstream config(greengrass_path_ / "config" / "config.yaml");
        config << "test";
        config.close();
        EXPECT_TRUE(checker_->check_config_exists());
        std::filesystem::remove(greengrass_path_ / "config" / "config.yaml");
    }
    
    // Test YML format
    {
        std::ofstream config(greengrass_path_ / "config" / "config.yml");
        config << "test";
        config.close();
        EXPECT_TRUE(checker_->check_config_exists());
        std::filesystem::remove(greengrass_path_ / "config" / "config.yml");
    }
    
    // Test JSON format
    {
        std::ofstream config(greengrass_path_ / "config" / "config.json");
        config << "{}";
        config.close();
        EXPECT_TRUE(checker_->check_config_exists());
    }
}

TEST_F(ProvisioningCheckerTest, CheckCertificatesExist) {
    // No certs initially
    EXPECT_FALSE(checker_->check_certificates_exist());
    
    // Create empty certs directory
    std::filesystem::create_directories(greengrass_path_ / "certs");
    EXPECT_FALSE(checker_->check_certificates_exist());
    
    // Add certificate files
    std::ofstream cert(greengrass_path_ / "certs" / "device.cert.pem");
    cert << "CERT";
    cert.close();
    
    std::ofstream key(greengrass_path_ / "certs" / "device.private.key");
    key << "KEY";
    key.close();
    
    EXPECT_TRUE(checker_->check_certificates_exist());
}

TEST_F(ProvisioningCheckerTest, CheckGreengrassRootExists) {
    // No root initially
    EXPECT_FALSE(checker_->check_greengrass_root_exists());
    
    // Create root directory
    std::filesystem::create_directories(greengrass_path_ / "ggc-root");
    EXPECT_TRUE(checker_->check_greengrass_root_exists());
}

TEST_F(ProvisioningCheckerTest, ValidateEmptyConfigFile) {
    std::filesystem::create_directories(greengrass_path_ / "config");
    
    // Create empty config file
    std::ofstream config(greengrass_path_ / "config" / "config.yaml");
    config.close();
    
    EXPECT_FALSE(checker_->validate_config_file());
}

TEST_F(ProvisioningCheckerTest, ValidateInvalidYamlConfig) {
    std::filesystem::create_directories(greengrass_path_ / "config");
    
    // Create invalid YAML config
    std::ofstream config(greengrass_path_ / "config" / "config.yaml");
    config << "invalid: yaml: content: without proper structure\n";
    config.close();
    
    EXPECT_FALSE(checker_->validate_config_file());
}

TEST_F(ProvisioningCheckerTest, ValidateValidYamlConfig) {
    std::filesystem::create_directories(greengrass_path_ / "config");
    
    // Create valid YAML config
    std::ofstream config(greengrass_path_ / "config" / "config.yaml");
    config << "system:\n";
    config << "  thingName: TestDevice\n";
    config << "services:\n";
    config << "  aws.greengrass.Nucleus:\n";
    config << "    version: 2.9.0\n";
    config.close();
    
    EXPECT_TRUE(checker_->validate_config_file());
}

TEST_F(ProvisioningCheckerTest, ValidateValidJsonConfig) {
    std::filesystem::create_directories(greengrass_path_ / "config");
    
    // Create valid JSON config
    std::ofstream config(greengrass_path_ / "config" / "config.json");
    config << "{\n";
    config << "  \"coreThing\": {\n";
    config << "    \"thingName\": \"TestDevice\"\n";
    config << "  }\n";
    config << "}\n";
    config.close();
    
    EXPECT_TRUE(checker_->validate_config_file());
}

TEST_F(ProvisioningCheckerTest, ValidateInvalidJsonConfig) {
    std::filesystem::create_directories(greengrass_path_ / "config");
    
    // Create invalid JSON config
    std::ofstream config(greengrass_path_ / "config" / "config.json");
    config << "{ invalid json }\n";
    config.close();
    
    EXPECT_FALSE(checker_->validate_config_file());
}

TEST_F(ProvisioningCheckerTest, DetectGreengrassV2) {
    create_greengrass_v2_setup();
    
    auto status = checker_->check_provisioning_status();
    
    EXPECT_TRUE(status.is_provisioned);
    EXPECT_EQ(status.greengrass_version, "v2.x");
}

TEST_F(ProvisioningCheckerTest, DetectGreengrassV1) {
    create_greengrass_v1_setup();
    
    auto status = checker_->check_provisioning_status();
    
    EXPECT_TRUE(status.is_provisioned);
    EXPECT_EQ(status.greengrass_version, "v1.x");
    EXPECT_EQ(status.thing_name, "TestThingV1");
}

TEST_F(ProvisioningCheckerTest, MissingComponents) {
    // Create partial setup - missing certs
    std::filesystem::create_directories(greengrass_path_ / "config");
    std::filesystem::create_directories(greengrass_path_ / "ggc-root");
    
    std::ofstream config(greengrass_path_ / "config" / "config.yaml");
    config << "system:\n  thingName: Test\nservices:\n  test: {}\n";
    config.close();
    
    auto status = checker_->check_provisioning_status();
    
    EXPECT_FALSE(status.is_provisioned);
    EXPECT_NE(std::find(status.missing_components.begin(),
                        status.missing_components.end(),
                        "certificates"),
              status.missing_components.end());
}

TEST_F(ProvisioningCheckerTest, CorruptedConfigFile) {
    create_minimal_greengrass_setup();
    
    // Corrupt the config file
    std::ofstream config(greengrass_path_ / "config" / "config.yaml");
    config << "corrupted content without proper yaml structure";
    config.close();
    
    auto status = checker_->check_provisioning_status();
    
    EXPECT_FALSE(status.is_provisioned);
    EXPECT_EQ(status.details, "Configuration file is invalid or corrupted");
}

TEST_F(ProvisioningCheckerTest, ReadThingNameFromYaml) {
    std::filesystem::create_directories(greengrass_path_ / "config");
    
    std::ofstream config(greengrass_path_ / "config" / "config.yaml");
    config << "system:\n";
    config << "  thingName: MyTestDevice123\n";
    config << "services:\n";
    config << "  test: {}\n";
    config.close();
    
    create_minimal_greengrass_setup();  // This will overwrite, so recreate
    std::ofstream config2(greengrass_path_ / "config" / "config.yaml");
    config2 << "system:\n";
    config2 << "  thingName: MyTestDevice123\n";
    config2 << "services:\n";
    config2 << "  test: {}\n";
    config2.close();
    
    auto status = checker_->check_provisioning_status();
    
    EXPECT_EQ(status.thing_name, "MyTestDevice123");
}

TEST_F(ProvisioningCheckerTest, CertificateFilePatterns) {
    std::filesystem::create_directories(greengrass_path_ / "certs");
    
    // Test various certificate file patterns
    
    // Pattern 1: .cert.pem and .private.key
    {
        std::ofstream cert(greengrass_path_ / "certs" / "device.cert.pem");
        cert << "CERT";
        std::ofstream key(greengrass_path_ / "certs" / "device.private.key");
        key << "KEY";
        
        EXPECT_TRUE(checker_->check_certificates_exist());
        
        std::filesystem::remove(greengrass_path_ / "certs" / "device.cert.pem");
        std::filesystem::remove(greengrass_path_ / "certs" / "device.private.key");
    }
    
    // Pattern 2: .crt and .key
    {
        std::ofstream cert(greengrass_path_ / "certs" / "device.crt");
        cert << "CERT";
        std::ofstream key(greengrass_path_ / "certs" / "device.key");
        key << "KEY";
        
        EXPECT_TRUE(checker_->check_certificates_exist());
    }
}
