#include <gtest/gtest.h>
#include <filesystem>
#include <sqlite3.h>
#include "database/config_database.h"

using namespace greengrass::database;

class ConfigDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory for test
        test_dir_ = std::filesystem::temp_directory_path() / "greengrass_db_test";
        std::filesystem::create_directories(test_dir_);
        db_path_ = test_dir_ / "test.db";
        
        // Create test database
        create_test_database();
        
        // Create ConfigDatabase instance
        db_ = std::make_unique<ConfigDatabase>(db_path_.string());
    }
    
    void TearDown() override {
        db_.reset();
        std::filesystem::remove_all(test_dir_);
    }
    
    void create_test_database() {
        sqlite3* db;
        sqlite3_open(db_path_.string().c_str(), &db);
        
        // Create tables
        const char* create_tables = R"sql(
            CREATE TABLE device_config (
                device_id TEXT PRIMARY KEY,
                thing_name TEXT NOT NULL,
                iot_endpoint TEXT NOT NULL,
                aws_region TEXT NOT NULL,
                root_ca_path TEXT NOT NULL,
                certificate_pem TEXT NOT NULL,
                private_key_pem TEXT NOT NULL,
                role_alias TEXT NOT NULL,
                role_alias_endpoint TEXT NOT NULL,
                nucleus_version TEXT,
                deployment_group TEXT,
                initial_components TEXT,
                proxy_url TEXT,
                mqtt_port INTEGER,
                custom_domain TEXT
            );
            
            CREATE TABLE device_identifiers (
                device_id TEXT NOT NULL,
                mac_address TEXT,
                serial_number TEXT,
                FOREIGN KEY (device_id) REFERENCES device_config(device_id)
            );
        )sql";
        
        sqlite3_exec(db, create_tables, nullptr, nullptr, nullptr);
        
        // Insert test data
        const char* insert_device = R"sql(
            INSERT INTO device_config (
                device_id, thing_name, iot_endpoint, aws_region,
                root_ca_path, certificate_pem, private_key_pem,
                role_alias, role_alias_endpoint, nucleus_version,
                deployment_group, initial_components, mqtt_port
            ) VALUES (
                'test-device-001', 'TestThing', 'iot.us-east-1.amazonaws.com',
                'us-east-1', '/path/to/root.ca', 'CERT_CONTENT',
                'KEY_CONTENT', 'TestRole', 'cred.iot.us-east-1.amazonaws.com',
                '2.9.0', 'test-group', 'Component1,Component2', 8883
            );
        )sql";
        
        sqlite3_exec(db, insert_device, nullptr, nullptr, nullptr);
        
        // Insert device identifiers
        const char* insert_identifiers = R"sql(
            INSERT INTO device_identifiers (device_id, mac_address, serial_number)
            VALUES 
                ('test-device-001', 'aa:bb:cc:dd:ee:ff', 'SERIAL123'),
                ('test-device-001', '11:22:33:44:55:66', 'SERIAL456');
        )sql";
        
        sqlite3_exec(db, insert_identifiers, nullptr, nullptr, nullptr);
        
        sqlite3_close(db);
    }
    
    std::filesystem::path test_dir_;
    std::filesystem::path db_path_;
    std::unique_ptr<ConfigDatabase> db_;
};

TEST_F(ConfigDatabaseTest, ConnectToDatabase) {
    EXPECT_TRUE(db_->connect());
    EXPECT_TRUE(db_->is_connected());
}

TEST_F(ConfigDatabaseTest, ConnectToNonExistentDatabase) {
    auto bad_db = std::make_unique<ConfigDatabase>("/non/existent/path.db");
    EXPECT_FALSE(bad_db->connect());
    EXPECT_FALSE(bad_db->is_connected());
    EXPECT_FALSE(bad_db->get_last_error().empty());
}

