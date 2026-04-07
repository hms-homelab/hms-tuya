# HMS-Tuya - Multi-stage Docker build
# Produces ~30MB final image with C++ runtime

# =============================================================================
# Stage 1: C++ Builder
# =============================================================================
FROM debian:trixie-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ca-certificates \
    git \
    libssl-dev \
    libjsoncpp-dev \
    libyaml-cpp-dev \
    libpaho-mqtt-dev \
    libpaho-mqttpp-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY CMakeLists.txt VERSION ./
COPY src/ ./src/
COPY include/ ./include/

# nanotuya is fetched via FetchContent from GitHub
RUN mkdir build && cd build && \
    cmake -DHMS_TUYA_BUILD_TESTS=OFF .. && \
    make -j$(nproc) && \
    strip hms_tuya

# =============================================================================
# Stage 2: Runtime
# =============================================================================
FROM debian:trixie-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libssl3 \
    libjsoncpp26 \
    libyaml-cpp0.8 \
    libpaho-mqtt1.3 \
    libpaho-mqttpp3-1 \
    && rm -rf /var/lib/apt/lists/*

# Non-root user
RUN useradd -r -u 1000 -m -s /bin/bash tuya

# Copy binary
COPY --from=builder /build/build/hms_tuya /usr/local/bin/hms_tuya
RUN chmod +x /usr/local/bin/hms_tuya

# Copy config examples
COPY config/hms-tuya.yaml.example /etc/hms-tuya/hms-tuya.yaml
COPY config/devices.json.example /etc/hms-tuya/devices.json
RUN chown -R tuya:tuya /etc/hms-tuya

USER tuya
WORKDIR /home/tuya

ENTRYPOINT ["/usr/local/bin/hms_tuya", "--config", "/etc/hms-tuya/hms-tuya.yaml"]
