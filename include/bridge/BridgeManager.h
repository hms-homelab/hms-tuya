#pragma once
#include "bridge/DeviceWorker.h"
#include "bridge/MqttBridge.h"
#include "ConfigManager.h"
#include <map>
#include <memory>
#include <mutex>

class BridgeManager {
public:
    BridgeManager(std::shared_ptr<MqttBridge> mqtt,
                  std::shared_ptr<ConfigManager> config);

    void start();   // create workers, publish discovery
    void stop();    // stop all workers

    // For web API
    Json::Value getStatus() const;
    Json::Value getDeviceStatus(const std::string& name) const;
    bool sendCommand(const std::string& name, const Json::Value& cmd);
    void reloadDevices();  // hot-reload from devices.json

private:
    void onMqttCommand(const std::string& device_name, const Json::Value& cmd);
    void publishCallback(const std::string& name, const std::string& type,
                         const Json::Value& state, bool power_on);

    std::shared_ptr<MqttBridge> mqtt_;
    std::shared_ptr<ConfigManager> config_;
    std::map<std::string, std::unique_ptr<DeviceWorker>> workers_;
    std::map<std::string, std::string> device_types_;  // name -> "bulb"/"switch"
    mutable std::mutex mutex_;
};
