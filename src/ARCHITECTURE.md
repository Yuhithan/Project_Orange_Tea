# Minimal kernel architecture

The kernel retains the existing Multiboot2 entry and x86_64 transition under
`arch/x86/boot`. It initializes the early page allocator, installs CPU exception
handlers, and prints through `kernel/core/print.c` to VGA text memory. Hardware
interrupts remain disabled; no timer, input, storage, network, audio, GUI, or
application subsystem is initialized.