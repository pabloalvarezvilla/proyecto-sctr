cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(proyecto_SCTR C CXX ASM)

pico_sdk_init()

add_executable(proyecto_SCTR main.c)

# Habilitar salida por USB
pico_enable_stdio_usb(proyecto_SCTR 1)
pico_enable_stdio_uart(proyecto_SCTR 0)

# Librerías necesarias
target_link_libraries(proyecto_SCTR pico_stdlib hardware_gpio hardware_timer)

pico_add_extra_outputs(proyecto_SCTR)