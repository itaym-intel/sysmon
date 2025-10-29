#include "sysmon/system_monitor.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <map>

namespace sysmon {

SystemMonitor::SystemMonitor(const std::string& config_path)
    : config_path_(config_path)
    , config_manager_(config_path)
{
}

bool SystemMonitor::initialize() {
    // Load configuration
    if (!config_manager_.load()) {
        std::cerr << "Failed to load configuration from " << config_path_ << "\n";
        return false;
    }
    
    // Validate configuration
    std::string validation_error;
    if (!config_manager_.validate_config(validation_error)) {
        std::cerr << "Configuration validation failed: " << validation_error << "\n";
        return false;
    }
    
    const auto& config = config_manager_.get_config();
    
    // Initialize components
    metrics_collector_ = create_metrics_collector();
    alert_engine_ = std::make_unique<AlertEngine>(config.alerts);
    display_ = std::make_unique<Display>(config.display);
    
    return true;
}

void SystemMonitor::run() {
    if (!metrics_collector_ || !alert_engine_ || !display_) {
        std::cerr << "System monitor not initialized. Call initialize() first.\n";
        return;
    }
    
    running_ = true;
    monitoring_loop();
}

void SystemMonitor::stop() {
    running_ = false;
}

void SystemMonitor::monitoring_loop() {
    const auto& config = config_manager_.get_config();
    
    std::cout << "SysMon started. Monitoring system with config: " << config_path_ << "\n";
    std::cout << "Press Ctrl+C to exit.\n\n";
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    while (running_) {
        auto loop_start = std::chrono::steady_clock::now();
        
        // Hot-reload configuration if changed
        if (config_manager_.check_and_reload()) {
            const auto& new_config = config_manager_.get_config();
            display_->update_config(new_config.display);
            alert_engine_->update_config(new_config.alerts);
        }
        
        const auto& current_config = config_manager_.get_config();
        
        // Collect metrics
        CpuMetrics cpu_metrics;
        MemoryMetrics memory_metrics;
        std::vector<DiskMetrics> disk_metrics;
        std::vector<NetworkMetrics> network_metrics;
        
        if (current_config.cpu.enabled) {
            cpu_metrics = metrics_collector_->collect_cpu();
        }
        if (current_config.memory.enabled) {
            memory_metrics = metrics_collector_->collect_memory();
        }
        if (current_config.disk.enabled) {
            std::vector<std::string> mount_points;
            for (const auto& mp : current_config.disk.mount_points) {
                mount_points.push_back(mp.path);
            }
            disk_metrics = metrics_collector_->collect_disk(mount_points);
            
            // Apply labels
            for (size_t i = 0; i < disk_metrics.size() && i < current_config.disk.mount_points.size(); ++i) {
                disk_metrics[i].label = current_config.disk.mount_points[i].label;
            }
        }
        if (current_config.network.enabled) {
            network_metrics = metrics_collector_->collect_network(current_config.network.interfaces);
            
            // Calculate bandwidth rates from previous samples
            struct NetworkSample {
                uint64_t bytes_received;
                uint64_t bytes_sent;
                std::chrono::steady_clock::time_point timestamp;
            };
            static std::map<std::string, NetworkSample> prev_samples;
            auto now = std::chrono::steady_clock::now();
            
            for (auto& net : network_metrics) {
                auto it = prev_samples.find(net.interface_name);
                if (it != prev_samples.end()) {
                    auto time_diff = std::chrono::duration<double>(now - it->second.timestamp).count();
                    
                    if (time_diff > 0) {
                        double rx_bytes_diff = static_cast<double>(net.bytes_received - it->second.bytes_received);
                        double tx_bytes_diff = static_cast<double>(net.bytes_sent - it->second.bytes_sent);
                        
                        net.download_mbps = (rx_bytes_diff * 8.0) / (time_diff * 1000000.0);
                        net.upload_mbps = (tx_bytes_diff * 8.0) / (time_diff * 1000000.0);
                    }
                }
                prev_samples[net.interface_name] = {net.bytes_received, net.bytes_sent, now};
            }
        }
        
        // Update history
        cpu_history_.push_back(cpu_metrics.overall_usage);
        memory_history_.push_back(memory_metrics.usage_percent);
        if (cpu_history_.size() > static_cast<size_t>(current_config.history_size)) {
            cpu_history_.pop_front();
            memory_history_.pop_front();
        }
        
        // Check for alerts
        active_alerts_.clear();
        if (current_config.cpu.enabled) {
            auto cpu_alerts = alert_engine_->check_cpu(cpu_metrics, current_config.cpu);
            active_alerts_.insert(active_alerts_.end(), cpu_alerts.begin(), cpu_alerts.end());
        }
        if (current_config.memory.enabled) {
            auto mem_alerts = alert_engine_->check_memory(memory_metrics, current_config.memory);
            active_alerts_.insert(active_alerts_.end(), mem_alerts.begin(), mem_alerts.end());
        }
        if (current_config.disk.enabled) {
            auto disk_alerts = alert_engine_->check_disk(disk_metrics, current_config.disk);
            active_alerts_.insert(active_alerts_.end(), disk_alerts.begin(), disk_alerts.end());
        }
        
        // Log alerts
        for (const auto& alert : active_alerts_) {
            alert_engine_->log_alert(alert);
            if (alert.level == AlertLevel::Critical) {
                alert_engine_->beep_if_enabled();
            }
        }
        
        // Render display
        display_->render(cpu_metrics, memory_metrics, disk_metrics, network_metrics, active_alerts_, 
                        cpu_history_, memory_history_,
                        current_config.cpu.thresholds,
                        current_config.memory.thresholds,
                        current_config.disk.thresholds);
        
        // Sleep until next update
        auto loop_end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(loop_end - loop_start);
        auto sleep_duration = std::chrono::seconds(current_config.update_interval) - elapsed;
        
        if (sleep_duration.count() > 0) {
            std::this_thread::sleep_for(sleep_duration);
        }
    }
}

} // namespace sysmon
