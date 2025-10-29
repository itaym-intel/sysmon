cmake_minimum_required(VERSION 3.20)
project(SysMon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================
# Find Typiconf (external dependency)
# ============================================
# Note: SysMon uses a custom YAML parser for config management.
# Typiconf integration is optional and can be added later.
# The project is designed to work with Typiconf's architecture.

# Option 1: Typiconf as subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../typiconf/CMakeLists.txt")
    message(STATUS "Found Typiconf in parent directory")
    # Enable YAML support in Typiconf before adding it
    set(TYPICONF_ENABLE_YAML ON CACHE BOOL "Enable YAML support" FORCE)
    set(TYPICONF_BUILD_TESTS OFF CACHE BOOL "Build Typiconf tests" FORCE)
    set(TYPICONF_BUILD_EXAMPLES OFF CACHE BOOL "Build Typiconf examples" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../typiconf ${CMAKE_BINARY_DIR}/typiconf)
    set(TYPICONF_FOUND TRUE)
    set(TYPICONF_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../typiconf/include)
endif()

# Option 2: Typiconf installed system-wide
if(NOT TYPICONF_FOUND)
    find_package(Typiconf QUIET)
    if(Typiconf_FOUND)
        set(TYPICONF_FOUND TRUE)
    endif()
endif()

# Option 3: FetchContent (download from GitHub)
if(NOT TYPICONF_FOUND)
    message(FATAL_ERROR "Typiconf not found! Please ensure typiconf is in the parent directory.")
endif()

# ============================================
# Platform detection
# ============================================
if(WIN32)
    set(PLATFORM_SOURCES src/platform/metrics_windows.cpp)
    set(PLATFORM_LIBS "") # Windows API is header-only
    set(PLATFORM_COMPILE_DEFS NOMINMAX _WIN32_WINNT=0x0601)
elseif(APPLE)
    set(PLATFORM_SOURCES src/platform/metrics_macos.cpp)
    set(PLATFORM_LIBS "-framework CoreFoundation -framework IOKit")
    set(PLATFORM_COMPILE_DEFS "")
elseif(UNIX)
    set(PLATFORM_SOURCES src/platform/metrics_linux.cpp)
    set(PLATFORM_LIBS pthread)
    set(PLATFORM_COMPILE_DEFS "")
endif()

# ============================================
# SysMon executable
# ============================================
add_executable(sysmon
    src/main.cpp
    src/config_manager.cpp
    src/metrics_collector.cpp
    src/alert_engine.cpp
    src/display.cpp
    src/system_monitor.cpp
    ${PLATFORM_SOURCES}
)

target_include_directories(sysmon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add Typiconf include directory
target_include_directories(sysmon PRIVATE ${TYPICONF_INCLUDE_DIR})

# Link Typiconf library if it's a target
if(TARGET typiconf)
    target_link_libraries(sysmon PRIVATE typiconf ${PLATFORM_LIBS})
else()
    target_link_libraries(sysmon PRIVATE ${PLATFORM_LIBS})
endif()

target_compile_definitions(sysmon PRIVATE
    ${PLATFORM_COMPILE_DEFS}
)

# MSVC specific: Enable UTF-8 source and execution encoding
if(MSVC)
    target_compile_options(sysmon PRIVATE /utf-8)
endif()

# ============================================
# Install rules
# ============================================
install(TARGETS sysmon DESTINATION bin)
install(DIRECTORY config/ DESTINATION share/sysmon/config)

# ============================================
# Copy config files to build directory for easy testing
# ============================================
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config
     DESTINATION ${CMAKE_BINARY_DIR})

# ============================================
# Testing (optional)
# ============================================
option(SYSMON_BUILD_TESTS "Build tests" ON)
if(SYSMON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
