#include "sysmon/metrics_collector.hpp"
#include "sysmon/config_manager.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <sys/statvfs.h>
#include <unistd.h>

namespace sysmon {

// Initialize static member
bool DebugLogger::enabled_ = false;

class LinuxMetricsCollector : public MetricsCollector {
public:
    LinuxMetricsCollector() {
        core_count_ = sysconf(_SC_NPROCESSORS_ONLN);
        // Get initial CPU stats
        read_cpu_stats(prev_total_, prev_idle_);
        
        // Get hardware model names (cache them)
        cpu_model_ = get_cpu_model();
        memory_model_ = get_memory_model();
    }
    
    void set_config(const SysMonConfig* config) override {
        config_ = config;
        if (config_) {
            DebugLogger::set_enabled(config_->debug_logging);
        }
    }
    
    CpuMetrics collect_cpu() override {
        CpuMetrics metrics;
        metrics.core_count = core_count_;
        metrics.model_name = cpu_model_;
        
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
        metrics.model_name = memory_model_;
        
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
            disk.model_name = get_disk_model(mount_point);
            
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
            net.model_name = get_network_model(if_name);
            
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
    
    // Helper function to get CPU model from /proc/cpuinfo
    std::string get_cpu_model() {
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos) {
                    std::string model = line.substr(colon_pos + 1);
                    // Trim leading/trailing spaces
                    size_t start = model.find_first_not_of(" \t");
                    size_t end = model.find_last_not_of(" \t");
                    if (start != std::string::npos) {
                        return model.substr(start, end - start + 1);
                    }
                }
                break;
            }
        }
        return "Unknown CPU";
    }
    
    // Helper function to get memory model/manufacturer
    std::string get_memory_model() {
        // Try to get memory information from dmidecode (requires root/sudo)
        // For non-root users, we'll provide a generic description
        std::ifstream dmidecode_check("/sys/firmware/dmi/tables/DMI");
        if (!dmidecode_check.good()) {
            return "System Memory";
        }
        
        // Try reading DMI information from sysfs
        std::ifstream dmi_type("/sys/firmware/dmi/entries/17-0/raw");
        if (dmi_type.good()) {
            // DMI type 17 is Memory Device, but parsing raw DMI is complex
            // For simplicity, return a generic name
            return "System Memory";
        }
        
        return "System Memory";
    }
    
    // Helper function to get disk model for a mount point
    std::string get_disk_model(const std::string& mount_point) {
        // Find the device for this mount point
        std::ifstream mounts("/proc/mounts");
        std::string line;
        std::string device;
        
        while (std::getline(mounts, line)) {
            std::istringstream iss(line);
            std::string dev, mnt;
            iss >> dev >> mnt;
            
            if (mnt == mount_point) {
                device = dev;
                break;
            }
        }
        
        if (device.empty() || device.find("/dev/") != 0) {
            return "Unknown Drive";
        }
        
        // Extract the base device name (e.g., sda from /dev/sda1)
        std::string base_device = device.substr(5); // Remove "/dev/"
        // Remove partition number if present
        while (!base_device.empty() && std::isdigit(base_device.back())) {
            base_device.pop_back();
        }
        
        // Try to read model from /sys/block/*/device/model
        std::string model_path = "/sys/block/" + base_device + "/device/model";
        std::ifstream model_file(model_path);
        if (model_file.good()) {
            std::string model;
            std::getline(model_file, model);
            // Trim spaces
            size_t start = model.find_first_not_of(" \t");
            size_t end = model.find_last_not_of(" \t");
            if (start != std::string::npos) {
                return model.substr(start, end - start + 1);
            }
        }
        
        return "Unknown Drive";
    }
    
    // Helper function to get network adapter model
    std::string get_network_model(const std::string& interface_name) {
        // Try to read from /sys/class/net/<interface>/device/modalias or uevent
        std::string device_path = "/sys/class/net/" + interface_name + "/device/uevent";
        std::ifstream uevent_file(device_path);
        
        if (uevent_file.good()) {
            std::string line;
            std::string driver, pci_id;
            
            while (std::getline(uevent_file, line)) {
                if (line.find("DRIVER=") == 0) {
                    driver = line.substr(7);
                } else if (line.find("PCI_ID=") == 0) {
                    pci_id = line.substr(7);
                }
            }
            
            if (!driver.empty()) {
                return driver + " Network Adapter";
            }
        }
        
        return interface_name + " Adapter";
    }
    
    uint32_t core_count_;
    unsigned long long prev_total_ = 0;
    unsigned long long prev_idle_ = 0;
    std::string cpu_model_;
    std::string memory_model_;
    const SysMonConfig* config_ = nullptr;
};

std::unique_ptr<MetricsCollector> create_linux_metrics_collector() {
    return std::make_unique<LinuxMetricsCollector>();
}

} // namespace sysmon
