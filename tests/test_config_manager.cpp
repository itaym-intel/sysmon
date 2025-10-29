#include <catch2/catch_test_macros.hpp>
#include "sysmon/config_manager.hpp"
#include <fstream>

TEST_CASE("ConfigManager loads valid YAML", "[config]") {
    // Create a temporary test config
    const char* test_config = R"(
version: "1.0"
update_interval: 2
history_size: 30

cpu:
  enabled: true
  thresholds:
    warning: 70.0
    critical: 90.0
  show_per_core: true

memory:
  enabled: true
  thresholds:
    warning: 80.0
    critical: 95.0
  show_swap: true

disk:
  enabled: true
  thresholds:
    warning: 75.0
    critical: 90.0
  mount_points:
    - path: "/"
      label: "Root"

network:
  enabled: false

display:
  color_scheme: "default"
  refresh_rate: 1
  show_graphs: true
  graph_height: 10

alerts:
  enabled: true
  beep_on_critical: false
  log_to_file: true
  log_path: "./test.log"
)";

    std::ofstream out("test_config.yaml");
    out << test_config;
    out.close();

    sysmon::ConfigManager manager("test_config.yaml");
    REQUIRE(manager.load());
    
    const auto& config = manager.get_config();
    REQUIRE(config.cpu.enabled == true);
    REQUIRE(config.cpu.thresholds.warning == 70.0);
    REQUIRE(config.update_interval == 2);
    REQUIRE(config.history_size == 30);
}

TEST_CASE("ConfigManager validates thresholds", "[config]") {
    // Create invalid config
    const char* invalid_config = R"(
version: "1.0"
update_interval: 2
history_size: 30

cpu:
  enabled: true
  thresholds:
    warning: 95.0
    critical: 70.0
  show_per_core: true

memory:
  enabled: true
  thresholds:
    warning: 80.0
    critical: 95.0
  show_swap: true

disk:
  enabled: true
  thresholds:
    warning: 75.0
    critical: 90.0
  mount_points:
    - path: "/"
      label: "Root"

network:
  enabled: false

display:
  color_scheme: "default"
  refresh_rate: 1
  show_graphs: true
  graph_height: 10

alerts:
  enabled: true
  beep_on_critical: false
  log_to_file: false
  log_path: ""
)";

    std::ofstream out("invalid_config.yaml");
    out << invalid_config;
    out.close();

    sysmon::ConfigManager manager("invalid_config.yaml");
    manager.load();
    
    std::string error;
    REQUIRE_FALSE(manager.validate_config(error));
}

TEST_CASE("ThresholdConfig validates correctly", "[config]") {
    sysmon::ThresholdConfig valid{70.0, 90.0};
    REQUIRE(valid.validate());
    
    sysmon::ThresholdConfig invalid{90.0, 70.0};
    REQUIRE_FALSE(invalid.validate());
    
    sysmon::ThresholdConfig out_of_range{-10.0, 110.0};
    REQUIRE_FALSE(out_of_range.validate());
}
