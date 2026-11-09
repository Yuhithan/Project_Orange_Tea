# ============================================================================
# ORTOS - Minimal Kernel Build
# ============================================================================

set(ORTOS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/ortos-objects")

# ============================================================================
# Build tools
# ============================================================================

find_program(ORTOS_C_COMPILER NAMES x86_64-elf-gcc gcc)
find_program(ORTOS_LINKER NAMES x86_64-elf-ld ld)
find_program(ORTOS_ASSEMBLER NAMES nasm)
find_program(GRUB_MKRESCUE NAMES grub-mkrescue)

if(NOT ORTOS_C_COMPILER)
    message(FATAL_ERROR "A C compiler is required.")
endif()

if(NOT ORTOS_LINKER)
    message(FATAL_ERROR "A linker is required.")
endif()

if(NOT ORTOS_ASSEMBLER)
    message(FATAL_ERROR "NASM is required.")
endif()

# ============================================================================
# Validate required directories
# ============================================================================

foreach(required_var ORTOS_SOURCE_DIR ORTOS_INCLUDE_DIR ORTOS_TARGET_DIR)
    if(NOT DEFINED ${required_var})
        message(FATAL_ERROR "${required_var} is not defined.")
    endif()
endforeach()

# ============================================================================
# Architecture
# ============================================================================

set(ORTOS_BOOT_ARCH x86_64)

# ============================================================================
# Kernel C sources
# ============================================================================

file(GLOB ORTOS_INTERRUPT_SOURCES CONFIGURE_DEPENDS
    "${ORTOS_SOURCE_DIR}/arch/${ORTOS_BOOT_ARCH}/interrupts/*.c"
)

set(ORTOS_KERNEL_C_SOURCES
    ${ORTOS_INTERRUPT_SOURCES}

    "${ORTOS_SOURCE_DIR}/kernel/core/print.c"
    "${ORTOS_SOURCE_DIR}/kernel/main.c"
    "${ORTOS_SOURCE_DIR}/kernel/memory/memory.c"

    "${ORTOS_SOURCE_DIR}/drivers/port.c"
    "${ORTOS_SOURCE_DIR}/drivers/rtc.c"
    "${ORTOS_SOURCE_DIR}/drivers/pic.c"
    "${ORTOS_SOURCE_DIR}/drivers/idt.c"
    "${ORTOS_SOURCE_DIR}/drivers/ps2.c"
    "${ORTOS_SOURCE_DIR}/drivers/keyboard/keyboard.c"
)

# ============================================================================
# Validate source files
# ============================================================================

foreach(source IN LISTS ORTOS_KERNEL_C_SOURCES)
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "Required source file not found: ${source}")
    endif()
endforeach()

# ============================================================================
# Kernel headers
# ============================================================================

file(GLOB_RECURSE ORTOS_KERNEL_HEADERS CONFIGURE_DEPENDS
    "${ORTOS_INCLUDE_DIR}/core/*.h"
    "${ORTOS_INCLUDE_DIR}/drivers/*.h"
    "${ORTOS_INCLUDE_DIR}/apps/shell/*.h"
    "${ORTOS_INCLUDE_DIR}/${ORTOS_BOOT_ARCH}/*.h"
)

# ============================================================================
# Boot assembly
# ============================================================================

set(ORTOS_BOOT_ASM_SOURCES
    "${ORTOS_SOURCE_DIR}/arch/${ORTOS_BOOT_ARCH}/boot/header.asm"
    "${ORTOS_SOURCE_DIR}/arch/${ORTOS_BOOT_ARCH}/boot/interrupts.asm"
    "${ORTOS_SOURCE_DIR}/arch/${ORTOS_BOOT_ARCH}/boot/main.asm"
    "${ORTOS_SOURCE_DIR}/arch/${ORTOS_BOOT_ARCH}/boot/main64.asm"
    "${ORTOS_SOURCE_DIR}/arch/${ORTOS_BOOT_ARCH}/port.asm"
)

foreach(source IN LISTS ORTOS_BOOT_ASM_SOURCES)
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "Required boot file not found: ${source}")
    endif()
endforeach()

# ============================================================================
# Compiler flags
# ============================================================================

set(ORTOS_C_FLAGS
    -ffreestanding
    -m64
    -fno-pie
    -fno-stack-protector
    -mno-red-zone
    -Wall
    -Wextra
    -Werror
)

# ============================================================================
# Kernel C objects
# ============================================================================

set(ORTOS_KERNEL_OBJECTS)

