#pragma once

#include "sysmon/config_manager.hpp"
#include "sysmon/metrics_collector.hpp"
#include "sysmon/alert_engine.hpp"
#include <deque>
#include <string>

namespace sysmon {

class Display {
public:
    explicit Display(const DisplayConfig& config);
    
    // Clear screen and render full dashboard
    void render(const CpuMetrics& cpu,
                const MemoryMetrics& memory,
                const std::vector<DiskMetrics>& disks,
                const std::vector<Alert>& active_alerts,
                const std::deque<double>& cpu_history,
                const std::deque<double>& memory_history);
    
    // Update configuration (for hot-reload)
    void update_config(const DisplayConfig& config);

private:
    void render_header();
    void render_cpu(const CpuMetrics& cpu, const ThresholdConfig& thresholds);
    void render_memory(const MemoryMetrics& memory, const ThresholdConfig& thresholds);
    void render_disks(const std::vector<DiskMetrics>& disks, const ThresholdConfig& thresholds);
    void render_alerts(const std::vector<Alert>& alerts);
    void render_history(const std::deque<double>& cpu_history, const std::deque<double>& memory_history);
    void render_footer();
    
    // Helper rendering functions
    std::string create_progress_bar(double percentage, int width, AlertLevel level);
    std::string create_graph(const std::deque<double>& data, int height);
    AlertLevel get_alert_level(double value, const ThresholdConfig& thresholds);
    
    // Color helpers (ANSI escape codes)
    std::string colorize(const std::string& text, AlertLevel level);
    std::string color_code(AlertLevel level);
    std::string reset_color();
    void clear_screen();
    
    DisplayConfig config_;
};

// Helper functions for formatting
std::string format_bytes(uint64_t bytes);
std::string alert_icon(AlertLevel level);

} // namespace sysmon
