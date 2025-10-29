#include "sysmon/alert_engine.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace sysmon {

AlertEngine::AlertEngine(const AlertConfig& config)
    : alert_config_(config)
{
    if (alert_config_.log_to_file && alert_config_.enabled) {
        log_file_.open(alert_config_.log_path, std::ios::app);
        if (!log_file_.is_open()) {
            std::cerr << "Warning: Failed to open log file: " << alert_config_.log_path << "\n";
        }
    }
}

AlertEngine::~AlertEngine() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void AlertEngine::update_config(const AlertConfig& config) {
    alert_config_ = config;
    
    // Reopen log file if needed
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    if (alert_config_.log_to_file && alert_config_.enabled) {
        log_file_.open(alert_config_.log_path, std::ios::app);
    }
}

AlertLevel AlertEngine::determine_level(double value, const ThresholdConfig& thresholds) {
    if (value >= thresholds.critical) {
        return AlertLevel::Critical;
    } else if (value >= thresholds.warning) {
        return AlertLevel::Warning;
    }
    return AlertLevel::Normal;
}

std::string AlertEngine::format_timestamp(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::vector<Alert> AlertEngine::check_cpu(const CpuMetrics& metrics, const CpuConfig& config) {
    std::vector<Alert> alerts;
    
    if (!alert_config_.enabled || !config.enabled) {
        return alerts;
    }
    
    // Check overall CPU
    AlertLevel level = determine_level(metrics.overall_usage, config.thresholds);
    if (level != AlertLevel::Normal) {
        Alert alert;
        alert.category = "CPU";
        alert.level = level;
        alert.timestamp = std::chrono::system_clock::now();
        
        std::ostringstream oss;
        oss << "CPU usage: " << std::fixed << std::setprecision(1) << metrics.overall_usage << "%";
        if (level == AlertLevel::Critical) {
            oss << " (critical threshold: " << config.thresholds.critical << "%)";
        } else {
            oss << " (warning threshold: " << config.thresholds.warning << "%)";
        }
        alert.message = oss.str();
        alerts.push_back(alert);
    }
    
    // Check per-core if enabled
    if (config.show_per_core) {
        for (size_t i = 0; i < metrics.per_core_usage.size(); ++i) {
            AlertLevel core_level = determine_level(metrics.per_core_usage[i], config.thresholds);
            if (core_level == AlertLevel::Critical) {
                Alert alert;
                alert.category = "CPU";
                alert.level = core_level;
                alert.timestamp = std::chrono::system_clock::now();
                
                std::ostringstream oss;
                oss << "CPU Core " << i << " usage: " << std::fixed << std::setprecision(1) 
                    << metrics.per_core_usage[i] << "% (critical)";
                alert.message = oss.str();
                alerts.push_back(alert);
            }
        }
    }
    
    return alerts;
}

std::vector<Alert> AlertEngine::check_memory(const MemoryMetrics& metrics, const MemoryConfig& config) {
    std::vector<Alert> alerts;
    
    if (!alert_config_.enabled || !config.enabled) {
        return alerts;
    }
    
    AlertLevel level = determine_level(metrics.usage_percent, config.thresholds);
    if (level != AlertLevel::Normal) {
        Alert alert;
        alert.category = "Memory";
        alert.level = level;
        alert.timestamp = std::chrono::system_clock::now();
        
        std::ostringstream oss;
        oss << "Memory usage: " << std::fixed << std::setprecision(1) << metrics.usage_percent << "%";
        oss << " (" << (metrics.used_bytes / (1024*1024*1024)) << " GB / " 
            << (metrics.total_bytes / (1024*1024*1024)) << " GB)";
        alert.message = oss.str();
        alerts.push_back(alert);
    }
    
    return alerts;
}

std::vector<Alert> AlertEngine::check_disk(const std::vector<DiskMetrics>& metrics, const DiskConfig& config) {
    std::vector<Alert> alerts;
    
    if (!alert_config_.enabled || !config.enabled) {
        return alerts;
    }
    
    for (const auto& disk : metrics) {
        AlertLevel level = determine_level(disk.usage_percent, config.thresholds);
        if (level != AlertLevel::Normal) {
            Alert alert;
            alert.category = "Disk";
            alert.level = level;
            alert.timestamp = std::chrono::system_clock::now();
            
            std::ostringstream oss;
            oss << disk.label << " (" << disk.mount_point << ") usage: " 
                << std::fixed << std::setprecision(1) << disk.usage_percent << "%";
            oss << " (" << (disk.used_bytes / (1024*1024*1024)) << " GB / " 
                << (disk.total_bytes / (1024*1024*1024)) << " GB)";
            alert.message = oss.str();
            alerts.push_back(alert);
        }
    }
    
    return alerts;
}

void AlertEngine::log_alert(const Alert& alert) {
    if (!alert_config_.log_to_file || !log_file_.is_open()) {
        return;
    }
    
    std::string level_str;
    switch (alert.level) {
        case AlertLevel::Warning:  level_str = "WARNING";  break;
        case AlertLevel::Critical: level_str = "CRITICAL"; break;
        default:                   level_str = "INFO";     break;
    }
    
    log_file_ << "[" << format_timestamp(alert.timestamp) << "] "
              << level_str << " - " << alert.category << ": " << alert.message << "\n";
    log_file_.flush();
}

void AlertEngine::beep_if_enabled() {
    if (!alert_config_.beep_on_critical) {
        return;
    }
    
#ifdef _WIN32
    // Windows beep - need windows.h header
    ::Beep(750, 300);
#else
    // Unix terminal bell
    std::cout << "\a" << std::flush;
#endif
}

} // namespace sysmon
