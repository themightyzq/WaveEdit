#!/bin/bash

# WaveEdit Build Script
# Professional build automation with verification
# Usage: ./build.sh [clean|debug|release|run]

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
APP_PATH="${BUILD_DIR}/WaveEdit_artefacts/Release/WaveEdit.app"
APP_PATH_DEBUG="${BUILD_DIR}/WaveEdit_artefacts/Debug/WaveEdit.app"
CORES=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Functions
print_header() {
    echo -e "\n${BLUE}════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}════════════════════════════════════════════════════════${NC}\n"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}ℹ️  $1${NC}"
}

check_prerequisites() {
    print_header "Checking Prerequisites"

    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        print_error "CMake not found. Please install CMake 3.15 or later."
        exit 1
    fi

    # Check CMake version
    CMAKE_VERSION=$(cmake --version | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
    print_info "CMake version: $CMAKE_VERSION"

    # Check for C++ compiler
    if ! command -v c++ &> /dev/null; then
        print_error "C++ compiler not found. Please install Xcode Command Line Tools."
        exit 1
    fi

    # Check JUCE submodule
    if [ ! -f "${PROJECT_ROOT}/JUCE/CMakeLists.txt" ]; then
        print_info "JUCE submodule not initialized. Initializing..."
        cd "$PROJECT_ROOT"
        git submodule update --init --recursive
    fi

    print_success "All prerequisites met"
}

clean_build() {
    print_header "Cleaning Build Directory"
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    else
        print_info "Build directory already clean"
    fi
}

configure_project() {
    local BUILD_TYPE=$1
    print_header "Configuring Project ($BUILD_TYPE)"

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    echo "Running CMake configuration..."
    if cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON; then
        print_success "Configuration successful"
    else
        print_error "Configuration failed"
        exit 1
    fi
}

build_project() {
    local BUILD_TYPE=$1
    print_header "Building Project ($BUILD_TYPE)"

    cd "$BUILD_DIR"
    echo "Building with $CORES parallel jobs..."

    if cmake --build . --config "$BUILD_TYPE" -j"$CORES"; then
        print_success "Build completed successfully"

        # Get binary size
        if [ "$BUILD_TYPE" = "Release" ] && [ -f "${APP_PATH}/Contents/MacOS/WaveEdit" ]; then
            SIZE=$(du -sh "${APP_PATH}/Contents/MacOS/WaveEdit" | cut -f1)
            print_info "Binary size: $SIZE"
        elif [ "$BUILD_TYPE" = "Debug" ] && [ -f "${APP_PATH_DEBUG}/Contents/MacOS/WaveEdit" ]; then
            SIZE=$(du -sh "${APP_PATH_DEBUG}/Contents/MacOS/WaveEdit" | cut -f1)
            print_info "Binary size: $SIZE"
        fi
    else
        print_error "Build failed"
        exit 1
    fi
}

verify_build() {
    local BUILD_TYPE=$1
    print_header "Verifying Build"

    local CHECK_PATH="$APP_PATH"
    if [ "$BUILD_TYPE" = "Debug" ]; then
        CHECK_PATH="$APP_PATH_DEBUG"
    fi

    if [ -f "${CHECK_PATH}/Contents/MacOS/WaveEdit" ]; then
        print_success "Executable found at: ${CHECK_PATH}"

        # Check if it's a valid executable
        if file "${CHECK_PATH}/Contents/MacOS/WaveEdit" | grep -q "Mach-O"; then
            print_success "Valid macOS executable"
        else
            print_error "Invalid executable format"
            exit 1
        fi
    else
        print_error "Executable not found"
        exit 1
    fi
}

run_application() {
    local BUILD_TYPE=$1
    print_header "Running WaveEdit"

    local RUN_PATH="$APP_PATH"
    if [ "$BUILD_TYPE" = "Debug" ]; then
        RUN_PATH="$APP_PATH_DEBUG"
    fi

    if [ -d "$RUN_PATH" ]; then
        print_info "Launching ${RUN_PATH}..."
        open "$RUN_PATH"
    else
        print_error "Application not found. Please build first."
        exit 1
    fi
}

print_usage() {
    echo "WaveEdit Build Script"
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  clean    - Clean build directory"
    echo "  debug    - Build debug version"
    echo "  release  - Build release version (default)"
    echo "  run      - Run the application"
    echo "  help     - Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0           # Build release version"
    echo "  $0 clean     # Clean and rebuild release"
    echo "  $0 debug     # Build debug version"
    echo "  $0 run       # Build and run release version"
}

# Main script
print_header "WaveEdit Build System"

case "${1:-release}" in
    clean)
        check_prerequisites
        clean_build
        configure_project "Release"
        build_project "Release"
        verify_build "Release"
        print_success "Clean build completed"
        ;;
    debug)
        check_prerequisites
        if [ ! -d "$BUILD_DIR" ]; then
            configure_project "Debug"
        fi
        build_project "Debug"
        verify_build "Debug"
        print_success "Debug build completed"
        ;;
    release)
        check_prerequisites
        if [ ! -d "$BUILD_DIR" ]; then
            configure_project "Release"
        fi
        build_project "Release"
        verify_build "Release"
        print_success "Release build completed"
        ;;
    run)
        check_prerequisites
        if [ ! -d "$BUILD_DIR" ]; then
            configure_project "Release"
            build_project "Release"
        elif [ ! -d "$APP_PATH" ]; then
            build_project "Release"
        fi
        verify_build "Release"
        run_application "Release"
        ;;
    help)
        print_usage
        ;;
    *)
        print_error "Unknown command: $1"
        print_usage
        exit 1
        ;;
esac

print_info "Build script completed at $(date '+%Y-%m-%d %H:%M:%S')"