TEST_F(ConfigDatabaseTest, DisconnectFromDatabase) {
    EXPECT_TRUE(db_->connect());
    EXPECT_TRUE(db_->is_connected());
    
    db_->disconnect();
    EXPECT_FALSE(db_->is_connected());
}

TEST_F(ConfigDatabaseTest, GetDeviceConfigById) {
    EXPECT_TRUE(db_->connect());
    
    auto config = db_->get_device_config("test-device-001");
    
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->device_id, "test-device-001");
    EXPECT_EQ(config->thing_name, "TestThing");
    EXPECT_EQ(config->iot_endpoint, "iot.us-east-1.amazonaws.com");
    EXPECT_EQ(config->aws_region, "us-east-1");
    EXPECT_EQ(config->role_alias, "TestRole");
    EXPECT_EQ(config->nucleus_version, "2.9.0");
    EXPECT_EQ(config->deployment_group, "test-group");
    EXPECT_TRUE(config->mqtt_port.has_value());
    EXPECT_EQ(config->mqtt_port.value(), 8883);
    EXPECT_FALSE(config->proxy_url.has_value());
}

TEST_F(ConfigDatabaseTest, GetDeviceConfigByNonExistentId) {
    EXPECT_TRUE(db_->connect());
    
    auto config = db_->get_device_config("non-existent-device");
    
    EXPECT_FALSE(config.has_value());
}

TEST_F(ConfigDatabaseTest, GetDeviceConfigByMacAddress) {
    EXPECT_TRUE(db_->connect());
    
    auto config = db_->get_device_config_by_identifier("aa:bb:cc:dd:ee:ff");
    
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->device_id, "test-device-001");
    EXPECT_EQ(config->thing_name, "TestThing");
}

TEST_F(ConfigDatabaseTest, GetDeviceConfigBySerialNumber) {
    EXPECT_TRUE(db_->connect());
    
    auto config = db_->get_device_config_by_identifier("SERIAL123");
    
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->device_id, "test-device-001");
}

TEST_F(ConfigDatabaseTest, GetDeviceConfigByNonExistentIdentifier) {
    EXPECT_TRUE(db_->connect());
    
    auto config = db_->get_device_config_by_identifier("non-existent-mac");
    
    EXPECT_FALSE(config.has_value());
}

TEST_F(ConfigDatabaseTest, ParseInitialComponents) {
    EXPECT_TRUE(db_->connect());
    
    auto config = db_->get_device_config("test-device-001");
    
    ASSERT_TRUE(config.has_value());
    ASSERT_EQ(config->initial_components.size(), 2);
    EXPECT_EQ(config->initial_components[0], "Component1");
    EXPECT_EQ(config->initial_components[1], "Component2");
}

TEST_F(ConfigDatabaseTest, ListDeviceIds) {
    EXPECT_TRUE(db_->connect());
    
    auto device_ids = db_->list_device_ids();
    
    ASSERT_EQ(device_ids.size(), 1);
    EXPECT_EQ(device_ids[0], "test-device-001");
}

TEST_F(ConfigDatabaseTest, ListDeviceIdsEmptyDatabase) {
    // Create new empty database
    auto empty_db_path = test_dir_ / "empty.db";
    sqlite3* empty_db;
    sqlite3_open(empty_db_path.string().c_str(), &empty_db);
    
    const char* create_table = R"sql(
        CREATE TABLE device_config (
            device_id TEXT PRIMARY KEY,
            thing_name TEXT NOT NULL,
            iot_endpoint TEXT NOT NULL,
            aws_region TEXT NOT NULL,
            root_ca_path TEXT NOT NULL,
            certificate_pem TEXT NOT NULL,
            private_key_pem TEXT NOT NULL,
            role_alias TEXT NOT NULL,
            role_alias_endpoint TEXT NOT NULL,
            nucleus_version TEXT,
            deployment_group TEXT,
            initial_components TEXT,
            proxy_url TEXT,
            mqtt_port INTEGER,
            custom_domain TEXT
        );
    )sql";
    
    sqlite3_exec(empty_db, create_table, nullptr, nullptr, nullptr);
    sqlite3_close(empty_db);
    
    auto db = std::make_unique<ConfigDatabase>(empty_db_path.string());
    EXPECT_TRUE(db->connect());
    
    auto device_ids = db->list_device_ids();
    EXPECT_TRUE(device_ids.empty());
}

