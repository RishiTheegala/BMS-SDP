@echo off
::
:: build.bat - Helper script to configure and build the CMake project
::
:: Usage:
::   build.bat            (Builds in Debug mode by default)
::   build.bat Debug      (Builds in Debug mode)
::   build.bat Release    (Builds in Release mode)
::   build.bat clean      (Removes the build directory)
::

:: --- Configuration ---
SET BUILD_DIR=build
SET TOOLCHAIN_FILE=toolchain.cmake
SET DEFAULT_BUILD_TYPE=Debug

:: --- Script Logic ---

:: 1. Handle 'clean' command
::    Note: /I makes the string comparison case-insensitive
IF /I "%1"=="clean" GOTO :clean

:: 2. Determine Build Type
SET "BUILD_TYPE=%1"
IF "%BUILD_TYPE%"=="" SET "BUILD_TYPE=%DEFAULT_BUILD_TYPE%"

echo "Configuring for %BUILD_TYPE% build..."

:: 3. Create build directory if it doesn't exist
IF NOT EXIST %BUILD_DIR% (
    echo "Creating build directory: %BUILD_DIR%"
    mkdir %BUILD_DIR%
    IF %ERRORLEVEL% NEQ 0 (
        echo "ERROR: Failed to create build directory."
        exit /b %ERRORLEVEL%
    )
)

:: 4. Run CMake Configuration
echo "Configuring project..."
cd %BUILD_DIR%

:: We run cmake from within the build directory
cmake -DCMAKE_TOOLCHAIN_FILE=../%TOOLCHAIN_FILE% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ..

:: Check if CMake configuration failed
IF %ERRORLEVEL% NEQ 0 (
    echo "ERROR: CMake configuration failed."
    cd ..
    exit /b %ERRORLEVEL%
)

:: 5. Run the Build
echo "Building project..."

:: --parallel flag tells CMake to use multiple cores for building (like -j)
cmake --build . --parallel

:: Check if the build failed
IF %ERRORLEVEL% NEQ 0 (
    echo "ERROR: Build failed."
    cd ..
    exit /b %ERRORLEVEL%
)

echo "Build complete. Artifacts are in %BUILD_DIR%"
cd ..
GOTO :eof

:: --- Clean Target ---
:clean
echo "Cleaning project..."
IF EXIST %BUILD_DIR% (
    :: /S deletes all subdirectories and files
    :: /Q runs in quiet mode (no confirmation)
    rmdir /S /Q %BUILD_DIR%
    echo "Clean complete."
) ELSE (
    echo "Build directory not found. Nothing to clean."
)
GOTO :eof
