# ORTos source architecture

The source tree is split by ownership rather than by build order:

- `arch/x86` contains boot assembly, interrupt entry points and the PIT.
- `kernel` contains boot mode, console output, memory, processes and syscalls.
- `drivers` contains framebuffer, PS/2 input and the Ethernet device boundary.
- `fs` contains the current bounded storage/VFS implementation.
- `net` contains protocol-layer contracts. Hardware and protocol providers are
  not present yet and return explicit errors.
- `ui` contains ORgui, desktop/taskbar rendering and the terminal window.
- `apps/shell` contains the shell command interpreter used by both modes.
- `security` contains fail-closed contracts until account, isolation and crypto
  providers are available.

All current kernel sources remain C and are compiled freestanding. There is no
C++ source yet because the existing framebuffer UI does not require a C++
runtime or application framework.