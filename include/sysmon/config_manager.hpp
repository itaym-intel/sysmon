#pragma once

#include <typiconf/typiconf.hpp>
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
    
    TYPICONF_DEFINE_FIELDS(ThresholdConfig,
        TYPICONF_FIELD(warning),
        TYPICONF_FIELD(critical)
    )
};

struct CpuConfig {
    bool enabled = true;
    ThresholdConfig thresholds;
    bool show_per_core = true;
    bool show_model_name = true;
    
    TYPICONF_DEFINE_FIELDS(CpuConfig,
        TYPICONF_FIELD(enabled),
        TYPICONF_FIELD(thresholds),
        TYPICONF_FIELD(show_per_core),
        TYPICONF_FIELD(show_model_name)
    )
};

struct MemoryConfig {
    bool enabled = true;
    ThresholdConfig thresholds;
    bool show_swap = true;
    bool show_model_name = true;
    
    TYPICONF_DEFINE_FIELDS(MemoryConfig,
        TYPICONF_FIELD(enabled),
        TYPICONF_FIELD(thresholds),
        TYPICONF_FIELD(show_swap),
        TYPICONF_FIELD(show_model_name)
    )
};

struct MountPointConfig {
    std::string path;
    std::string label;
    ThresholdConfig thresholds;  // Now required, not optional
    
    TYPICONF_DEFINE_FIELDS(MountPointConfig,
        TYPICONF_FIELD(path),
        TYPICONF_FIELD(label),
        TYPICONF_FIELD(thresholds)
    )
};

struct DiskConfig {
    bool enabled = true;
    ThresholdConfig thresholds;
    std::vector<MountPointConfig> mount_points;
    bool show_model_name = true;
    
    TYPICONF_DEFINE_FIELDS(DiskConfig,
        TYPICONF_FIELD(enabled),
        TYPICONF_FIELD(thresholds),
        TYPICONF_FIELD(mount_points),
        TYPICONF_FIELD(show_model_name)
    )
};

struct NetworkConfig {
    bool enabled = false;
    double upload_mbps = 10.0;
    double download_mbps = 50.0;
    std::vector<std::string> interfaces;
    bool show_model_name = true;
    
    TYPICONF_DEFINE_FIELDS(NetworkConfig,
        TYPICONF_FIELD(enabled),
        TYPICONF_FIELD(upload_mbps),
        TYPICONF_FIELD(download_mbps),
        TYPICONF_FIELD(interfaces),
        TYPICONF_FIELD(show_model_name)
    )
};

struct DisplayConfig {
    std::string color_scheme = "default";
    int refresh_rate = 1;
    bool show_graphs = true;
    int graph_height = 10;
    
    TYPICONF_DEFINE_FIELDS(DisplayConfig,
        TYPICONF_FIELD(color_scheme),
        TYPICONF_FIELD(refresh_rate),
        TYPICONF_FIELD(show_graphs),
        TYPICONF_FIELD(graph_height)
    )
};

struct AlertConfig {
    bool enabled = true;
    bool beep_on_critical = false;
    bool log_to_file = true;
    std::string log_path = "./sysmon.log";
    
    TYPICONF_DEFINE_FIELDS(AlertConfig,
        TYPICONF_FIELD(enabled),
        TYPICONF_FIELD(beep_on_critical),
        TYPICONF_FIELD(log_to_file),
        TYPICONF_FIELD(log_path)
    )
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
    
    TYPICONF_DEFINE_FIELDS(SysMonConfig,
        TYPICONF_FIELD(version),
        TYPICONF_FIELD(update_interval),
        TYPICONF_FIELD(history_size),
        TYPICONF_FIELD(cpu),
        TYPICONF_FIELD(memory),
        TYPICONF_FIELD(disk),
        TYPICONF_FIELD(network),
        TYPICONF_FIELD(display),
        TYPICONF_FIELD(alerts)
    )
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
