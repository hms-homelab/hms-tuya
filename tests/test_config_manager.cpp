#include <gtest/gtest.h>
#include "ConfigManager.h"
#include <fstream>
#include <filesystem>
#include <json/json.h>

namespace fs = std::filesystem;

class ConfigManagerTest : public ::testing::Test {
protected:
    fs::path tmp_dir_;

    void SetUp() override {
        tmp_dir_ = fs::temp_directory_path() / ("hms_tuya_test_" + std::to_string(getpid()));
        fs::create_directories(tmp_dir_);
    }

    void TearDown() override {
        fs::remove_all(tmp_dir_);
    }

    // Write a string to a file under tmp_dir_
    void writeFile(const std::string& name, const std::string& content) {
        std::ofstream f(tmp_dir_ / name);
        f << content;
    }

    // Write a full valid YAML config
    void writeValidYaml(const std::string& name = "config.yaml",
                        const std::string& devices_file = "devices.json") {
        std::string yaml = R"(
server:
  port: 9000
  threads: 4
mqtt:
  broker: "192.168.2.15"
  port: 1884
  username: "testuser"
  password: "testpass"
  client_id: "test_tuya"
  topic_prefix: "test/tuya"
tuya:
  poll_interval: 15
  socket_timeout: 8
  min_backoff: 5.0
  max_backoff: 120.0
  cmd_max_retries: 3
  cmd_retry_delay: 2
  cmd_timeout: 30
devices_file: ")" + devices_file + R"("
)";
        writeFile(name, yaml);
    }

    // Write a devices.json with 2 devices (one bulb, one switch)
    void writeTwoDevices(const std::string& name = "devices.json") {
        std::string json = R"([
  {
    "id": "device001",
    "ip": "192.168.2.100",
    "local_key": "0123456789abcdef",
    "version": "3.3",
    "port": 6668,
    "timeout_ms": 5000,
    "name": "patio",
    "friendly_name": "Patio Light",
    "type": "bulb",
    "enabled": true
  },
  {
    "id": "device002",
    "ip": "192.168.2.101",
    "local_key": "abcdef0123456789",
    "version": "3.4",
    "port": 6668,
    "timeout_ms": 3000,
    "name": "office",
    "friendly_name": "Office Switch",
    "type": "switch",
    "enabled": true
  }
])";
        writeFile(name, json);
    }

    // Build a DeviceEntry for testing
    DeviceEntry makeDevice(const std::string& name, const std::string& id,
                           const std::string& ip, const std::string& type = "switch") {
        DeviceEntry dev;
        dev.tuya_config.id = id;
        dev.tuya_config.ip = ip;
        dev.tuya_config.local_key = "aaaaaaaaaaaaaaaa";
        dev.tuya_config.version = nanotuya::TuyaVersion::V33;
        dev.tuya_config.port = 6668;
        dev.tuya_config.timeout_ms = 5000;
        dev.name = name;
        dev.friendly_name = name;
        dev.type = type;
        dev.enabled = true;
        return dev;
    }
};

// 1. Load valid YAML
TEST_F(ConfigManagerTest, LoadValidYaml) {
    writeValidYaml();
    writeTwoDevices();

    ConfigManager mgr;
    ASSERT_TRUE(mgr.load((tmp_dir_ / "config.yaml").string()));

    const auto& cfg = mgr.appConfig();
    EXPECT_EQ(cfg.server_port, 9000);
    EXPECT_EQ(cfg.server_threads, 4);
    EXPECT_EQ(cfg.mqtt_broker, "192.168.2.15");
    EXPECT_EQ(cfg.mqtt_port, 1884);
    EXPECT_EQ(cfg.mqtt_username, "testuser");
    EXPECT_EQ(cfg.mqtt_password, "testpass");
    EXPECT_EQ(cfg.mqtt_client_id, "test_tuya");
    EXPECT_EQ(cfg.mqtt_topic_prefix, "test/tuya");
    EXPECT_EQ(cfg.poll_interval, 15);
    EXPECT_EQ(cfg.socket_timeout, 8);
    EXPECT_DOUBLE_EQ(cfg.min_backoff, 5.0);
    EXPECT_DOUBLE_EQ(cfg.max_backoff, 120.0);
    EXPECT_EQ(cfg.cmd_max_retries, 3);
    EXPECT_EQ(cfg.cmd_retry_delay, 2);
    EXPECT_EQ(cfg.cmd_timeout, 30);
}

