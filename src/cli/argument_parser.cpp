#include "cli/argument_parser.h"
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

namespace greengrass {
namespace cli {

ArgumentParser::ArgumentParser() = default;

std::optional<ProgramOptions> ArgumentParser::parse(int argc, char* argv[]) {
    CLI::App app{"AWS Greengrass Provisioning Service", "greengrass_provisioning_service"};
    
    // Required arguments
    app.add_option("-d,--database-path", options_.database_path, "Path to the configuration database")
        ->required()
        ->check(CLI::ExistingFile);
    
    app.add_option("-g,--greengrass-path", options_.greengrass_path, "Path where Greengrass will be set up")
        ->required()
        ->check(CLI::ExistingDirectory);
    
    // Optional arguments
    app.add_option("-s,--status-file", options_.status_file, "Path to the status file")
        ->default_val("/var/run/greengrass-provisioning.status");
    
    app.add_flag("-v,--verbose", options_.verbose, "Enable verbose logging");
    
    try {
        app.parse(argc, argv);
        
        // Configure logging based on verbose flag
        if (options_.verbose) {
            spdlog::set_level(spdlog::level::debug);
        } else {
            spdlog::set_level(spdlog::level::info);
        }
        
        spdlog::debug("Parsed arguments:");
        spdlog::debug("  Database path: {}", options_.database_path);
        spdlog::debug("  Greengrass path: {}", options_.greengrass_path);
        spdlog::debug("  Status file: {}", options_.status_file);
        
        return options_;
    } catch (const CLI::ParseError& e) {
        spdlog::error("Failed to parse arguments: {}", e.what());
        return std::nullopt;
    }
}

std::string ArgumentParser::get_help_message() const {
    return R"(AWS Greengrass Provisioning Service

Usage: greengrass_provisioning_service [OPTIONS]

Required Options:
  -d, --database-path PATH    Path to the configuration database
  -g, --greengrass-path PATH  Path where Greengrass will be set up

Optional Options:
  -s, --status-file PATH      Path to the status file (default: /var/run/greengrass-provisioning.status)
  -v, --verbose               Enable verbose logging
  -h, --help                  Show this help message

Example:
  greengrass_provisioning_service -d /opt/config/devices.db -g /greengrass/v2
)";
}

} // namespace cli
} // namespace greengrass 