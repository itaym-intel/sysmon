#include "sysmon/metrics_collector.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <sys/statvfs.h>
#include <unistd.h>

namespace sysmon {

class LinuxMetricsCollector : public MetricsCollector {
public:
    LinuxMetricsCollector() {
        core_count_ = sysconf(_SC_NPROCESSORS_ONLN);
        // Get initial CPU stats
        read_cpu_stats(prev_total_, prev_idle_);
    }
    
    CpuMetrics collect_cpu() override {
        CpuMetrics metrics;
        metrics.core_count = core_count_;
        
        // Read current stats
        unsigned long long total, idle;
        read_cpu_stats(total, idle);
        
        // Calculate usage
        unsigned long long total_diff = total - prev_total_;
        unsigned long long idle_diff = idle - prev_idle_;
        
        if (total_diff > 0) {
            metrics.overall_usage = 100.0 * (1.0 - static_cast<double>(idle_diff) / total_diff);
        }
        
        prev_total_ = total;
        prev_idle_ = idle;
        
        // Read per-core stats
        std::ifstream stat_file("/proc/stat");
        std::string line;
        std::getline(stat_file, line); // Skip first line (total)
        
        while (std::getline(stat_file, line)) {
            if (line.substr(0, 3) != "cpu") break;
            
            std::istringstream iss(line);
            std::string cpu;
            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
            iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            
            unsigned long long total_time = user + nice + system + idle + iowait + irq + softirq + steal;
            unsigned long long idle_time = idle + iowait;
            
            double usage = 100.0 * (1.0 - static_cast<double>(idle_time) / total_time);
            metrics.per_core_usage.push_back(usage);
        }
        
        return metrics;
    }
    
    MemoryMetrics collect_memory() override {
        MemoryMetrics metrics;
        
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        
        while (std::getline(meminfo, line)) {
            std::istringstream iss(line);
            std::string key;
            uint64_t value;
            std::string unit;
            
            iss >> key >> value >> unit;
            
            // Convert kB to bytes
            value *= 1024;
            
            if (key == "MemTotal:") {
                metrics.total_bytes = value;
            } else if (key == "MemAvailable:") {
                metrics.available_bytes = value;
            } else if (key == "SwapTotal:") {
                metrics.swap_total_bytes = value;
            } else if (key == "SwapFree:") {
                metrics.swap_used_bytes = metrics.swap_total_bytes - value;
            }
        }
        
        metrics.used_bytes = metrics.total_bytes - metrics.available_bytes;
        if (metrics.total_bytes > 0) {
            metrics.usage_percent = static_cast<double>(metrics.used_bytes) / metrics.total_bytes * 100.0;
        }
        
        return metrics;
    }
    
    std::vector<DiskMetrics> collect_disk(const std::vector<std::string>& mount_points) override {
        std::vector<DiskMetrics> metrics;
        
        for (const auto& mount_point : mount_points) {
            DiskMetrics disk;
            disk.mount_point = mount_point;
            disk.label = mount_point;
            
            struct statvfs stat;
            if (statvfs(mount_point.c_str(), &stat) == 0) {
                disk.total_bytes = stat.f_blocks * stat.f_frsize;
                disk.used_bytes = (stat.f_blocks - stat.f_bfree) * stat.f_frsize;
                if (disk.total_bytes > 0) {
                    disk.usage_percent = static_cast<double>(disk.used_bytes) / disk.total_bytes * 100.0;
                }
            }
            
            metrics.push_back(disk);
        }
        
        return metrics;
    }
    
    std::vector<NetworkMetrics> collect_network(const std::vector<std::string>& interfaces) override {
        std::vector<NetworkMetrics> network_metrics;
        std::ifstream net_file("/proc/net/dev");
        std::string line;
        
        // Skip header lines
        std::getline(net_file, line);
        std::getline(net_file, line);
        
        while (std::getline(net_file, line)) {
            std::istringstream iss(line);
            std::string if_name;
            uint64_t rx_bytes, rx_packets, rx_errs, rx_drop;
            uint64_t tx_bytes, tx_packets, tx_errs, tx_drop;
            
            // Parse interface name (format: "  eth0:")
            iss >> if_name;
            if_name.erase(if_name.find(':'));
            
            // Skip loopback
            if (if_name == "lo") continue;
            
            // Parse statistics
            iss >> rx_bytes >> rx_packets >> rx_errs >> rx_drop;
            iss.ignore(256, ' '); // Skip remaining RX fields
            iss >> tx_bytes >> tx_packets >> tx_errs >> tx_drop;
            
            // Filter by requested interfaces if specified
            if (!interfaces.empty()) {
                bool found = false;
                for (const auto& req_if : interfaces) {
                    if (if_name == req_if || if_name.find(req_if) != std::string::npos) {
                        found = true;
                        break;
                    }
                }
                if (!found) continue;
            }
            
            NetworkMetrics net;
            net.interface_name = if_name;
            net.bytes_received = rx_bytes;
            net.bytes_sent = tx_bytes;
            
            network_metrics.push_back(net);
        }
        
        return network_metrics;
    }
    
private:
    void read_cpu_stats(unsigned long long& total, unsigned long long& idle) {
        std::ifstream stat_file("/proc/stat");
        std::string line;
        std::getline(stat_file, line);
        
        std::istringstream iss(line);
        std::string cpu;
        unsigned long long user, nice, system, idle_val, iowait, irq, softirq, steal;
        iss >> cpu >> user >> nice >> system >> idle_val >> iowait >> irq >> softirq >> steal;
        
        total = user + nice + system + idle_val + iowait + irq + softirq + steal;
        idle = idle_val + iowait;
    }
    
    uint32_t core_count_;
    unsigned long long prev_total_ = 0;
    unsigned long long prev_idle_ = 0;
};

std::unique_ptr<MetricsCollector> create_linux_metrics_collector() {
    return std::make_unique<LinuxMetricsCollector>();
}

} // namespace sysmon