TEST_F(ConfigDatabaseTest, GetLastErrorAfterFailedOperation) {
    // Don't connect first
    auto config = db_->get_device_config("test-device-001");
    
    EXPECT_FALSE(config.has_value());
    EXPECT_FALSE(db_->get_last_error().empty());
    EXPECT_NE(db_->get_last_error().find("not connected"), std::string::npos);
}

TEST_F(ConfigDatabaseTest, MultipleDevices) {
    // Disconnect the existing connection first
    db_->disconnect();
    
    // Add another device
    sqlite3* db_ptr = nullptr;
    int rc = sqlite3_open(db_path_.string().c_str(), &db_ptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    const char* insert_device2 = R"sql(
        INSERT INTO device_config (
            device_id, thing_name, iot_endpoint, aws_region,
            root_ca_path, certificate_pem, private_key_pem,
            role_alias, role_alias_endpoint
        ) VALUES (
            'test-device-002', 'TestThing2', 'iot.us-west-2.amazonaws.com',
            'us-west-2', '/path/to/root2.ca', 'CERT_CONTENT2',
            'KEY_CONTENT2', 'TestRole2', 'cred.iot.us-west-2.amazonaws.com'
        );
    )sql";
    
    rc = sqlite3_exec(db_ptr, insert_device2, nullptr, nullptr, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    sqlite3_close(db_ptr);
    
    // Reconnect with the original database instance
    EXPECT_TRUE(db_->connect());
    
    auto device_ids = db_->list_device_ids();
    ASSERT_EQ(device_ids.size(), 2);
    
    // Check both devices can be retrieved
    auto config1 = db_->get_device_config("test-device-001");
    auto config2 = db_->get_device_config("test-device-002");
    
    ASSERT_TRUE(config1.has_value());
    ASSERT_TRUE(config2.has_value());
    EXPECT_EQ(config1->thing_name, "TestThing");
    EXPECT_EQ(config2->thing_name, "TestThing2");
    EXPECT_EQ(config2->aws_region, "us-west-2");
}

TEST_F(ConfigDatabaseTest, OptionalFieldsHandling) {
    // Disconnect first
    db_->disconnect();
    
    // Add device with minimal fields
    sqlite3* db_ptr = nullptr;
    int rc = sqlite3_open(db_path_.string().c_str(), &db_ptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    const char* insert_minimal = R"sql(
        INSERT INTO device_config (
            device_id, thing_name, iot_endpoint, aws_region,
            root_ca_path, certificate_pem, private_key_pem,
            role_alias, role_alias_endpoint
        ) VALUES (
            'minimal-device', 'MinimalThing', 'iot.amazonaws.com',
            'us-east-1', '/root.ca', 'CERT', 'KEY',
            'Role', 'cred.iot.amazonaws.com'
        );
    )sql";
    
    rc = sqlite3_exec(db_ptr, insert_minimal, nullptr, nullptr, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    sqlite3_close(db_ptr);
    
    // Reconnect and test
    EXPECT_TRUE(db_->connect());
    
    auto config = db_->get_device_config("minimal-device");
    
    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(config->nucleus_version.empty());
    EXPECT_TRUE(config->deployment_group.empty());
    EXPECT_TRUE(config->initial_components.empty());
    EXPECT_FALSE(config->proxy_url.has_value());
    EXPECT_FALSE(config->mqtt_port.has_value());
    EXPECT_FALSE(config->custom_domain.has_value());
}