// 2. Load YAML with missing mqtt section -- defaults used
TEST_F(ConfigManagerTest, LoadYamlMissingSections) {
    std::string yaml = R"(
server:
  port: 7777
tuya:
  poll_interval: 20
)";
    writeFile("config.yaml", yaml);
    // No devices.json needed (will just get empty list)

    ConfigManager mgr;
    ASSERT_TRUE(mgr.load((tmp_dir_ / "config.yaml").string()));

    const auto& cfg = mgr.appConfig();
    // mqtt defaults
    EXPECT_EQ(cfg.mqtt_broker, "localhost");
    EXPECT_EQ(cfg.mqtt_port, 1883);
    EXPECT_EQ(cfg.mqtt_username, "");
    EXPECT_EQ(cfg.mqtt_password, "");
    EXPECT_EQ(cfg.mqtt_client_id, "hms_tuya");
    EXPECT_EQ(cfg.mqtt_topic_prefix, "tuya");
    // server was set
    EXPECT_EQ(cfg.server_port, 7777);
    // tuya was set
    EXPECT_EQ(cfg.poll_interval, 20);
    // tuya defaults for unset fields
    EXPECT_EQ(cfg.socket_timeout, 5);
    EXPECT_DOUBLE_EQ(cfg.min_backoff, 10.0);
}

// 3. Load nonexistent YAML returns false
TEST_F(ConfigManagerTest, LoadNonexistentYaml) {
    ConfigManager mgr;
    EXPECT_FALSE(mgr.load("/tmp/hms_tuya_nonexistent_file_xyz.yaml"));
}

// 4. getDevices returns loaded devices with correct fields
TEST_F(ConfigManagerTest, GetDevicesReturnsLoadedDevices) {
    writeValidYaml();
    writeTwoDevices();

    ConfigManager mgr;
    ASSERT_TRUE(mgr.load((tmp_dir_ / "config.yaml").string()));

    auto devices = mgr.getDevices();
    ASSERT_EQ(devices.size(), 2u);

    // First device: patio bulb
    EXPECT_EQ(devices[0].name, "patio");
    EXPECT_EQ(devices[0].friendly_name, "Patio Light");
    EXPECT_EQ(devices[0].type, "bulb");
    EXPECT_EQ(devices[0].tuya_config.id, "device001");
    EXPECT_EQ(devices[0].tuya_config.ip, "192.168.2.100");
    EXPECT_EQ(devices[0].tuya_config.local_key, "0123456789abcdef");
    EXPECT_EQ(devices[0].tuya_config.port, 6668);
    EXPECT_EQ(devices[0].tuya_config.timeout_ms, 5000);
    EXPECT_TRUE(devices[0].enabled);

    // Second device: office switch
    EXPECT_EQ(devices[1].name, "office");
    EXPECT_EQ(devices[1].friendly_name, "Office Switch");
    EXPECT_EQ(devices[1].type, "switch");
    EXPECT_EQ(devices[1].tuya_config.id, "device002");
    EXPECT_EQ(devices[1].tuya_config.ip, "192.168.2.101");
    EXPECT_EQ(devices[1].tuya_config.timeout_ms, 3000);
    EXPECT_TRUE(devices[1].enabled);
}

// 5. Device version parsing: "3.1" -> V31, "3.3" -> V33, "3.4" -> V34
TEST_F(ConfigManagerTest, DeviceVersionParsing) {
    std::string json = R"([
  {"id":"d1","ip":"1.1.1.1","local_key":"aaaaaaaaaaaaaaaa","version":"3.1","name":"v31dev","type":"switch"},
  {"id":"d2","ip":"1.1.1.2","local_key":"bbbbbbbbbbbbbbbb","version":"3.3","name":"v33dev","type":"switch"},
  {"id":"d3","ip":"1.1.1.3","local_key":"cccccccccccccccc","version":"3.4","name":"v34dev","type":"switch"}
])";
    writeFile("devices.json", json);
    writeValidYaml();

    ConfigManager mgr;
    ASSERT_TRUE(mgr.load((tmp_dir_ / "config.yaml").string()));

    auto devices = mgr.getDevices();
    ASSERT_EQ(devices.size(), 3u);

    EXPECT_EQ(devices[0].tuya_config.version, nanotuya::TuyaVersion::V31);
    EXPECT_EQ(devices[1].tuya_config.version, nanotuya::TuyaVersion::V33);
    EXPECT_EQ(devices[2].tuya_config.version, nanotuya::TuyaVersion::V34);
}

