#!/bin/sh
set -eu

if command -v docker >/dev/null 2>&1; then
    docker run --rm -v "$PWD:/root/env" -w /root/env ort-build \
        sh -lc 'cmake -S . -B build-docker && cmake --build build-docker --parallel'
else
    cmake -S . -B build
    cmake --build build --parallel
fi
