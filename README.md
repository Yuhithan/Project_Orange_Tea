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

## NTFS assembly driver

`src/arch/x86_64/fs/ntfs.asm` provides a read-only primary-master ATA PIO
driver and NTFS volume/file helpers. Its exported entry points are
`ntfs_mount`, `ntfs_read_sectors`, `ntfs_find_root_file`, and `ntfs_read_file`.
Mounting accepts either a direct NTFS volume or an MBR partition of type `0x07`.
File lookup supports ASCII names in the root directory; reads support resident
and uncompressed, non-resident unnamed data streams. The driver does not modify
disks, traverse subdirectories, or handle compressed/encrypted streams. A disk
must be attached as the primary IDE master; the default `start.sh` QEMU command
does not attach one.
