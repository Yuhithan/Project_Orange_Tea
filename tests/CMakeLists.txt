add_library(ortos_storage STATIC "${ORTOS_SOURCE_DIR}/fs/storage/storage.c")
target_include_directories(ortos_storage PUBLIC "${ORTOS_INCLUDE_DIR}")

add_executable(ora_format_test
    ora_format_test.c
    "${ORTOS_SOURCE_DIR}/kernel/exec/ora.c")
target_include_directories(ora_format_test PRIVATE "${ORTOS_INCLUDE_DIR}")

add_executable(ora_pack "${ORTOS_SOURCE_DIR}/../tools/ora_pack.c")
target_include_directories(ora_pack PRIVATE "${ORTOS_INCLUDE_DIR}")

add_library(ortos_update STATIC "${ORTOS_SOURCE_DIR}/../tools/update_lib.c")
target_include_directories(ortos_update PUBLIC "${ORTOS_INCLUDE_DIR}")

add_executable(ortos_update_cli "${ORTOS_SOURCE_DIR}/../tools/ortos_update.c")
target_link_libraries(ortos_update_cli PRIVATE ortos_update)
target_include_directories(ortos_update_cli PRIVATE "${ORTOS_INCLUDE_DIR}")

add_executable(updater_test updater_test.c)
target_link_libraries(updater_test PRIVATE ortos_update)
target_include_directories(updater_test PRIVATE "${ORTOS_INCLUDE_DIR}")

if(ORTOS_ASSEMBLER)
    set(ORA_EXAMPLE_TEXT "${CMAKE_CURRENT_BINARY_DIR}/hello.text.bin")
    set(ORA_EXAMPLE_DATA "${CMAKE_CURRENT_BINARY_DIR}/hello.data.bin")
    set(ORA_EXAMPLE "${CMAKE_CURRENT_BINARY_DIR}/hello.ora")
    add_custom_command(
        OUTPUT "${ORA_EXAMPLE_TEXT}"
        COMMAND "${ORTOS_ASSEMBLER}" -f bin
            "${ORTOS_SOURCE_DIR}/../examples/hello.text.asm"
            -o "${ORA_EXAMPLE_TEXT}"
        DEPENDS "${ORTOS_SOURCE_DIR}/../examples/hello.text.asm"
        VERBATIM)
    add_custom_command(
        OUTPUT "${ORA_EXAMPLE_DATA}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${ORA_EXAMPLE_DATA}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${ORA_EXAMPLE}"
        COMMAND "$<TARGET_FILE:ora_pack>" "${ORA_EXAMPLE_TEXT}"
            "${ORA_EXAMPLE_DATA}" "${ORA_EXAMPLE}"
        DEPENDS ora_pack "${ORA_EXAMPLE_TEXT}" "${ORA_EXAMPLE_DATA}"
        VERBATIM)
    add_custom_target(ora_example ALL DEPENDS "${ORA_EXAMPLE}")
endif()

add_executable(storage_path_test storage_path_test.c)
target_link_libraries(storage_path_test PRIVATE ortos_storage)

add_executable(storage_persistence_test storage_persistence_test.c)
target_link_libraries(storage_persistence_test PRIVATE ortos_storage)

add_executable(terminal_input_buffer_test terminal_input_buffer_test.c)
target_include_directories(terminal_input_buffer_test PRIVATE "${ORTOS_INCLUDE_DIR}" "${ORTOS_SOURCE_DIR}")

add_executable(core_subsystem_test
    core_subsystem_test.c
    "${ORTOS_SOURCE_DIR}/kernel/memory/memory.c"
    "${ORTOS_SOURCE_DIR}/kernel/process/process.c"
    "${ORTOS_SOURCE_DIR}/kernel/syscall/syscall.c")
target_link_libraries(core_subsystem_test PRIVATE ortos_storage)

add_executable(network_stack_test network_stack_test.c
    "${ORTOS_SOURCE_DIR}/net/config/network_stack.c")
target_include_directories(network_stack_test PRIVATE "${ORTOS_INCLUDE_DIR}" "${ORTOS_SOURCE_DIR}")

add_executable(ethernet_test ethernet_test.c "${ORTOS_SOURCE_DIR}/net/ethernet.c")
target_include_directories(ethernet_test PRIVATE "${ORTOS_INCLUDE_DIR}")

add_executable(browser_test browser_test.c
    "${ORTOS_SOURCE_DIR}/apps/browser.c"
    "${ORTOS_SOURCE_DIR}/drivers/display/framebuffer.c"
    "${ORTOS_SOURCE_DIR}/ui/core/ORgui.c")
target_include_directories(browser_test PRIVATE "${ORTOS_INCLUDE_DIR}" "${ORTOS_SOURCE_DIR}")

if(BUILD_TESTING)
    add_test(NAME storage_path_test COMMAND storage_path_test)
    add_test(NAME storage_persistence_test COMMAND storage_persistence_test)
    add_test(NAME ora_format_test COMMAND ora_format_test)
    add_test(NAME updater_test COMMAND updater_test)
    add_test(NAME terminal_input_buffer_test COMMAND terminal_input_buffer_test)
    add_test(NAME core_subsystem_test COMMAND core_subsystem_test)
    add_test(NAME network_stack_test COMMAND network_stack_test)
    add_test(NAME ethernet_test COMMAND ethernet_test)
    add_test(NAME browser_test COMMAND browser_test)
endif()