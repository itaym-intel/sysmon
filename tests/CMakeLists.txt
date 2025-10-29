cmake_minimum_required(VERSION 3.20)

# Fetch Catch2
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(Catch2)

# Test executable - needs to link against the actual implementation
add_executable(sysmon_tests
    test_config_manager.cpp
    test_alert_engine.cpp
    test_main.cpp
    ${CMAKE_SOURCE_DIR}/src/config_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/alert_engine.cpp
)

target_include_directories(sysmon_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Add Typiconf include directory if found
if(TYPICONF_INCLUDE_DIR)
    target_include_directories(sysmon_tests PRIVATE ${TYPICONF_INCLUDE_DIR})
endif()

target_link_libraries(sysmon_tests PRIVATE
    Catch2::Catch2WithMain
)

# Windows specific requirements
if(WIN32)
    target_link_libraries(sysmon_tests PRIVATE pdh)
    target_compile_definitions(sysmon_tests PRIVATE NOMINMAX _WIN32_WINNT=0x0601)
endif()

# Copy test configs if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_configs)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_configs
         DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

# Discover tests
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(sysmon_tests)
