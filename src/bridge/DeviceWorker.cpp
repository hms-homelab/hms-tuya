#include "bridge/DeviceWorker.h"
#include "ConfigManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
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

void logInfo(const std::string& device, const std::string& msg) {
    std::cout << timestamp() << " [" << device << "] " << msg << std::endl;
}

void logWarning(const std::string& device, const std::string& msg) {
    std::cout << timestamp() << " [" << device << "] WARNING: " << msg << std::endl;
}

void logError(const std::string& device, const std::string& msg) {
    std::cerr << timestamp() << " [" << device << "] ERROR: " << msg << std::endl;
}

std::string jsonToString(const Json::Value& v) {
    Json::StreamWriterBuilder wb;
    wb["indentation"] = "";
    return Json::writeString(wb, v);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

DeviceWorker::DeviceWorker(const DeviceEntry& config, PublishCallback publish_cb,
                           bool persistent, int poll_interval, int heartbeat_interval,
                           double min_backoff, double max_backoff,
                           int cmd_max_retries, int cmd_retry_delay, int cmd_timeout)
    : config_(config)
    , publish_cb_(std::move(publish_cb))
    , persistent_(persistent)
    , backoff_(min_backoff)
    , next_poll_(std::chrono::steady_clock::now())
    , next_heartbeat_(std::chrono::steady_clock::now())
    , poll_interval_(poll_interval)
    , heartbeat_interval_(heartbeat_interval)
    , min_backoff_(min_backoff)
    , max_backoff_(max_backoff)
    , cmd_max_retries_(cmd_max_retries)
    , cmd_retry_delay_(cmd_retry_delay)
    , cmd_timeout_(cmd_timeout)
{
}

DeviceWorker::~DeviceWorker() {
    stop();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void DeviceWorker::start() {
    if (running_.load()) return;
    running_ = true;
    thread_ = std::thread(&DeviceWorker::run, this);
}

void DeviceWorker::stop() {
    if (!running_.load()) return;
    running_ = false;
    queue_cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
    if (device_) {
        device_->disconnect();
        device_.reset();
    }
    logInfo(config_.name, "Worker stopped");
}

void DeviceWorker::enqueueCommand(const Json::Value& cmd) {
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        cmd_queue_.push({cmd, std::chrono::steady_clock::now()});
    }
    queue_cv_.notify_one();
}

Json::Value DeviceWorker::lastState() const {
    std::lock_guard<std::mutex> lk(state_mutex_);
    return last_state_;
}

std::string DeviceWorker::deviceName() const {
    return config_.name;
}

// ---------------------------------------------------------------------------
// Main loop -- dispatches to persistent or burst
// ---------------------------------------------------------------------------

void DeviceWorker::run() {
    std::string mode_str = persistent_ ? "persistent" : "burst";
    logInfo(config_.name, "Worker started (" + config_.type + " @ " +
            config_.tuya_config.ip + ", mode=" + mode_str + ")");

    if (persistent_) {
        runPersistent();
    } else {
        runBurst();
    }
}

// ---------------------------------------------------------------------------
// runBurst -- original behavior: fresh connection per poll/command
// ---------------------------------------------------------------------------

void DeviceWorker::runBurst() {
    while (running_.load()) {
        auto now = std::chrono::steady_clock::now();

        drainCommands();

        if (now >= next_poll_) {
            if (poll()) {
                if (backoff_ > min_backoff_) {
                    logInfo(config_.name,
                            "Recovered (was backing off " +
                            std::to_string(static_cast<int>(backoff_)) + "s)");
                }
                backoff_ = min_backoff_;
                next_poll_ = now + std::chrono::seconds(poll_interval_);
            } else {
                backoff_ = std::min(backoff_ * 2.0, max_backoff_);
                next_poll_ = now + std::chrono::duration_cast<
                    std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(backoff_));
                logWarning(config_.name,
                           "Poll failed (backoff " +
                           std::to_string(static_cast<int>(backoff_)) + "s)");
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// ---------------------------------------------------------------------------
// runPersistent -- keep connection alive, heartbeat, reconnect on failure
// ---------------------------------------------------------------------------

void DeviceWorker::runPersistent() {
    device_ = std::make_unique<nanotuya::TuyaDevice>(config_.tuya_config);

    while (running_.load()) {
        auto now = std::chrono::steady_clock::now();

        // Ensure connected
        if (!device_->isConnected()) {
            if (!device_->connect()) {
                backoff_ = std::min(backoff_ * 2.0, max_backoff_);
                logWarning(config_.name,
                           "Connect failed (backoff " +
                           std::to_string(static_cast<int>(backoff_)) +
                           "s): " + device_->lastError());
                std::this_thread::sleep_for(
                    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(backoff_)));
                continue;
            }
            if (backoff_ > min_backoff_) {
                logInfo(config_.name, "Reconnected (was backing off " +
                        std::to_string(static_cast<int>(backoff_)) + "s)");
            }
            backoff_ = min_backoff_;
            logInfo(config_.name, "Connected");
            next_heartbeat_ = now + std::chrono::seconds(heartbeat_interval_);
            next_poll_ = now;
        }

        // Commands always take priority
        drainCommandsPersistent();

        // Poll on schedule
        if (now >= next_poll_) {
            if (pollPersistent()) {
                backoff_ = min_backoff_;
                next_poll_ = now + std::chrono::seconds(poll_interval_);
            } else {
                logWarning(config_.name, "Poll failed, reconnecting");
                device_->disconnect();
                continue;
            }
        }

        // Heartbeat to keep connection alive
        if (now >= next_heartbeat_) {
            if (device_->heartbeat()) {
                next_heartbeat_ = now + std::chrono::seconds(heartbeat_interval_);
            } else {
                logWarning(config_.name, "Heartbeat failed, reconnecting");
                device_->disconnect();
                continue;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (device_) {
        device_->disconnect();
    }
}

// ---------------------------------------------------------------------------
// poll() -- burst mode: fresh connection each time
// ---------------------------------------------------------------------------

bool DeviceWorker::poll() {
    try {
        auto device = std::make_unique<nanotuya::TuyaDevice>(config_.tuya_config);
        auto data = device->queryStatus();

        if (!data.has_value()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            device = std::make_unique<nanotuya::TuyaDevice>(config_.tuya_config);
            data = device->queryStatus();
        }

        if (!data.has_value()) {
            return false;
        }

        publishState(data.value());
        return true;

    } catch (const std::exception& e) {
        logWarning(config_.name, std::string("Poll exception: ") + e.what());
        return false;
    }
}

// ---------------------------------------------------------------------------
// pollPersistent() -- reuse existing connection
// ---------------------------------------------------------------------------

bool DeviceWorker::pollPersistent() {
    try {
        if (!device_ || !device_->isConnected()) return false;

        auto data = device_->queryStatus();
        if (!data.has_value()) {
            return false;
        }

        publishState(data.value());
        return true;

    } catch (const std::exception& e) {
        logWarning(config_.name, std::string("Poll exception: ") + e.what());
        return false;
    }
}

// ---------------------------------------------------------------------------
// drainCommands() -- burst mode
// ---------------------------------------------------------------------------

void DeviceWorker::drainCommands() {
    std::vector<QueuedCommand> cmds;
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        while (!cmd_queue_.empty()) {
            cmds.push_back(std::move(cmd_queue_.front()));
            cmd_queue_.pop();
        }
    }

    auto now = std::chrono::steady_clock::now();

    for (auto& qc : cmds) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - qc.queued_at).count();
        if (age > cmd_timeout_) {
            logWarning(config_.name,
                       "Dropped stale command (>" + std::to_string(cmd_timeout_) +
                       "s): " + jsonToString(qc.cmd));
            continue;
        }

        publishOptimistic(qc.cmd);

        bool success = false;
        for (int attempt = 0; attempt < cmd_max_retries_; ++attempt) {
            if (executeCommand(qc.cmd)) {
                logInfo(config_.name,
                        "Command OK (attempt " + std::to_string(attempt + 1) +
                        "): " + jsonToString(qc.cmd));
                backoff_ = min_backoff_;
                next_poll_ = std::chrono::steady_clock::now() +
                             std::chrono::seconds(poll_interval_);
                success = true;
                break;
            }
            if (attempt < cmd_max_retries_ - 1) {
                logInfo(config_.name,
                        "Retry " + std::to_string(attempt + 1) + "/" +
                        std::to_string(cmd_max_retries_) + ": " +
                        jsonToString(qc.cmd));
                std::this_thread::sleep_for(
                    std::chrono::seconds(cmd_retry_delay_));
            }
        }

        if (!success) {
            logError(config_.name,
                     "Command FAILED after " + std::to_string(cmd_max_retries_) +
                     " attempts: " + jsonToString(qc.cmd));
        }
    }
}

// ---------------------------------------------------------------------------
// drainCommandsPersistent() -- reuse connection, reconnect on failure
// ---------------------------------------------------------------------------

void DeviceWorker::drainCommandsPersistent() {
    std::vector<QueuedCommand> cmds;
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        while (!cmd_queue_.empty()) {
            cmds.push_back(std::move(cmd_queue_.front()));
            cmd_queue_.pop();
        }
    }

    auto now = std::chrono::steady_clock::now();

    for (auto& qc : cmds) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - qc.queued_at).count();
        if (age > cmd_timeout_) {
            logWarning(config_.name,
                       "Dropped stale command (>" + std::to_string(cmd_timeout_) +
                       "s): " + jsonToString(qc.cmd));
            continue;
        }

        publishOptimistic(qc.cmd);

        bool success = false;
        for (int attempt = 0; attempt < cmd_max_retries_; ++attempt) {
            if (!device_ || !device_->isConnected()) {
                device_ = std::make_unique<nanotuya::TuyaDevice>(config_.tuya_config);
                if (!device_->connect()) {
                    logWarning(config_.name, "Reconnect failed during command");
                    break;
                }
            }

            if (executeCommandPersistent(qc.cmd)) {
                logInfo(config_.name,
                        "Command OK (attempt " + std::to_string(attempt + 1) +
                        "): " + jsonToString(qc.cmd));
                backoff_ = min_backoff_;
                next_poll_ = std::chrono::steady_clock::now() +
                             std::chrono::seconds(poll_interval_);
                success = true;
                break;
            }

            if (attempt < cmd_max_retries_ - 1) {
                logInfo(config_.name,
                        "Retry " + std::to_string(attempt + 1) + "/" +
                        std::to_string(cmd_max_retries_) + ": " +
                        jsonToString(qc.cmd));
                device_->disconnect();
                std::this_thread::sleep_for(
                    std::chrono::seconds(cmd_retry_delay_));
            }
        }

        if (!success) {
            logError(config_.name,
                     "Command FAILED after " + std::to_string(cmd_max_retries_) +
                     " attempts: " + jsonToString(qc.cmd));
        }
    }
}

