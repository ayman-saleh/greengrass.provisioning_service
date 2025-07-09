#pragma once

#include <string>
#include <optional>

namespace greengrass {
namespace cli {

struct ProgramOptions {
    std::string database_path;
    std::string greengrass_path;
    std::string status_file = "/var/run/greengrass-provisioning.status";
    bool verbose = false;
    bool help = false;
};

class ArgumentParser {
public:
    ArgumentParser();
    ~ArgumentParser() = default;
    
    // Parse command line arguments
    std::optional<ProgramOptions> parse(int argc, char* argv[]);
    
    // Get the help message
    std::string get_help_message() const;
    
private:
    ProgramOptions options_;
};

} // namespace cli
} // namespace greengrass 