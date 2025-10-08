#!/bin/bash
#
# WaveEdit - Build and Run Script
# This is the source of truth for building and launching the application
#
# Usage: ./build-and-run.command [options]
#
# Options:
#   clean       - Clean build directory before building
#   debug       - Build in Debug mode (default is Release)
#   run-only    - Skip build, just run the existing binary
#   help        - Show this help message
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
BUILD_TYPE="Release"
DO_CLEAN=false
RUN_ONLY=false

# Print colored message
print_msg() {
    local color=$1
    shift
    echo -e "${color}$@${NC}"
}

# Print section header
print_header() {
    echo ""
    print_msg "$BLUE" "═══════════════════════════════════════════════════════"
    print_msg "$BLUE" "  $1"
    print_msg "$BLUE" "═══════════════════════════════════════════════════════"
}

# Print error and exit
error_exit() {
    print_msg "$RED" "ERROR: $1"
    exit 1
}

# Parse command line arguments
parse_args() {
    for arg in "$@"; do
        case $arg in
            clean)
                DO_CLEAN=true
                ;;
            debug)
                BUILD_TYPE="Debug"
                ;;
            run-only)
                RUN_ONLY=true
                ;;
            help|--help|-h)
                show_help
                exit 0
                ;;
            *)
                error_exit "Unknown option: $arg. Use 'help' for usage."
                ;;
        esac
    done
}

# Show help message
show_help() {
    cat << EOF
WaveEdit - Build and Run Script

Usage: ./build-and-run.command [options]

Options:
    clean       - Clean build directory before building
    debug       - Build in Debug mode (default is Release)
    run-only    - Skip build, just run the existing binary
    help        - Show this help message

Examples:
    ./build-and-run.command              # Build Release and run
    ./build-and-run.command clean        # Clean build then run
    ./build-and-run.command debug        # Build Debug version
    ./build-and-run.command clean debug  # Clean Debug build
    ./build-and-run.command run-only     # Just run existing binary

EOF
}

# Check prerequisites
check_prerequisites() {
    print_header "Checking Prerequisites"

    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        error_exit "CMake not found. Please install CMake 3.15 or later."
    fi

    local cmake_version=$(cmake --version | head -n1 | awk '{print $3}')
    print_msg "$GREEN" "✓ CMake $cmake_version found"

    # Check for C++ compiler
    if ! command -v c++ &> /dev/null; then
        error_exit "C++ compiler not found. Please install Xcode Command Line Tools (macOS) or GCC/Clang."
    fi

    print_msg "$GREEN" "✓ C++ compiler found"

    # Check JUCE submodule
    if [ ! -d "${PROJECT_ROOT}/JUCE/modules" ]; then
        print_msg "$YELLOW" "⚠ JUCE submodule not initialized. Initializing..."
        cd "$PROJECT_ROOT"
        git submodule update --init --recursive || error_exit "Failed to initialize JUCE submodule"
        print_msg "$GREEN" "✓ JUCE submodule initialized"
    else
        print_msg "$GREEN" "✓ JUCE submodule present"
    fi
}

# Clean build directory
clean_build() {
    print_header "Cleaning Build Directory"

    if [ -d "$BUILD_DIR" ]; then
        print_msg "$YELLOW" "Removing $BUILD_DIR..."
        rm -rf "$BUILD_DIR"
        print_msg "$GREEN" "✓ Build directory cleaned"
    else
        print_msg "$YELLOW" "Build directory doesn't exist, nothing to clean"
    fi
}

# Configure with CMake
configure_cmake() {
    print_header "Configuring CMake ($BUILD_TYPE)"

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" || error_exit "CMake configuration failed"

    print_msg "$GREEN" "✓ CMake configuration successful"
}

# Build the project
build_project() {
    print_header "Building WaveEdit ($BUILD_TYPE)"

    cd "$BUILD_DIR"

    # Determine number of parallel jobs
    if [[ "$OSTYPE" == "darwin"* ]]; then
        JOBS=$(sysctl -n hw.ncpu)
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        JOBS=$(nproc)
    else
        JOBS=4
    fi

    print_msg "$BLUE" "Building with $JOBS parallel jobs..."

    cmake --build . --config "$BUILD_TYPE" -j "$JOBS" || error_exit "Build failed"

    print_msg "$GREEN" "✓ Build successful"
}

# Find and run the executable
run_application() {
    print_header "Launching WaveEdit"

    cd "$BUILD_DIR"

    # Determine executable path based on platform
    local executable=""

    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        executable="WaveEdit_artefacts/${BUILD_TYPE}/WaveEdit.app/Contents/MacOS/WaveEdit"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Linux
        executable="WaveEdit_artefacts/${BUILD_TYPE}/WaveEdit"
    else
        error_exit "Unsupported platform: $OSTYPE"
    fi

    if [ ! -f "$executable" ]; then
        error_exit "Executable not found at: $executable\nPlease build the project first."
    fi

    print_msg "$GREEN" "✓ Found executable: $executable"
    print_msg "$BLUE" "Launching WaveEdit..."
    echo ""

    # Run the executable
    "$executable"
}

# Print summary
print_summary() {
    echo ""
    print_header "Build Summary"
    echo "  Build Type:      $BUILD_TYPE"
    echo "  Build Directory: $BUILD_DIR"
    echo "  Project Root:    $PROJECT_ROOT"
    echo ""
    print_msg "$GREEN" "✓ WaveEdit is ready to use!"
    echo ""
}

# Main execution flow
main() {
    # Parse arguments
    parse_args "$@"

    # Print banner
    clear
    print_msg "$BLUE" "╔═══════════════════════════════════════════════════════╗"
    print_msg "$BLUE" "║                                                       ║"
    print_msg "$BLUE" "║               WaveEdit Build System                   ║"
    print_msg "$BLUE" "║          Professional Audio Editor - v0.1.0           ║"
    print_msg "$BLUE" "║                                                       ║"
    print_msg "$BLUE" "╚═══════════════════════════════════════════════════════╝"

    # If run-only mode, skip build
    if [ "$RUN_ONLY" = true ]; then
        run_application
        exit 0
    fi

    # Execute build steps
    check_prerequisites

    if [ "$DO_CLEAN" = true ]; then
        clean_build
    fi

    configure_cmake
    build_project
    print_summary

    # Ask user if they want to run the application
    echo ""
    read -p "Do you want to run WaveEdit now? (Y/n): " -n 1 -r
    echo ""

    if [[ $REPLY =~ ^[Yy]$ ]] || [[ -z $REPLY ]]; then
        run_application
    else
        print_msg "$YELLOW" "Skipping launch. Run './build-and-run.command run-only' to launch later."
    fi
}

# Run main function with all arguments
main "$@"
