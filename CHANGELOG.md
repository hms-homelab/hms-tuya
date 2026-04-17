# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-04-07

### Added
- Initial release
- MQTT bridge with per-device worker threads
- Home Assistant auto-discovery (light + switch entities)
- Command queue with retry (5 attempts, 3s delay)
- Exponential backoff on poll failure (10s -> 300s)
- Optimistic state publishing for instant HA feedback
- ConfigManager: YAML config + JSON device list with CRUD
- BridgeManager: orchestrator with hot-reload support
- systemd service file
- 10 unit tests (ConfigManager)
- Uses [nanotuya](https://github.com/hms-homelab/nanotuya) v1.0.0 for Tuya protocol
- Tested with 9 real Tuya devices (8 bulbs + 1 switch)
- 2.2 MB memory footprint (vs 20 MB Python bridge)

### Protocol Support (via nanotuya)
- Tuya v3.1, v3.3, v3.4
- AES-128-ECB encryption
- v3.4 session key negotiation
- Fresh TCP connection per operation

### DPS Support
- Bulbs: switch (DP1), mode (DP2), brightness (DP3), color_temp (DP4), colour (DP5)
- Switches: switch (DP1), countdown (DP9)

## [1.2.1] - 2026-04-16

### Fixed
- v3.4 switch control: dinner device now responds to ON/OFF commands instantly
- nanotuya upgraded to v1.2.1 (v3.4 CONTROL_NEW payload fix + sendReceive response handling)

### Added
- Persistent TCP connections per device (configurable via `tuya.mode: persistent|burst`)
- Heartbeat keepalive with configurable interval (`tuya.heartbeat_interval`)
- Auto-reconnect with exponential backoff on connection failure
- Connection mode settings in web admin UI
- GitHub Actions: release workflow for Linux + macOS binaries

### Changed
- Default mode is `persistent` (one long-lived TCP connection per device)
- Settings page maps flat API response to grouped UI sections

## [1.1.0] - 2026-04-07

### Added
- Angular 19 web admin UI (dark theme, device management)
- Drogon web server serving SPA on port 8899
- REST API: 13 endpoints (devices CRUD, status, commands, logs, settings, cloud)
- Tuya Cloud device discovery via nanotuya TuyaCloud
- LogBuffer: thread-safe ring buffer for web log viewer
- Cloud import: auto-discover devices from Tuya IoT Platform
- 3-stage Dockerfile (Angular + C++ + runtime)
- HEALTHCHECK on port 8899

### Changed
- nanotuya upgraded to v1.1.0 (adds TuyaCloud with libcurl)
- Dockerfile now includes Angular UI build stage
- CMakeLists.txt adds BUILD_WITH_WEB option (Drogon)

### Fixed
- MQTT topic paths: bulb -> light (matching Python bridge)
- executeCommand: check setValue return value
- Docker CI: missing pkg-config and libcurl in build deps
