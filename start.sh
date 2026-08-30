#!/bin/bash
set -eu

echo "[1/3] Configuring host build..."
cmake -S . -B build

echo "[2/3] Building host project..."
cmake --build build --parallel

echo "[3/3] Running host tests..."
ctest --test-dir build --output-on-failure

if command -v docker >/dev/null 2>&1; then
    echo "Docker is available; building the OrangeTea ISO target."
    mkdir -p build-docker
    docker run --rm -v "$PWD:/root/env" -w /root/env ort-build \
      sh -lc 'cmake -S . -B build-docker && cmake --build build-docker --target orangetea_iso && cp -f build-docker/OrangeteaOS.iso /root/env/OrangeTeaOS.iso'
else
    echo "Docker not found; skipping dockerized build. Host build already succeeded."
fi
