#include "database/config_database.h"
#include <sqlite3.h>
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <sstream>

namespace greengrass {
namespace database {

ConfigDatabase::ConfigDatabase(const std::string& database_path)
    : database_path_(database_path) {
    spdlog::debug("ConfigDatabase initialized with path: {}", database_path);
}

ConfigDatabase::~ConfigDatabase() {
    disconnect();
}

bool ConfigDatabase::connect() {
    if (db_ != nullptr) {
        spdlog::warn("Database already connected");
        return true;
    }
    
    int rc = sqlite3_open(database_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        last_error_ = fmt::format("Cannot open database: {}", sqlite3_errmsg(db_));
        spdlog::error(last_error_);
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    
    spdlog::info("Successfully connected to database: {}", database_path_);
    return true;
}

void ConfigDatabase::disconnect() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
        spdlog::debug("Disconnected from database");
    }
}

bool ConfigDatabase::is_connected() const {
    return db_ != nullptr;
}

std::optional<DeviceConfig> ConfigDatabase::get_device_config(const std::string& device_id) {
    if (!is_connected()) {
        last_error_ = "Database not connected";
        spdlog::error(last_error_);
        return std::nullopt;
    }
    
    // Prepare SQL query
    std::string query = fmt::format(
        "SELECT device_id, thing_name, iot_endpoint, aws_region, "
        "root_ca_path, certificate_pem, private_key_pem, role_alias, "
        "role_alias_endpoint, nucleus_version, deployment_group, "
        "initial_components, proxy_url, mqtt_port, custom_domain "
        "FROM device_config WHERE device_id = '{}' LIMIT 1",
        device_id
    );
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        last_error_ = fmt::format("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        spdlog::error(last_error_);
        return std::nullopt;
    }
    
    DeviceConfig config;
    bool found = false;
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        found = true;
        
        // Extract data from result
        // Helper lambda to safely extract text from SQLite columns
        auto get_text = [](sqlite3_stmt* stmt, int col) -> std::string {
            const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
            return text ? text : "";
        };
        
        config.device_id = get_text(stmt, 0);
        config.thing_name = get_text(stmt, 1);
        config.iot_endpoint = get_text(stmt, 2);
        config.aws_region = get_text(stmt, 3);
        config.root_ca_path = get_text(stmt, 4);
        config.certificate_pem = get_text(stmt, 5);
        config.private_key_pem = get_text(stmt, 6);
        config.role_alias = get_text(stmt, 7);
        config.role_alias_endpoint = get_text(stmt, 8);
        config.nucleus_version = get_text(stmt, 9);
        config.deployment_group = get_text(stmt, 10);
        
        // Parse initial_components (comma-separated string)
        const char* components_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        if (components_str != nullptr) {
            std::stringstream ss(components_str);
            std::string component;
            while (std::getline(ss, component, ',')) {
                if (!component.empty()) {
                    config.initial_components.push_back(component);
                }
            }
        }
        
        // Optional fields
        if (sqlite3_column_type(stmt, 12) != SQLITE_NULL) {
            config.proxy_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        }
        
        if (sqlite3_column_type(stmt, 13) != SQLITE_NULL) {
            config.mqtt_port = sqlite3_column_int(stmt, 13);
        }
        
        if (sqlite3_column_type(stmt, 14) != SQLITE_NULL) {
            config.custom_domain = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
        }
        
        spdlog::info("Found device configuration for device_id: {}", device_id);
    } else if (rc == SQLITE_DONE) {
        spdlog::warn("No device configuration found for device_id: {}", device_id);
    } else {
        last_error_ = fmt::format("Error reading from database: {}", sqlite3_errmsg(db_));
        spdlog::error(last_error_);
    }
    
    sqlite3_finalize(stmt);
    
    return found ? std::optional<DeviceConfig>(config) : std::nullopt;
}

std::optional<DeviceConfig> ConfigDatabase::get_device_config_by_identifier(const std::string& identifier) {
    if (!is_connected()) {
        last_error_ = "Database not connected";
        spdlog::error(last_error_);
        return std::nullopt;
    }
    
    // First, try to find device_id by MAC address or serial number
    std::string lookup_query = fmt::format(
        "SELECT device_id FROM device_identifiers "
        "WHERE mac_address = '{}' OR serial_number = '{}' LIMIT 1",
        identifier, identifier
    );
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, lookup_query.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        last_error_ = fmt::format("Failed to prepare identifier lookup: {}", sqlite3_errmsg(db_));
        spdlog::error(last_error_);
        return std::nullopt;
    }
    
    std::string device_id;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char* id_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (id_text) {
            device_id = id_text;
            spdlog::debug("Found device_id {} for identifier {}", device_id, identifier);
        }
    }
    
    sqlite3_finalize(stmt);
    
    if (device_id.empty()) {
        spdlog::warn("No device found for identifier: {}", identifier);
        return std::nullopt;
    }
    
    // Now get the device config using the device_id
    return get_device_config(device_id);
}

std::vector<std::string> ConfigDatabase::list_device_ids() {
    std::vector<std::string> device_ids;
    
    if (!is_connected()) {
        last_error_ = "Database not connected";
        spdlog::error(last_error_);
        return device_ids;
    }
    
    const char* query = "SELECT device_id FROM device_config ORDER BY device_id";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        last_error_ = fmt::format("Failed to prepare list query: {}", sqlite3_errmsg(db_));
        spdlog::error(last_error_);
        return device_ids;
    }
    
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char* device_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (device_id) {
            device_ids.push_back(device_id);
        }
    }
    
    if (rc != SQLITE_DONE) {
        last_error_ = fmt::format("Error listing devices: {}", sqlite3_errmsg(db_));
        spdlog::error(last_error_);
    }
    
    sqlite3_finalize(stmt);
    
    spdlog::debug("Found {} devices in database", device_ids.size());
    return device_ids;
}

std::string ConfigDatabase::get_last_error() const {
    return last_error_;
}

bool ConfigDatabase::execute_query(const std::string& query) {
    if (!is_connected()) {
        last_error_ = "Database not connected";
        return false;
    }
    
    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &error_msg);
    
    if (rc != SQLITE_OK) {
        last_error_ = fmt::format("SQL error: {}", error_msg);
        sqlite3_free(error_msg);
        return false;
    }
    
    return true;
}

} // namespace database
} // namespace greengrass 