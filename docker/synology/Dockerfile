# syntax=docker/dockerfile:1.7
# Adapted from Madders/darkride-interface@685bb3c0fd8b65b5676dd16be2198653a228e1f2
ARG UBUNTU_VERSION=24.04
FROM ubuntu:${UBUNTU_VERSION} AS toolchain
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ninja-build cmake git curl ca-certificates zip unzip tar xz-utils pkg-config \
    autoconf automake libtool python3 nodejs patchelf patch perl \
    qtbase5-dev qtbase5-private-dev qtdeclarative5-dev qtscript5-dev qtmultimedia5-dev \
    libqt5websockets5-dev libqt5webchannel5-dev libqt5xmlpatterns5-dev libqt5svg5-dev \
    libssl-dev zlib1g-dev libminizip-dev libgl1-mesa-dev libxxf86vm-dev libxi-dev libxrandr-dev \
    libasound2-dev libpulse-dev libudev-dev \
    && rm -rf /var/lib/apt/lists/*
# Build the current source's guest with a checksum-pinned SDK.
RUN curl -fL --retry 3 -o /tmp/wasi.tar.gz \
    https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-34/wasi-sdk-34.0-x86_64-linux.tar.gz \
    && echo 'b761e3a0721dbae9c09a0059e5fdb2bf917d1b4a8a7b430fb3b5aafb0984b2c4  /tmp/wasi.tar.gz' | sha256sum -c - \
    && mkdir /opt/wasi-sdk && tar -xzf /tmp/wasi.tar.gz -C /opt/wasi-sdk --strip-components=1 \
    && rm /tmp/wasi.tar.gz
FROM toolchain AS build
ARG BUILD_JOBS=2
ARG SOURCE_REVISION=unknown
ENV TIVOLI_VCPKG_BASE=/vcpkg RELEASE_TYPE=PRODUCTION RELEASE_NUMBER=1.1-beta WASI_SDK=/opt/wasi-sdk
COPY . /src
# Only the Linux container's source copy is patched, never the Windows working tree.
RUN cd /src && patch --batch --fuzz=0 -p1 < docker/synology/linux-build.patch
RUN sh /src/tools/wasm-js/build.sh || { status=$?; \
    cat /src/tools/wasm-js/build/quickjs-configure.log /src/tools/wasm-js/build/quickjs-build.log 2>/dev/null || true; \
    exit "$status"; }
RUN --mount=type=cache,id=darkride-11-vcpkg,target=/vcpkg \
    --mount=type=cache,id=darkride-11-vcpkg-archives,target=/root/.cache/vcpkg \
    set -eu; export VCPKG_MAX_CONCURRENCY="$BUILD_JOBS"; \
    cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DQT_CMAKE_PREFIX_PATH="/usr/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/cmake" \
      -DSERVER_ONLY=ON -DBUILD_SERVER=ON -DBUILD_CLIENT=OFF \
      -DBUILD_TESTS=OFF -DBUILD_TOOLS=OFF -DBUILD_INSTALLER=OFF; \
    cmake --build /build --parallel "$BUILD_JOBS" --target domain-server assignment-client; \
    sh /src/docker/synology/install-server.sh /build /opt/darkride; \
    printf '%s\n' "$SOURCE_REVISION" > /opt/darkride/SOURCE_REVISION
FROM ubuntu:${UBUNTU_VERSION} AS runtime
ARG SOURCE_REVISION=unknown
LABEL org.opencontainers.image.revision=$SOURCE_REVISION
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates python3 libqt5core5t64 libqt5gui5t64 libqt5network5t64 libqt5widgets5t64 libqt5qml5 libqt5quick5 \
    libqt5script5 libqt5scripttools5 libqt5websockets5 libminizip1t64 libssl3t64 libgl1 libopengl0 \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd -g 10001 darkride && useradd -u 10001 -g 10001 -d /data/home darkride
COPY --from=build /opt/darkride /opt/darkride
COPY docker/synology/run-server.sh docker/synology/init-config.py /opt/darkride/
# Fail on unresolved libraries before publishing.
RUN set -eu; for f in /opt/darkride/domain-server /opt/darkride/assignment-client /opt/darkride/lib/*.so* /opt/darkride/plugins/*.so; do \
      [ -f "$f" ] || continue; ldd "$f" > /tmp/ldd.txt; cat /tmp/ldd.txt; \
      if grep -q 'not found' /tmp/ldd.txt; then exit 1; fi; done; rm /tmp/ldd.txt
ENV TIVOLI_APP_DATA_DIR=/data HOME=/data/home XDG_CONFIG_HOME=/data/config \
    XDG_CACHE_HOME=/data/cache XDG_DATA_HOME=/data/share QT_QPA_PLATFORM=offscreen
USER 10001:10001
WORKDIR /opt/darkride
STOPSIGNAL SIGTERM
CMD ["bash", "/opt/darkride/run-server.sh"]
