# Include layout

Public headers are grouped by subsystem:

- `apps`: shell, ORA loader, and kernel test APIs
- `core`: kernel, memory, process, IRQ, timer, and syscall APIs
- `drivers`: VGA text output, keyboard, storage, and device interfaces
- `fs`: filesystem and VFS APIs
- `net`: networking and Ethernet interfaces
- `security`: security and policy APIs

Top-level compatibility headers remain for existing kernel and shell callers.
