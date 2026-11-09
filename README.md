# ORTos

ORTos is a minimal freestanding x86_64 kernel booted by GRUB 2. It retains the
Multiboot2 boot path, a small early memory allocator, CPU exception handling,
and `print.c` as its VGA 80-by-25 text-mode output interface. It starts without
a shell, GUI, input devices, networking, audio, or storage services.

## Build
```sh
 docker run --rm \
  -v "$PWD:/root/env" \
  -w /root/env \
  ort-build \
  sh -lc 'nasm -v && rm -rf build-docker && cmake -S . -B build-docker && cmake --build build-docker -j2'
```

The bare-metal kernel requires an x86_64-elf C compiler/linker and NASM. The ISO
target additionally requires `grub-mkrescue` and `xorriso`.

The Multiboot2 header does not request a graphics framebuffer, and the GRUB
configuration retains text mode. The kernel initializes VGA text output,
validates the Multiboot2 handoff, initializes memory and exception handling,
then halts while remaining visible on the text console.
