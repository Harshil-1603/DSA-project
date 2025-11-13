# PowerShell script to build using MSYS2 GCC
# This sets up the environment and builds the project

Write-Host "Building with MSYS2 GCC 15.2.0..." -ForegroundColor Green
Write-Host ""

$projectPath = "C:\Users\Fagun\Desktop\New folder\DSA-project"
$msys2Path = "C:\msys64"

# Check if MSYS2 exists
if (-not (Test-Path $msys2Path)) {
    Write-Host "MSYS2 not found at $msys2Path" -ForegroundColor Red
    exit 1
}

# Clean build directory
if (Test-Path "$projectPath\build") {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "$projectPath\build"
}

# Set up environment
$env:PATH = "$msys2Path\mingw64\bin;$msys2Path\usr\bin;$env:PATH"

# Change to project directory
Set-Location $projectPath

# Configure CMake
Write-Host "Configuring CMake..." -ForegroundColor Yellow
& cmake -B build -G "MinGW Makefiles" `
    -DCMAKE_C_COMPILER="$msys2Path\mingw64\bin\x86_64-w64-mingw32-gcc.exe" `
    -DCMAKE_CXX_COMPILER="$msys2Path\mingw64\bin\x86_64-w64-mingw32-g++.exe" `
    -DCMAKE_MAKE_PROGRAM="$msys2Path\mingw64\bin\make.exe"

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    Write-Host "Try running from MSYS2 terminal instead:" -ForegroundColor Yellow
    Write-Host "  cd /c/Users/Fagun/Desktop/New\ folder/DSA-project" -ForegroundColor White
    Write-Host "  ./build_with_msys2.sh" -ForegroundColor White
    exit 1
}

# Build
Write-Host ""
Write-Host "Building project..." -ForegroundColor Yellow
& cmake --build build --config Release

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "Build successful!" -ForegroundColor Green
    Write-Host ""
    Write-Host "To run the backend server:" -ForegroundColor Cyan
    Write-Host "  .\build\route_finder.exe" -ForegroundColor White
} else {
    Write-Host ""
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

