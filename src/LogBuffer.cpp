#include "LogBuffer.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

LogBuffer& LogBuffer::instance() {
    static LogBuffer buf;
    return buf;
}

void LogBuffer::info(const std::string& device, const std::string& msg) {
    add("INFO", device, msg);
}

void LogBuffer::warning(const std::string& device, const std::string& msg) {
    add("WARNING", device, msg);
}

void LogBuffer::error(const std::string& device, const std::string& msg) {
    add("ERROR", device, msg);
}

void LogBuffer::add(const std::string& level, const std::string& device, const std::string& msg) {
    LogEntry entry;
    entry.timestamp = makeTimestamp();
    entry.level = level;
    entry.device = device;
    entry.message = msg;

    // Print to stdout (same format as DeviceWorker logging)
    std::cout << entry.timestamp << " [" << level << "] "
              << device << ": " << msg << std::endl;

    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(std::move(entry));
    while (entries_.size() > MAX_ENTRIES) {
        entries_.pop_front();
    }
}

Json::Value LogBuffer::getRecent(int count) const {
    Json::Value arr(Json::arrayValue);
    std::lock_guard<std::mutex> lock(mutex_);

    int start = 0;
    if (count > 0 && static_cast<size_t>(count) < entries_.size()) {
        start = static_cast<int>(entries_.size()) - count;
    }

    for (size_t i = static_cast<size_t>(start); i < entries_.size(); ++i) {
        Json::Value obj;
        obj["timestamp"] = entries_[i].timestamp;
        obj["level"] = entries_[i].level;
        obj["device"] = entries_[i].device;
        obj["message"] = entries_[i].message;
        arr.append(obj);
    }

    return arr;
}

void LogBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

std::string LogBuffer::makeTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&time_t_now, &tm_buf);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
