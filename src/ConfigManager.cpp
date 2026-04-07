#include "ConfigManager.h"
#include <yaml-cpp/yaml.h>
#include <json/json.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <cstdio>

namespace fs = std::filesystem;

bool ConfigManager::load(const std::string& yaml_path) {
    try {
        YAML::Node root = YAML::LoadFile(yaml_path);

        // Server section
        if (auto server = root["server"]) {
            if (server["port"])    app_config_.server_port    = server["port"].as<int>();
            if (server["threads"]) app_config_.server_threads = server["threads"].as<int>();
        }

        // MQTT section
        if (auto mqtt = root["mqtt"]) {
            if (mqtt["broker"])       app_config_.mqtt_broker       = mqtt["broker"].as<std::string>();
            if (mqtt["port"])         app_config_.mqtt_port         = mqtt["port"].as<int>();
            if (mqtt["username"])     app_config_.mqtt_username     = mqtt["username"].as<std::string>();
            if (mqtt["password"])     app_config_.mqtt_password     = mqtt["password"].as<std::string>();
            if (mqtt["client_id"])    app_config_.mqtt_client_id    = mqtt["client_id"].as<std::string>();
            if (mqtt["topic_prefix"]) app_config_.mqtt_topic_prefix = mqtt["topic_prefix"].as<std::string>();
        }

        // Tuya section
        if (auto tuya = root["tuya"]) {
            if (tuya["poll_interval"])   app_config_.poll_interval   = tuya["poll_interval"].as<int>();
            if (tuya["socket_timeout"])  app_config_.socket_timeout  = tuya["socket_timeout"].as<int>();
            if (tuya["min_backoff"])     app_config_.min_backoff     = tuya["min_backoff"].as<double>();
            if (tuya["max_backoff"])     app_config_.max_backoff     = tuya["max_backoff"].as<double>();
            if (tuya["cmd_max_retries"]) app_config_.cmd_max_retries = tuya["cmd_max_retries"].as<int>();
            if (tuya["cmd_retry_delay"]) app_config_.cmd_retry_delay = tuya["cmd_retry_delay"].as<int>();
            if (tuya["cmd_timeout"])     app_config_.cmd_timeout     = tuya["cmd_timeout"].as<int>();
        }

        // Devices file path
        if (auto devices = root["devices_file"]) {
            app_config_.devices_file = devices.as<std::string>();
        }

        // Resolve devices_file relative to YAML directory
        fs::path yaml_dir = fs::path(yaml_path).parent_path();
        fs::path dev_path = fs::path(app_config_.devices_file);
        if (dev_path.is_relative()) {
            devices_path_ = (yaml_dir / dev_path).string();
        } else {
            devices_path_ = dev_path.string();
        }

        loadDevices();
        return true;

    } catch (const std::exception& e) {
        fprintf(stderr, "ConfigManager::load error: %s\n", e.what());
        return false;
    }
}

std::vector<DeviceEntry> ConfigManager::getDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_;
}

bool ConfigManager::addDevice(const DeviceEntry& dev) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check for duplicate name
    for (const auto& d : devices_) {
        if (d.name == dev.name) {
            return false;
        }
    }

    devices_.push_back(dev);
    saveDevices();
    return true;
}

bool ConfigManager::updateDevice(const std::string& name, const DeviceEntry& dev) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& d : devices_) {
        if (d.name == name) {
            d = dev;
            saveDevices();
            return true;
        }
    }
    return false;
}

bool ConfigManager::removeDevice(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::remove_if(devices_.begin(), devices_.end(),
        [&name](const DeviceEntry& d) { return d.name == name; });

    if (it == devices_.end()) {
        return false;
    }

    devices_.erase(it, devices_.end());
    saveDevices();
    return true;
}

void ConfigManager::loadDevices() {
    devices_.clear();

    std::ifstream file(devices_path_);
    if (!file.is_open()) {
        // No devices file yet -- start with empty list
        return;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, file, &root, &errs)) {
        fprintf(stderr, "ConfigManager: failed to parse %s: %s\n",
                devices_path_.c_str(), errs.c_str());
        return;
    }

    if (!root.isArray()) {
        fprintf(stderr, "ConfigManager: devices.json root is not an array\n");
        return;
    }

    for (const auto& item : root) {
        DeviceEntry entry;
        entry.tuya_config.id        = item.get("id", "").asString();
        entry.tuya_config.ip        = item.get("ip", "").asString();
        entry.tuya_config.local_key = item.get("local_key", "").asString();
        entry.tuya_config.version   = parseVersion(item.get("version", "3.3").asString());
        entry.tuya_config.port      = item.get("port", 6668).asInt();
        entry.tuya_config.timeout_ms = item.get("timeout_ms", 5000).asInt();
        entry.name                  = item.get("name", "").asString();
        entry.friendly_name         = item.get("friendly_name", "").asString();
        entry.type                  = item.get("type", "switch").asString();
        entry.enabled               = item.get("enabled", true).asBool();

        if (!entry.name.empty() && !entry.tuya_config.id.empty()) {
            devices_.push_back(std::move(entry));
        }
    }
}

void ConfigManager::saveDevices() {
    Json::Value root(Json::arrayValue);

    for (const auto& dev : devices_) {
        Json::Value item;
        item["id"]            = dev.tuya_config.id;
        item["ip"]            = dev.tuya_config.ip;
        item["local_key"]     = dev.tuya_config.local_key;
        item["port"]          = dev.tuya_config.port;
        item["timeout_ms"]    = dev.tuya_config.timeout_ms;
        item["name"]          = dev.name;
        item["friendly_name"] = dev.friendly_name;
        item["type"]          = dev.type;
        item["enabled"]       = dev.enabled;

        // Convert version enum back to string
        switch (dev.tuya_config.version) {
            case nanotuya::TuyaVersion::V31: item["version"] = "3.1"; break;
            case nanotuya::TuyaVersion::V33: item["version"] = "3.3"; break;
            case nanotuya::TuyaVersion::V34: item["version"] = "3.4"; break;
        }

        root.append(item);
    }

    // Atomic write: write to tmp file, then rename
    std::string tmp_path = devices_path_ + ".tmp";

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "  ";
    std::string output = Json::writeString(writer, root);

    std::ofstream file(tmp_path);
    if (!file.is_open()) {
        fprintf(stderr, "ConfigManager: cannot write %s\n", tmp_path.c_str());
        return;
    }

    file << output;
    file.close();

    if (file.fail()) {
        fprintf(stderr, "ConfigManager: write failed for %s\n", tmp_path.c_str());
        std::remove(tmp_path.c_str());
        return;
    }

    if (std::rename(tmp_path.c_str(), devices_path_.c_str()) != 0) {
        fprintf(stderr, "ConfigManager: rename failed %s -> %s\n",
                tmp_path.c_str(), devices_path_.c_str());
        std::remove(tmp_path.c_str());
    }
}

nanotuya::TuyaVersion ConfigManager::parseVersion(const std::string& v) {
    if (v == "3.1") return nanotuya::TuyaVersion::V31;
    if (v == "3.4") return nanotuya::TuyaVersion::V34;
    return nanotuya::TuyaVersion::V33;  // default
}
