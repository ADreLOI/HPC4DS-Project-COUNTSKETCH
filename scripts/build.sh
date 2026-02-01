#!/bin/bash
# 
# BUILD SCRIPT - Count Sketch MPI Project
# 
# This script builds all three versions of the Count Sketch:
#   1. app_seq    - Sequential (baseline)
#   2. app_mpi    - Pure MPI
#   3. app_hybrid - MPI + OpenMP
#
# Usage:
#   ./scripts/build.sh [clean|debug|release]
#
# Options:
#   clean   - Remove build directory and rebuild
#   debug   - Build with debug symbols (-g)
#   release - Build with optimizations (-O3)
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get script directory and project root
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
BUILD_DIR="$PROJECT_ROOT/build"
BIN_DIR="$PROJECT_ROOT/bin"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Count Sketch MPI - Build Script${NC}"
echo -e "${GREEN}========================================${NC}"

# Parse arguments
BUILD_TYPE="Release"
CLEAN_BUILD=false

for arg in "$@"; do
    case $arg in
        clean)
            CLEAN_BUILD=true
            ;;
        debug)
            BUILD_TYPE="Debug"
            ;;
        release)
            BUILD_TYPE="Release"
            ;;
        *)
            echo -e "${RED}Unknown argument: $arg${NC}"
            echo "Usage: $0 [clean|debug|release]"
            exit 1
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
mkdir -p "$BIN_DIR"

# Navigate to build directory
cd "$BUILD_DIR"

# Configure with CMake
echo -e "${GREEN}Configuring with CMake (${BUILD_TYPE})...${NC}"
cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" "$PROJECT_ROOT"

# Build
echo -e "${GREEN}Building...${NC}"
cmake --build . --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Copy executables to bin directory
echo -e "${GREEN}Copying executables to bin/...${NC}"
for exe in app_seq app_mpi app_hybrid; do
    if [ -f "$exe" ]; then
        cp "$exe" "$BIN_DIR/"
        echo "  ✓ $exe"
    elif [ -f "$exe.exe" ]; then
        cp "$exe.exe" "$BIN_DIR/"
        echo "  ✓ $exe.exe"
    fi
done

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Executables are in: $BIN_DIR"
echo ""
echo "Quick test commands:"
echo "  Sequential:  ./bin/app_seq datasets/input_data_100.txt"
echo "  MPI (4 proc): mpirun -np 4 ./bin/app_mpi datasets/input_data_100.txt --benchmark"
echo ""
