#include "sysmon/config_manager.hpp"
#include <typiconf/parsers/yaml_parser.hpp>
#include <iostream>
#include <fstream>

namespace sysmon {

ConfigManager::ConfigManager(const std::string& config_path)
    : config_path_(config_path)
    , last_modified_{}
{
}

bool ConfigManager::load() {
    try {
        // Update last modified time
        if (std::filesystem::exists(config_path_)) {
            last_modified_ = std::filesystem::last_write_time(config_path_);
        }
        
        // Use Typiconf to load the YAML config
        Typiconf::ConfigLoader loader;
        config_ = loader.from_file(config_path_).load<SysMonConfig>();
        
        return true;
    } catch (const Typiconf::ConfigError& e) {
        std::cerr << "Config error: " << e.what() << "\n";
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << "\n";
        return false;
    }
}

bool ConfigManager::check_and_reload() {
    if (!std::filesystem::exists(config_path_)) {
        return false;
    }
    
    auto current_modified = std::filesystem::last_write_time(config_path_);
    if (current_modified != last_modified_) {
        std::cout << "Config file changed, reloading...\n";
        return load();
    }
    
    return false;
}

bool ConfigManager::validate_config(std::string& error_msg) const {
    if (!config_.validate()) {
        error_msg = "Configuration validation failed";
        return false;
    }
    return true;
}

bool SysMonConfig::validate() const {
    if (update_interval <= 0) {
        return false;
    }
    if (history_size <= 0) {
        return false;
    }
    if (!cpu.thresholds.validate()) {
        return false;
    }
    if (!memory.thresholds.validate()) {
        return false;
    }
    if (!disk.thresholds.validate()) {
        return false;
    }
    return true;
}

} // namespace sysmon
