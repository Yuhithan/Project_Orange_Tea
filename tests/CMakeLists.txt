add_library(ortos_storage STATIC "${ORTOS_SOURCE_DIR}/fs/storage/storage.c")
target_include_directories(ortos_storage PUBLIC "${ORTOS_INCLUDE_DIR}")

add_executable(storage_path_test storage_path_test.c)
target_link_libraries(storage_path_test PRIVATE ortos_storage)

add_executable(core_subsystem_test
    core_subsystem_test.c
    "${ORTOS_SOURCE_DIR}/kernel/memory/memory.c"
    "${ORTOS_SOURCE_DIR}/kernel/process/process.c"
    "${ORTOS_SOURCE_DIR}/kernel/syscall/syscall.c")
target_link_libraries(core_subsystem_test PRIVATE ortos_storage)

if(BUILD_TESTING)
    add_test(NAME storage_path_test COMMAND storage_path_test)
    add_test(NAME core_subsystem_test COMMAND core_subsystem_test)
endif()