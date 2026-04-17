#include "web/ApiController.h"
#include "ConfigManager.h"
#include "bridge/BridgeManager.h"
#include "LogBuffer.h"
#include <nanotuya/TuyaCloud.h>

// Static member initialization
std::shared_ptr<BridgeManager> ApiController::bridge_;
std::shared_ptr<ConfigManager> ApiController::config_;
std::chrono::steady_clock::time_point ApiController::start_time_ =
    std::chrono::steady_clock::now();

void ApiController::setBridgeManager(std::shared_ptr<BridgeManager> bridge) {
    bridge_ = std::move(bridge);
}

void ApiController::setConfigManager(std::shared_ptr<ConfigManager> config) {
    config_ = std::move(config);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

drogon::HttpResponsePtr ApiController::jsonResponse(const Json::Value& json, int status) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(static_cast<drogon::HttpStatusCode>(status));
    return resp;
}

drogon::HttpResponsePtr ApiController::errorResponse(const std::string& msg, int status) {
    Json::Value err;
    err["error"] = msg;
    return jsonResponse(err, status);
}

// ---------------------------------------------------------------------------
// GET /health
// ---------------------------------------------------------------------------

void ApiController::health(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();

    Json::Value result;
    result["status"] = "ok";
    result["uptime"] = static_cast<Json::Int64>(uptime);
    result["version"] = "1.0.1";

    if (bridge_) {
        auto status = bridge_->getStatus();
        if (status.isMember("total_devices")) {
            result["total_devices"] = status["total_devices"];
        }
        if (status.isMember("online_devices")) {
            result["online_devices"] = status["online_devices"];
        }
    }

    callback(jsonResponse(result));
}

// ---------------------------------------------------------------------------
// GET /api/status
// ---------------------------------------------------------------------------

void ApiController::getStatus(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    if (!bridge_) {
        callback(errorResponse("bridge not initialized", 500));
        return;
    }
    callback(jsonResponse(bridge_->getStatus()));
}

// ---------------------------------------------------------------------------
// GET /api/devices
// ---------------------------------------------------------------------------

void ApiController::getDevices(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    if (!config_) {
        callback(errorResponse("config not initialized", 500));
        return;
    }

    auto devices = config_->getDevices();
    Json::Value arr(Json::arrayValue);

    for (const auto& dev : devices) {
        Json::Value obj;
        obj["name"] = dev.name;
        obj["friendly_name"] = dev.friendly_name;
        obj["ip"] = dev.tuya_config.ip;
        obj["id"] = dev.tuya_config.id;
        obj["local_key"] = "****";  // masked
        obj["type"] = dev.type;
        obj["enabled"] = dev.enabled;

        // Version to string
        switch (dev.tuya_config.version) {
            case nanotuya::TuyaVersion::V31: obj["version"] = "3.1"; break;
            case nanotuya::TuyaVersion::V33: obj["version"] = "3.3"; break;
            case nanotuya::TuyaVersion::V34: obj["version"] = "3.4"; break;
        }

        arr.append(obj);
    }

    callback(jsonResponse(arr));
}

// ---------------------------------------------------------------------------
// POST /api/devices
// ---------------------------------------------------------------------------

void ApiController::addDevice(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    if (!config_ || !bridge_) {
        callback(errorResponse("not initialized", 500));
        return;
    }

    auto body = req->getJsonObject();
    if (!body) {
        callback(errorResponse("invalid JSON body"));
        return;
    }

    DeviceEntry entry;
    entry.name = (*body).get("name", "").asString();
    entry.friendly_name = (*body).get("friendly_name", entry.name).asString();
    entry.tuya_config.id = (*body).get("id", "").asString();
    entry.tuya_config.ip = (*body).get("ip", "").asString();
    entry.tuya_config.local_key = (*body).get("local_key", "").asString();
    entry.type = (*body).get("type", "switch").asString();
    entry.enabled = (*body).get("enabled", true).asBool();
    entry.tuya_config.port = (*body).get("port", 6668).asInt();
    entry.tuya_config.timeout_ms = (*body).get("timeout_ms", 5000).asInt();

    // Parse version string
    std::string ver = (*body).get("version", "3.3").asString();
    if (ver == "3.1") entry.tuya_config.version = nanotuya::TuyaVersion::V31;
    else if (ver == "3.4") entry.tuya_config.version = nanotuya::TuyaVersion::V34;
    else entry.tuya_config.version = nanotuya::TuyaVersion::V33;

    if (entry.name.empty() || entry.tuya_config.id.empty()) {
        callback(errorResponse("name and id are required"));
        return;
    }

    if (!config_->addDevice(entry)) {
        callback(errorResponse("device with that name already exists", 409));
        return;
    }

    bridge_->reloadDevices();
    LogBuffer::instance().info("bridge", "Added device: " + entry.name);

    Json::Value result;
    result["status"] = "created";
    result["name"] = entry.name;
    callback(jsonResponse(result, 201));
}

