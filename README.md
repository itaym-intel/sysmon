# SysMon - System Monitor Dashboard

![Version](https://img.shields.io/badge/version-1.0.0-blue)
![C++](https://img.shields.io/badge/C++-20-brightgreen)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)

**SysMon** is a real-time system monitoring tool that displays CPU, memory, disk, and network statistics with configurable alert thresholds. It features a beautiful terminal UI with color-coded displays, ASCII graphs, and hot-reload configuration support.

## ✨ Features

- 🖥️ **Real-time Monitoring**: CPU, memory, disk, and network metrics
- 🎨 **Beautiful Terminal UI**: ANSI colors, progress bars, and ASCII graphs
- ⚡ **Hot-Reload Configuration**: Update thresholds without restarting
- 🔔 **Smart Alerts**: Configurable warning and critical thresholds
- 📊 **Historical Data**: Track trends with in-memory history
- 📝 **Alert Logging**: Log alerts to file for later analysis
- 🔧 **Multiple Profiles**: Pre-configured profiles for different scenarios
- 🌍 **Cross-Platform**: Windows, Linux, and macOS support

## 🚀 Quick Start

### Prerequisites

- CMake 3.20 or higher
- C++20 compatible compiler (GCC 10+, Clang 11+, MSVC 2019+)
- [Typiconf library](https://github.com/itaym-intel/typiconf) (automatically fetched)

### Building

```bash
# Clone the repository
cd sysmon

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
cmake --build . --config Release

# Run
./sysmon
```

### Windows (PowerShell)

```powershell
mkdir build; cd build
cmake ..
cmake --build . --config Release
.\Release\sysmon.exe
```

## 📖 Usage

### Basic Usage

```bash
# Use default configuration
./sysmon

# Use custom configuration
./sysmon config/profiles/server.yaml

# Show help
./sysmon --help
```

### Configuration

SysMon uses YAML configuration files. Here's a minimal example:

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

SysMon comes with three pre-configured profiles:

1. **Default** (`config/default_config.yaml`): Balanced settings for general use
2. **Developer** (`config/profiles/developer.yaml`): Relaxed thresholds, detailed display
3. **Server** (`config/profiles/server.yaml`): Strict thresholds, minimal display
4. **Minimal** (`config/profiles/minimal.yaml`): Minimal monitoring, no graphs

```bash
# Use developer profile
./sysmon config/profiles/developer.yaml

# Use server profile
./sysmon config/profiles/server.yaml
```

### Hot-Reload

Edit your configuration file while SysMon is running, and it will automatically reload the new settings:

```bash
# 1. Start SysMon
./sysmon config/default_config.yaml

# 2. In another terminal, edit the config
nano config/default_config.yaml

# 3. SysMon detects changes and reloads automatically
```

## 🎯 Configuration Options

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

## 📊 Display Example

```
╔════════════════════════════════════════════════════════╗
║              SYSMON - System Monitor v1.0              ║
╚════════════════════════════════════════════════════════╝

[CPU]  ████████████████████░░░░  78.3%  ⚠ WARNING
  Core 0: ██████████████░░░░░░  72%
  Core 1: ████████████████████  95%  🔴
  Core 2: ████████████░░░░░░░░  65%
  Core 3: ██████████████████░░  85%  ⚠

[Memory]  ██████████████░░░░░░  68.2% (10.9 GB / 16.0 GB)  ✓ OK

[Disk]
  System (C:\)    ████████████████░░░░  79% (158 GB / 200 GB)  ⚠
  Root (/)        ██████████░░░░░░░░░░  52% (521 GB / 1 TB)   ✓

[Alerts - Last 5]
  🔴 14:32:15 | CPU Core 1 usage: 95% (critical)
  ⚠  14:32:10 | Root disk usage: 79% (warning)

[History - Last 30s]
CPU:  ▁▂▃▄▅▆▇███████▇▆▅▄▃▂▁
MEM:  ▅▅▅▅▅▆▆▆▆▆▆▅▅▅▅▅▅▅▅

Press Ctrl+C to quit, Config hot-reload enabled
```

## 🏗️ Architecture

SysMon follows a modular architecture:

```
┌─────────────────────────────────────────┐
│           User Interface Layer          │
│  (Display, Terminal Rendering)          │
└────────────────┬────────────────────────┘
                 │
┌────────────────▼────────────────────────┐
│         Application Layer               │
│  (SystemMonitor, AlertEngine)           │
└────────────┬───────────────┬────────────┘
             │               │
┌────────────▼──────┐ ┌─────▼────────────┐
│  ConfigManager    │ │ MetricsCollector │
│  (Typiconf)       │ │ (Platform-specific)│
└───────────────────┘ └──────────────────┘
```

### Components

1. **ConfigManager**: Loads and validates YAML configuration using Typiconf
2. **MetricsCollector**: Platform-specific system metrics collection
3. **AlertEngine**: Threshold checking and alert generation
4. **Display**: Terminal UI rendering with ANSI colors
5. **SystemMonitor**: Main orchestration and monitoring loop

## 🧪 Testing

```bash
cd build
cmake .. -DSYSMON_BUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

## 🛠️ Development

### Project Structure

```
sysmon/
├── CMakeLists.txt           # Build configuration
├── config/                  # Configuration files
│   ├── default_config.yaml
│   └── profiles/
├── include/sysmon/          # Public headers
│   ├── config_manager.hpp
│   ├── metrics_collector.hpp
│   ├── alert_engine.hpp
│   ├── display.hpp
│   └── system_monitor.hpp
├── src/                     # Implementation
│   ├── main.cpp
│   ├── config_manager.cpp
│   ├── metrics_collector.cpp
│   ├── alert_engine.cpp
│   ├── display.cpp
│   ├── system_monitor.cpp
│   └── platform/            # Platform-specific code
│       ├── metrics_windows.cpp
│       ├── metrics_linux.cpp
│       └── metrics_macos.cpp
└── tests/                   # Unit tests
    ├── test_config_manager.cpp
    └── test_alert_engine.cpp
```

### Adding New Metrics

1. Update `MetricsCollector` interface in `metrics_collector.hpp`
2. Implement platform-specific collection in `platform/` files
3. Add threshold configuration to `config_manager.hpp`
4. Update display rendering in `display.cpp`
5. Add alert checking in `alert_engine.cpp`

## 📦 Dependencies

- **Typiconf**: Type-safe configuration library (auto-fetched)
- **Catch2**: Testing framework (auto-fetched for tests)
- **Platform APIs**:
  - Windows: PDH (Performance Data Helper), Windows API
  - Linux: /proc filesystem, statvfs
  - macOS: Mach kernel APIs, sysctl

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 🙏 Acknowledgments

- Uses [Typiconf](https://github.com/itaym-intel/typiconf) for configuration management
- Inspired by htop, btop, and other system monitoring tools

## 📞 Support

For issues, questions, or suggestions, please open an issue on the GitHub repository.

---

**Made with ❤️ for system monitoring enthusiasts**
