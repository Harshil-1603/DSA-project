#!/bin/bash
# Build script to run from MSYS2 terminal

cd "/c/Users/Fagun/Desktop/New folder/DSA-project"

# Check if cmake is installed
if ! command -v cmake &> /dev/null; then
    echo "CMake not found. Installing CMake..."
    pacman -S --noconfirm mingw-w64-x86_64-cmake
fi

# Clean build directory
rm -rf build

# Configure with MSYS2 GCC (no need to specify compiler paths in MSYS2)
cmake -B build -G "MinGW Makefiles"

# Build
cmake --build build --config Release

echo ""
if [ -f "./build/route_finder.exe" ]; then
    echo "Build complete! To run the server:"
    echo "  ./build/route_finder.exe"
else
    echo "Build may have failed. Check the output above."
fi
echo ""

