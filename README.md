You are the lead OS engineer working directly inside my existing **ORTos** repository.

Read the repository first and understand how it currently works before making changes.

# ORTos

ORTos (Operating Re-systemize Technology) is my **pure bare-metal operating system**, built from the ground up without depending on another operating system.

## Build with CMake

The project is configured from its repository root:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
then

```sh
docker run --rm -v "$PWD:/root/env" -w /root/env ort-build \
  sh -lc 'cmake -S . -B build-docker && cmake --build build-docker'
```

The bare-metal target requires `x86_64-elf-gcc`, `x86_64-elf-ld`, `nasm`,
`grub-mkrescue`, and `xorriso`. The documented Docker environment in
`buildenv/Dockerfile` provides these tools. The host compiler is used for the
`storage_path_test` executable.

Current known features:

* GRUB bootloader support
* Display/framebuffer support
* Keyboard input
* Basic shell V1
* Modular architecture
* Bare-metal execution

I want you to implement the **first real graphical desktop environment for ORTos**.

Call the GUI framework:

# ORgui

The result must be a real framebuffer-rendered GUI that is compiled into ORTos and runs on the actual OS.

This is NOT a Linux distro.

Do NOT use:

* GTK
* Qt
* SDL
* X11
* Wayland
* Win32
* libc GUI APIs
* host OS APIs
* fake terminal graphics
* screenshots
* simulated GUI behavior

Everything must run directly on the ORTos framebuffer and existing hardware/input infrastructure.

---

# STEP 1 — INSPECT THE ENTIRE PROJECT

Before writing code, inspect:

* repository structure
* architecture (x86/x86_64/etc.)
* GRUB configuration
* linker script
* kernel entry point
* framebuffer/display implementation
* keyboard driver
* mouse support, if any
* shell implementation
* shell command registration
* memory allocator
* timer/RTC support
* font/text rendering
* Makefile/build scripts
* existing C headers and coding conventions

Search the repository rather than guessing function names.

Reuse existing implementations wherever possible.

Do NOT replace working framebuffer or keyboard code just because you are adding ORgui.

If an existing component can be extended cleanly, extend it.

---

# STEP 2 — DESIGN THE GUI ARCHITECTURE

Create a lightweight layered architecture:

```text
ORTos Shell
     |
     | gui command
     v
GUI Mode
     |
     v
Desktop
     |
     +----------------------+
     |                      |
  Taskbar                 ORgui
                            |
                  +---------+---------+
                  |                   |
               Window             First App
                  |
             Framebuffer
                  |
             Display Hardware
```

The important rule is:

**Desktop and applications use ORgui. ORgui uses the framebuffer.**

Avoid having every application directly manipulate framebuffer memory.

---

# STEP 3 — FRAMEBUFFER ABSTRACTION

Create or adapt:

```text
framebuffer.h
framebuffer.c
```

Use the project's existing framebuffer implementation if one already exists.

Do not create duplicate framebuffer initialization.

Provide a clean API similar to:

```c
void fb_init(void);
void fb_clear(uint32_t color);
void fb_put_pixel(int x, int y, uint32_t color);
void fb_fill_rect(int x, int y, int width, int height, uint32_t color);
void fb_draw_rect(int x, int y, int width, int height, uint32_t color);
void fb_draw_line(...);
void fb_draw_char(...);
void fb_draw_string(...);
```

Adapt the API to the actual ORTos framebuffer.

The framebuffer abstraction must correctly account for:

* address
* width
* height
* pitch
* bytes per pixel
* pixel format

Do NOT hardcode 1024x768.

Use the actual framebuffer dimensions supplied by GRUB/ORTos.

Prevent out-of-bounds writes.

---

# STEP 4 — ORgui LIBRARY

Create:

```text
ORgui.h
ORgui.c
```

ORgui is the reusable GUI framework for ORTos.

It should provide primitives for:

* initialization
* colors/theme
* rectangles
* panels
* text
* windows
* buttons
* events
* mouse cursor
* keyboard input
* window management
* z-order
* active window
* basic controls

Design clean C APIs.

A possible architecture:

```c
typedef struct {
    int x;
    int y;
    int width;
    int height;

    uint32_t background;
    uint32_t border;
    uint32_t titlebar;

    const char *title;

    int visible;
    int movable;
} ORWindow;
```

and:

```c
void ORgui_init(void);

void ORgui_begin_frame(void);
void ORgui_end_frame(void);

ORWindow *ORgui_create_window(...);
void ORgui_destroy_window(ORWindow *window);

void ORgui_draw_window(ORWindow *window);

void ORgui_draw_button(...);
void ORgui_draw_panel(...);
void ORgui_draw_text(...);

void ORgui_handle_event(...);
void ORgui_update(void);
void ORgui_draw(void);
```

