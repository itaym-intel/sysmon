#include "sysmon/display.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

namespace sysmon {

Display::Display(const DisplayConfig& config)
    : config_(config)
{
#ifdef _WIN32
    // Set console to UTF-8 mode for Unicode characters
    SetConsoleOutputCP(CP_UTF8);
    
    // Enable ANSI escape codes on Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}

void Display::update_config(const DisplayConfig& config) {
    config_ = config;
}

std::string Display::color_code(AlertLevel level) {
    if (config_.color_scheme == "mono") {
        return "";
    }
    
    switch (level) {
        case AlertLevel::Normal:   return "\033[32m";  // Green
        case AlertLevel::Warning:  return "\033[33m";  // Yellow
        case AlertLevel::Critical: return "\033[31m";  // Red
        default:                   return "\033[0m";   // Reset
    }
}

std::string Display::reset_color() {
    if (config_.color_scheme == "mono") {
        return "";
    }
    return "\033[0m";
}

std::string Display::colorize(const std::string& text, AlertLevel level) {
    return color_code(level) + text + reset_color();
}

void Display::clear_screen() {
#ifdef _WIN32
    std::cout << "\033[2J\033[H";
#else
    std::cout << "\033[2J\033[H";
#endif
    std::cout << std::flush;
}

std::string format_bytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return oss.str();
}

std::string alert_icon(AlertLevel level) {
    switch (level) {
        case AlertLevel::Normal:   return "\u2713";  // ✓
        case AlertLevel::Warning:  return "\u26A0";  // ⚠
        case AlertLevel::Critical: return "\U0001F534"; // 🔴
        default:                   return " ";
    }
}

AlertLevel Display::get_alert_level(double value, const ThresholdConfig& thresholds) {
    if (value >= thresholds.critical) {
        return AlertLevel::Critical;
    } else if (value >= thresholds.warning) {
        return AlertLevel::Warning;
    }
    return AlertLevel::Normal;
}

std::string Display::create_progress_bar(double percentage, int width, AlertLevel level) {
    int filled = static_cast<int>(percentage / 100.0 * width);
    std::string bar = std::string(filled, '\u2588') + std::string(width - filled, '\u2591');
    return colorize(bar, level);
}

std::string Display::create_graph(const std::deque<double>& data, int height) {
    if (data.empty()) {
        return std::string(30, '\u2581');
    }
    
    // Find max value for scaling
    double max_val = *std::max_element(data.begin(), data.end());
    if (max_val == 0.0) max_val = 1.0;
    
    // Unicode block characters for different heights
    const char* blocks[] = {" ", "\u2581", "\u2582", "\u2583", "\u2584", "\u2585", "\u2586", "\u2587", "\u2588"};
    
    std::ostringstream oss;
    for (double val : data) {
        int block_index = static_cast<int>((val / max_val) * 8);
        block_index = std::min(8, std::max(0, block_index));
        oss << blocks[block_index];
    }
    
    return oss.str();
}

void Display::render_header() {
    const int box_width = 60;
    const std::string title = "SYSMON";
    const int padding = (box_width - title.length()) / 2;
    
    std::cout << "\u2554";
    for (int i = 0; i < box_width; ++i) std::cout << "\u2550";
    std::cout << "\u2557\n";
    
    std::cout << "\u2551";
    for (int i = 0; i < padding; ++i) std::cout << " ";
    std::cout << title;
    for (int i = 0; i < box_width - padding - title.length(); ++i) std::cout << " ";
    std::cout << "\u2551\n";
    
    std::cout << "\u255A";
    for (int i = 0; i < box_width; ++i) std::cout << "\u2550";
    std::cout << "\u255D\n\n";
}

void Display::render_cpu(const CpuMetrics& cpu, const CpuConfig& cpu_config) {
    AlertLevel level = get_alert_level(cpu.overall_usage, cpu_config.thresholds);
    
    std::cout << "[CPU]  ";
    std::cout << create_progress_bar(cpu.overall_usage, 20, level);
    std::cout << "  " << colorize(std::to_string(static_cast<int>(cpu.overall_usage)) + "%", level);
    std::cout << "  " << alert_icon(level);
    
    if (level == AlertLevel::Warning) {
        std::cout << colorize(" WARNING", level);
    } else if (level == AlertLevel::Critical) {
        std::cout << colorize(" CRITICAL", level);
    } else {
        std::cout << colorize(" OK", level);
    }
    std::cout << "\n";
    
    // Per-thread display (only if enabled and reasonable number of threads)
    if (cpu_config.show_per_core && !cpu.per_core_usage.empty() && cpu.per_core_usage.size() <= 32) {
        for (size_t i = 0; i < cpu.per_core_usage.size(); ++i) {
            AlertLevel core_level = get_alert_level(cpu.per_core_usage[i], cpu_config.thresholds);
            std::cout << "  Thread " << std::setw(2) << i << ": ";
            std::cout << create_progress_bar(cpu.per_core_usage[i], 20, core_level);
            std::cout << "  " << std::setw(3) << static_cast<int>(cpu.per_core_usage[i]) << "%";
            if (core_level != AlertLevel::Normal) {
                std::cout << "  " << colorize(alert_icon(core_level), core_level);
            }
            std::cout << "\n";
        }
    }
    std::cout << "\n";
}

void Display::render_memory(const MemoryMetrics& memory, const ThresholdConfig& thresholds) {
    AlertLevel level = get_alert_level(memory.usage_percent, thresholds);
    
    std::cout << "[Memory]  ";
    std::cout << create_progress_bar(memory.usage_percent, 20, level);
    std::cout << "  " << colorize(std::to_string(static_cast<int>(memory.usage_percent)) + "%", level);
    std::cout << " (" << format_bytes(memory.used_bytes) << " / " << format_bytes(memory.total_bytes) << ")";
    std::cout << "  " << alert_icon(level);
    
    if (level == AlertLevel::Warning) {
        std::cout << colorize(" WARNING", level);
    } else if (level == AlertLevel::Critical) {
        std::cout << colorize(" CRITICAL", level);
    } else {
        std::cout << colorize(" OK", level);
    }
    std::cout << "\n\n";
}

void Display::render_disks(const std::vector<DiskMetrics>& disks, const ThresholdConfig& thresholds) {
    std::cout << "[Disk]\n";
    
    for (const auto& disk : disks) {
        AlertLevel level = get_alert_level(disk.usage_percent, thresholds);
        
        std::cout << "  " << std::setw(15) << std::left << (disk.label + " (" + disk.mount_point + ")");
        std::cout << create_progress_bar(disk.usage_percent, 20, level);
        std::cout << "  " << std::setw(3) << std::right << static_cast<int>(disk.usage_percent) << "%";
        std::cout << " (" << format_bytes(disk.used_bytes) << " / " << format_bytes(disk.total_bytes) << ")";
        std::cout << "  " << alert_icon(level);
        std::cout << "\n";
    }
    std::cout << "\n";
}

void Display::render_network(const std::vector<NetworkMetrics>& network) {
    std::cout << "[Network]\n";
    
    for (const auto& net : network) {
        std::cout << "  " << std::setw(20) << std::left << net.interface_name;
        
        // Show download speed
        std::cout << "     ↓" << std::setw(5) << std::right << std::fixed << std::setprecision(2) 
                  << net.download_mbps << " Mbps";
        
        // Show upload speed
        std::cout << "     ↑" << std::setw(5) << std::right << std::fixed << std::setprecision(2) 
                  << net.upload_mbps << " Mbps";
        
        // Show total bytes
        std::cout << "  (RX: " << format_bytes(net.bytes_received) 
                  << ", TX: " << format_bytes(net.bytes_sent) << ")";
        
        std::cout << "\n";
    }
    std::cout << "\n";
}

void Display::render_alerts(const std::vector<Alert>& alerts) {
    std::cout << "[Alerts - Last " << std::min(5, static_cast<int>(alerts.size())) << "]\n";
    
    if (alerts.empty()) {
        std::cout << "  " << colorize("No active alerts", AlertLevel::Normal) << "\n";
    } else {
        // Show last 5 alerts
        size_t start = alerts.size() > 5 ? alerts.size() - 5 : 0;
        for (size_t i = start; i < alerts.size(); ++i) {
            const auto& alert = alerts[i];
            
            auto time_t = std::chrono::system_clock::to_time_t(alert.timestamp);
            std::tm tm;
#ifdef _WIN32
            localtime_s(&tm, &time_t);
#else
            localtime_r(&time_t, &tm);
#endif
            
            std::cout << "  " << alert_icon(alert.level) << " ";
            std::cout << std::put_time(&tm, "%H:%M:%S") << " | ";
            std::cout << colorize(alert.message, alert.level) << "\n";
        }
    }
    std::cout << "\n";
}

void Display::render_history(const std::deque<double>& cpu_history, const std::deque<double>& memory_history, int update_interval) {
    if (!config_.show_graphs || cpu_history.empty()) {
        return;
    }
    
    int total_seconds = cpu_history.size() * update_interval;
    std::cout << "[History - Last " << total_seconds << "s]\n";
    std::cout << "CPU:  " << create_graph(cpu_history, config_.graph_height) << "\n";
    std::cout << "MEM:  " << create_graph(memory_history, config_.graph_height) << "\n";
    std::cout << "\n";
}

void Display::render_footer() {
    std::cout << "Press Ctrl+C to quit, Config hot-reload enabled\n";
}

void Display::render(const CpuMetrics& cpu,
                    const MemoryMetrics& memory,
                    const std::vector<DiskMetrics>& disks,
                    const std::vector<NetworkMetrics>& network,
                    const std::vector<Alert>& active_alerts,
                    const std::deque<double>& cpu_history,
                    const std::deque<double>& memory_history,
                    const CpuConfig& cpu_config,
                    const ThresholdConfig& memory_thresholds,
                    const ThresholdConfig& disk_thresholds,
                    int update_interval)
{
    clear_screen();
    
    render_header();
    render_cpu(cpu, cpu_config);
    render_memory(memory, memory_thresholds);
    render_disks(disks, disk_thresholds);
    
    if (!network.empty()) {
        render_network(network);
    }
    
    render_alerts(active_alerts);
    render_history(cpu_history, memory_history, update_interval);
    render_footer();
}

} // namespace sysmon
