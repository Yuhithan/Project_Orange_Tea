# ORTos source architecture

The source tree is split by ownership rather than by build order:

- `arch/x86` contains boot assembly, interrupt entry points and the PIT.
- `kernel` contains boot mode, console output, memory, processes and syscalls.
- `drivers` contains VGA text output, framebuffer compatibility code, PS/2
  input and the Ethernet device boundary.
- `fs` contains the current bounded storage/VFS implementation.
- `net` contains protocol-layer contracts. Hardware and protocol providers are
  not present yet and return explicit errors.
- `ui/core/desktop_storage.c` initializes persistent directories and legacy
  desktop metadata on the storage volume.
- `apps/shell` contains the shell command interpreter used by both modes.
- `security` contains fail-closed contracts until account, isolation and crypto
  providers are available.

## Persistent C: volume

The shell initializes its persistent VFS volume under `/C:/`. User files live
under `/C:/Users`, installed-application data under `/C:/ProgramData`, and the
reserved `/C:/Program Files/ORTos/Apps` directory is separate from user data.
The current application binaries remain compiled into the boot image; this
directory is available for future installed applications. System configuration,
temporary files, logs and backups have separate top-level directories.
The current VFS limit is 64 entries, so `Temp`, `Logs` and `Backups` are kept
ready for use without preallocating unused subdirectories.

On storage initialization, legacy menu and desktop-task files are copied into their new
locations only when the destination is absent. Original files are retained so
older paths remain readable, and repeated initialization does not replace
existing files.

The kernel writes through the VGA text buffer in 80-by-25 text mode. The
framebuffer implementation remains available but is not part of display
initialization.