# 1. Set minimum version and project name
cmake_minimum_required(VERSION 3.10)
project(aer_bms LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 2. Define the executable target
# We will add sources to it later
add_executable(aer_bms.elf "")

# 3. Define MCU-specific compiler flags
# These are the core flags from your Makefile's "MCU" variable
set(MCU_FLAGS
    "-mcpu=cortex-m4"
    "-mthumb"
    "-mfpu=fpv4-sp-d16"
    "-mfloat-abi=hard"
)
# Add these flags to both C and ASM compilers
target_compile_options(aer_bms.elf PRIVATE ${MCU_FLAGS})


# 4. Set global compiler flags (for all build types)
# -Wall comes from your CFLAGS
# -fdata-sections and -ffunction-sections are crucial for embedded optimization
target_compile_options(aer_bms.elf PRIVATE
    -Wall
    -fdata-sections
    -ffunction-sections
)

# 5. Set build-type specific flags
# This replaces your DEBUG=1 and OPT=-Og
# In CMake, you run: cmake -DCMAKE_BUILD_TYPE=Debug ..
target_compile_options(aer_bms.elf PRIVATE
    $<$<CONFIG:Debug>:-Og -g -gdwarf-2>
    $<$<CONFIG:Release>:-O2> # You can define release flags, e.g., -O2 or -Os
)

# 6. Add compile definitions (from C_DEFS)
target_compile_definitions(aer_bms.elf PRIVATE
    USE_HAL_DRIVER
    STM32F303x8
)

# 7. Add include directories (from C_INCLUDES)
# ${CMAKE_SOURCE_DIR} is the path to this CMakeLists.txt
target_include_directories(aer_bms.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F3xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F3xx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F3xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)

# 8. Add all your source files
target_sources(aer_bms.elf PRIVATE
    # C Sources
    Core/Src/main.c
    Core/Src/gpio.c
    Core/Src/can.c
    Core/Src/usart.c
    Core/Src/stm32f3xx_it.c
    Core/Src/stm32f3xx_hal_msp.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_can.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_rcc.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_rcc_ex.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_gpio.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_dma.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_cortex.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_pwr.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_pwr_ex.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_flash.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_flash_ex.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_i2c.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_i2c_ex.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_exti.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_uart.c
    Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_uart_ex.c
    Core/Src/system_stm32f3xx.c
    Core/Src/sysmem.c
    Core/Src/syscalls.c
	#	Core/Src/BQ_Comm.c
	#Core/Src/Packet.c

    # ASM Sources
    startup_stm32f303x8.s
)

# 9. Set Linker Flags and Script
# This combines all your LDFLAGS
target_link_options(aer_bms.elf PRIVATE
    ${MCU_FLAGS} # MCU flags are needed for linking too
    -specs=nano.specs
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map,--cref
    -T${CMAKE_SOURCE_DIR}/STM32F303XX_FLASH.ld
)

# 10. Link libraries (from LIBS)
target_link_libraries(aer_bms.elf PRIVATE
    c
    m
    nosys
)

# 11. Add post-build steps to create .hex, .bin, and print size
# This replaces your .hex and .bin targets and the $(SZ) command
add_custom_command(TARGET aer_bms.elf POST_BUILD
    # Create .hex
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:aer_bms.elf> ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex
    # Create .bin
    COMMAND ${CMAKE_OBJCOPY} -O binary -S $<TARGET_FILE:aer_bms.elf> ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    # Print size
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:aer_bms.elf>
    # Add a comment for clarity in the build log
    COMMENT "Generating .hex, .bin, and printing size..."
)
