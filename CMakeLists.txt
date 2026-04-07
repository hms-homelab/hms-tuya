cmake_minimum_required(VERSION 3.16)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HMS_TUYA_VERSION)
string(STRIP "${HMS_TUYA_VERSION}" HMS_TUYA_VERSION)

project(hms-tuya VERSION ${HMS_TUYA_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -pthread")
endif()

# ── nanotuya: local path first, FetchContent fallback ──
set(NANOTUYA_LOCAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../nanotuya")
if(EXISTS "${NANOTUYA_LOCAL_PATH}/CMakeLists.txt")
    message(STATUS "Using local nanotuya from ${NANOTUYA_LOCAL_PATH}")
    add_subdirectory(${NANOTUYA_LOCAL_PATH} nanotuya)
else()
    message(STATUS "Fetching nanotuya from GitHub")
    include(FetchContent)
    FetchContent_Declare(nanotuya
        GIT_REPOSITORY https://github.com/hms-homelab/nanotuya.git
        GIT_TAG v1.0.0
    )
    FetchContent_MakeAvailable(nanotuya)
endif()

# ── Dependencies ──
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
find_package(yaml-cpp REQUIRED)

# Paho MQTT
find_library(PAHO_MQTTPP3 paho-mqttpp3 REQUIRED)
find_library(PAHO_MQTT3AS paho-mqtt3as REQUIRED)

# ── Main executable ──
add_executable(hms_tuya
    src/main.cpp
    src/ConfigManager.cpp
    src/bridge/DeviceWorker.cpp
    src/bridge/MqttBridge.cpp
    src/bridge/BridgeManager.cpp
)

target_include_directories(hms_tuya PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${JSONCPP_INCLUDE_DIRS}
)

target_link_libraries(hms_tuya
    nanotuya
    yaml-cpp
    ${PAHO_MQTTPP3}
    ${PAHO_MQTT3AS}
    ${JSONCPP_LIBRARIES}
    OpenSSL::Crypto
)

# ── Install ──
install(TARGETS hms_tuya DESTINATION bin)

# ── Tests ──
option(HMS_TUYA_BUILD_TESTS "Build hms-tuya tests" OFF)
if(HMS_TUYA_BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    add_executable(hms_tuya_tests
        tests/test_config_manager.cpp
        src/ConfigManager.cpp
    )
    target_include_directories(hms_tuya_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${JSONCPP_INCLUDE_DIRS}
    )
    target_link_libraries(hms_tuya_tests
        nanotuya
        yaml-cpp
        ${JSONCPP_LIBRARIES}
        GTest::gtest GTest::gtest_main
    )
    add_test(NAME hms_tuya_unit_tests COMMAND hms_tuya_tests)
endif()
