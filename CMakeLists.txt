cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(ayrenos-rtos C ASM)

add_executable(ayrenos.elf
    src/main.c
    src/kernel/task.c
    src/kernel/scheduler.c
    src/kernel/sync.c
    src/kernel/debug.c
    src/port/cortex_m3/port.c
    src/port/cortex_m3/context.s
    boards/nucleo-f103rb/board.c
    boards/nucleo-f103rb/uart.c
    boards/nucleo-f103rb/startup.s
)

set_target_properties(ayrenos.elf PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)

target_compile_options(ayrenos.elf PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -ffreestanding
    -fno-builtin
    -fno-common
    -Wall
    -Wextra
)

target_link_options(ayrenos.elf PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -nostdlib
    -Wl,--build-id=none
    -Wl,-Map,${CMAKE_BINARY_DIR}/ayrenos.map
    -Wl,-T,${CMAKE_SOURCE_DIR}/boards/nucleo-f103rb/linker.ld
)

target_include_directories(ayrenos.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/nucleo-f103rb
)