// ---------------------------------------------------------------------------
// executeCommand() -- burst mode: fresh connection per attempt
// ---------------------------------------------------------------------------

bool DeviceWorker::executeCommand(const Json::Value& cmd) {
    try {
        bool any_success = false;

        if (cmd.isMember("state")) {
            std::string state = cmd["state"].asString();
            for (auto& c : state) c = static_cast<char>(std::tolower(c));

            bool target = true;
            if (state == "on" || state == "true" || state == "1") {
                target = true;
            } else if (state == "off" || state == "false" || state == "0") {
                target = false;
            } else if (state == "toggle") {
                std::lock_guard<std::mutex> lk(state_mutex_);
                target = !(last_state_.isMember("on") && last_state_["on"].asBool());
            }

            auto device = std::make_unique<nanotuya::TuyaDevice>(config_.tuya_config);
            if (device->setValue("1", Json::Value(target))) {
                any_success = true;
            } else {
                logWarning(config_.name, "setValue(1) failed: " + device->lastError());
                return false;
            }
        }

        if (config_.type == "bulb") {
            if (cmd.isMember("brightness")) {
                int bval = pctToBrightness(cmd["brightness"].asInt());
                auto d = std::make_unique<nanotuya::TuyaDevice>(config_.tuya_config);
                if (d->setValue("3", Json::Value(bval))) {
                    any_success = true;
                }
            }
            if (cmd.isMember("color_temp")) {
                auto d = std::make_unique<nanotuya::TuyaDevice>(config_.tuya_config);
                if (d->setValue("4", Json::Value(cmd["color_temp"].asInt()))) {
                    any_success = true;
                }
            }
            if (cmd.isMember("mode")) {
                auto d = std::make_unique<nanotuya::TuyaDevice>(config_.tuya_config);
                if (d->setValue("2", Json::Value(cmd["mode"].asString()))) {
                    any_success = true;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        auto readback = std::make_unique<nanotuya::TuyaDevice>(config_.tuya_config);
        auto data = readback->queryStatus();
        if (data.has_value()) {
            publishState(data.value());
        }

        return any_success;

    } catch (const std::exception& e) {
        logWarning(config_.name, std::string("Cmd exec error: ") + e.what());
        return false;
    }
}

// ---------------------------------------------------------------------------
// executeCommandPersistent() -- reuse persistent connection
// ---------------------------------------------------------------------------

bool DeviceWorker::executeCommandPersistent(const Json::Value& cmd) {
    try {
        if (!device_ || !device_->isConnected()) return false;

        bool any_success = false;

        if (cmd.isMember("state")) {
            std::string state = cmd["state"].asString();
            for (auto& c : state) c = static_cast<char>(std::tolower(c));

            bool target = true;
            if (state == "on" || state == "true" || state == "1") {
                target = true;
            } else if (state == "off" || state == "false" || state == "0") {
                target = false;
            } else if (state == "toggle") {
                std::lock_guard<std::mutex> lk(state_mutex_);
                target = !(last_state_.isMember("on") && last_state_["on"].asBool());
            }

            if (device_->setValue("1", Json::Value(target))) {
                any_success = true;
            } else {
                logWarning(config_.name, "setValue(1) failed: " + device_->lastError());
                return false;
            }
        }

        if (config_.type == "bulb") {
            if (cmd.isMember("brightness")) {
                int bval = pctToBrightness(cmd["brightness"].asInt());
                if (device_->setValue("3", Json::Value(bval))) {
                    any_success = true;
                }
            }
            if (cmd.isMember("color_temp")) {
                if (device_->setValue("4", Json::Value(cmd["color_temp"].asInt()))) {
                    any_success = true;
                }
            }
            if (cmd.isMember("mode")) {
                if (device_->setValue("2", Json::Value(cmd["mode"].asString()))) {
                    any_success = true;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        auto data = device_->queryStatus();
        if (data.has_value()) {
            publishState(data.value());
        }

        return any_success;

    } catch (const std::exception& e) {
        logWarning(config_.name, std::string("Cmd exec error: ") + e.what());
        return false;
    }
}

// ---------------------------------------------------------------------------
// publishState()
// ---------------------------------------------------------------------------

void DeviceWorker::publishState(const Json::Value& dps) {
    Json::Value state;
    if (config_.type == "bulb") {
        state = parseBulbState(dps);
        state["brightness_pct"] = brightnessPct(
            state.get("brightness", 25).asInt());
    } else {
        state = parseSwitchState(dps);
    }

    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        if (state == last_state_) {
            return;
        }
    }

    bool power_on = state.get("on", false).asBool();

    Json::Value payload;
    if (config_.type == "bulb") {
        payload["state"] = power_on ? "ON" : "OFF";
        payload["brightness"] = state.get("brightness_pct", 0);
        payload["color_mode"] = "brightness";
        payload["mode"] = state.get("mode", "white");
        payload["color_temp"] = state.get("color_temp", 0);
        payload["device"] = config_.name;
        payload["friendly_name"] = config_.friendly_name;
    } else {
        payload["state"] = power_on ? "on" : "off";
        payload["device"] = config_.name;
        payload["friendly_name"] = config_.friendly_name;
    }

    std::string mqtt_type = (config_.type == "bulb") ? "light" : "switch";
    publish_cb_(config_.name, mqtt_type, payload, power_on);

    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        if (!last_state_.isNull()) {
            std::vector<std::string> changes;
            for (const auto& key : state.getMemberNames()) {
                if (key == "colour_data") continue;
                if (state[key] != last_state_.get(key, Json::nullValue)) {
                    changes.push_back(key + "=" + jsonToString(state[key]));
                }
            }
            if (!changes.empty()) {
                std::string msg = "Changed: ";
                for (size_t i = 0; i < changes.size(); ++i) {
                    if (i > 0) msg += ", ";
                    msg += changes[i];
                }
                logInfo(config_.name, msg);
            }
        } else {
            if (config_.type == "bulb") {
                logInfo(config_.name,
                        "State: " + std::string(power_on ? "on" : "off") +
                        " brightness=" + std::to_string(
                            state.get("brightness_pct", 0).asInt()) +
                        "% mode=" + state.get("mode", "white").asString());
            } else {
                logInfo(config_.name,
                        "State: " + std::string(power_on ? "on" : "off"));
            }
        }
        last_state_ = state;
    }
}

// ---------------------------------------------------------------------------
// publishOptimistic()
// ---------------------------------------------------------------------------

void DeviceWorker::publishOptimistic(const Json::Value& cmd) {
    if (!cmd.isMember("state")) return;

    std::string state_str = cmd["state"].asString();
    for (auto& c : state_str) c = static_cast<char>(std::tolower(c));

    bool desired_on;
    if (state_str == "toggle") {
        std::lock_guard<std::mutex> lk(state_mutex_);
        desired_on = !last_state_.get("on", false).asBool();
    } else {
        desired_on = (state_str == "on" || state_str == "true" || state_str == "1");
    }

    Json::Value payload;
    if (config_.type == "bulb") {
        int brt;
        if (cmd.isMember("brightness")) {
            brt = cmd["brightness"].asInt();
        } else {
            std::lock_guard<std::mutex> lk(state_mutex_);
            brt = last_state_.get("brightness_pct", 100).asInt();
        }
        payload["state"] = desired_on ? "ON" : "OFF";
        payload["brightness"] = desired_on ? brt : 0;
        payload["color_mode"] = "brightness";
        {
            std::lock_guard<std::mutex> lk(state_mutex_);
            payload["mode"] = last_state_.get("mode", "white");
            payload["color_temp"] = last_state_.get("color_temp", 0);
        }
        payload["device"] = config_.name;
        payload["friendly_name"] = config_.friendly_name;
    } else {
        payload["state"] = desired_on ? "on" : "off";
        payload["device"] = config_.name;
        payload["friendly_name"] = config_.friendly_name;
    }

    std::string mqtt_type = (config_.type == "bulb") ? "light" : "switch";
    publish_cb_(config_.name, mqtt_type, payload, desired_on);
}

// ---------------------------------------------------------------------------
// DPS parsers
// ---------------------------------------------------------------------------

Json::Value DeviceWorker::parseBulbState(const Json::Value& dps) {
    Json::Value state;
    state["on"] = dps.get("1", false).asBool();
    state["mode"] = dps.get("2", "white").asString();
    state["brightness"] = dps.get("3", 25).asInt();
    state["color_temp"] = dps.get("4", 0).asInt();
    state["colour_data"] = dps.get("5", "").asString();
    state["brightness_pct"] = brightnessPct(state["brightness"].asInt());
    return state;
}

Json::Value DeviceWorker::parseSwitchState(const Json::Value& dps) {
    Json::Value state;
    state["on"] = dps.get("1", false).asBool();
    state["countdown"] = dps.get("9", 0).asInt();
    return state;
}

// ---------------------------------------------------------------------------
// Brightness conversion  (Tuya 25-255 <-> percentage 0-100)
// ---------------------------------------------------------------------------

int DeviceWorker::brightnessPct(int value) {
    return std::max(0, std::min(100,
        static_cast<int>(std::round(
            static_cast<double>(value - 25) / (255.0 - 25.0) * 100.0))));
}

int DeviceWorker::pctToBrightness(int pct) {
    return std::max(25, std::min(255,
        static_cast<int>(std::round(
            static_cast<double>(pct) / 100.0 * (255.0 - 25.0) + 25.0))));
}
