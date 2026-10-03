# Include layout

This directory is organized by subsystem to make the ORTos public API easier to navigate.

- apps: shell, browser, ORA loader, and test entry points
- core: kernel, memory, process, IRQ, timer, and syscall APIs
- drivers: hardware-facing interfaces for storage, keyboard, I/O, and device access
- fs: filesystem and VFS APIs
- net: networking APIs and Ethernet driver interfaces
- security: security and policy headers
- ui: GUI, desktop, framebuffer, and terminal APIs

Compatibility shim headers remain in the top-level include directory so existing source files keep building without churn while the implementation now lives under the subsystem folders.
