#include "sysmon/metrics_collector.hpp"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600  // Windows Vista or later for GetIfTable2
#endif

// winsock2.h MUST be included before windows.h to avoid conflicts
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <iostream>
#include <thread>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace sysmon {

class WindowsMetricsCollector : public MetricsCollector {
public:
    WindowsMetricsCollector() {
        // Initialize PDH for CPU monitoring
        PdhOpenQuery(nullptr, 0, &cpu_query_);
        PdhAddEnglishCounterW(cpu_query_, L"\\Processor(_Total)\\% Processor Time", 0, &cpu_total_);
        PdhCollectQueryData(cpu_query_);
        
        // Get number of logical processors (threads, including hyperthreading)
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        core_count_ = sysInfo.dwNumberOfProcessors;
        
        // Add per-thread counters
        for (DWORD i = 0; i < core_count_; ++i) {
            PDH_HCOUNTER counter;
            std::wstring path = L"\\Processor(" + std::to_wstring(i) + L")\\% Processor Time";
            if (PdhAddEnglishCounterW(cpu_query_, path.c_str(), 0, &counter) == ERROR_SUCCESS) {
                cpu_cores_.push_back(counter);
            }
        }
        
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
        
        PdhCollectQueryData(cpu_query_);
        
        // Total CPU usage
        PDH_FMT_COUNTERVALUE counterVal;
        if (PdhGetFormattedCounterValue(cpu_total_, PDH_FMT_DOUBLE, nullptr, &counterVal) == ERROR_SUCCESS) {
            metrics.overall_usage = counterVal.doubleValue;
        }
        
        // per (logical) core usage
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
        std::vector<NetworkMetrics> network_metrics;
        
        PMIB_IF_TABLE2 if_table = nullptr;
        
        DWORD result = GetIfTable2(&if_table);
        if (result != NO_ERROR) {
            return network_metrics;
        }
        
        for (ULONG i = 0; i < if_table->NumEntries; i++) {
            MIB_IF_ROW2& row = if_table->Table[i];
            
            // Skip loopback, non-operational, and tunnel interfaces
            if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK || 
                row.Type == IF_TYPE_TUNNEL ||
                row.OperStatus != IfOperStatusUp) {
                continue;
            }
            
            // Only include physical network adapters (Ethernet, WiFi, etc.)
            // Skip if it's a filter/miniport without a real physical adapter
            if (row.InterfaceAndOperStatusFlags.FilterInterface || 
                row.InterfaceAndOperStatusFlags.NotMediaConnected) {
                continue;
            }
            
            // Convert wide string to narrow string
            char interface_name[256];
            WideCharToMultiByte(CP_UTF8, 0, row.Alias, -1, interface_name, sizeof(interface_name), nullptr, nullptr);
            std::string if_name(interface_name);
            
            // If specific interfaces requested, check if this one matches
            if (!interfaces.empty()) {
                bool found = false;
                for (const auto& req_if : interfaces) {
                    if (if_name.find(req_if) != std::string::npos) {
                        found = true;
                        break;
                    }
                }
                if (!found) continue;
            }
            
            NetworkMetrics net;
            net.interface_name = if_name;
            net.bytes_sent = row.OutOctets;
            net.bytes_received = row.InOctets;
            
            network_metrics.push_back(net);
        }
        
        FreeMibTable(if_table);
        return network_metrics;
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
