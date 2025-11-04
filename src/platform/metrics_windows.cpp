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
#include <winioctl.h>
#include <ntddscsi.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <iostream>
#include <thread>
#include <sstream>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wbemuuid.lib")

namespace sysmon {

// Helper class for WMI queries
class WMIHelper {
public:
    WMIHelper() {
        HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            return;
        }
        com_initialized_ = true;

        hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
            RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

        hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&locator_);
        if (FAILED(hr)) {
            return;
        }

        hr = locator_->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0,
            NULL, 0, 0, &services_);
        if (FAILED(hr)) {
            return;
        }

        CoSetProxyBlanket(services_, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    }

    ~WMIHelper() {
        if (services_) services_->Release();
        if (locator_) locator_->Release();
        if (com_initialized_) CoUninitialize();
    }

    std::string query_single_property(const wchar_t* wql_query, const wchar_t* property) {
        if (!services_) return "";

        IEnumWbemClassObject* enumerator = nullptr;
        HRESULT hr = services_->ExecQuery(bstr_t("WQL"), bstr_t(wql_query),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);

        if (FAILED(hr) || !enumerator) {
            return "";
        }

        IWbemClassObject* obj = nullptr;
        ULONG returned = 0;
        std::string result;

        if (enumerator->Next(WBEM_INFINITE, 1, &obj, &returned) == WBEM_S_NO_ERROR) {
            VARIANT vtProp;
            VariantInit(&vtProp);

            hr = obj->Get(property, 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
                _bstr_t bstr(vtProp.bstrVal);
                result = (const char*)bstr;
            }

            VariantClear(&vtProp);
            obj->Release();
        }

        enumerator->Release();
        return result;
    }

    std::vector<std::string> query_multiple_properties(const wchar_t* wql_query, 
        const std::vector<const wchar_t*>& properties) {
        std::vector<std::string> results;
        std::cerr << "[DEBUG WMIHelper::query_multiple_properties] Starting query" << std::endl;
        if (!services_) {
            std::cerr << "[DEBUG WMIHelper::query_multiple_properties] services_ is NULL!" << std::endl;
            return results;
        }
        IEnumWbemClassObject* enumerator = nullptr;
        HRESULT hr = services_->ExecQuery(bstr_t("WQL"), bstr_t(wql_query),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
        std::cerr << "[DEBUG WMIHelper::query_multiple_properties] ExecQuery HRESULT: 0x"
                  << std::hex << hr << std::dec << std::endl;
        if (FAILED(hr) || !enumerator) {
            std::cerr << "[DEBUG WMIHelper::query_multiple_properties] Query failed or enumerator is NULL" << std::endl;
            return results;
        }
        IWbemClassObject* obj = nullptr;
        ULONG returned = 0;
        int object_count = 0;
        while (enumerator->Next(WBEM_INFINITE, 1, &obj, &returned) == WBEM_S_NO_ERROR) {
            object_count++;
            std::cerr << "[DEBUG WMIHelper::query_multiple_properties] Processing object " << object_count << std::endl;
            for (const auto& prop : properties) {
                VARIANT vtProp;
                VariantInit(&vtProp);
                hr = obj->Get(prop, 0, &vtProp, 0, 0);
                std::cerr << "[DEBUG WMIHelper::query_multiple_properties] Property '" << (prop ? _bstr_t(prop) : L"(null)") << "' Get HRESULT: 0x"
                          << std::hex << hr << std::dec << ", VT type: " << vtProp.vt << std::endl;
                // Log value for all VT types
                switch (vtProp.vt) {
                    case VT_BSTR:
                        if (vtProp.bstrVal) {
                            _bstr_t bstr(vtProp.bstrVal);
                            std::string value = (const char*)bstr;
                            std::cerr << "[DEBUG WMIHelper::query_multiple_properties] VT_BSTR value: '" << value << "'" << std::endl;
                            results.push_back(value);
                        } else {
                            std::cerr << "[DEBUG WMIHelper::query_multiple_properties] VT_BSTR but value is NULL" << std::endl;
                            results.push_back("");
                        }
                        break;
                    case VT_NULL:
                        std::cerr << "[DEBUG WMIHelper::query_multiple_properties] VT_NULL" << std::endl;
                        results.push_back("");
                        break;
                    case VT_I4:
                        std::cerr << "[DEBUG WMIHelper::query_multiple_properties] VT_I4 value: " << vtProp.lVal << std::endl;
                        results.push_back(std::to_string(vtProp.lVal));
                        break;
                    case VT_UI4:
                        std::cerr << "[DEBUG WMIHelper::query_multiple_properties] VT_UI4 value: " << vtProp.ulVal << std::endl;
                        results.push_back(std::to_string(vtProp.ulVal));
                        break;
                    case VT_I8:
                        std::cerr << "[DEBUG WMIHelper::query_multiple_properties] VT_I8 value: " << vtProp.llVal << std::endl;
                        results.push_back(std::to_string(vtProp.llVal));
                        break;
                    case VT_UI8:
                        std::cerr << "[DEBUG WMIHelper::query_multiple_properties] VT_UI8 value: " << vtProp.ullVal << std::endl;
                        results.push_back(std::to_string(vtProp.ullVal));
                        break;
                    default:
                        std::cerr << "[DEBUG WMIHelper::query_multiple_properties] Unhandled VT type: " << vtProp.vt << std::endl;
                        results.push_back("");
                        break;
                }
                VariantClear(&vtProp);
            }
            obj->Release();
        }
        std::cerr << "[DEBUG WMIHelper::query_multiple_properties] Total objects (modules) found: " << object_count << std::endl;
        enumerator->Release();
        return results;
    }

private:
    IWbemLocator* locator_ = nullptr;
    IWbemServices* services_ = nullptr;
    bool com_initialized_ = false;
};

