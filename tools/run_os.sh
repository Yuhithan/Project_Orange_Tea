#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ISO=${1:-"$ROOT/build/OrangeteaOS.iso"}

if [ ! -f "$ISO" ]; then
    printf '%s\n' "Missing $ISO; build the orangetea_iso target first." >&2
    exit 1
fi

if command -v qemu-system-x86_64 >/dev/null 2>&1; then
    exec qemu-system-x86_64 -m 256 -cdrom "$ISO" -boot order=d \
        -display curses -no-reboot
fi

printf '%s\n' "QEMU is unavailable; install qemu-system-x86_64 to run the ISO." >&2
exit 1