You are free to improve this API after inspecting the existing ORTos architecture.

Keep it lightweight.

Prefer fixed-size arrays over complicated dynamic memory management if ORTos does not yet have a mature allocator.

---

# STEP 5 — ORgui EVENT SYSTEM

Create a simple event abstraction.

For example:

```c
typedef enum {
    OR_EVENT_NONE,
    OR_EVENT_KEY_DOWN,
    OR_EVENT_KEY_UP,
    OR_EVENT_MOUSE_MOVE,
    OR_EVENT_MOUSE_DOWN,
    OR_EVENT_MOUSE_UP
} OREventType;
```

with an event structure containing relevant information.

Integrate this with the existing ORTos keyboard driver.

If mouse support already exists, integrate it too.

If ORTos does not currently have a mouse driver:

* do NOT invent fake mouse hardware
* make ORgui's event architecture mouse-ready
* keep the GUI functional with keyboard input
* clearly document that mouse support is pending

---

# STEP 6 — WILDFIRE ORTOS THEME

Give ORgui a distinctive **wildfire-inspired visual theme**.

The desktop should feel like an operating system UI rather than a debugging screen.

Use a palette based around:

```text
very dark background
deep red
dark crimson
orange
bright orange
warm yellow
light text
dark panels
```

Create named constants such as:

```c
OR_COLOR_BACKGROUND
OR_COLOR_PANEL
OR_COLOR_WINDOW
OR_COLOR_BORDER
OR_COLOR_TITLEBAR
OR_COLOR_FIRE_RED
OR_COLOR_FIRE_ORANGE
OR_COLOR_FIRE_YELLOW
OR_COLOR_TEXT
OR_COLOR_TEXT_DIM
```

Keep the design performant.

Do not require alpha blending or GPU acceleration.

Software framebuffer rendering is sufficient.

---

# STEP 7 — DESKTOP

Create:

```text
desktop.c
```

The desktop should:

* fill the screen with the ORTos wildfire theme
* display ORTos branding
* display a taskbar
* manage ORgui windows
* launch the first application
* process events
* redraw when needed

Create functions similar to:

```c
void desktop_init(void);
void desktop_run(void);
void desktop_draw(void);
```

Adapt them to the actual project architecture.

The desktop should use ORgui instead of manually implementing all UI elements.

---

# STEP 8 — TASKBAR

Create:

```text
taskbar.c
```

The taskbar should sit at the bottom of the framebuffer.

Design it as an actual OS taskbar.

Include:

### Left

An ORTos launcher/menu button.

### Center

Buttons representing open applications/windows.

### Right

A status area.

If ORTos already has a timer/RTC, show a simple clock.

If not, display another useful status indicator instead.

The taskbar should be implemented through ORgui.

---

# STEP 9 — FIRST GRAPHICAL APPLICATION

Create:

```text
first_app.c
```

This is the first real ORTos GUI application.

Give it an ORgui window containing:

* title bar
* close button
* application content area
* ORTos information
* a button/control
* visible text

It should demonstrate that ORgui is actually capable of hosting applications.

Suggested title:

```text
Welcome to ORTos
```

Content can include:

```text
ORTos Graphical Environment

Powered by ORgui
Bare-metal desktop environment
```

Add at least one interactive element.

For example:

```text
[ Click Me ]
```

which changes some visible state/text when activated.

The application should have functions similar to:

```c
void first_app_init(void);
void first_app_update(void);
void first_app_draw(void);
void first_app_handle_event(...);
```

Again, adapt to the architecture you actually find.

---

# STEP 10 — WINDOW MANAGEMENT

ORgui should support a basic window manager.

Implement:

* window creation
* window destruction
* visibility
* active window
* z-order
* moving windows
* close button
* drawing windows
* taskbar representation

Do not build a huge desktop compositor.

A simple software-rendered window system is enough.

For example:

```text
+--------------------------------------+
| Welcome to ORTos                 [X] |
+--------------------------------------+
|                                      |
|        Welcome to ORTos!             |
|                                      |
|        [ Click Me ]                  |
|                                      |
+--------------------------------------+
```

The title bar should visually distinguish the active window.

---

# STEP 11 — GUI COMMAND

Find the existing ORTos shell.

Add:

```text
gui
```

to the existing command system.

When the user enters:

```text
> gui
```

the system should transition from the shell into GUI mode.

Prefer:

```text
shell
  ↓
gui command
  ↓
desktop_init()
  ↓
desktop_run()
```

