#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cstdint>

namespace sysmon {

struct CpuMetrics {
    double overall_usage = 0.0;              // 0-100%
    std::vector<double> per_core_usage;      // Per-core percentages
    uint32_t core_count = 0;
};

struct MemoryMetrics {
    uint64_t total_bytes = 0;
    uint64_t available_bytes = 0;
    uint64_t used_bytes = 0;
    double usage_percent = 0.0;
    
    uint64_t swap_total_bytes = 0;
    uint64_t swap_used_bytes = 0;
};

struct DiskMetrics {
    std::string mount_point;
    std::string label;
    uint64_t total_bytes = 0;
    uint64_t used_bytes = 0;
    double usage_percent = 0.0;
};

struct NetworkMetrics {
    std::string interface_name;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    double upload_mbps = 0.0;
    double download_mbps = 0.0;
};

class MetricsCollector {
public:
    virtual ~MetricsCollector() = default;
    
    virtual CpuMetrics collect_cpu() = 0;
    virtual MemoryMetrics collect_memory() = 0;
    virtual std::vector<DiskMetrics> collect_disk(const std::vector<std::string>& mount_points) = 0;
    virtual std::vector<NetworkMetrics> collect_network(const std::vector<std::string>& interfaces) = 0;
};

// Factory function
std::unique_ptr<MetricsCollector> create_metrics_collector();

} // namespace sysmon
