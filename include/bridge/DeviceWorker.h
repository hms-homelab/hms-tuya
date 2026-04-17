#pragma once
#include <nanotuya/TuyaDevice.h>
#include <json/json.h>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <string>
#include <functional>

#include "ConfigManager.h"

class MqttBridge;  // forward decl

class DeviceWorker {
public:
    // publish_cb: called to publish state via MQTT
    using PublishCallback = std::function<void(const std::string& name, const std::string& type,
                                               const Json::Value& state, bool power_on)>;

    DeviceWorker(const DeviceEntry& config, PublishCallback publish_cb,
                 bool persistent, int poll_interval, int heartbeat_interval,
                 double min_backoff, double max_backoff,
                 int cmd_max_retries, int cmd_retry_delay, int cmd_timeout);
    ~DeviceWorker();

    void start();
    void stop();
    void enqueueCommand(const Json::Value& cmd);

    bool isRunning() const { return running_.load(); }
    Json::Value lastState() const;
    std::string deviceName() const;

private:
    void run();
    void runPersistent();
    void runBurst();
    bool poll();
    bool pollPersistent();
    void drainCommands();
    void drainCommandsPersistent();
    bool executeCommand(const Json::Value& cmd);
    bool executeCommandPersistent(const Json::Value& cmd);
    void publishState(const Json::Value& dps);
    void publishOptimistic(const Json::Value& cmd);

    Json::Value parseBulbState(const Json::Value& dps);
    Json::Value parseSwitchState(const Json::Value& dps);
    int brightnessPct(int value);
    int pctToBrightness(int pct);

    DeviceEntry config_;
    PublishCallback publish_cb_;
    bool persistent_;

    std::thread thread_;
    std::atomic<bool> running_{false};

    // Persistent connection (owned by worker thread)
    std::unique_ptr<nanotuya::TuyaDevice> device_;

    // Command queue
    struct QueuedCommand {
        Json::Value cmd;
        std::chrono::steady_clock::time_point queued_at;
    };
    std::queue<QueuedCommand> cmd_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // State
    Json::Value last_state_;
    mutable std::mutex state_mutex_;

    // Timing
    double backoff_;
    std::chrono::steady_clock::time_point next_poll_;
    std::chrono::steady_clock::time_point next_heartbeat_;

    // Config
    int poll_interval_;
    int heartbeat_interval_;
    double min_backoff_;
    double max_backoff_;
    int cmd_max_retries_;
    int cmd_retry_delay_;
    int cmd_timeout_;
};