Do NOT reboot the physical computer unless ORTos's architecture genuinely requires rebooting to initialize graphics.

The user's experience should be:

```text
ORTos shell
> gui
```

and then the framebuffer becomes the graphical ORTos desktop.

If a true reboot is required by the current architecture, inspect the boot process and implement the appropriate transition rather than inventing an unsafe reboot mechanism.

---

# STEP 12 — GUI MODE / SHELL MODE

Keep the architecture modular enough that the shell and GUI are separate modes.

Conceptually:

```c
typedef enum {
    ORTOS_MODE_SHELL,
    ORTOS_MODE_GUI
} ORTOSMode;
```

Do not introduce this exact enum if the existing architecture has a better solution.

The important part is that GUI mode has a clear entry point and lifecycle.

---

# STEP 13 — BUILD SYSTEM

Update the actual ORTos build system.

Ensure these are compiled and linked:

```text
framebuffer.c
ORgui.c
desktop.c
taskbar.c
first_app.c
```

Make sure the required headers are included correctly.

Check:

* Makefile
* linker script
* object files
* source lists
* include paths
* compiler flags

Do not just create files without linking them.

---

# STEP 14 — FONT/TEXT RENDERING

Inspect whether ORTos already has a font renderer.

If it does:

**reuse it.**

If it does not:

implement a tiny built-in bitmap font suitable for the GUI.

Do not add a huge external font library.

The GUI only needs enough text rendering to make the desktop and first application usable.

---

# STEP 15 — PERFORMANCE

Remember this is a bare-metal OS.

Avoid:

* unnecessary full-screen redraws when possible
* huge allocations
* complex data structures
* expensive graphics effects
* external libraries

Start with full framebuffer redraws if necessary for simplicity, but structure ORgui so dirty-region rendering could be added later.

---

# STEP 16 — ERROR HANDLING

Do not allow GUI code to crash because of:

* invalid window coordinates
* framebuffer boundaries
* NULL pointers
* missing framebuffer
* unsupported pixel format
* missing mouse driver
* invalid events

Use the project's existing logging/panic conventions where appropriate.

---

# STEP 17 — TEST EVERYTHING

After implementation:

1. Build ORTos.
2. Fix compiler errors.
3. Fix linker errors.
4. Boot it through GRUB.
5. Confirm the existing shell still works.
6. Run:

```text
gui
```

7. Confirm the desktop appears.
8. Confirm the taskbar appears.
9. Confirm the first application appears.
10. Confirm text renders.
11. Confirm the close button works.
12. Confirm keyboard input works where supported.
13. Confirm mouse input works if the driver exists.
14. Confirm window movement works if mouse support exists.
15. Confirm returning/restarting the shell is handled safely if supported.

Do not claim that something works unless the code/build actually supports it.

---

# IMPORTANT DEVELOPMENT RULE

Do not blindly follow my suggested filenames if the repository already has a better architecture.

The required conceptual modules are:

```text
Framebuffer
ORgui
Desktop
Taskbar
First Application
Shell → GUI transition
```

If the repository structure requires different directories, use the project's conventions.

However, keep these logical source files where practical:

```text
framebuffer.h
framebuffer.c

ORgui.h
ORgui.c

desktop.c
taskbar.c
first_app.c
```

---

# FINAL RESULT

I want ORTos to boot into its normal shell as it does currently.

Then:

```text
> gui
```

should transition into a real graphical environment:

```text
┌──────────────────────────────────────────────────────┐
│                                                      │
│                    ORTos Desktop                     │
│                                                      │
│       ┌───────────────────────────────────┐          │
│       │ Welcome to ORTos              [X] │          │
│       ├───────────────────────────────────┤          │
│       │                                   │          │
│       │       ORTos Graphical             │          │
│       │       Environment                 │          │
│       │                                   │          │
│       │          [ Click Me ]             │          │
│       │                                   │          │
│       └───────────────────────────────────┘          │
│                                                      │
├──────────────────────────────────────────────────────┤
│ 🔥 ORTos │ Welcome to ORTos              │  STATUS   │
└──────────────────────────────────────────────────────┘
```

The exact appearance can be better than this example.

The important requirement is that this is **actually rendered by ORTos on its framebuffer and linked into the bare-metal OS**.

Make the implementation clean enough that ORgui can eventually become the foundation for multiple ORTos applications.

Before finishing, inspect your changes for architectural problems and clean up anything that was unnecessarily duplicated or hardcoded.

Finally report:

* files created
* files modified
* GUI architecture
* shell → GUI transition
* framebuffer implementation
* ORgui API
* desktop features
* taskbar features
* first application
* input support
* build command
* known limitations

and please continue to give support or code advice for making an better OS