# ORTos

ORTos is a freestanding x86_64 kernel booted by GRUB 2. Its console uses VGA
80-by-25 text mode; the shell, keyboard input, memory management, networking,
and storage subsystems run directly in the kernel.

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The bare-metal kernel and ISO targets require a C compiler/linker, NASM,
`grub-mkrescue`, and `xorriso`. The `storage_path_test` and related host tests
use the host compiler.

The Multiboot2 header does not request a graphics framebuffer, and the GRUB
configuration retains text mode. The kernel initializes VGA text output before
printing boot diagnostics and enters the interactive shell after initializing
the memory, device, timer, and storage subsystems.
