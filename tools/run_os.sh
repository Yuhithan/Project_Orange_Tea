#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ISO="$ROOT/build-docker/OrangeteaOS.iso"
DISK="$ROOT/build-docker/ortos-storage.img"

if [ ! -f "$ISO" ]; then
    printf '%s\n' "Missing $ISO; build the orangetea_iso target first."
    exit 1
fi

if [ ! -e "$DISK" ]; then
    if command -v qemu-img >/dev/null 2>&1; then
        qemu-img create -f raw "$DISK" 64M
    elif command -v docker >/dev/null 2>&1; then
        docker run --rm -v "$ROOT:/root/env" -w /root/env ort-build \
            qemu-img create -f raw build-docker/ortos-storage.img 64M
    else
        printf '%s\n' "QEMU/qemu-img or Docker with the ort-build image is required."
        exit 1
    fi
fi

if command -v qemu-system-x86_64 >/dev/null 2>&1; then
    exec qemu-system-x86_64 -m 256 -cdrom "$ISO" -boot order=d \
        -drive "file=$DISK,format=raw,if=ide,index=0" -display curses \
        -monitor none -no-reboot
fi

if command -v docker >/dev/null 2>&1; then
    exec docker run --rm -it -e TERM="${TERM:-xterm}" \
        -v "$ROOT:/root/env" -w /root/env ort-build \
        qemu-system-x86_64 -m 256 -cdrom /root/env/build-docker/OrangeteaOS.iso \
        -boot order=d -drive file=/root/env/build-docker/ortos-storage.img,format=raw,if=ide,index=0 \
        -display curses -monitor none -no-reboot
fi

printf '%s\n' "QEMU is unavailable. Rebuild ort-build using buildenv/Dockerfile."
exit 1