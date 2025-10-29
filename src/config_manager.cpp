#include "sysmon/config_manager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

// Simple YAML parser implementation for SysMon (minimal, specific to our needs)
// In production, this would use typiconf with YAML support
namespace sysmon {

bool SysMonConfig::validate() const {
    if (update_interval <= 0 || history_size <= 0) {
        return false;
    }
    if (!cpu.thresholds.validate() || !memory.thresholds.validate() || !disk.thresholds.validate()) {
        return false;
    }
    return true;
}

ConfigManager::ConfigManager(const std::string& config_path)
    : config_path_(config_path)
    , last_modified_{}
{
}

// Helper function to trim whitespace
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

// Helper to parse double value from string
static double parse_double(const std::string& value) {
    try {
        return std::stod(value);
    } catch (...) {
        return 0.0;
    }
}

// Helper to parse int value from string
static int parse_int(const std::string& value) {
    try {
        return std::stoi(value);
    } catch (...) {
        return 0;
    }
}

// Helper to parse bool value from string
static bool parse_bool(const std::string& value) {
    std::string lower = value;
    for (char& c : lower) c = std::tolower(c);
    return lower == "true" || lower == "yes" || lower == "1";
}

bool ConfigManager::load() {
    std::ifstream file(config_path_);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << config_path_ << "\n";
        return false;
    }
    
    // Store file modification time
    try {
        last_modified_ = std::filesystem::last_write_time(config_path_);
    } catch (const std::exception& e) {
        std::cerr << "Failed to get file modification time: " << e.what() << "\n";
        return false;
    }
    
    // Reset config to defaults
    config_ = SysMonConfig{};
    
    // Simple YAML parser (very basic, handles our specific format)
    std::string line;
    std::string current_section;
    std::string current_subsection;
    MountPointConfig current_mount_point;
    bool in_mount_points = false;
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Check for section headers (no indent)
        if (line.back() == ':' && line.find_first_of(' ') == std::string::npos) {
            current_section = line.substr(0, line.length() - 1);
            current_subsection = "";
            in_mount_points = false;
            continue;
        }
        
        // Parse key-value pairs
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = trim(line.substr(0, colon_pos));
            std::string value = trim(line.substr(colon_pos + 1));
            
            // Remove quotes from string values
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }
            
            // Determine indentation level
            size_t indent = line.find_first_not_of(' ');
            
            // Subsection (2 spaces)
            if (indent == 2 && value.empty() && key.back() != ':') {
                current_subsection = key;
                if (current_section == "disk" && key == "mount_points") {
                    in_mount_points = true;
                }
                continue;
            }
            
            // Handle mount_points array items
            if (in_mount_points && indent == 4 && line[4] == '-') {
                // Save previous mount point if exists
                if (!current_mount_point.path.empty()) {
                    config_.disk.mount_points.push_back(current_mount_point);
                }
                current_mount_point = MountPointConfig{};
                continue;
            }
            
            if (in_mount_points && indent >= 6) {
                if (key == "path") {
                    current_mount_point.path = value;
                } else if (key == "label") {
                    current_mount_point.label = value;
                }
                continue;
            }
            
            // Top-level keys
            if (current_section.empty()) {
                if (key == "version") config_.version = value;
                else if (key == "update_interval") config_.update_interval = parse_int(value);
                else if (key == "history_size") config_.history_size = parse_int(value);
            }
            // Section-specific keys
            else if (current_section == "cpu") {
                if (current_subsection == "thresholds") {
                    if (key == "warning") config_.cpu.thresholds.warning = parse_double(value);
                    else if (key == "critical") config_.cpu.thresholds.critical = parse_double(value);
                } else {
                    if (key == "enabled") config_.cpu.enabled = parse_bool(value);
                    else if (key == "show_per_core") config_.cpu.show_per_core = parse_bool(value);
                }
            }
            else if (current_section == "memory") {
                if (current_subsection == "thresholds") {
                    if (key == "warning") config_.memory.thresholds.warning = parse_double(value);
                    else if (key == "critical") config_.memory.thresholds.critical = parse_double(value);
                } else {
                    if (key == "enabled") config_.memory.enabled = parse_bool(value);
                    else if (key == "show_swap") config_.memory.show_swap = parse_bool(value);
                }
            }
            else if (current_section == "disk") {
                if (current_subsection == "thresholds" && !in_mount_points) {
                    if (key == "warning") config_.disk.thresholds.warning = parse_double(value);
                    else if (key == "critical") config_.disk.thresholds.critical = parse_double(value);
                } else if (!in_mount_points) {
                    if (key == "enabled") config_.disk.enabled = parse_bool(value);
                }
            }
            else if (current_section == "network") {
                if (key == "enabled") config_.network.enabled = parse_bool(value);
                else if (key == "upload_mbps") config_.network.upload_mbps = parse_double(value);
                else if (key == "download_mbps") config_.network.download_mbps = parse_double(value);
            }
            else if (current_section == "display") {
                if (key == "color_scheme") config_.display.color_scheme = value;
                else if (key == "refresh_rate") config_.display.refresh_rate = parse_int(value);
                else if (key == "show_graphs") config_.display.show_graphs = parse_bool(value);
                else if (key == "graph_height") config_.display.graph_height = parse_int(value);
            }
            else if (current_section == "alerts") {
                if (key == "enabled") config_.alerts.enabled = parse_bool(value);
                else if (key == "beep_on_critical") config_.alerts.beep_on_critical = parse_bool(value);
                else if (key == "log_to_file") config_.alerts.log_to_file = parse_bool(value);
                else if (key == "log_path") config_.alerts.log_path = value;
            }
        }
    }
    
    // Save last mount point if exists
    if (in_mount_points && !current_mount_point.path.empty()) {
        config_.disk.mount_points.push_back(current_mount_point);
    }
    
    // Add default mount point if none specified
    if (config_.disk.mount_points.empty()) {
#ifdef _WIN32
        config_.disk.mount_points.push_back({"C:\\", "System", {}});
#else
        config_.disk.mount_points.push_back({"/", "Root", {}});
#endif
    }
    
    return true;
}

bool ConfigManager::check_and_reload() {
    try {
        auto current_time = std::filesystem::last_write_time(config_path_);
        if (current_time != last_modified_) {
            return load();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error checking file modification: " << e.what() << "\n";
    }
    return false;
}

bool ConfigManager::validate_config(std::string& error_msg) const {
    if (!config_.validate()) {
        error_msg = "Configuration validation failed: invalid thresholds or intervals";
        return false;
    }
    
    if (!config_.cpu.thresholds.validate()) {
        error_msg = "CPU thresholds invalid: warning must be less than critical";
        return false;
    }
    
    if (!config_.memory.thresholds.validate()) {
        error_msg = "Memory thresholds invalid: warning must be less than critical";
        return false;
    }
    
    if (!config_.disk.thresholds.validate()) {
        error_msg = "Disk thresholds invalid: warning must be less than critical";
        return false;
    }
    
    return true;
}

} // namespace sysmon
