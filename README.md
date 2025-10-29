# sysmon

![Version](https://img.shields.io/badge/version-1.0.0-blue)
![C++](https://img.shields.io/badge/C++-20-brightgreen)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)

**sysmon** is a real-time system monitor that displays CPU, memory, disk, and network statistics and custom alerts.

### Prerequisites

- CMake 3.20 or higher
- C++20 compatible compiler (GCC 10+, Clang 11+, MSVC 2019+)
- [Typiconf library](https://github.com/itaym-intel/typiconf) (automatically fetched)

### Build

```bash
cd sysmon

mkdir build && cd build

cmake .. # see cmakelists
cmake --build . --config Release

./sysmon
```

### Configuration

SysMon uses YAML configuration files.

```yaml
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

display:
  color_scheme: "default"  # default, mono
  refresh_rate: 1
  show_graphs: true
  graph_height: 10

alerts:
  enabled: true
  beep_on_critical: false
  log_to_file: true
  log_path: "./sysmon.log"
```

### Configuration Profiles

You can use sysmon with a pre-configured profile.

```bash
./sysmon config/profiles/developer.yaml
```

Sysmon has hot reload can edit your configuration file while sysmon is running.

```bash
./sysmon config/default_config.yaml

vim config/default_config.yaml # then edit your config
```

## Options

### Top-Level Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `version` | string | "1.0" | Configuration version |
| `update_interval` | int | 2 | Seconds between metric updates |
| `history_size` | int | 30 | Number of historical samples to keep |

### CPU Monitoring

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `cpu.enabled` | bool | true | Enable CPU monitoring |
| `cpu.thresholds.warning` | float | 70.0 | Warning threshold (%) |
| `cpu.thresholds.critical` | float | 90.0 | Critical threshold (%) |
| `cpu.show_per_core` | bool | true | Show per-core statistics |

### Memory Monitoring

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `memory.enabled` | bool | true | Enable memory monitoring |
| `memory.thresholds.warning` | float | 80.0 | Warning threshold (%) |
| `memory.thresholds.critical` | float | 95.0 | Critical threshold (%) |
| `memory.show_swap` | bool | true | Show swap memory info |

### Disk Monitoring

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `disk.enabled` | bool | true | Enable disk monitoring |
| `disk.thresholds.warning` | float | 75.0 | Warning threshold (%) |
| `disk.thresholds.critical` | float | 90.0 | Critical threshold (%) |
| `disk.mount_points` | array | - | List of mount points to monitor |

### Display Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `display.color_scheme` | string | "default" | Color scheme (default, mono) |
| `display.refresh_rate` | int | 1 | Display refresh rate (seconds) |
| `display.show_graphs` | bool | true | Show ASCII history graphs |
| `display.graph_height` | int | 10 | Height of ASCII graphs |

### Alerts

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `alerts.enabled` | bool | true | Enable alert system |
| `alerts.beep_on_critical` | bool | false | Beep on critical alerts |
| `alerts.log_to_file` | bool | true | Log alerts to file |
| `alerts.log_path` | string | "./sysmon.log" | Path to log file |