@echo off
echo Building Route Finder with CURL support...
echo.

REM Check if vcpkg exists
if not exist "vcpkg\vcpkg.exe" (
    echo vcpkg not found. Installing vcpkg...
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    call bootstrap-vcpkg.bat
    cd ..
)

REM Install curl if not already installed
echo Checking for CURL installation...
vcpkg\vcpkg install curl:x64-windows

REM Clean build directory
if exist "build" (
    echo Cleaning old build directory...
    rmdir /s /q build
)

REM Create build directory
mkdir build
cd build

REM Configure with CMake
echo Configuring CMake...
cmake .. -G "NMake Makefiles" -DCMAKE_TOOLCHAIN_FILE=..\vcpkg\scripts\buildsystems\vcpkg.cmake

if errorlevel 1 (
    echo CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

REM Build
echo Building project...
nmake

if errorlevel 1 (
    echo Build failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build successful!
echo ========================================
echo.
echo To run the server:
echo   cd build
echo   route_finder.exe
echo.
echo The server will run on http://localhost:8080
echo.

cd ..
pause
