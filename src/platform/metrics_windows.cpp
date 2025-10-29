#include "sysmon/metrics_collector.hpp"
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "pdh.lib")

namespace sysmon {

class WindowsMetricsCollector : public MetricsCollector {
public:
    WindowsMetricsCollector() {
        // Initialize PDH for CPU monitoring
        PdhOpenQuery(nullptr, 0, &cpu_query_);
        PdhAddEnglishCounterW(cpu_query_, L"\\Processor(_Total)\\% Processor Time", 0, &cpu_total_);
        PdhCollectQueryData(cpu_query_);
        
        // Get number of processors
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        core_count_ = sysInfo.dwNumberOfProcessors;
        
        // Add per-core counters
        for (DWORD i = 0; i < core_count_; ++i) {
            PDH_HCOUNTER counter;
            std::wstring path = L"\\Processor(" + std::to_wstring(i) + L")\\% Processor Time";
            if (PdhAddEnglishCounterW(cpu_query_, path.c_str(), 0, &counter) == ERROR_SUCCESS) {
                cpu_cores_.push_back(counter);
            }
        }
        
        // Initial collection
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        PdhCollectQueryData(cpu_query_);
    }
    
    ~WindowsMetricsCollector() {
        if (cpu_query_) {
            PdhCloseQuery(cpu_query_);
        }
    }
    
    CpuMetrics collect_cpu() override {
        CpuMetrics metrics;
        metrics.core_count = core_count_;
        
        // Collect new sample
        PdhCollectQueryData(cpu_query_);
        
        // Get total CPU usage
        PDH_FMT_COUNTERVALUE counterVal;
        if (PdhGetFormattedCounterValue(cpu_total_, PDH_FMT_DOUBLE, nullptr, &counterVal) == ERROR_SUCCESS) {
            metrics.overall_usage = counterVal.doubleValue;
        }
        
        // Get per-core usage
        for (auto& counter : cpu_cores_) {
            if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &counterVal) == ERROR_SUCCESS) {
                metrics.per_core_usage.push_back(counterVal.doubleValue);
            }
        }
        
        return metrics;
    }
    
    MemoryMetrics collect_memory() override {
        MemoryMetrics metrics;
        
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            metrics.total_bytes = memInfo.ullTotalPhys;
            metrics.available_bytes = memInfo.ullAvailPhys;
            metrics.used_bytes = metrics.total_bytes - metrics.available_bytes;
            metrics.usage_percent = static_cast<double>(metrics.used_bytes) / metrics.total_bytes * 100.0;
            
            // Windows uses page file instead of traditional swap
            metrics.swap_total_bytes = memInfo.ullTotalPageFile - memInfo.ullTotalPhys;
            metrics.swap_used_bytes = (memInfo.ullTotalPageFile - memInfo.ullAvailPageFile) - metrics.used_bytes;
        }
        
        return metrics;
    }
    
    std::vector<DiskMetrics> collect_disk(const std::vector<std::string>& mount_points) override {
        std::vector<DiskMetrics> metrics;
        
        for (const auto& mount_point : mount_points) {
            DiskMetrics disk;
            disk.mount_point = mount_point;
            disk.label = mount_point;
            
            ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
            if (GetDiskFreeSpaceExA(mount_point.c_str(), &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
                disk.total_bytes = totalBytes.QuadPart;
                disk.used_bytes = totalBytes.QuadPart - totalFreeBytes.QuadPart;
                disk.usage_percent = static_cast<double>(disk.used_bytes) / disk.total_bytes * 100.0;
            }
            
            metrics.push_back(disk);
        }
        
        return metrics;
    }
    
    std::vector<NetworkMetrics> collect_network(const std::vector<std::string>& interfaces) override {
        // Network metrics collection on Windows would require more complex WMI or IP Helper API
        // For now, return empty vector
        return {};
    }
    
private:
    PDH_HQUERY cpu_query_ = nullptr;
    PDH_HCOUNTER cpu_total_ = nullptr;
    std::vector<PDH_HCOUNTER> cpu_cores_;
    DWORD core_count_ = 0;
};

std::unique_ptr<MetricsCollector> create_windows_metrics_collector() {
    return std::make_unique<WindowsMetricsCollector>();
}

} // namespace sysmon
