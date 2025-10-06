#!/bin/bash

# AzureBot Demo Build Script
# Works on both macOS and Linux

echo "========================================"
echo "  AzureBot Demo Build Script"
echo "========================================"

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macOS"
    CPU_CORES=$(sysctl -n hw.ncpu)
else
    OS="Linux"
    CPU_CORES=$(nproc)
fi

echo "Operating System: $OS"
echo "CPU Cores: $CPU_CORES"
echo ""

# Create and enter build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Run CMake
echo ""
echo "Running CMake..."
cmake .. || {
    echo ""
    echo "❌ CMake configuration failed!"
    echo ""
    echo "Common fixes:"
    echo "  1. Make sure OpenRM is installed:"
    echo "     cd /path/to/OpenRM-2024 && sudo ./run.sh"
    echo ""
    echo "  2. Install missing dependencies:"
    echo "     macOS:  brew install opencv ceres-solver eigen"
    echo "     Linux:  sudo apt-get install libopencv-dev libceres-dev libeigen3-dev"
    echo ""
    exit 1
}

# Build
echo ""
echo "Building demos (using $CPU_CORES cores)..."
make -j"$CPU_CORES" || {
    echo ""
    echo "❌ Build failed!"
    exit 1
}

echo ""
echo "========================================"
echo "  ✅ Build Successful!"
echo "========================================"
echo ""
echo "Run demos:"
echo "  ./build/camera_test             # Test camera"
echo "  ./build/armor_detector_demo     # Full detection"
echo ""
echo "See README.md for more usage examples."
echo ""

