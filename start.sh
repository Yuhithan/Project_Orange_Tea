
#!/bin/sh
set -eu

OSrm() {
    rm -f build-docker/OrangeteaOS.iso
    sleep 1
}

OSbuild() {
    docker run --rm -v "$PWD:/root/env" -w /root/env ort-build sh -lc 'cmake -S . -B build-docker && cmake --build build-docker --clean-first -j6'
}

OSrun() {
    qemu-system-x86_64 -m 4G -cdrom build-docker/OrangeteaOS.iso -vga std -d int,cpu_reset,guest_errors -D qemu.log
}

# Usage: ./orangetea.sh OSrm
#        ./orangetea.sh OSbuild
#        ./orangetea.sh OSrun

case "${1:-}" in
    OSrm)   OSrm ;;
    OSbuild) OSbuild ;;
    OSrun)  OSrun ;;
    *)
        echo "Usage: $0 {OSrm|OSbuild|OSrun}"
        exit 1
        ;;
esac
