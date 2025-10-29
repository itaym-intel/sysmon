#include "sysmon/metrics_collector.hpp"
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <vector>

namespace sysmon {

class MacOSMetricsCollector : public MetricsCollector {
public:
    MacOSMetricsCollector() {
        int mib[2] = {CTL_HW, HW_NCPU};
        size_t len = sizeof(core_count_);
        sysctl(mib, 2, &core_count_, &len, nullptr, 0);
    }
    
    CpuMetrics collect_cpu() override {
        CpuMetrics metrics;
        metrics.core_count = core_count_;
        
        host_cpu_load_info_data_t cpu_info;
        mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
        
        if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, 
                           (host_info_t)&cpu_info, &count) == KERN_SUCCESS) {
            unsigned long long total_ticks = 0;
            for (int i = 0; i < CPU_STATE_MAX; i++) {
                total_ticks += cpu_info.cpu_ticks[i];
            }
            
            unsigned long long idle_ticks = cpu_info.cpu_ticks[CPU_STATE_IDLE];
            
            if (prev_total_ticks_ > 0) {
                unsigned long long total_diff = total_ticks - prev_total_ticks_;
                unsigned long long idle_diff = idle_ticks - prev_idle_ticks_;
                
                if (total_diff > 0) {
                    metrics.overall_usage = 100.0 * (1.0 - static_cast<double>(idle_diff) / total_diff);
                }
            }
            
            prev_total_ticks_ = total_ticks;
            prev_idle_ticks_ = idle_ticks;
        }
        
        // Per-core stats would require more complex processor_info calls
        // For simplicity, just replicate overall usage
        for (uint32_t i = 0; i < core_count_; ++i) {
            metrics.per_core_usage.push_back(metrics.overall_usage);
        }
        
        return metrics;
    }
    
    MemoryMetrics collect_memory() override {
        MemoryMetrics metrics;
        
        // Total physical memory
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        uint64_t total_mem;
        size_t len = sizeof(total_mem);
        sysctl(mib, 2, &total_mem, &len, nullptr, 0);
        metrics.total_bytes = total_mem;
        
        // VM statistics
        vm_statistics64_data_t vm_stat;
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64, 
                             (host_info64_t)&vm_stat, &count) == KERN_SUCCESS) {
            vm_size_t page_size;
            host_page_size(mach_host_self(), &page_size);
            
            uint64_t free_mem = vm_stat.free_count * page_size;
            uint64_t inactive_mem = vm_stat.inactive_count * page_size;
            
            metrics.available_bytes = free_mem + inactive_mem;
            metrics.used_bytes = metrics.total_bytes - metrics.available_bytes;
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
            
            struct statfs stat;
            if (statfs(mount_point.c_str(), &stat) == 0) {
                disk.total_bytes = stat.f_blocks * stat.f_bsize;
                disk.used_bytes = (stat.f_blocks - stat.f_bfree) * stat.f_bsize;
                if (disk.total_bytes > 0) {
                    disk.usage_percent = static_cast<double>(disk.used_bytes) / disk.total_bytes * 100.0;
                }
            }
            
            metrics.push_back(disk);
        }
        
        return metrics;
    }
    
    std::vector<NetworkMetrics> collect_network(const std::vector<std::string>& interfaces) override {
        // Network metrics would require sysctl with NET_RT_IFLIST
        // For now, return empty vector
        return {};
    }
    
private:
    uint32_t core_count_;
    unsigned long long prev_total_ticks_ = 0;
    unsigned long long prev_idle_ticks_ = 0;
};

std::unique_ptr<MetricsCollector> create_macos_metrics_collector() {
    return std::make_unique<MacOSMetricsCollector>();
}

} // namespace sysmon
