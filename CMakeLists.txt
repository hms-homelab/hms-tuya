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
    set(BUILD_WITH_CLOUD ON CACHE BOOL "Enable TuyaCloud in nanotuya" FORCE)
    add_subdirectory(${NANOTUYA_LOCAL_PATH} nanotuya)
else()
    message(STATUS "Fetching nanotuya from GitHub")
    include(FetchContent)
    FetchContent_Declare(nanotuya
        GIT_REPOSITORY https://github.com/hms-homelab/nanotuya.git
        GIT_TAG main
    )
    set(BUILD_WITH_CLOUD ON CACHE BOOL "Enable TuyaCloud in nanotuya" FORCE)
    FetchContent_MakeAvailable(nanotuya)
endif()

# ── Dependencies ──
find_package(OpenSSL REQUIRED)

# Find jsoncpp: try pkg-config first (Linux), fall back to find_package (vcpkg/Windows)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(JSONCPP jsoncpp)
endif()
if(NOT JSONCPP_FOUND)
    find_package(jsoncpp CONFIG QUIET)
    if(NOT jsoncpp_FOUND)
        find_package(JsonCpp CONFIG REQUIRED)
    endif()
    set(JSONCPP_LIBRARIES JsonCpp::JsonCpp)
    set(JSONCPP_INCLUDE_DIRS "")
endif()

find_package(yaml-cpp REQUIRED)
find_package(CURL REQUIRED)

# Paho MQTT
if(MSVC)
    find_package(PahoMqttCpp CONFIG REQUIRED)
    set(PAHO_MQTT_LIBS PahoMqttCpp::paho-mqttpp3)
else()
    find_library(PAHO_MQTTPP3 paho-mqttpp3 REQUIRED)
    find_library(PAHO_MQTT3AS paho-mqtt3as REQUIRED)
    set(PAHO_MQTT_LIBS ${PAHO_MQTTPP3} ${PAHO_MQTT3AS})
endif()

# ── Optional Drogon web UI ──
option(BUILD_WITH_WEB "Build with Drogon web UI" ON)
if(BUILD_WITH_WEB)
    find_package(Drogon CONFIG REQUIRED)
    add_definitions(-DBUILD_WITH_WEB)
endif()

# ── Source files ──
set(HMS_TUYA_SOURCES
    src/main.cpp
    src/ConfigManager.cpp
    src/LogBuffer.cpp
    src/bridge/DeviceWorker.cpp
    src/bridge/MqttBridge.cpp
    src/bridge/BridgeManager.cpp
)

if(BUILD_WITH_WEB)
    list(APPEND HMS_TUYA_SOURCES src/web/ApiController.cpp)
endif()

# ── Main executable ──
add_executable(hms_tuya ${HMS_TUYA_SOURCES})

target_include_directories(hms_tuya PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${JSONCPP_INCLUDE_DIRS}
)

target_link_libraries(hms_tuya
    nanotuya
    yaml-cpp
    ${PAHO_MQTT_LIBS}
    ${JSONCPP_LIBRARIES}
    OpenSSL::Crypto
    CURL::libcurl
)

if(BUILD_WITH_WEB)
    target_link_libraries(hms_tuya Drogon::Drogon)
endif()

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
