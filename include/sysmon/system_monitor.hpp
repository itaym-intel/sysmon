#pragma once

#include "sysmon/config_manager.hpp"
#include "sysmon/metrics_collector.hpp"
#include "sysmon/alert_engine.hpp"
#include "sysmon/display.hpp"
#include <memory>
#include <deque>
#include <atomic>

namespace sysmon {

class SystemMonitor {
public:
    SystemMonitor(const std::string& config_path);
    
    // Initialize all components
    bool initialize();
    
    // Main monitoring loop
    void run();
    
    // Stop monitoring
    void stop();

private:
    void monitoring_loop();
    
    std::string config_path_;
    ConfigManager config_manager_;
    std::unique_ptr<MetricsCollector> metrics_collector_;
    std::unique_ptr<AlertEngine> alert_engine_;
    std::unique_ptr<Display> display_;
    
    std::deque<double> cpu_history_;
    std::deque<double> memory_history_;
    std::vector<Alert> active_alerts_;
    
    std::atomic<bool> running_{false};
};

} // namespace sysmon