// 6. addDevice persists to disk
TEST_F(ConfigManagerTest, AddDevice) {
    writeValidYaml();
    writeTwoDevices();

    ConfigManager mgr;
    ASSERT_TRUE(mgr.load((tmp_dir_ / "config.yaml").string()));
    ASSERT_EQ(mgr.getDevices().size(), 2u);

    auto newDev = makeDevice("bedroom", "device003", "192.168.2.102", "bulb");
    EXPECT_TRUE(mgr.addDevice(newDev));

    // In-memory check
    auto devices = mgr.getDevices();
    ASSERT_EQ(devices.size(), 3u);
    EXPECT_EQ(devices[2].name, "bedroom");
    EXPECT_EQ(devices[2].tuya_config.id, "device003");
    EXPECT_EQ(devices[2].type, "bulb");

    // On-disk check: reload from scratch
    ConfigManager mgr2;
    ASSERT_TRUE(mgr2.load((tmp_dir_ / "config.yaml").string()));
    EXPECT_EQ(mgr2.getDevices().size(), 3u);

    // Duplicate name should fail
    EXPECT_FALSE(mgr.addDevice(newDev));
}

// 7. updateDevice changes fields
TEST_F(ConfigManagerTest, UpdateDevice) {
    writeValidYaml();
    writeTwoDevices();

    ConfigManager mgr;
    ASSERT_TRUE(mgr.load((tmp_dir_ / "config.yaml").string()));

    // Update patio's IP
    auto devices = mgr.getDevices();
    ASSERT_GE(devices.size(), 1u);
    auto updated = devices[0];
    updated.tuya_config.ip = "10.0.0.50";
    updated.friendly_name = "Patio Updated";

    EXPECT_TRUE(mgr.updateDevice("patio", updated));

    auto after = mgr.getDevices();
    EXPECT_EQ(after[0].tuya_config.ip, "10.0.0.50");
    EXPECT_EQ(after[0].friendly_name, "Patio Updated");

    // Update nonexistent device returns false
    EXPECT_FALSE(mgr.updateDevice("nonexistent", updated));
}

// 8. removeDevice removes exactly one device
TEST_F(ConfigManagerTest, RemoveDevice) {
    writeValidYaml();
    writeTwoDevices();

    ConfigManager mgr;
    ASSERT_TRUE(mgr.load((tmp_dir_ / "config.yaml").string()));
    ASSERT_EQ(mgr.getDevices().size(), 2u);

    EXPECT_TRUE(mgr.removeDevice("patio"));

    auto devices = mgr.getDevices();
    ASSERT_EQ(devices.size(), 1u);
    EXPECT_EQ(devices[0].name, "office");

    // Remove nonexistent returns false
    EXPECT_FALSE(mgr.removeDevice("patio"));

    // On-disk check
    ConfigManager mgr2;
    ASSERT_TRUE(mgr2.load((tmp_dir_ / "config.yaml").string()));
    EXPECT_EQ(mgr2.getDevices().size(), 1u);
}

// 9. Disabled devices are still included in getDevices()
TEST_F(ConfigManagerTest, DisabledDevicesIncluded) {
    std::string json = R"([
  {"id":"d1","ip":"1.1.1.1","local_key":"aaaaaaaaaaaaaaaa","version":"3.3","name":"active","type":"switch","enabled":true},
  {"id":"d2","ip":"1.1.1.2","local_key":"bbbbbbbbbbbbbbbb","version":"3.3","name":"inactive","type":"bulb","enabled":false}
])";
    writeFile("devices.json", json);
    writeValidYaml();

    ConfigManager mgr;
    ASSERT_TRUE(mgr.load((tmp_dir_ / "config.yaml").string()));

    auto devices = mgr.getDevices();
    ASSERT_EQ(devices.size(), 2u);

    // Find the disabled one
    bool found_disabled = false;
    for (const auto& d : devices) {
        if (d.name == "inactive") {
            EXPECT_FALSE(d.enabled);
            found_disabled = true;
        }
    }
    EXPECT_TRUE(found_disabled);
}

// 10. devices_file resolved relative to YAML directory
TEST_F(ConfigManagerTest, DevicesFileRelativeToYaml) {
    // Create a subdirectory for the YAML
    fs::path sub = tmp_dir_ / "subdir";
    fs::create_directories(sub);

    // Write YAML in subdir with devices_file="devices.json"
    {
        std::ofstream f(sub / "config.yaml");
        f << "devices_file: \"devices.json\"\n";
    }

    // Write devices.json in subdir (NOT in tmp_dir_ root)
    {
        std::ofstream f(sub / "devices.json");
        f << R"([{"id":"d1","ip":"1.1.1.1","local_key":"aaaaaaaaaaaaaaaa","name":"sub_device","type":"switch"}])";
    }

    ConfigManager mgr;
    ASSERT_TRUE(mgr.load((sub / "config.yaml").string()));

    auto devices = mgr.getDevices();
    ASSERT_EQ(devices.size(), 1u);
    EXPECT_EQ(devices[0].name, "sub_device");
}
