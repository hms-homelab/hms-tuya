#include "bridge/MqttBridge.h"
#include "ConfigManager.h"
#include <sstream>
#include <cstdio>

MqttBridge::MqttBridge(const std::string& broker, int port,
                       const std::string& username, const std::string& password,
                       const std::string& client_id, const std::string& topic_prefix)
    : topic_prefix_(topic_prefix)
    , username_(username)
    , password_(password)
{
    std::string uri = "tcp://" + broker + ":" + std::to_string(port);
    client_ = std::make_unique<mqtt::async_client>(uri, client_id);
    client_->set_callback(*this);
}

MqttBridge::~MqttBridge() {
    disconnect();
}

bool MqttBridge::connect() {
    try {
        auto conn_opts = mqtt::connect_options_builder()
            .clean_session(true)
            .keep_alive_interval(std::chrono::seconds(60))
            .automatic_reconnect(std::chrono::seconds(1), std::chrono::seconds(64))
            .will(mqtt::message("tuya_bridge/status", "offline", 1, true))
            .finalize();

        if (!username_.empty()) {
            conn_opts.set_user_name(username_);
            conn_opts.set_password(password_);
        }

        client_->connect(conn_opts)->wait();
        return true;

    } catch (const mqtt::exception& e) {
        fprintf(stderr, "MqttBridge::connect error: %s\n", e.what());
        return false;
    }
}

void MqttBridge::disconnect() {
    try {
        if (client_ && client_->is_connected()) {
            publishBridgeStatus("offline");
            client_->disconnect();
        }
    } catch (const mqtt::exception& e) {
        fprintf(stderr, "MqttBridge::disconnect error: %s\n", e.what());
    }
}

bool MqttBridge::isConnected() const {
    return client_ && client_->is_connected();
}

void MqttBridge::publishState(const std::string& device_name, const std::string& type,
                              const Json::Value& state) {
    if (!isConnected()) return;

    std::string topic = topicPrefix(type, device_name) + "/state";

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::string payload = Json::writeString(writer, state);

    try {
        client_->publish(topic, payload, 1, true);
    } catch (const mqtt::exception& e) {
        fprintf(stderr, "MqttBridge::publishState error: %s\n", e.what());
    }
}

void MqttBridge::publishPower(const std::string& device_name, const std::string& type,
                              bool on) {
    if (!isConnected()) return;

    std::string topic = topicPrefix(type, device_name) + "/power";
    std::string payload = on ? "on" : "off";

    try {
        client_->publish(topic, payload, 1, true);
    } catch (const mqtt::exception& e) {
        fprintf(stderr, "MqttBridge::publishPower error: %s\n", e.what());
    }
}

void MqttBridge::publishBridgeStatus(const std::string& status) {
    if (!isConnected()) return;

    std::string topic = "tuya_bridge/status";

    try {
        client_->publish(topic, status, 1, true);
    } catch (const mqtt::exception& e) {
        fprintf(stderr, "MqttBridge::publishBridgeStatus error: %s\n", e.what());
    }
}

void MqttBridge::publishDiscovery(const std::vector<DeviceEntry>& devices) {
    if (!isConnected()) return;

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";

    for (const auto& dev : devices) {
        if (!dev.enabled) continue;

        std::string ha_type;
        Json::Value config;

        if (dev.type == "bulb") {
            ha_type = "light";
            config = makeLightDiscovery(dev);
        } else {
            ha_type = "switch";
            config = makeSwitchDiscovery(dev);
        }

        std::string topic = "homeassistant/" + ha_type + "/tuya_" + dev.name + "/config";
        std::string payload = Json::writeString(writer, config);

        try {
            client_->publish(topic, payload, 1, true);
        } catch (const mqtt::exception& e) {
            fprintf(stderr, "MqttBridge::publishDiscovery error for %s: %s\n",
                    dev.name.c_str(), e.what());
        }
    }
}

void MqttBridge::subscribeCommands(const std::vector<DeviceEntry>& devices,
                                   CommandCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    command_cb_ = std::move(callback);
    subscribed_devices_ = devices;

    if (!isConnected()) return;

    for (const auto& dev : devices) {
        if (!dev.enabled) continue;

        std::string type = (dev.type == "bulb") ? "light" : "switch";
        std::string topic = topicPrefix(type, dev.name) + "/set";

        try {
            client_->subscribe(topic, 1);
        } catch (const mqtt::exception& e) {
            fprintf(stderr, "MqttBridge::subscribeCommands error for %s: %s\n",
                    dev.name.c_str(), e.what());
        }
    }
}

// --- mqtt::callback overrides ---

