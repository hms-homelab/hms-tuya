#include "ConfigManager.h"
#include "bridge/MqttBridge.h"
#include "bridge/BridgeManager.h"
#include "LogBuffer.h"

#ifdef BUILD_WITH_WEB
#include "web/ApiController.h"
#include <drogon/drogon.h>
#endif

#include <iostream>
#include <csignal>
#include <fstream>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>

static std::atomic<bool> g_running{true};
static std::mutex g_mutex;
static std::condition_variable g_cv;

static void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
    g_cv.notify_all();
#ifdef BUILD_WITH_WEB
    drogon::app().quit();
#endif
}

static std::string findConfigPath(int argc, char* argv[]) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--config") {
            return argv[i + 1];
        }
    }
    const std::vector<std::string> paths = {
        "/etc/hms-tuya/hms-tuya.yaml",
        "config/hms-tuya.yaml",
        "hms-tuya.yaml"
    };
    for (const auto& p : paths) {
        std::ifstream f(p);
        if (f.good()) return p;
    }
    return "config/hms-tuya.yaml";
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::string config_path = findConfigPath(argc, argv);
    std::cout << "Loading config from " << config_path << std::endl;

    // Load configuration
    auto config = std::make_shared<ConfigManager>();
    if (!config->load(config_path)) {
        std::cerr << "Failed to load config from " << config_path << std::endl;
        return 1;
    }

    auto& app = config->appConfig();
    auto devices = config->getDevices();
    std::cout << "Loaded " << devices.size() << " devices" << std::endl;

    // Create MQTT bridge
    auto mqtt = std::make_shared<MqttBridge>(
        app.mqtt_broker, app.mqtt_port,
        app.mqtt_username, app.mqtt_password,
        app.mqtt_client_id, app.mqtt_topic_prefix);

    if (!mqtt->connect()) {
        std::cerr << "Failed to connect to MQTT broker at "
                  << app.mqtt_broker << ":" << app.mqtt_port << std::endl;
        return 1;
    }
    std::cout << "MQTT connected to " << app.mqtt_broker << ":" << app.mqtt_port << std::endl;

    // Create and start bridge manager
    auto bridge = std::make_shared<BridgeManager>(mqtt, config);
    bridge->start();

    LogBuffer::instance().info("bridge", "hms-tuya started");

#ifdef BUILD_WITH_WEB
    // Configure Drogon web server
    ApiController::setBridgeManager(bridge);
    ApiController::setConfigManager(config);

    // Read index.html for SPA fallback
    std::string static_dir = "./static/browser";
    std::string index_html;
    {
        std::ifstream f(static_dir + "/index.html");
        if (f.good()) {
            index_html = std::string(std::istreambuf_iterator<char>(f),
                                     std::istreambuf_iterator<char>());
        }
    }

    // SPA fallback: return index.html for non-API 404s
    auto spa_fallback = index_html;
    drogon::app().setCustomErrorHandler(
        [spa_fallback](drogon::HttpStatusCode code) -> drogon::HttpResponsePtr {
            if (code == drogon::k404NotFound && !spa_fallback.empty()) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k200OK);
                resp->setContentTypeCode(drogon::CT_TEXT_HTML);
                resp->setBody(spa_fallback);
                return resp;
            }
            return nullptr;
        });

    std::cout << "Web UI on port " << app.server_port << std::endl;

    drogon::app()
        .setLogLevel(trantor::Logger::kWarn)
        .addListener("0.0.0.0", app.server_port)
        .setThreadNum(app.server_threads)
        .setDocumentRoot(static_dir)
        .setIdleConnectionTimeout(120)
        .run();
#else
    std::cout << "hms-tuya bridge running (no web UI)" << std::endl;

    // Block main thread until signal
    {
        std::unique_lock<std::mutex> lock(g_mutex);
        g_cv.wait(lock, [] { return !g_running.load(); });
    }
#endif

    // Graceful shutdown
    std::cout << "Shutting down..." << std::endl;
    bridge->stop();
    mqtt->disconnect();
    std::cout << "Stopped" << std::endl;

    return 0;
}
