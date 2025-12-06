#!/bin/bash
#
# build.sh - Helper script to configure and build the CMake project
#
# Usage:
#   ./build.sh            (Builds in Debug mode by default)
#   ./build.sh debug      (Builds in Debug mode)
#   ./build.sh release    (Builds in Release mode)
#   ./build.sh clean      (Removes the build directory)
#

# --- Configuration ---
BUILD_DIR="build"
TOOLCHAIN_FILE="cmake/gcc-arm-none-eabi.cmake"
DEFAULT_BUILD_TYPE="Debug"

# --- Script Logic ---

# Exit immediately if a command exits with a non-zero status.
set -e

# 1. Handle the 'clean' command
if [ "$1" == "clean" ]; then
    echo "Cleaning project..."
    rm -rf $BUILD_DIR
    echo "Clean complete."
    exit 0
fi

# 2. Determine Build Type (default to Debug)
BUILD_TYPE=$DEFAULT_BUILD_TYPE
if [ ! -z "$1" ]; then
    # Convert argument to lowercase and capitalize first letter
    LOWER_ARG=$(echo "$1" | tr '[:upper:]' '[:lower:]')
    BUILD_TYPE="$(tr '[:lower:]' '[:upper:]' <<< ${LOWER_ARG:0:1})${LOWER_ARG:1}"
fi

# 3. Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory: $BUILD_DIR"
    mkdir $BUILD_DIR
fi

# 4. Run CMake Configuration
echo "Configuring project for $BUILD_TYPE build..."
cd $BUILD_DIR
cmake -DCMAKE_TOOLCHAIN_FILE=../$TOOLCHAIN_FILE \
      -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      ..

# 5. Run the Build
echo "Building project..."
# Using -j lets it build in parallel, which is faster
cmake --build . -- -j

echo "Build complete. Artifacts are in $BUILD_DIR"
