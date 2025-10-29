#include "sysmon/system_monitor.hpp"
#include <iostream>
#include <csignal>
#include <memory>

// Global pointer for signal handler
sysmon::SystemMonitor* g_monitor = nullptr;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n\nReceived shutdown signal. Stopping monitor...\n";
        if (g_monitor) {
            g_monitor->stop();
        }
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [config_file]\n";
    std::cout << "\n";
    std::cout << "Options:\n";
    std::cout << "  config_file    Path to YAML configuration file (default: config/default_config.yaml)\n";
    std::cout << "  -h, --help     Show this help message\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << "\n";
    std::cout << "  " << program_name << " config/profiles/server.yaml\n";
    std::cout << "  " << program_name << " custom_config.yaml\n";
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    // Parse command-line arguments
    std::string config_path = "config/default_config.yaml";
    
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        config_path = arg;
    }
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Create and initialize system monitor
    auto monitor = std::make_unique<sysmon::SystemMonitor>(config_path);
    g_monitor = monitor.get();
    
    if (!monitor->initialize()) {
        std::cerr << "Failed to initialize system monitor\n";
        return 1;
    }
    
    // Run monitoring loop
    monitor->run();
    
    std::cout << "SysMon stopped.\n";
    return 0;
}