// ---------------------------------------------------------------------------
// PUT /api/devices/{name}
// ---------------------------------------------------------------------------

void ApiController::updateDevice(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                 const std::string& name) {
    if (!config_ || !bridge_) {
        callback(errorResponse("not initialized", 500));
        return;
    }

    auto body = req->getJsonObject();
    if (!body) {
        callback(errorResponse("invalid JSON body"));
        return;
    }

    DeviceEntry entry;
    entry.name = (*body).get("name", name).asString();
    entry.friendly_name = (*body).get("friendly_name", "").asString();
    entry.tuya_config.id = (*body).get("id", "").asString();
    entry.tuya_config.ip = (*body).get("ip", "").asString();
    entry.tuya_config.local_key = (*body).get("local_key", "").asString();
    entry.type = (*body).get("type", "switch").asString();
    entry.enabled = (*body).get("enabled", true).asBool();
    entry.tuya_config.port = (*body).get("port", 6668).asInt();
    entry.tuya_config.timeout_ms = (*body).get("timeout_ms", 5000).asInt();

    std::string ver = (*body).get("version", "3.3").asString();
    if (ver == "3.1") entry.tuya_config.version = nanotuya::TuyaVersion::V31;
    else if (ver == "3.4") entry.tuya_config.version = nanotuya::TuyaVersion::V34;
    else entry.tuya_config.version = nanotuya::TuyaVersion::V33;

    if (!config_->updateDevice(name, entry)) {
        callback(errorResponse("device not found: " + name, 404));
        return;
    }

    bridge_->reloadDevices();
    LogBuffer::instance().info("bridge", "Updated device: " + name);

    Json::Value result;
    result["status"] = "updated";
    result["name"] = name;
    callback(jsonResponse(result));
}

// ---------------------------------------------------------------------------
// DELETE /api/devices/{name}
// ---------------------------------------------------------------------------

void ApiController::deleteDevice(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                 const std::string& name) {
    if (!config_ || !bridge_) {
        callback(errorResponse("not initialized", 500));
        return;
    }

    if (!config_->removeDevice(name)) {
        callback(errorResponse("device not found: " + name, 404));
        return;
    }

    bridge_->reloadDevices();
    LogBuffer::instance().info("bridge", "Deleted device: " + name);

    Json::Value result;
    result["status"] = "deleted";
    result["name"] = name;
    callback(jsonResponse(result));
}

// ---------------------------------------------------------------------------
// GET /api/devices/{name}/state
// ---------------------------------------------------------------------------

void ApiController::getDeviceState(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                   const std::string& name) {
    if (!bridge_) {
        callback(errorResponse("bridge not initialized", 500));
        return;
    }

    auto state = bridge_->getDeviceStatus(name);
    if (state.isNull()) {
        callback(errorResponse("device not found: " + name, 404));
        return;
    }

    callback(jsonResponse(state));
}

// ---------------------------------------------------------------------------
// POST /api/devices/{name}/command
// ---------------------------------------------------------------------------

void ApiController::sendCommand(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                const std::string& name) {
    if (!bridge_) {
        callback(errorResponse("bridge not initialized", 500));
        return;
    }

    auto body = req->getJsonObject();
    if (!body) {
        callback(errorResponse("invalid JSON body"));
        return;
    }

    if (!bridge_->sendCommand(name, *body)) {
        callback(errorResponse("device not found: " + name, 404));
        return;
    }

    LogBuffer::instance().info(name, "Command queued: " + body->toStyledString());

    Json::Value result;
    result["status"] = "queued";
    result["device"] = name;
    callback(jsonResponse(result));
}

// ---------------------------------------------------------------------------
// GET /api/logs
// ---------------------------------------------------------------------------

void ApiController::getLogs(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    int count = 200;
    auto count_param = req->getParameter("count");
    if (!count_param.empty()) {
        try {
            count = std::stoi(count_param);
        } catch (...) {
            // keep default
        }
    }

    callback(jsonResponse(LogBuffer::instance().getRecent(count)));
}

// ---------------------------------------------------------------------------
// GET /api/settings
// ---------------------------------------------------------------------------