class WindowsMetricsCollector : public MetricsCollector {
public:
    WindowsMetricsCollector() {
        // Initialize WMI
        wmi_ = std::make_unique<WMIHelper>();
        
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
        
        // Get hardware model names using WMI (cache them)
        cpu_model_ = get_cpu_model();
        memory_model_ = get_memory_model();
    }
    
    ~WindowsMetricsCollector() {
        if (cpu_query_) {
            PdhCloseQuery(cpu_query_);
        }
    }
    
    CpuMetrics collect_cpu() override {
        CpuMetrics metrics;
        metrics.core_count = core_count_;
        metrics.model_name = cpu_model_;
        
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
        metrics.model_name = memory_model_;
        
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
            disk.model_name = get_disk_model(mount_point);
            
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
            
            // Skip virtual/software adapters
            if (row.Type != IF_TYPE_ETHERNET_CSMACD && row.Type != IF_TYPE_IEEE80211) {
                continue;
            }
            
            if (row.InterfaceAndOperStatusFlags.FilterInterface || 
                row.InterfaceAndOperStatusFlags.NotMediaConnected) {
                continue;
            }
            
            // Convert wide string to narrow string
            char interface_name[256];
            WideCharToMultiByte(CP_UTF8, 0, row.Alias, -1, interface_name, sizeof(interface_name), nullptr, nullptr);
            std::string if_name(interface_name);
            
            // Skip virtual adapters by name (Wi-Fi Direct, Hyper-V, VirtualBox, etc.)
            if (if_name.find("*") != std::string::npos ||
                if_name.find("Virtual") != std::string::npos ||
                if_name.find("vEthernet") != std::string::npos ||
                if_name.find("Hyper-V") != std::string::npos ||
                if_name.find("VirtualBox") != std::string::npos ||
                if_name.find("VMware") != std::string::npos ||
                if_name.find("Bluetooth") != std::string::npos) {
                continue;
            }
            
            // Specific interfaces requested, check if any one matches
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
            
            // Get adapter description (model name)
            char desc[256];
            WideCharToMultiByte(CP_UTF8, 0, row.Description, -1, desc, sizeof(desc), nullptr, nullptr);
            net.model_name = std::string(desc);
            
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
    std::string cpu_model_;
    std::string memory_model_;
    std::unique_ptr<WMIHelper> wmi_;
    
    // Helper function to get CPU model using WMI (more reliable than registry)
    std::string get_cpu_model() {
        if (!wmi_) return "Unknown CPU";
        
        std::string name = wmi_->query_single_property(
            L"SELECT Name FROM Win32_Processor", L"Name");
        
        if (!name.empty()) {
            // Trim spaces
            size_t start = name.find_first_not_of(" \t");
            size_t end = name.find_last_not_of(" \t");
            if (start != std::string::npos) {
                return name.substr(start, end - start + 1);
            }
        }
        
        // Fallback to registry if WMI fails
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, 
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char buffer[256];
            DWORD bufferSize = sizeof(buffer);
            if (RegQueryValueExA(hKey, "ProcessorNameString", nullptr, nullptr, 
                (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                std::string fallback_name(buffer);
                size_t start = fallback_name.find_first_not_of(" \t");
                size_t end = fallback_name.find_last_not_of(" \t");
                return (start == std::string::npos) ? "" : fallback_name.substr(start, end - start + 1);
            }
            RegCloseKey(hKey);
        }
        return "Unknown CPU";
    }
    
    // Helper function to get memory manufacturer/model using WMI
    std::string get_memory_model() {
        if (!wmi_) {
            std::cerr << "[DEBUG] WMI not initialized for memory query\n";
            return "System Memory";
        }
        std::cerr << "[DEBUG] Querying WMI for memory information...\n";
        // Try more properties for diagnostics
        std::vector<const wchar_t*> props = {L"Manufacturer", L"PartNumber", L"BankLabel", L"SerialNumber"};
        auto results = wmi_->query_multiple_properties(
            L"SELECT Manufacturer, PartNumber, BankLabel, SerialNumber FROM Win32_PhysicalMemory",
            props);
        std::cerr << "[DEBUG] WMI query returned " << results.size() << " results\n";
        for (size_t i = 0; i < results.size(); ++i) {
            std::cerr << "[DEBUG] Result[" << i << "]: '" << results[i] << "'\n";
        }
        // Try to build a model string from available info
        std::string manufacturer, part_number, bank_label, serial_number;
        if (results.size() >= 4) {
            manufacturer = results[0];
            part_number = results[1];
            bank_label = results[2];
            serial_number = results[3];
        }
        auto trim = [](std::string& s) {
            size_t start = s.find_first_not_of(" \t");
            size_t end = s.find_last_not_of(" \t");
            if (start != std::string::npos) {
                s = s.substr(start, end - start + 1);
            }
        };
        trim(manufacturer);
        trim(part_number);
        trim(bank_label);
        trim(serial_number);
        std::cerr << "[DEBUG] Trimmed manufacturer: '" << manufacturer << "'\n";
        std::cerr << "[DEBUG] Trimmed part_number: '" << part_number << "'\n";
        std::cerr << "[DEBUG] Trimmed bank_label: '" << bank_label << "'\n";
        std::cerr << "[DEBUG] Trimmed serial_number: '" << serial_number << "'\n";
        if (!manufacturer.empty() && !part_number.empty()) {
            std::string result = manufacturer + " " + part_number;
            std::cerr << "[DEBUG] Returning: '" << result << "'\n";
            return result;
        } else if (!manufacturer.empty()) {
            std::string result = manufacturer + " RAM";
            std::cerr << "[DEBUG] Returning (manufacturer only): '" << result << "'\n";
            return result;
        } else if (!bank_label.empty()) {
            std::string result = "Bank " + bank_label;
            std::cerr << "[DEBUG] Returning (bank label only): '" << result << "'\n";
            return result;
        } else if (!serial_number.empty()) {
            std::string result = "Serial " + serial_number;
            std::cerr << "[DEBUG] Returning (serial number only): '" << result << "'\n";
            return result;
        }
        std::cerr << "[DEBUG] Falling back to 'System Memory'\n";
        return "System Memory";
    }
    
    // Helper function to get disk model using WMI
    std::string get_disk_model(const std::string& mount_point) {
        if (mount_point.length() < 2 || !wmi_) return "Unknown Drive";
        
        // Extract drive letter (e.g., "C" from "C:\\")
        char drive_letter = mount_point[0];
        
        // First, try WMI approach
        std::wstringstream query;
        query << L"ASSOCIATORS OF {Win32_LogicalDisk.DeviceID='" << drive_letter 
              << L":'} WHERE AssocClass=Win32_LogicalDiskToPartition";
        
        std::string partition_query_str = wmi_->query_single_property(
            query.str().c_str(), L"DeviceID");
        
        if (!partition_query_str.empty()) {
            // Now get the physical disk
            std::wstringstream disk_query;
            disk_query << L"ASSOCIATORS OF {Win32_DiskPartition.DeviceID='" 
                      << std::wstring(partition_query_str.begin(), partition_query_str.end())
                      << L"'} WHERE AssocClass=Win32_DiskDriveToDiskPartition";
            
            std::string model = wmi_->query_single_property(disk_query.str().c_str(), L"Model");
            
            if (!model.empty()) {
                // Trim spaces
                size_t start = model.find_first_not_of(" \t");
                size_t end = model.find_last_not_of(" \t");
                if (start != std::string::npos) {
                    return model.substr(start, end - start + 1);
                }
            }
        }
        
        // Fallback to direct WMI query for first physical drive
        std::string model = wmi_->query_single_property(
            L"SELECT Model FROM Win32_DiskDrive WHERE Index=0", L"Model");
        
        if (!model.empty()) {
            size_t start = model.find_first_not_of(" \t");
            size_t end = model.find_last_not_of(" \t");
            if (start != std::string::npos) {
                return model.substr(start, end - start + 1);
            }
        }
        
        // Final fallback to IOCTL method
        std::string drive = mount_point.substr(0, 2);
        std::string physical_drive = "\\\\.\\PhysicalDrive0";
        
        // Open the physical drive
        HANDLE hDevice = CreateFileA(physical_drive.c_str(), 0, 
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        
        if (hDevice == INVALID_HANDLE_VALUE) {
            return "Unknown Drive";
        }
        
        STORAGE_PROPERTY_QUERY storage_query;
        storage_query.PropertyId = StorageDeviceProperty;
        storage_query.QueryType = PropertyStandardQuery;
        
        BYTE buffer[4096];
        DWORD bytesReturned;
        
        if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &storage_query, sizeof(storage_query),
            buffer, sizeof(buffer), &bytesReturned, nullptr)) {
            
            STORAGE_DEVICE_DESCRIPTOR* desc = (STORAGE_DEVICE_DESCRIPTOR*)buffer;
            
            std::string model;
            if (desc->ProductIdOffset > 0) {
                model = (char*)(buffer + desc->ProductIdOffset);
                // Trim spaces
                size_t start = model.find_first_not_of(" \t");
                size_t end = model.find_last_not_of(" \t");
                if (start != std::string::npos) {
                    model = model.substr(start, end - start + 1);
                }
            }
            
            CloseHandle(hDevice);
            return model.empty() ? "Unknown Drive" : model;
        }
        
        CloseHandle(hDevice);
        return "Unknown Drive";
    }
};

std::unique_ptr<MetricsCollector> create_windows_metrics_collector() {
    return std::make_unique<WindowsMetricsCollector>();
}

} // namespace sysmon
