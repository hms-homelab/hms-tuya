#pragma once
#include <json/json.h>
#include <string>
#include <deque>
#include <mutex>
#include <chrono>

struct LogEntry {
    std::string timestamp;
    std::string level;    // "INFO", "WARNING", "ERROR"
    std::string device;   // device name or "bridge"
    std::string message;
};

class LogBuffer {
public:
    static LogBuffer& instance();

    void info(const std::string& device, const std::string& msg);
    void warning(const std::string& device, const std::string& msg);
    void error(const std::string& device, const std::string& msg);

    Json::Value getRecent(int count = 200) const;
    void clear();

private:
    LogBuffer() = default;
    void add(const std::string& level, const std::string& device, const std::string& msg);
    std::string makeTimestamp() const;

    std::deque<LogEntry> entries_;
    mutable std::mutex mutex_;
    static constexpr size_t MAX_ENTRIES = 500;
};
