#/bin/bash

rm -rf build-docker
mkdir build-docker

cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

docker run --rm -v "$PWD:/root/env" -w /root/env ort-build \
  sh -lc 'cmake -S . -B build-docker && cmake --build build-docker'
