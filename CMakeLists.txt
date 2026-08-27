cmake_minimum_required(VERSION 3.20)

project(ORTos LANGUAGES C)

include(CTest)

set(ORTOS_ARCH x86_64 CACHE STRING "ORTos target architecture")
set(ORTOS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(ORTOS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(ORTOS_TARGET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/targets/${ORTOS_ARCH}")

add_subdirectory(src)
add_subdirectory(tests)
