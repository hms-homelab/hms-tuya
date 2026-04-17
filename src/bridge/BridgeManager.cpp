#include "bridge/BridgeManager.h"
#include <iostream>
#include <chrono>
#include <ctime>

namespace {

std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

BridgeManager::BridgeManager(std::shared_ptr<MqttBridge> mqtt,
                             std::shared_ptr<ConfigManager> config)
    : mqtt_(std::move(mqtt))
    , config_(std::move(config))
{
}

// ---------------------------------------------------------------------------
// start()  -- create workers for each enabled device, publish discovery
// ---------------------------------------------------------------------------

void BridgeManager::start() {
    std::lock_guard<std::mutex> lk(mutex_);

    auto devices = config_->getDevices();
    const auto& app = config_->appConfig();

    std::cout << timestamp() << " [bridge] Starting with "
              << devices.size() << " devices" << std::endl;

    // Publish HA MQTT discovery for all devices
    std::cout << timestamp() << " [bridge] Publishing discovery..." << std::endl;
    mqtt_->publishDiscovery(devices);
    std::cout << timestamp() << " [bridge] Discovery published" << std::endl;

    // Subscribe to command topics
    std::cout << timestamp() << " [bridge] Subscribing commands..." << std::endl;
    mqtt_->subscribeCommands(devices,
        [this](const std::string& name, const Json::Value& cmd) {
            onMqttCommand(name, cmd);
        });
    std::cout << timestamp() << " [bridge] Commands subscribed" << std::endl;

    // Create a worker for each enabled device
    for (const auto& dev : devices) {
        if (!dev.enabled) {
            std::cout << timestamp() << " [bridge] Skipping disabled device: "
                      << dev.name << std::endl;
            continue;
        }

        device_types_[dev.name] = (dev.type == "bulb") ? "light" : "switch";

        auto cb = [this](const std::string& name, const std::string& type,
                         const Json::Value& state, bool power_on) {
            publishCallback(name, type, state, power_on);
        };

        bool persistent = (app.mode == "persistent");
        auto worker = std::make_unique<DeviceWorker>(
            dev, cb, persistent,
            app.poll_interval, app.heartbeat_interval,
            app.min_backoff, app.max_backoff,
            app.cmd_max_retries, app.cmd_retry_delay, app.cmd_timeout);

        worker->start();
        workers_[dev.name] = std::move(worker);
    }

    // Announce bridge online
    mqtt_->publishBridgeStatus("online");

    std::cout << timestamp() << " [bridge] Started " << workers_.size()
              << " device workers" << std::endl;
}

// ---------------------------------------------------------------------------
// stop()  -- stop all workers, announce offline
// ---------------------------------------------------------------------------

void BridgeManager::stop() {
    std::lock_guard<std::mutex> lk(mutex_);

    std::cout << timestamp() << " [bridge] Stopping all workers" << std::endl;

    for (auto& [name, worker] : workers_) {
        worker->stop();
    }
    workers_.clear();
    device_types_.clear();

    mqtt_->publishBridgeStatus("offline");

    std::cout << timestamp() << " [bridge] All workers stopped" << std::endl;
}

// ---------------------------------------------------------------------------
// onMqttCommand()  -- route MQTT command to correct worker
// ---------------------------------------------------------------------------

void BridgeManager::onMqttCommand(const std::string& device_name,
                                   const Json::Value& cmd) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = workers_.find(device_name);
    if (it != workers_.end()) {
        it->second->enqueueCommand(cmd);
    } else {
        std::cerr << timestamp() << " [bridge] Command for unknown device: "
                  << device_name << std::endl;
    }
}

// ---------------------------------------------------------------------------
// publishCallback()  -- called by workers to publish state via MQTT
// ---------------------------------------------------------------------------

void BridgeManager::publishCallback(const std::string& name,
                                     const std::string& type,
                                     const Json::Value& state,
                                     bool power_on) {
    mqtt_->publishState(name, type, state);
    mqtt_->publishPower(name, type, power_on);
}

// ---------------------------------------------------------------------------
// getStatus()  -- JSON status for web API
// ---------------------------------------------------------------------------

Json::Value BridgeManager::getStatus() const {
    std::lock_guard<std::mutex> lk(mutex_);

    Json::Value result;
    result["status"] = "running";
    result["device_count"] = static_cast<int>(workers_.size());

    Json::Value devices(Json::arrayValue);
    for (const auto& [name, worker] : workers_) {
        Json::Value dev;
        dev["name"] = name;
        dev["type"] = device_types_.count(name) ? device_types_.at(name) : "unknown";
        dev["running"] = worker->isRunning();
        dev["last_state"] = worker->lastState();
        devices.append(dev);
    }
    result["devices"] = devices;
    return result;
}

// ---------------------------------------------------------------------------
// getDeviceStatus()  -- single device status for web API
// ---------------------------------------------------------------------------

Json::Value BridgeManager::getDeviceStatus(const std::string& name) const {
    std::lock_guard<std::mutex> lk(mutex_);

    auto it = workers_.find(name);
    if (it == workers_.end()) {
        Json::Value err;
        err["error"] = "Device not found: " + name;
        return err;
    }

    Json::Value dev;
    dev["name"] = name;
    dev["type"] = device_types_.count(name) ? device_types_.at(name) : "unknown";
    dev["running"] = it->second->isRunning();
    dev["last_state"] = it->second->lastState();
    return dev;
}

// ---------------------------------------------------------------------------
// sendCommand()  -- enqueue command for a device (web API entry point)
// ---------------------------------------------------------------------------

bool BridgeManager::sendCommand(const std::string& name, const Json::Value& cmd) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = workers_.find(name);
    if (it == workers_.end()) {
        return false;
    }
    it->second->enqueueCommand(cmd);
    return true;
}

// ---------------------------------------------------------------------------
// reloadDevices()  -- hot-reload: stop all workers, re-read config, restart
// ---------------------------------------------------------------------------

void BridgeManager::reloadDevices() {
    std::cout << timestamp() << " [bridge] Reloading devices" << std::endl;

    // Stop existing workers (releases lock internally via stop())
    {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto& [name, worker] : workers_) {
            worker->stop();
        }
        workers_.clear();
        device_types_.clear();
    }

    // Reload config from disk
    // ConfigManager::load re-reads devices.json via loadDevices()
    // We call start() which re-reads getDevices() from the config manager
    start();

    std::cout << timestamp() << " [bridge] Reload complete" << std::endl;
}
