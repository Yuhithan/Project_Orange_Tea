set(ORTOS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/ortos-objects")

find_program(ORTOS_C_COMPILER NAMES x86_64-elf-gcc gcc)
find_program(ORTOS_LINKER NAMES x86_64-elf-ld ld)
find_program(ORTOS_ASSEMBLER NAMES nasm)
find_program(GRUB_MKRESCUE NAMES grub-mkrescue)

if(NOT ORTOS_C_COMPILER OR NOT ORTOS_LINKER)
    message(FATAL_ERROR "A C compiler and linker are required to build ORTos.")
endif()

file(GLOB_RECURSE ORTOS_KERNEL_C_SOURCES CONFIGURE_DEPENDS
    "${ORTOS_SOURCE_DIR}/arch/*.c"
    "${ORTOS_SOURCE_DIR}/kernel/*.c"
    "${ORTOS_SOURCE_DIR}/drivers/*.c"
    "${ORTOS_SOURCE_DIR}/fs/*.c"
    "${ORTOS_SOURCE_DIR}/net/*.c"
    "${ORTOS_SOURCE_DIR}/ui/*.c"
    "${ORTOS_SOURCE_DIR}/apps/*.c"
    "${ORTOS_SOURCE_DIR}/security/*.c"
    "${ORTOS_SOURCE_DIR}/lib/*.c"
    "${ORTOS_SOURCE_DIR}/test/*.c"
)

set(ORTOS_CURSOR_RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/cursor_resource.c")
add_custom_command(
    OUTPUT "${ORTOS_CURSOR_RESOURCE}"
    COMMAND "${CMAKE_COMMAND}"
        -DINPUT=${ORTOS_SOURCE_DIR}/sysroot/cursor/cursor_arrow.cur
        -DOUTPUT=${ORTOS_CURSOR_RESOURCE}
        -P "${ORTOS_SOURCE_DIR}/../cmake/embed_binary.cmake"
    DEPENDS "${ORTOS_SOURCE_DIR}/sysroot/cursor/cursor_arrow.cur"
        "${ORTOS_SOURCE_DIR}/../cmake/embed_binary.cmake"
    VERBATIM
)
list(APPEND ORTOS_KERNEL_C_SOURCES "${ORTOS_CURSOR_RESOURCE}")
set(ORTOS_BOOT_ARCH x86)
if(ORTOS_ARCH STREQUAL "x86")
    set(ORTOS_BOOT_ARCH x86)
endif()
file(GLOB ORTOS_BOOT_ASM_SOURCES CONFIGURE_DEPENDS
    "${ORTOS_SOURCE_DIR}/arch/${ORTOS_BOOT_ARCH}/boot/*.asm")

set(ORTOS_KERNEL_OBJECTS)
foreach(source IN LISTS ORTOS_KERNEL_C_SOURCES)
    get_filename_component(name "${source}" NAME_WE)
    string(MD5 source_id "${source}")
    set(object "${ORTOS_BUILD_DIR}/${name}_${source_id}.o")
    add_custom_command(
        OUTPUT "${object}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ORTOS_BUILD_DIR}"
        COMMAND "${ORTOS_C_COMPILER}" -c "${source}" -o "${object}"
            -I${ORTOS_INCLUDE_DIR} -ffreestanding -fno-pie -fno-stack-protector
            -mno-red-zone -Wall -Wextra
        DEPENDS "${source}"
        VERBATIM
    )
    list(APPEND ORTOS_KERNEL_OBJECTS "${object}")
endforeach()

set(ORTOS_BOOT_OBJECTS)
if(ORTOS_ASSEMBLER)
    foreach(source IN LISTS ORTOS_BOOT_ASM_SOURCES)
        get_filename_component(name "${source}" NAME_WE)
        set(object "${ORTOS_BUILD_DIR}/boot_${name}.o")
        add_custom_command(
            OUTPUT "${object}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${ORTOS_BUILD_DIR}"
            COMMAND "${ORTOS_ASSEMBLER}" -f elf64 "${source}" -o "${object}"
            DEPENDS "${source}"
            VERBATIM
        )
        list(APPEND ORTOS_BOOT_OBJECTS "${object}")
    endforeach()
else()
    message(WARNING "nasm was not found; bare-metal kernel and ISO targets are unavailable.")
endif()

if(ORTOS_ASSEMBLER AND ORTOS_BOOT_OBJECTS)
    set(ORTOS_KERNEL_BIN "${CMAKE_CURRENT_BINARY_DIR}/../kernel.bin")
    add_custom_command(
        OUTPUT "${ORTOS_KERNEL_BIN}"
        COMMAND "${ORTOS_LINKER}" -n -o "${ORTOS_KERNEL_BIN}"
            -T "${ORTOS_TARGET_DIR}/linker.ld"
            ${ORTOS_KERNEL_OBJECTS} ${ORTOS_BOOT_OBJECTS}
        DEPENDS ${ORTOS_KERNEL_OBJECTS} ${ORTOS_BOOT_OBJECTS}
            "${ORTOS_TARGET_DIR}/linker.ld"
        VERBATIM
    )
    add_custom_target(ortos_kernel ALL DEPENDS "${ORTOS_KERNEL_BIN}")

    if(GRUB_MKRESCUE)
        set(ORTOS_ISO_ROOT "${CMAKE_CURRENT_BINARY_DIR}/iso")
        set(ORTOS_ISO "${CMAKE_CURRENT_BINARY_DIR}/../OrangeteaOS.iso")
        configure_file("${ORTOS_TARGET_DIR}/iso/boot/grub/grub.cfg"
            "${ORTOS_ISO_ROOT}/boot/grub/grub.cfg" COPYONLY)
        add_custom_command(
            OUTPUT "${ORTOS_ISO}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${ORTOS_KERNEL_BIN}"
                "${ORTOS_ISO_ROOT}/boot/kernel.bin"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${ORTOS_ISO_ROOT}/cursor"
            COMMAND "${CMAKE_COMMAND}" -E copy
                "${ORTOS_SOURCE_DIR}/sysroot/cursor/cursor_arrow.cur"
                "${ORTOS_ISO_ROOT}/cursor/cursor_arrow.cur"
            COMMAND "${GRUB_MKRESCUE}" /usr/lib/grub/i386-pc
                -o "${ORTOS_ISO}" "${ORTOS_ISO_ROOT}"
            DEPENDS ortos_kernel "${ORTOS_TARGET_DIR}/iso/boot/grub/grub.cfg"
                "${ORTOS_SOURCE_DIR}/sysroot/cursor/cursor_arrow.cur"
            VERBATIM)
        add_custom_target(orangetea_iso ALL DEPENDS "${ORTOS_ISO}")
    else()
        message(WARNING "grub-mkrescue was not found; only the kernel target is available.")
    endif()
endif()