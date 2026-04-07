#pragma once
#include <mqtt/async_client.h>
#include <json/json.h>
#include <functional>
#include <string>
#include <vector>
#include <mutex>

struct DeviceEntry;  // forward decl

class MqttBridge : public mqtt::callback {
public:
    using CommandCallback = std::function<void(const std::string& device_name, const Json::Value& cmd)>;

    MqttBridge(const std::string& broker, int port,
               const std::string& username, const std::string& password,
               const std::string& client_id, const std::string& topic_prefix);
    ~MqttBridge();

    bool connect();
    void disconnect();
    bool isConnected() const;

    // Publish device state
    void publishState(const std::string& device_name, const std::string& type,
                      const Json::Value& state);
    void publishPower(const std::string& device_name, const std::string& type,
                      bool on);
    void publishBridgeStatus(const std::string& status);

    // HA auto-discovery
    void publishDiscovery(const std::vector<DeviceEntry>& devices);

    // Subscribe to command topics and set callback
    void subscribeCommands(const std::vector<DeviceEntry>& devices,
                           CommandCallback callback);

    // mqtt::callback overrides
    void connected(const std::string& cause) override;
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;

private:
    std::string topicPrefix(const std::string& type, const std::string& name);
    Json::Value makeLightDiscovery(const DeviceEntry& dev);
    Json::Value makeSwitchDiscovery(const DeviceEntry& dev);

    std::unique_ptr<mqtt::async_client> client_;
    std::string topic_prefix_;
    std::string username_;
    std::string password_;
    CommandCallback command_cb_;
    std::vector<DeviceEntry> subscribed_devices_;
    mutable std::mutex mutex_;
};