void MqttBridge::connected(const std::string& cause) {
    fprintf(stdout, "MqttBridge: connected (%s)\n",
            cause.empty() ? "initial" : cause.c_str());

    // Publish online status
    try {
        client_->publish("tuya_bridge/status", "online", 1, true);
    } catch (const mqtt::exception& e) {
        fprintf(stderr, "MqttBridge::connected publish error: %s\n", e.what());
    }

    // Re-subscribe to command topics
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& dev : subscribed_devices_) {
        if (!dev.enabled) continue;

        std::string type = (dev.type == "bulb") ? "light" : "switch";
        std::string topic = topicPrefix(type, dev.name) + "/set";

        try {
            client_->subscribe(topic, 1);
        } catch (const mqtt::exception& e) {
            fprintf(stderr, "MqttBridge::connected re-subscribe error for %s: %s\n",
                    dev.name.c_str(), e.what());
        }
    }
}

void MqttBridge::connection_lost(const std::string& cause) {
    fprintf(stderr, "MqttBridge: connection lost (%s)\n",
            cause.empty() ? "unknown" : cause.c_str());
}

void MqttBridge::message_arrived(mqtt::const_message_ptr msg) {
    std::string topic = msg->get_topic();
    std::string payload_str = msg->to_string();

    // Parse topic: {prefix}/{type}/{name}/set
    // Find the device name from the topic
    std::string set_suffix = "/set";
    if (topic.size() < set_suffix.size() ||
        topic.substr(topic.size() - set_suffix.size()) != set_suffix) {
        return;  // Not a command topic
    }

    // Strip prefix and /set
    std::string prefix_slash = topic_prefix_ + "/";
    if (topic.find(prefix_slash) != 0) {
        return;
    }

    // Remaining: "{type}/{name}/set" -> strip "/set"
    std::string remainder = topic.substr(prefix_slash.size(),
                                         topic.size() - prefix_slash.size() - set_suffix.size());

    // Split by '/' to get type and name
    auto slash_pos = remainder.find('/');
    if (slash_pos == std::string::npos) {
        return;
    }

    std::string device_name = remainder.substr(slash_pos + 1);

    // Parse payload
    Json::Value cmd;
    Json::CharReaderBuilder reader;
    std::string errs;
    std::istringstream stream(payload_str);

    if (!Json::parseFromStream(reader, stream, &cmd, &errs)) {
        // Not valid JSON -- wrap as simple state command
        cmd = Json::Value();
        cmd["state"] = payload_str;
    }

    // Invoke callback
    std::lock_guard<std::mutex> lock(mutex_);
    if (command_cb_) {
        command_cb_(device_name, cmd);
    }
}

// --- Private helpers ---

std::string MqttBridge::topicPrefix(const std::string& type, const std::string& name) {
    return topic_prefix_ + "/" + type + "/" + name;
}

Json::Value MqttBridge::makeLightDiscovery(const DeviceEntry& dev) {
    Json::Value config;

    config["name"] = Json::nullValue;
    config["unique_id"] = "tuya_" + dev.name;
    config["object_id"] = "tuya_" + dev.name;
    config["schema"] = "json";
    config["state_topic"] = topicPrefix("light", dev.name) + "/state";
    config["command_topic"] = topicPrefix("light", dev.name) + "/set";
    config["availability_topic"] = "tuya_bridge/status";
    config["brightness"] = true;
    config["brightness_scale"] = 100;

    Json::Value color_modes(Json::arrayValue);
    color_modes.append("brightness");
    config["supported_color_modes"] = color_modes;
    config["color_mode"] = true;

    Json::Value device;
    Json::Value identifiers(Json::arrayValue);
    identifiers.append("tuya_" + dev.name);
    device["identifiers"] = identifiers;
    device["name"] = "Tuya " + dev.friendly_name;
    device["manufacturer"] = "Tuya";
    device["model"] = "WiFi Bulb";
    device["via_device"] = "tuya_bridge";
    config["device"] = device;

    return config;
}

Json::Value MqttBridge::makeSwitchDiscovery(const DeviceEntry& dev) {
    Json::Value config;

    config["name"] = Json::nullValue;
    config["unique_id"] = "tuya_" + dev.name;
    config["object_id"] = "tuya_" + dev.name;
    config["state_topic"] = topicPrefix("switch", dev.name) + "/state";
    config["command_topic"] = topicPrefix("switch", dev.name) + "/set";
    config["availability_topic"] = "tuya_bridge/status";
    config["value_template"] = "{{ value_json.state | upper }}";
    config["payload_on"] = "{\"state\":\"on\"}";
    config["payload_off"] = "{\"state\":\"off\"}";
    config["state_on"] = "ON";
    config["state_off"] = "OFF";

    Json::Value device;
    Json::Value identifiers(Json::arrayValue);
    identifiers.append("tuya_" + dev.name);
    device["identifiers"] = identifiers;
    device["name"] = "Tuya " + dev.friendly_name;
    device["manufacturer"] = "Tuya";
    device["model"] = "WiFi Breaker";
    device["via_device"] = "tuya_bridge";
    config["device"] = device;

    return config;
}
