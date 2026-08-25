cmake_minimum_required(VERSION 3.20)

project(ORTos NONE)

include(CTest)

set(ORTOS_ARCH x86_64 CACHE STRING "ORTos target architecture")
set(ORTOS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/ortos-objects")
set(ORTOS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(ORTOS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(ORTOS_TARGET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/targets/${ORTOS_ARCH}")

find_program(ORTOS_C_COMPILER NAMES x86_64-elf-gcc gcc)
find_program(ORTOS_LINKER NAMES x86_64-elf-ld ld)
find_program(ORTOS_ASSEMBLER NAMES nasm)
find_program(GRUB_MKRESCUE NAMES grub-mkrescue)

if(NOT ORTOS_C_COMPILER)
    message(FATAL_ERROR "No C compiler found. Install x86_64-elf-gcc or gcc.")
endif()

if(NOT ORTOS_LINKER)
    message(FATAL_ERROR "No linker found. Install x86_64-elf-ld or ld.")
endif()

set(ORTOS_KERNEL_C_SOURCES
    "${ORTOS_SOURCE_DIR}/kernel/main.c"
    "${ORTOS_SOURCE_DIR}/x86_64/boot_mode.c"
    "${ORTOS_SOURCE_DIR}/x86_64/framebuffer.c"
    "${ORTOS_SOURCE_DIR}/x86_64/irq.c"
    "${ORTOS_SOURCE_DIR}/x86_64/keyboard.c"
    "${ORTOS_SOURCE_DIR}/x86_64/network.c"
    "${ORTOS_SOURCE_DIR}/x86_64/print.c"
    "${ORTOS_SOURCE_DIR}/x86_64/shell.c"
    "${ORTOS_SOURCE_DIR}/x86_64/storage.c"
    "${ORTOS_SOURCE_DIR}/x86_64/test.c"
    "${ORTOS_SOURCE_DIR}/x86_64/timer.c"
)

set(ORTOS_BOOT_ASM_SOURCES
    "${ORTOS_SOURCE_DIR}/x86_64/boot/header.asm"
    "${ORTOS_SOURCE_DIR}/x86_64/boot/main.asm"
    "${ORTOS_SOURCE_DIR}/x86_64/boot/main64.asm"
)

set(ORTOS_KERNEL_OBJECTS)
foreach(source IN LISTS ORTOS_KERNEL_C_SOURCES)
    get_filename_component(name "${source}" NAME_WE)
    set(object "${ORTOS_BUILD_DIR}/${name}.o")
    add_custom_command(
        OUTPUT "${object}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ORTOS_BUILD_DIR}"
        COMMAND "${ORTOS_C_COMPILER}" -c "${source}" -o "${object}"
            -I"${ORTOS_INCLUDE_DIR}" -ffreestanding -fno-pie -fno-stack-protector
        DEPENDS "${source}"
        VERBATIM
    )
    list(APPEND ORTOS_KERNEL_OBJECTS "${object}")
endforeach()

set(ORTOS_BOOT_OBJECTS)
if(ORTOS_ASSEMBLER)
    foreach(source IN LISTS ORTOS_BOOT_ASM_SOURCES)
        get_filename_component(name "${source}" NAME_WE)
        set(object "${ORTOS_BUILD_DIR}/${name}.o")
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
    message(WARNING "nasm was not found; the kernel and ISO targets will be unavailable.")
endif()

if(ORTOS_ASSEMBLER AND ORTOS_BOOT_OBJECTS)
    set(ORTOS_KERNEL_BIN "${CMAKE_CURRENT_BINARY_DIR}/kernel.bin")
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
        set(ORTOS_ISO "${CMAKE_CURRENT_BINARY_DIR}/OrangeTeaOS.iso")
        configure_file(
            "${ORTOS_TARGET_DIR}/iso/boot/grub/grub.cfg"
            "${ORTOS_ISO_ROOT}/boot/grub/grub.cfg"
            COPYONLY
        )
        add_custom_command(
            OUTPUT "${ORTOS_ISO}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${ORTOS_KERNEL_BIN}"
                "${ORTOS_ISO_ROOT}/boot/kernel.bin"
            COMMAND "${GRUB_MKRESCUE}" /usr/lib/grub/i386-pc
                -o "${ORTOS_ISO}" "${ORTOS_ISO_ROOT}"
            DEPENDS ortos_kernel "${ORTOS_TARGET_DIR}/iso/boot/grub/grub.cfg"
            VERBATIM
        )
        add_custom_target(ortos_iso ALL DEPENDS "${ORTOS_ISO}")
    else()
        message(WARNING "grub-mkrescue was not found; only the kernel target is available.")
    endif()
endif()

enable_language(C)
add_library(ortos_storage STATIC "${ORTOS_SOURCE_DIR}/x86_64/storage.c")
target_include_directories(ortos_storage PUBLIC "${ORTOS_INCLUDE_DIR}")

add_executable(storage_path_test tests/storage_path_test.c)
target_link_libraries(storage_path_test PRIVATE ortos_storage)

if(BUILD_TESTING)
    add_test(NAME storage_path_test COMMAND storage_path_test)
endif()