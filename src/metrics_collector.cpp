#include "sysmon/metrics_collector.hpp"

namespace sysmon {

// Platform-specific implementations are in platform/ subdirectory

#ifdef _WIN32
    // Forward declare Windows implementation
    class WindowsMetricsCollector;
    std::unique_ptr<MetricsCollector> create_metrics_collector() {
        extern std::unique_ptr<MetricsCollector> create_windows_metrics_collector();
        return create_windows_metrics_collector();
    }
#elif __linux__
    // Forward declare Linux implementation
    class LinuxMetricsCollector;
    std::unique_ptr<MetricsCollector> create_metrics_collector() {
        extern std::unique_ptr<MetricsCollector> create_linux_metrics_collector();
        return create_linux_metrics_collector();
    }
#elif __APPLE__
    // Forward declare macOS implementation
    class MacOSMetricsCollector;
    std::unique_ptr<MetricsCollector> create_metrics_collector() {
        extern std::unique_ptr<MetricsCollector> create_macos_metrics_collector();
        return create_macos_metrics_collector();
    }
#else
    #error "Unsupported platform"
#endif

} // namespace sysmon
