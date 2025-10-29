#pragma once

#include "sysmon/config_manager.hpp"
#include "sysmon/metrics_collector.hpp"
#include <vector>
#include <chrono>
#include <fstream>

namespace sysmon {

enum class AlertLevel {
    Normal,
    Warning,
    Critical
};

struct Alert {
    std::string category;    // "CPU", "Memory", "Disk"
    std::string message;     // "CPU usage is 92%"
    AlertLevel level;
    std::chrono::system_clock::time_point timestamp;
};

class AlertEngine {
public:
    explicit AlertEngine(const AlertConfig& config);
    ~AlertEngine();
    
    // Check metrics against thresholds
    std::vector<Alert> check_cpu(const CpuMetrics& metrics, const CpuConfig& config);
    std::vector<Alert> check_memory(const MemoryMetrics& metrics, const MemoryConfig& config);
    std::vector<Alert> check_disk(const std::vector<DiskMetrics>& metrics, const DiskConfig& config);
    
    // Log alerts to file
    void log_alert(const Alert& alert);
    
    // Trigger system beep (optional)
    void beep_if_enabled();
    
    // Update configuration
    void update_config(const AlertConfig& config);

private:
    AlertLevel determine_level(double value, const ThresholdConfig& thresholds);
    std::string format_timestamp(const std::chrono::system_clock::time_point& tp);
    
    AlertConfig alert_config_;
    std::ofstream log_file_;
};

} // namespace sysmon
