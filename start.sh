#!/bin/sh
set -eu

rm -xf build-docker/OrangeteaOS.iso 
sleep 1
docker run --rm -v "$PWD:/root/env" -w /root/env ort-build sh -lc 'cmake -S . -B build-docker && cmake --build build-docker --clean-first -j6'

qemu-system-x86_64 -m 4G -cdrom build-docker/OrangeteaOS.iso -vga std -d int,cpu_reset,guest_errors -D qemu.log