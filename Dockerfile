# HMS-Tuya - Multi-stage Docker build
# C++ Tuya WiFi MQTT bridge with Angular web admin

# =============================================================================
# Stage 1: Angular UI Builder
# =============================================================================
FROM node:22-slim AS ui-builder

WORKDIR /ui
COPY frontend/package*.json ./
RUN npm ci --no-audit --no-fund
COPY frontend/ ./
RUN npx ng build --configuration production

# =============================================================================
# Stage 2: C++ Builder
# =============================================================================
FROM debian:trixie-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ca-certificates \
    git \
    pkg-config \
    libssl-dev \
    libjsoncpp-dev \
    libyaml-cpp-dev \
    libpaho-mqtt-dev \
    libpaho-mqttpp-dev \
    libcurl4-openssl-dev \
    libdrogon-dev \
    uuid-dev libhiredis-dev libbrotli-dev zlib1g-dev \
    libpq-dev libmariadb-dev libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY CMakeLists.txt VERSION ./
COPY src/ ./src/
COPY include/ ./include/

# nanotuya fetched via FetchContent from GitHub
RUN mkdir build && cd build && \
    cmake -DHMS_TUYA_BUILD_TESTS=OFF -DBUILD_WITH_WEB=ON .. && \
    make -j$(nproc) && \
    strip hms_tuya

# =============================================================================
# Stage 3: Runtime
# =============================================================================
FROM debian:trixie-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    libssl3 \
    libjsoncpp26 \
    libyaml-cpp0.8 \
    libpaho-mqtt1.3 \
    libpaho-mqttpp3-1 \
    libcurl4t64 \
    libdrogon1t64 \
    libtrantor1 \
    && rm -rf /var/lib/apt/lists/*

# Non-root user
RUN useradd -r -u 1000 -m -s /bin/bash tuya

# Copy binary
COPY --from=builder /build/build/hms_tuya /usr/local/bin/hms_tuya
RUN chmod +x /usr/local/bin/hms_tuya

# Copy Angular UI
COPY --from=ui-builder /ui/dist/frontend/browser/ /home/tuya/static/browser/

# Copy config examples
COPY config/hms-tuya.yaml.example /etc/hms-tuya/hms-tuya.yaml
COPY config/devices.json.example /etc/hms-tuya/devices.json
RUN chown -R tuya:tuya /etc/hms-tuya /home/tuya/static

USER tuya
WORKDIR /home/tuya

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8899/health || exit 1

EXPOSE 8899

ENTRYPOINT ["/usr/local/bin/hms_tuya", "--config", "/etc/hms-tuya/hms-tuya.yaml"]
