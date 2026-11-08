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
    CREATOR="$ROOT/build-docker/src/create_sysroot"
    if [ ! -x "$CREATOR" ]; then
        printf '%s\n' "Missing $CREATOR; build the ortos_sysroot_image target first."
        exit 1
    fi
    "$CREATOR" "$DISK" "$ROOT/src/sysroot" "$ROOT/build-docker/kernel.bin" \
        "$ROOT/targets/x86_64/iso/boot/grub/grub.cfg" \
        "$ROOT/build-docker/tests/hello.ora"
fi

if command -v qemu-system-x86_64 >/dev/null 2>&1; then
    # PulseAudio provides live host playback; QEMU's wav backend captures to a file.
    exec qemu-system-x86_64 -m 256 -cdrom "$ISO" -boot order=d \
        -drive "file=$DISK,format=raw,if=ide,index=0" -display curses \
        -audiodev driver=pa,id=audio0 -machine pcspk-audiodev=audio0 \
        -monitor none -no-reboot
fi

if command -v docker >/dev/null 2>&1; then
    exec docker run --rm -it -e TERM="${TERM:-xterm}" \
        -v "$ROOT:/root/env" -w /root/env ort-build \
        qemu-system-x86_64 -m 256 -cdrom /root/env/build-docker/OrangeteaOS.iso \
        -boot order=d -drive file=/root/env/build-docker/ortos-storage.img,format=raw,if=ide,index=0 \
        -display curses -audiodev driver=pa,id=audio0 -machine pcspk-audiodev=audio0 \
        -monitor none -no-reboot
fi

printf '%s\n' "QEMU is unavailable. Rebuild ort-build using buildenv/Dockerfile."
exit 1