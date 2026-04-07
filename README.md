# hms-tuya

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-support-%23FFDD00.svg?logo=buy-me-a-coffee)](https://www.buymeacoffee.com/aamat09)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

C++ Tuya WiFi MQTT bridge with Home Assistant auto-discovery. Controls Tuya WiFi devices locally over the LAN. 2 MB memory.

Built on [nanotuya](https://github.com/hms-homelab/nanotuya), the C++ Tuya local protocol library.

## Features

- Per-device worker threads (no device blocks another)
- Command queue with retry and exponential backoff
- Home Assistant MQTT auto-discovery (light + switch entities)
- Optimistic state publishing (instant HA feedback)
- Web admin API for device management
- YAML config + JSON device list (hot-reloadable)
- Fresh TCP connection per operation (proven reliability)
- 2.2 MB memory footprint

## Quick Start

### 1. Build

```bash
# Dependencies (Debian/Ubuntu)
sudo apt install libssl-dev libjsoncpp-dev libyaml-cpp-dev \
    libpaho-mqttpp-dev libpaho-mqtt3as-dev libgtest-dev

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 2. Configure

```bash
sudo mkdir -p /etc/hms-tuya
sudo cp config/hms-tuya.yaml.example /etc/hms-tuya/hms-tuya.yaml
sudo cp config/devices.json.example /etc/hms-tuya/devices.json
# Edit with your MQTT broker and device credentials
```

### 3. Run

```bash
./hms_tuya --config /etc/hms-tuya/hms-tuya.yaml
```

### 4. Install as Service

```bash
sudo cp build/hms_tuya /usr/local/bin/
sudo cp hms-tuya.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now hms-tuya
```

## Configuration

### hms-tuya.yaml

```yaml
server:
  port: 8899

mqtt:
  broker: "192.168.1.100"
  port: 1883
  username: "user"
  password: "pass"
  client_id: "tuya_mqtt_bridge"
  topic_prefix: "tuya"

devices_file: "devices.json"

tuya:
  poll_interval: 10      # seconds between status polls
  socket_timeout: 5      # TCP timeout per device
  min_backoff: 10        # initial backoff on failure
  max_backoff: 300       # max backoff (5 minutes)
  cmd_max_retries: 5     # command retry attempts
  cmd_retry_delay: 3     # seconds between retries
```

### devices.json

```json
[
  {
    "id": "your_device_id",
    "name": "living_room",
    "friendly_name": "Living Room Light",
    "ip": "192.168.1.50",
    "local_key": "your_16char_key!",
    "version": "3.3",
    "type": "bulb",
    "enabled": true
  }
]
```

Get device credentials with [tinytuya](https://github.com/jasonacox/tinytuya):
```bash
pip install tinytuya && python -m tinytuya wizard
```

## MQTT Topics

```
tuya/light/{name}/state     # JSON state (retained)
tuya/light/{name}/set       # commands (ON/OFF/JSON)
tuya/light/{name}/power     # simple "on"/"off" (retained)
tuya/switch/{name}/state
tuya/switch/{name}/set
tuya/switch/{name}/power
tuya_bridge/status          # "online"/"offline" (LWT)
```

### HA Auto-Discovery

Devices appear automatically in Home Assistant via MQTT discovery. No YAML configuration needed.

## Architecture

```
Home Assistant
    |
    | MQTT (tuya/light/{name}/set)
    v
hms-tuya Bridge
    |
    | Per-device worker thread
    | Command queue + retry
    | Exponential backoff
    v
nanotuya library
    |
    | TCP:6668 (fresh connection per operation)
    | AES-128-ECB + CRC32/HMAC
    v
Tuya WiFi Device
```

## Building with Local nanotuya

The CMake build checks for a local nanotuya copy first, then falls back to GitHub:

```cmake
# Automatic: uses ../nanotuya if present, fetches from GitHub otherwise
cmake ..

# Force GitHub fetch
cmake .. -DNANOTUYA_LOCAL_PATH=/nonexistent
```

## Docker

```bash
# Pull and run
docker pull ghcr.io/hms-homelab/hms-tuya:latest
docker run -v ./config:/etc/hms-tuya --network host ghcr.io/hms-homelab/hms-tuya:latest

# Or use docker-compose
docker compose up -d
```

Supports `linux/amd64` and `linux/arm64` (Raspberry Pi).

## Related Projects

- [nanotuya](https://github.com/hms-homelab/nanotuya) -- C++ Tuya WiFi local protocol library
- [hms-esp-tuya-ble](https://github.com/hms-homelab/hms-esp-tuya-ble) -- ESP32-C3 BLE bridge for Tuya BLE devices

## License

MIT License -- see [LICENSE](LICENSE) for details.
