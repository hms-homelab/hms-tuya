#pragma once
#include <nanotuya/TuyaDefs.h>
#include <string>
#include <vector>
#include <mutex>

struct AppConfig {
    // Server
    int server_port = 8899;
    int server_threads = 2;

    // MQTT
    std::string mqtt_broker = "localhost";
    int mqtt_port = 1883;
    std::string mqtt_username;
    std::string mqtt_password;
    std::string mqtt_client_id = "hms_tuya";
    std::string mqtt_topic_prefix = "tuya";

    // Tuya tuning
    std::string mode = "persistent";  // "persistent" or "burst"
    int poll_interval = 10;
    int socket_timeout = 5;
    int heartbeat_interval = 10;
    double min_backoff = 10.0;
    double max_backoff = 300.0;
    int cmd_max_retries = 5;
    int cmd_retry_delay = 3;
    int cmd_timeout = 60;

    // Tuya Cloud API (for device discovery)
    std::string tuya_api_key;
    std::string tuya_api_secret;
    std::string tuya_api_region = "us";

    // Paths
    std::string devices_file = "devices.json";
};

struct DeviceEntry {
    nanotuya::DeviceConfig tuya_config;  // id, ip, local_key, version, port, timeout
    std::string name;           // short name (e.g. "patio")
    std::string friendly_name;  // display name (e.g. "Patio")
    std::string type;           // "bulb" or "switch"
    std::string mac;            // MAC address (optional, for identification)
    bool enabled = true;
};

class ConfigManager {
public:
    bool load(const std::string& yaml_path);

    const AppConfig& appConfig() const { return app_config_; }
    std::vector<DeviceEntry> getDevices() const;

    // Device CRUD (writes to devices.json)
    bool addDevice(const DeviceEntry& dev);
    bool updateDevice(const std::string& name, const DeviceEntry& dev);
    bool removeDevice(const std::string& name);

private:
    void loadDevices();
    void saveDevices();
    nanotuya::TuyaVersion parseVersion(const std::string& v);

    AppConfig app_config_;
    std::string devices_path_;
    std::vector<DeviceEntry> devices_;
    mutable std::mutex mutex_;
};
