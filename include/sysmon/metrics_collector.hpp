#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <iostream>

namespace sysmon {

struct SysMonConfig;
class DebugLogger {
public:
    static void set_enabled(bool enabled) { enabled_ = enabled; }
    static bool is_enabled() { return enabled_; }
    
    template<typename... Args>
    static void log(Args&&... args) {
        if (enabled_) {
            std::cerr << "[DEBUG] ";
            ((std::cerr << args), ...);
            std::cerr << std::endl;
        }
    }
    
private:
    static bool enabled_;
};

struct CpuMetrics {
    double overall_usage = 0.0;              // 0-100%
    std::vector<double> per_core_usage;      // Per logical processor (thread) percentages
    uint32_t core_count = 0;                 // Number of logical processors (threads)
    std::string model_name;                  // CPU model name
};

struct MemoryMetrics {
    uint64_t total_bytes = 0;
    uint64_t available_bytes = 0;
    uint64_t used_bytes = 0;
    double usage_percent = 0.0;
    
    uint64_t swap_total_bytes = 0;
    uint64_t swap_used_bytes = 0;
    std::string model_name;                  // Memory model/manufacturer
};

struct DiskMetrics {
    std::string mount_point;
    std::string label;
    uint64_t total_bytes = 0;
    uint64_t used_bytes = 0;
    double usage_percent = 0.0;
    std::string model_name;                  // Disk model name
};

struct NetworkMetrics {
    std::string interface_name;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    double upload_mbps = 0.0;
    double download_mbps = 0.0;
    std::string model_name;                  // Network adapter model
};

class MetricsCollector {
public:
    virtual ~MetricsCollector() = default;
    
    // Set config for debug logging
    virtual void set_config(const SysMonConfig* config) = 0;
    
    virtual CpuMetrics collect_cpu() = 0;
    virtual MemoryMetrics collect_memory() = 0;
    virtual std::vector<DiskMetrics> collect_disk(const std::vector<std::string>& mount_points) = 0;
    virtual std::vector<NetworkMetrics> collect_network(const std::vector<std::string>& interfaces) = 0;
};

// Factory function
std::unique_ptr<MetricsCollector> create_metrics_collector();

} // namespace sysmon
