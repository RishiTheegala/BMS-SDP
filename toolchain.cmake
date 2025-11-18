# Set generic system name
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Specify the cross-compiler prefix
set(TOOLCHAIN_PREFIX "arm-none-eabi-")

# Find the compilers and other tools
# (Assumes "arm-none-eabi-gcc", etc., are in your system PATH)
find_program(CMAKE_C_COMPILER NAMES ${TOOLCHAIN_PREFIX}gcc)
find_program(CMAKE_ASM_COMPILER NAMES ${TOOLCHAIN_PREFIX}gcc)
find_program(CMAKE_OBJCOPY NAMES ${TOOLCHAIN_PREFIX}objcopy)
find_program(CMAKE_OBJDUMP NAMES ${TOOLCHAIN_PREFIX}objdump)
find_program(CMAKE_SIZE NAMES ${TOOLCHAIN_PREFIX}size)
find_program(CMAKE_AR NAMES ${TOOLCHAIN_PREFIX}ar)

# Set the CMAKE_C_COMPILER and CMAKE_ASM_COMPILER if not found
if(NOT CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
endif()

if(NOT CMAKE_ASM_COMPILER)
    set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
endif()

# Set required tool variables if find_program didn't set them
if(NOT CMAKE_OBJCOPY)
    set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
endif()

if(NOT CMAKE_SIZE)
    set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)
endif()

# Skip the default CMake compiler check, which often fails for embedded
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
