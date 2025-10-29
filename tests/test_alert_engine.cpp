#include <catch2/catch_test_macros.hpp>
#include "sysmon/alert_engine.hpp"

TEST_CASE("AlertEngine triggers warnings correctly", "[alerts]") {
    sysmon::AlertConfig alert_config;
    alert_config.enabled = true;
    alert_config.log_to_file = false;
    
    sysmon::AlertEngine engine(alert_config);
    
    sysmon::CpuConfig cpu_config;
    cpu_config.enabled = true;
    cpu_config.thresholds.warning = 70.0;
    cpu_config.thresholds.critical = 90.0;
    
    SECTION("Normal CPU usage generates no alerts") {
        sysmon::CpuMetrics metrics;
        metrics.overall_usage = 50.0;
        metrics.core_count = 4;
        
        auto alerts = engine.check_cpu(metrics, cpu_config);
        REQUIRE(alerts.empty());
    }
    
    SECTION("Warning threshold triggers warning alert") {
        sysmon::CpuMetrics metrics;
        metrics.overall_usage = 75.0;
        metrics.core_count = 4;
        
        auto alerts = engine.check_cpu(metrics, cpu_config);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].level == sysmon::AlertLevel::Warning);
        REQUIRE(alerts[0].category == "CPU");
    }
    
    SECTION("Critical threshold triggers critical alert") {
        sysmon::CpuMetrics metrics;
        metrics.overall_usage = 95.0;
        metrics.core_count = 4;
        
        auto alerts = engine.check_cpu(metrics, cpu_config);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].level == sysmon::AlertLevel::Critical);
    }
}

TEST_CASE("AlertEngine checks memory correctly", "[alerts]") {
    sysmon::AlertConfig alert_config;
    alert_config.enabled = true;
    alert_config.log_to_file = false;
    
    sysmon::AlertEngine engine(alert_config);
    
    sysmon::MemoryConfig memory_config;
    memory_config.enabled = true;
    memory_config.thresholds.warning = 80.0;
    memory_config.thresholds.critical = 95.0;
    
    SECTION("Normal memory usage") {
        sysmon::MemoryMetrics metrics;
        metrics.total_bytes = 16ULL * 1024 * 1024 * 1024;  // 16 GB
        metrics.used_bytes = 8ULL * 1024 * 1024 * 1024;    // 8 GB
        metrics.usage_percent = 50.0;
        
        auto alerts = engine.check_memory(metrics, memory_config);
        REQUIRE(alerts.empty());
    }
    
    SECTION("High memory usage triggers alert") {
        sysmon::MemoryMetrics metrics;
        metrics.total_bytes = 16ULL * 1024 * 1024 * 1024;
        metrics.used_bytes = 14ULL * 1024 * 1024 * 1024;
        metrics.usage_percent = 87.5;
        
        auto alerts = engine.check_memory(metrics, memory_config);
        REQUIRE(alerts.size() == 1);
        REQUIRE(alerts[0].level == sysmon::AlertLevel::Warning);
    }
}

TEST_CASE("AlertEngine can be disabled", "[alerts]") {
    sysmon::AlertConfig alert_config;
    alert_config.enabled = false;
    
    sysmon::AlertEngine engine(alert_config);
    
    sysmon::CpuConfig cpu_config;
    cpu_config.thresholds.warning = 70.0;
    cpu_config.thresholds.critical = 90.0;
    
    sysmon::CpuMetrics metrics;
    metrics.overall_usage = 95.0;  // Should trigger, but won't because disabled
    
    auto alerts = engine.check_cpu(metrics, cpu_config);
    REQUIRE(alerts.empty());
}
