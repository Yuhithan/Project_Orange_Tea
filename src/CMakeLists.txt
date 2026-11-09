set(ORTOS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/ortos-objects")

add_executable(create_sysroot "${ORTOS_SOURCE_DIR}/../tools/create_sysroot.c")
target_compile_options(create_sysroot PRIVATE -Wall -Wextra)

find_program(ORTOS_C_COMPILER NAMES x86_64-elf-gcc gcc)
find_program(ORTOS_LINKER NAMES x86_64-elf-ld ld)
find_program(ORTOS_ASSEMBLER NAMES nasm)
find_program(GRUB_MKRESCUE NAMES grub-mkrescue)

if(NOT ORTOS_C_COMPILER OR NOT ORTOS_LINKER)
    message(FATAL_ERROR "A C compiler and linker are required to build ORTos.")
endif()

# ---------------------------------------------------------------------------
# Kernel C sources
# ---------------------------------------------------------------------------

file(GLOB_RECURSE ORTOS_KERNEL_C_SOURCES CONFIGURE_DEPENDS
    "${ORTOS_SOURCE_DIR}/arch/*.c"
    "${ORTOS_SOURCE_DIR}/kernel/*.c"
    "${ORTOS_SOURCE_DIR}/drivers/*.c"
    "${ORTOS_SOURCE_DIR}/fs/*.c"
    "${ORTOS_SOURCE_DIR}/net/*.c"
    "${ORTOS_SOURCE_DIR}/ui/core/desktop_storage.c"
    "${ORTOS_SOURCE_DIR}/apps/*.c"
    "${ORTOS_SOURCE_DIR}/security/*.c"
    "${ORTOS_SOURCE_DIR}/lib/*.c"
    "${ORTOS_SOURCE_DIR}/test/*.c"
)

# ---------------------------------------------------------------------------
# Architecture
# ---------------------------------------------------------------------------

set(ORTOS_BOOT_ARCH x86)

if(ORTOS_ARCH STREQUAL "x86")
    set(ORTOS_BOOT_ARCH x86)
endif()

# ---------------------------------------------------------------------------
# Boot assembly
# ---------------------------------------------------------------------------

file(GLOB ORTOS_BOOT_ASM_SOURCES CONFIGURE_DEPENDS
    "${ORTOS_SOURCE_DIR}/arch/${ORTOS_BOOT_ARCH}/boot/*.asm"
)

# ---------------------------------------------------------------------------
# Kernel C objects
# ---------------------------------------------------------------------------

set(ORTOS_KERNEL_OBJECTS)

foreach(source IN LISTS ORTOS_KERNEL_C_SOURCES)

    get_filename_component(name "${source}" NAME_WE)

    string(MD5 source_id "${source}")

    set(object
        "${ORTOS_BUILD_DIR}/${name}_${source_id}.o"
    )

    add_custom_command(
        OUTPUT "${object}"

        COMMAND "${CMAKE_COMMAND}"
            -E make_directory
            "${ORTOS_BUILD_DIR}"

        COMMAND "${ORTOS_C_COMPILER}"
            -c "${source}"
            -o "${object}"

            "-I${ORTOS_INCLUDE_DIR}"

            -ffreestanding
            -fno-pie
            -fno-stack-protector
            -mno-red-zone

            -Wall
            -Wextra

            "-DSTORAGE_CAPACITY_SECTORS=${ORTOS_VIRTUAL_DISK_SECTORS}"

        DEPENDS
            "${source}"

        VERBATIM
    )

    list(APPEND ORTOS_KERNEL_OBJECTS
        "${object}"
    )

endforeach()

# ---------------------------------------------------------------------------
# Boot objects
# ---------------------------------------------------------------------------

set(ORTOS_BOOT_OBJECTS)

if(ORTOS_ASSEMBLER)

    foreach(source IN LISTS ORTOS_BOOT_ASM_SOURCES)

        get_filename_component(name "${source}" NAME_WE)

        set(object
            "${ORTOS_BUILD_DIR}/boot_${name}.o"
        )

        add_custom_command(
            OUTPUT "${object}"

            COMMAND "${CMAKE_COMMAND}"
                -E make_directory
                "${ORTOS_BUILD_DIR}"

            COMMAND "${ORTOS_ASSEMBLER}"
                -f elf64
                "${source}"
                -o "${object}"

            DEPENDS
                "${source}"

            VERBATIM
        )

        list(APPEND ORTOS_BOOT_OBJECTS
            "${object}"
        )

    endforeach()

else()

    message(
        WARNING
        "nasm was not found; bare-metal kernel and ISO targets are unavailable."
    )

endif()

# ---------------------------------------------------------------------------
# Kernel
# ---------------------------------------------------------------------------

if(ORTOS_ASSEMBLER AND ORTOS_BOOT_OBJECTS)

    set(ORTOS_KERNEL_BIN
        "${CMAKE_CURRENT_BINARY_DIR}/../kernel.bin"
    )

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
        ortos_kernel
        ALL
        DEPENDS "${ORTOS_KERNEL_BIN}"
    )

    # -----------------------------------------------------------------------
    # ISO
    # -----------------------------------------------------------------------

    if(GRUB_MKRESCUE)

        set(ORTOS_ISO_ROOT
            "${CMAKE_CURRENT_BINARY_DIR}/iso"
        )

        set(ORTOS_ISO
            "${CMAKE_CURRENT_BINARY_DIR}/../OrangeteaOS.iso"
        )

        # -------------------------------------------------------------------
        # GRUB configuration
        # -------------------------------------------------------------------

        configure_file(
            "${ORTOS_TARGET_DIR}/iso/boot/grub/grub.cfg"
            "${ORTOS_ISO_ROOT}/boot/grub/grub.cfg"
            COPYONLY
        )

        # -------------------------------------------------------------------
        # Create ISO
        # -------------------------------------------------------------------

        add_custom_command(
            OUTPUT "${ORTOS_ISO}"

            # Kernel
            COMMAND "${CMAKE_COMMAND}"
                -E make_directory
                "${ORTOS_ISO_ROOT}/boot"

            COMMAND "${CMAKE_COMMAND}"
                -E copy
                "${ORTOS_KERNEL_BIN}"
                "${ORTOS_ISO_ROOT}/boot/kernel.bin"

            # GRUB ISO
            COMMAND "${GRUB_MKRESCUE}"
                /usr/lib/grub/i386-pc
                -o "${ORTOS_ISO}"
                "${ORTOS_ISO_ROOT}"

            DEPENDS
                ortos_kernel
                "${ORTOS_TARGET_DIR}/iso/boot/grub/grub.cfg"

            VERBATIM
        )

        # -------------------------------------------------------------------
        # ISO target
        # -------------------------------------------------------------------

        add_custom_target(
            orangetea_iso
            ALL
            DEPENDS "${ORTOS_ISO}"
        )

        set(ORTOS_STORAGE_IMAGE
            "${CMAKE_CURRENT_BINARY_DIR}/../ortos-storage.img"
        )
        set(ORTOS_EXAMPLE_APP
            "${CMAKE_CURRENT_BINARY_DIR}/../tests/hello.ora"
        )

        add_custom_target(
            ortos_sysroot_image
            ALL
            COMMAND "$<TARGET_FILE:create_sysroot>"
                "${ORTOS_STORAGE_IMAGE}"
                "${ORTOS_SOURCE_DIR}/sysroot"
                "${ORTOS_KERNEL_BIN}"
                "${ORTOS_TARGET_DIR}/iso/boot/grub/grub.cfg"
                "${ORTOS_EXAMPLE_APP}"
            DEPENDS
                create_sysroot
                ortos_kernel
                "${ORTOS_SOURCE_DIR}/../tools/create_sysroot.c"
                "${ORTOS_TARGET_DIR}/iso/boot/grub/grub.cfg"
            VERBATIM
        )

        # -------------------------------------------------------------------
        # Run OS
        # -------------------------------------------------------------------

        add_custom_target(
            run_os

            COMMAND sh
                "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_os.sh"

            DEPENDS
                orangetea_iso
                ortos_sysroot_image

            WORKING_DIRECTORY
                "${CMAKE_CURRENT_SOURCE_DIR}/.."

            USES_TERMINAL
        )

    else()

        message(
            WARNING
            "grub-mkrescue was not found; only the kernel target is available."
        )

    endif()

endif()