#!/bin/bash
# Install required dependencies in MSYS2

echo "Installing build dependencies for MSYS2..."
echo ""

# Update package database
pacman -Syu --noconfirm

# Install CMake and make
pacman -S --noconfirm \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-make \
    base-devel

echo ""
echo "Dependencies installed!"
echo ""
echo "Now you can run: bash build_with_msys2.sh"
echo ""