void ApiController::getSettings(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    if (!config_) {
        callback(errorResponse("config not initialized", 500));
        return;
    }

    const auto& app = config_->appConfig();

    Json::Value result;
    result["server_port"] = app.server_port;
    result["server_threads"] = app.server_threads;
    result["mqtt_broker"] = app.mqtt_broker;
    result["mqtt_port"] = app.mqtt_port;
    result["mqtt_username"] = app.mqtt_username;
    result["mqtt_password"] = "********";  // masked
    result["mqtt_client_id"] = app.mqtt_client_id;
    result["mqtt_topic_prefix"] = app.mqtt_topic_prefix;
    result["mode"] = app.mode;
    result["poll_interval"] = app.poll_interval;
    result["heartbeat_interval"] = app.heartbeat_interval;
    result["socket_timeout"] = app.socket_timeout;
    result["min_backoff"] = app.min_backoff;
    result["max_backoff"] = app.max_backoff;
    result["cmd_max_retries"] = app.cmd_max_retries;
    result["cmd_retry_delay"] = app.cmd_retry_delay;
    result["cmd_timeout"] = app.cmd_timeout;
    result["devices_file"] = app.devices_file;

    callback(jsonResponse(result));
}

// ---------------------------------------------------------------------------
// PUT /api/settings
// ---------------------------------------------------------------------------

void ApiController::updateSettings(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Settings are read-only from YAML config at startup.
    // This endpoint is a placeholder for future runtime config updates.
    callback(errorResponse("settings are read-only (update hms-tuya.yaml and restart)", 501));
}

// ---------------------------------------------------------------------------
// POST /api/cloud/discover
// ---------------------------------------------------------------------------

void ApiController::cloudDiscover(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    // Cloud credentials can come from request body or config
    std::string api_key, api_secret, region;

    auto body = req->getJsonObject();
    if (body) {
        api_key = (*body).get("api_key", "").asString();
        api_secret = (*body).get("api_secret", "").asString();
        region = (*body).get("region", "us").asString();
    }

    if (api_key.empty() || api_secret.empty()) {
        callback(errorResponse("api_key and api_secret are required"));
        return;
    }

    try {
        nanotuya::TuyaCloud cloud(api_key, api_secret, region);
        auto devices = cloud.discoverDevices();

        if (devices.isNull()) {
            callback(errorResponse("cloud discovery failed: " + cloud.lastError(), 500));
            return;
        }

        Json::Value result;
        result["devices"] = devices;
        result["count"] = devices.size();
        callback(jsonResponse(result));

    } catch (const std::exception& e) {
        callback(errorResponse(std::string("cloud discovery error: ") + e.what(), 500));
    }
}

// ---------------------------------------------------------------------------
// POST /api/cloud/import
// ---------------------------------------------------------------------------

void ApiController::cloudImport(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    if (!config_ || !bridge_) {
        callback(errorResponse("not initialized", 500));
        return;
    }

    auto body = req->getJsonObject();
    if (!body || !(*body).isMember("devices") || !(*body)["devices"].isArray()) {
        callback(errorResponse("expected {\"devices\": [...]}"));
        return;
    }

    const auto& devices = (*body)["devices"];
    int imported = 0;

    for (const auto& dev : devices) {
        DeviceEntry entry;
        entry.tuya_config.id = dev.get("id", "").asString();
        entry.name = dev.get("name", "").asString();
        entry.friendly_name = dev.get("friendly_name", entry.name).asString();
        entry.tuya_config.local_key = dev.get("key", "").asString();
        entry.tuya_config.ip = dev.get("ip", "").asString();
        entry.type = dev.get("type", "switch").asString();
        entry.enabled = dev.get("enabled", true).asBool();

        std::string ver = dev.get("version", "3.3").asString();
        if (ver == "3.1") entry.tuya_config.version = nanotuya::TuyaVersion::V31;
        else if (ver == "3.4") entry.tuya_config.version = nanotuya::TuyaVersion::V34;
        else entry.tuya_config.version = nanotuya::TuyaVersion::V33;

        if (entry.name.empty() || entry.tuya_config.id.empty()) {
            continue;  // skip incomplete entries
        }

        if (config_->addDevice(entry)) {
            ++imported;
            LogBuffer::instance().info("bridge", "Imported device: " + entry.name);
        }
    }

    if (imported > 0) {
        bridge_->reloadDevices();
    }

    Json::Value result;
    result["imported"] = imported;
    callback(jsonResponse(result));
}
