#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <chrono>

namespace sysmon {

struct ThresholdConfig {
    double warning = 70.0;
    double critical = 90.0;
    
    bool validate() const {
        return warning >= 0.0 && warning <= 100.0 &&
               critical >= 0.0 && critical <= 100.0 &&
               warning < critical;
    }
};

struct CpuConfig {
    bool enabled = true;
    ThresholdConfig thresholds;
    bool show_per_core = true;
};

struct MemoryConfig {
    bool enabled = true;
    ThresholdConfig thresholds;
    bool show_swap = true;
};

struct MountPointConfig {
    std::string path;
    std::string label;
    std::optional<ThresholdConfig> thresholds;
};

struct DiskConfig {
    bool enabled = true;
    ThresholdConfig thresholds;
    std::vector<MountPointConfig> mount_points;
};

struct NetworkConfig {
    bool enabled = false;
    double upload_mbps = 10.0;
    double download_mbps = 50.0;
    std::vector<std::string> interfaces;
};

struct DisplayConfig {
    std::string color_scheme = "default";
    int refresh_rate = 1;
    bool show_graphs = true;
    int graph_height = 10;
};

struct AlertConfig {
    bool enabled = true;
    bool beep_on_critical = false;
    bool log_to_file = true;
    std::string log_path = "./sysmon.log";
};

struct SysMonConfig {
    std::string version = "1.0";
    int update_interval = 2;
    int history_size = 30;
    CpuConfig cpu;
    MemoryConfig memory;
    DiskConfig disk;
    NetworkConfig network;
    DisplayConfig display;
    AlertConfig alerts;
    
    bool validate() const;
};

class ConfigManager {
public:
    explicit ConfigManager(const std::string& config_path);
    
    // Load configuration
    bool load();
    
    // Reload if file changed (hot-reload)
    bool check_and_reload();
    
    // Access configuration
    const SysMonConfig& get_config() const { return config_; }
    
    // Validation
    bool validate_config(std::string& error_msg) const;

private:
    std::string config_path_;
    SysMonConfig config_;
    std::filesystem::file_time_type last_modified_;
};

} // namespace sysmon
