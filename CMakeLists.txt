cmake_minimum_required(VERSION 3.18)

# silvanus-pico
#
# This file assumes the following:
#   1. Visual Studio Code is your IDE
#   2. Docker and the Dev Containers vscode extension are installed
#   3. pico-sdk is cloned next to this workspace folder (at ../pico-sdk)
#   4. vscode has attached to the container "RPi Pico Build" 
#      (vscode will suggest this after opening this folder)
#
# If you don't like any of these assumptions, feel free to read the pi pico docs 
# and pick apart this file for just the parts you want.

# Set the name of your project
set(PROJECT_NAME "silvanus-pico")

# Choose between "pico" and "picow" for your target board
set(PICO_BOARD "pico_w")

# We assume the pico-sdk is located at /pico-sdk because
# this is where we mount it in the docker
set(PICO_SDK_PATH "/pico-sdk")

# This framework does not use RTTI or C++ exceptions.
# I don't recommend turning them on but you can
set(PICO_CXX_ENABLE_EXCEPTIONS 0)
set(PICO_CXX_ENABLE_RTTI 0)

# Pull in pico SDK (must be before project)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
if (PICO_SDK_VERSION_STRING VERSION_LESS "2.1.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.1.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# Create the project and specify our C and C++ versions
project(${PROJECT_NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# 
add_executable(${PROJECT_NAME}
  Silvanus.cpp
  Settings.cpp
)

# Add pi-pico-cpp and current dir to include directories
target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/deps/pi-pico-cpp/include
)

# Generate all PIO headers
file(GLOB pio_files "deps/pi-pico-cpp/pio/*.pio")
foreach(pio_file ${pio_files})
  pico_generate_pio_header(${PROJECT_NAME} ${pio_file})
endforeach()

target_compile_definitions(${PROJECT_NAME} PUBLIC "LOGGING_ENABLED")
target_compile_definitions(${PROJECT_NAME} PUBLIC "ENABLE_PICO_MULTICORE")

target_link_libraries(${PROJECT_NAME}
        pico_stdio 
        pico_stdlib
        pico_multicore
        hardware_flash
        hardware_rtc
        pico_time
        pico_sync 
        hardware_pio
        pico_cyw43_arch_lwip_poll
)

# Configure USB for stdio (disables uart)
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(${PROJECT_NAME})
