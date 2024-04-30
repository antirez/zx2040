cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(my_project)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project

################################################################################
add_executable(zx
    zx.c
)

# Enable USB, disable UART output
pico_enable_stdio_usb(zx 1)
pico_enable_stdio_uart(zx 0)
 
# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(zx pico_stdlib hardware_spi hardware_pwm pico_multicore)
#target_compile_options(zx PRIVATE -Ofast)
target_compile_options(zx PRIVATE -save-temps -fverbose-asm)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(zx)
pico_set_binary_type(zx copy_to_ram)
