#pragma once
#include <drogon/HttpController.h>
#include <json/json.h>
#include <memory>
#include <string>
#include <chrono>

class BridgeManager;
class ConfigManager;

class ApiController : public drogon::HttpController<ApiController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ApiController::health,          "/health",                    drogon::Get);
    ADD_METHOD_TO(ApiController::getStatus,       "/api/status",                drogon::Get);
    ADD_METHOD_TO(ApiController::getDevices,      "/api/devices",               drogon::Get);
    ADD_METHOD_TO(ApiController::addDevice,       "/api/devices",               drogon::Post);
    ADD_METHOD_TO(ApiController::updateDevice,    "/api/devices/{1}",           drogon::Put);
    ADD_METHOD_TO(ApiController::deleteDevice,    "/api/devices/{1}",           drogon::Delete);
    ADD_METHOD_TO(ApiController::getDeviceState,  "/api/devices/{1}/state",     drogon::Get);
    ADD_METHOD_TO(ApiController::sendCommand,     "/api/devices/{1}/command",   drogon::Post);
    ADD_METHOD_TO(ApiController::getLogs,          "/api/logs",                  drogon::Get);
    ADD_METHOD_TO(ApiController::getSettings,      "/api/settings",              drogon::Get);
    ADD_METHOD_TO(ApiController::updateSettings,   "/api/settings",              drogon::Put);
    ADD_METHOD_TO(ApiController::cloudDiscover,    "/api/cloud/discover",        drogon::Post);
    ADD_METHOD_TO(ApiController::cloudImport,      "/api/cloud/import",          drogon::Post);
    METHOD_LIST_END

    // Static setters for dependency injection (set from main.cpp)
    static void setBridgeManager(std::shared_ptr<BridgeManager> bridge);
    static void setConfigManager(std::shared_ptr<ConfigManager> config);

    // Handlers
    void health(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getStatus(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getDevices(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void addDevice(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void updateDevice(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      const std::string& name);
    void deleteDevice(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      const std::string& name);
    void getDeviceState(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                        const std::string& name);
    void sendCommand(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& name);
    void getLogs(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getSettings(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void updateSettings(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void cloudDiscover(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void cloudImport(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

private:
    static drogon::HttpResponsePtr jsonResponse(const Json::Value& json, int status = 200);
    static drogon::HttpResponsePtr errorResponse(const std::string& msg, int status = 400);

    static std::shared_ptr<BridgeManager> bridge_;
    static std::shared_ptr<ConfigManager> config_;
    static std::chrono::steady_clock::time_point start_time_;
};
