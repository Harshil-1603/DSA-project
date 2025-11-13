# Upgrading GCC to 7.1+ on Windows

## Option 1: Install MSYS2 (Recommended - Easiest)

1. **Download MSYS2:**
   - Go to https://www.msys2.org/
   - Download and install MSYS2 (e.g., `msys2-x86_64-*.exe`)

2. **Install GCC:**
   - Open MSYS2 terminal (not the regular command prompt)
   - Run these commands:
   ```bash
   pacman -Syu
   pacman -S mingw-w64-x86_64-gcc
   pacman -S mingw-w64-x86_64-cmake
   pacman -S mingw-w64-x86_64-make
   ```

3. **Add to PATH:**
   - Add `C:\msys64\mingw64\bin` to your Windows PATH environment variable
   - Or use the compiler directly: `C:\msys64\mingw64\bin\g++.exe`

4. **Verify:**
   ```bash
   g++ --version
   ```
   Should show GCC 13.x or similar (much newer than 7.1!)

## Option 2: Install WinLibs (Standalone - No Installation)

1. **Download WinLibs:**
   - Go to https://winlibs.com/
   - Download the latest release (e.g., GCC 13.2.0 + MinGW-w64)
   - Extract to a folder like `C:\mingw64`

2. **Add to PATH:**
   - Add `C:\mingw64\bin` to your Windows PATH environment variable

3. **Verify:**
   ```bash
   g++ --version
   ```

## Option 3: Use Chocolatey (If you have it)

```powershell
choco install mingw
```

## After Installation

1. **Update CMake to use new compiler:**
   - Delete the `build` folder
   - Re-run CMake configuration
   - Or specify the compiler explicitly:
   ```bash
   cmake -G "MinGW Makefiles" -DCMAKE_C_COMPILER=C:\msys64\mingw64\bin\gcc.exe -DCMAKE_CXX_COMPILER=C:\msys64\mingw64\bin\g++.exe ..
   ```

2. **Rebuild:**
   ```bash
   cmake --build . --config Release
   ```

## Verify Your Setup

After installation, verify:
```bash
g++ --version
```

You should see GCC 7.1 or higher (preferably 8.0+ for better C++17 support).