foreach(source IN LISTS ORTOS_KERNEL_C_SOURCES)

    get_filename_component(name "${source}" NAME_WE)
    string(MD5 source_id "${source}")

    set(object "${ORTOS_BUILD_DIR}/${name}_${source_id}.o")

    add_custom_command(
        OUTPUT "${object}"

        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${ORTOS_BUILD_DIR}"

        COMMAND "${ORTOS_C_COMPILER}"
                ${ORTOS_C_FLAGS}
                -c "${source}"
                -o "${object}"
                "-I${ORTOS_INCLUDE_DIR}"

        DEPENDS
            "${source}"
            ${ORTOS_KERNEL_HEADERS}

        VERBATIM
    )

    list(APPEND ORTOS_KERNEL_OBJECTS "${object}")

endforeach()

# ============================================================================
# Boot objects
# ============================================================================

set(ORTOS_BOOT_OBJECTS)

foreach(source IN LISTS ORTOS_BOOT_ASM_SOURCES)

    get_filename_component(name "${source}" NAME_WE)
    string(MD5 source_id "${source}")

    set(object "${ORTOS_BUILD_DIR}/boot_${name}_${source_id}.o")

    add_custom_command(
        OUTPUT "${object}"

        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${ORTOS_BUILD_DIR}"

        COMMAND "${ORTOS_ASSEMBLER}"
                -f elf64
                "${source}"
                -o "${object}"

        DEPENDS "${source}"

        VERBATIM
    )

    list(APPEND ORTOS_BOOT_OBJECTS "${object}")

endforeach()

# ============================================================================
# Kernel binary
# ============================================================================

set(ORTOS_KERNEL_BIN "${CMAKE_CURRENT_BINARY_DIR}/../kernel.bin")

add_custom_command(
    OUTPUT "${ORTOS_KERNEL_BIN}"

    COMMAND "${ORTOS_LINKER}"
            -n
            -o "${ORTOS_KERNEL_BIN}"
            -T "${ORTOS_TARGET_DIR}/linker.ld"
            ${ORTOS_KERNEL_OBJECTS}
            ${ORTOS_BOOT_OBJECTS}

    DEPENDS
        ${ORTOS_KERNEL_OBJECTS}
        ${ORTOS_BOOT_OBJECTS}
        "${ORTOS_TARGET_DIR}/linker.ld"

    VERBATIM
)

add_custom_target(
    ortos_kernel ALL
    DEPENDS "${ORTOS_KERNEL_BIN}"
)

# ============================================================================
# Bootable ISO
# ============================================================================

if(GRUB_MKRESCUE)

    set(ORTOS_ISO_ROOT "${CMAKE_CURRENT_BINARY_DIR}/iso")
    set(ORTOS_ISO "${CMAKE_CURRENT_BINARY_DIR}/../OrangeteaOS.iso")

    file(MAKE_DIRECTORY "${ORTOS_ISO_ROOT}/boot/grub")

    configure_file(
        "${ORTOS_TARGET_DIR}/iso/boot/grub/grub.cfg"
        "${ORTOS_ISO_ROOT}/boot/grub/grub.cfg"
        COPYONLY
    )

    add_custom_command(
        OUTPUT "${ORTOS_ISO}"

        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${ORTOS_ISO_ROOT}/boot"

        COMMAND "${CMAKE_COMMAND}" -E copy
                "${ORTOS_KERNEL_BIN}"
                "${ORTOS_ISO_ROOT}/boot/kernel.bin"

        COMMAND "${GRUB_MKRESCUE}"
                -o "${ORTOS_ISO}"
                "${ORTOS_ISO_ROOT}"

        DEPENDS
            ortos_kernel
            "${ORTOS_TARGET_DIR}/iso/boot/grub/grub.cfg"

        VERBATIM
    )

    add_custom_target(
        orangetea_iso ALL
        DEPENDS "${ORTOS_ISO}"
    )

    # ========================================================================
    # Run OS
    # ========================================================================

    add_custom_target(
        run_os

        COMMAND sh
                "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_os.sh"
                "${ORTOS_ISO}"

        DEPENDS orangetea_iso

        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."

        USES_TERMINAL
    )

else()

    message(WARNING
        "grub-mkrescue was not found. ISO target is unavailable."
    )

endif()

# ============================================================================
# Build summary
# ============================================================================

message(STATUS "ORTOS minimal kernel configuration")
message(STATUS "Architecture: ${ORTOS_BOOT_ARCH}")
message(STATUS "C compiler: ${ORTOS_C_COMPILER}")
message(STATUS "Assembler: ${ORTOS_ASSEMBLER}")
message(STATUS "Kernel C files: ${ORTOS_KERNEL_C_SOURCES}")
message(STATUS "Shell: excluded")
message(STATUS "GUI: excluded")
message(STATUS "Display: VGA")