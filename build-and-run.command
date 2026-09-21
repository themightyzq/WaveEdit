#!/bin/bash
#
# WaveEdit - Build and Run Script
# This is the source of truth for building and launching the application
#
# Usage: ./build-and-run.command [options]
#
# Options:
#   setup       - First-time setup: install dependencies and prepare environment
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
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
BUILD_TYPE="Release"
DO_CLEAN=false
RUN_ONLY=false
DO_SETUP=false

# JUCE configuration
JUCE_VERSION="7.0.12"  # Minimum recommended version
JUCE_GIT_URL="https://github.com/juce-framework/JUCE.git"

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

# Print sub-section
print_subheader() {
    print_msg "$CYAN" "--- $1 ---"
}

# Print error and exit
error_exit() {
    print_msg "$RED" "ERROR: $1"
    exit 1
}

# Print warning (non-fatal)
print_warning() {
    print_msg "$YELLOW" "WARNING: $1"
}

# Parse command line arguments
parse_args() {
    for arg in "$@"; do
        case $arg in
            setup)
                DO_SETUP=true
                ;;
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
    setup       - First-time setup on a new machine (installs dependencies)
    clean       - Clean build directory before building
    debug       - Build in Debug mode (default is Release)
    run-only    - Skip build, just run the existing binary
    help        - Show this help message

Examples:
    ./build-and-run.command setup         # First-time setup (run this first on new machines)
    ./build-and-run.command               # Build Release and run
    ./build-and-run.command clean         # Clean build then run
    ./build-and-run.command debug         # Build Debug version
    ./build-and-run.command clean debug   # Clean Debug build
    ./build-and-run.command run-only      # Just run existing binary

Prerequisites:
    macOS:  Xcode Command Line Tools, CMake 3.15+
    Linux:  GCC/Clang, CMake 3.15+, ALSA dev libraries, X11 dev libraries

The 'setup' command will attempt to install missing prerequisites automatically.

EOF
}

# ============================================================================
# Platform Detection
# ============================================================================

detect_platform() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        PLATFORM="macos"
        # Detect if Apple Silicon or Intel
        if [[ $(uname -m) == "arm64" ]]; then
            ARCH="arm64"
            HOMEBREW_PREFIX="/opt/homebrew"
        else
            ARCH="x86_64"
            HOMEBREW_PREFIX="/usr/local"
        fi
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        PLATFORM="linux"
        ARCH=$(uname -m)
        # Detect package manager
        if command -v apt-get &> /dev/null; then
            PKG_MANAGER="apt"
        elif command -v dnf &> /dev/null; then
            PKG_MANAGER="dnf"
        elif command -v pacman &> /dev/null; then
            PKG_MANAGER="pacman"
        else
            PKG_MANAGER="unknown"
        fi
    elif [[ "$OSTYPE" == "msys"* ]] || [[ "$OSTYPE" == "cygwin"* ]]; then
        PLATFORM="windows"
        ARCH=$(uname -m)
    else
        PLATFORM="unknown"
        ARCH=$(uname -m)
    fi

    print_msg "$GREEN" "✓ Platform: $PLATFORM ($ARCH)"
}

# ============================================================================
# macOS Specific Checks
# ============================================================================

check_xcode_cli_tools() {
    print_subheader "Checking Xcode Command Line Tools"

    if xcode-select -p &> /dev/null; then
        local xcode_path=$(xcode-select -p)
        print_msg "$GREEN" "✓ Xcode CLI Tools installed at: $xcode_path"
        return 0
    else
        print_warning "Xcode Command Line Tools not found"
        return 1
    fi
}

install_xcode_cli_tools() {
    print_msg "$YELLOW" "Installing Xcode Command Line Tools..."
    print_msg "$YELLOW" "A dialog will appear - please click 'Install' and wait for completion."

    # Trigger the installer
    xcode-select --install 2>/dev/null || true

    # Wait for installation
    print_msg "$YELLOW" "Waiting for Xcode CLI Tools installation to complete..."
    print_msg "$YELLOW" "Press Enter once the installation dialog has finished."
    read -r

    # Verify installation
    if xcode-select -p &> /dev/null; then
        print_msg "$GREEN" "✓ Xcode CLI Tools installed successfully"
        return 0
    else
        error_exit "Xcode CLI Tools installation failed. Please install manually with: xcode-select --install"
    fi
}

check_homebrew() {
    print_subheader "Checking Homebrew"

    if command -v brew &> /dev/null; then
        local brew_version=$(brew --version | head -n1)
        print_msg "$GREEN" "✓ Homebrew installed: $brew_version"
        return 0
    else
        print_warning "Homebrew not found"
        return 1
    fi
}

install_homebrew() {
    print_msg "$YELLOW" "Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

    # Add Homebrew to PATH for this session
    if [[ -f "${HOMEBREW_PREFIX}/bin/brew" ]]; then
        eval "$(${HOMEBREW_PREFIX}/bin/brew shellenv)"
        print_msg "$GREEN" "✓ Homebrew installed successfully"
    else
        error_exit "Homebrew installation failed"
    fi
}

# ============================================================================
# Linux Specific Checks
# ============================================================================

check_linux_dependencies() {
    print_subheader "Checking Linux Development Libraries"

    local missing_deps=()

    # Check for essential dev packages
    case $PKG_MANAGER in
        apt)
            # Check for ALSA dev
            if ! dpkg -l libasound2-dev &> /dev/null; then
                missing_deps+=("libasound2-dev")
            fi
            # Check for X11/Xorg dev
            if ! dpkg -l libx11-dev &> /dev/null; then
                missing_deps+=("libx11-dev")
            fi
            if ! dpkg -l libxrandr-dev &> /dev/null; then
                missing_deps+=("libxrandr-dev")
            fi
            if ! dpkg -l libxinerama-dev &> /dev/null; then
                missing_deps+=("libxinerama-dev")
            fi
            if ! dpkg -l libxcursor-dev &> /dev/null; then
                missing_deps+=("libxcursor-dev")
            fi
            # Check for FreeType
            if ! dpkg -l libfreetype6-dev &> /dev/null; then
                missing_deps+=("libfreetype6-dev")
            fi
            # Check for cURL (for web features if needed)
            if ! dpkg -l libcurl4-openssl-dev &> /dev/null; then
                missing_deps+=("libcurl4-openssl-dev")
            fi
            # Check for WebKit (for JUCE web browser if needed)
            if ! dpkg -l libwebkit2gtk-4.0-dev &> /dev/null; then
                missing_deps+=("libwebkit2gtk-4.0-dev")
            fi
            ;;
        dnf)
            # Fedora/RHEL dependencies
            if ! rpm -q alsa-lib-devel &> /dev/null; then
                missing_deps+=("alsa-lib-devel")
            fi
            if ! rpm -q libX11-devel &> /dev/null; then
                missing_deps+=("libX11-devel")
            fi
            if ! rpm -q libXrandr-devel &> /dev/null; then
                missing_deps+=("libXrandr-devel")
            fi
            if ! rpm -q freetype-devel &> /dev/null; then
                missing_deps+=("freetype-devel")
            fi
            ;;
        pacman)
            # Arch Linux dependencies
            if ! pacman -Q alsa-lib &> /dev/null; then
                missing_deps+=("alsa-lib")
            fi
            if ! pacman -Q libx11 &> /dev/null; then
                missing_deps+=("libx11")
            fi
            if ! pacman -Q freetype2 &> /dev/null; then
                missing_deps+=("freetype2")
            fi
            ;;
    esac

    if [ ${#missing_deps[@]} -eq 0 ]; then
        print_msg "$GREEN" "✓ All Linux development libraries found"
        return 0
    else
        print_warning "Missing libraries: ${missing_deps[*]}"
        MISSING_LINUX_DEPS=("${missing_deps[@]}")
        return 1
    fi
}

install_linux_dependencies() {
    if [ ${#MISSING_LINUX_DEPS[@]} -eq 0 ]; then
        return 0
    fi

    print_msg "$YELLOW" "Installing missing Linux dependencies..."

    case $PKG_MANAGER in
        apt)
            sudo apt-get update
            sudo apt-get install -y "${MISSING_LINUX_DEPS[@]}"
            ;;
        dnf)
            sudo dnf install -y "${MISSING_LINUX_DEPS[@]}"
            ;;
        pacman)
            sudo pacman -S --noconfirm "${MISSING_LINUX_DEPS[@]}"
            ;;
        *)
            print_warning "Unknown package manager. Please install manually: ${MISSING_LINUX_DEPS[*]}"
            return 1
            ;;
    esac

    print_msg "$GREEN" "✓ Linux dependencies installed"
}

# ============================================================================
# Common Prerequisite Checks
# ============================================================================

check_cmake() {
    print_subheader "Checking CMake"

    if command -v cmake &> /dev/null; then
        local cmake_version=$(cmake --version | head -n1 | awk '{print $3}')
        local cmake_major=$(echo "$cmake_version" | cut -d. -f1)
        local cmake_minor=$(echo "$cmake_version" | cut -d. -f2)

        if [ "$cmake_major" -ge 3 ] && [ "$cmake_minor" -ge 15 ]; then
            print_msg "$GREEN" "✓ CMake $cmake_version found (meets minimum 3.15)"
            return 0
        else
            print_warning "CMake $cmake_version found but 3.15+ required"
            return 1
        fi
    else
        print_warning "CMake not found"
        return 1
    fi
}

install_cmake() {
    print_msg "$YELLOW" "Installing CMake..."

    case $PLATFORM in
        macos)
            if command -v brew &> /dev/null; then
                brew install cmake
            else
                error_exit "Homebrew required to install CMake on macOS"
            fi
            ;;
        linux)
            case $PKG_MANAGER in
                apt)
                    sudo apt-get update
                    sudo apt-get install -y cmake
                    ;;
                dnf)
                    sudo dnf install -y cmake
                    ;;
                pacman)
                    sudo pacman -S --noconfirm cmake
                    ;;
                *)
                    error_exit "Please install CMake 3.15+ manually"
                    ;;
            esac
            ;;
        *)
            error_exit "Please install CMake 3.15+ manually from https://cmake.org"
            ;;
    esac

    print_msg "$GREEN" "✓ CMake installed"
}

check_compiler() {
    print_subheader "Checking C++ Compiler"

    if command -v c++ &> /dev/null; then
        local compiler_info=$(c++ --version | head -n1)
        print_msg "$GREEN" "✓ C++ compiler found: $compiler_info"
        return 0
    elif command -v clang++ &> /dev/null; then
        local compiler_info=$(clang++ --version | head -n1)
        print_msg "$GREEN" "✓ Clang++ found: $compiler_info"
        return 0
    elif command -v g++ &> /dev/null; then
        local compiler_info=$(g++ --version | head -n1)
        print_msg "$GREEN" "✓ G++ found: $compiler_info"
        return 0
    else
        print_warning "No C++ compiler found"
        return 1
    fi
}

check_git() {
    print_subheader "Checking Git"

    if command -v git &> /dev/null; then
        local git_version=$(git --version)
        print_msg "$GREEN" "✓ $git_version"
        return 0
    else
        print_warning "Git not found"
        return 1
    fi
}

install_git() {
    print_msg "$YELLOW" "Installing Git..."

    case $PLATFORM in
        macos)
            # Git comes with Xcode CLI tools, but can also install via Homebrew
            if command -v brew &> /dev/null; then
                brew install git
            else
                print_msg "$YELLOW" "Git should be available after Xcode CLI Tools installation"
            fi
            ;;
        linux)
            case $PKG_MANAGER in
                apt)
                    sudo apt-get install -y git
                    ;;
                dnf)
                    sudo dnf install -y git
                    ;;
                pacman)
                    sudo pacman -S --noconfirm git
                    ;;
            esac
            ;;
    esac
}

# ============================================================================
# JUCE Submodule / Clone Handling
# ============================================================================

check_juce() {
    print_subheader "Checking JUCE Framework"

    # Check if JUCE directory exists and has the modules folder
    if [ -d "${PROJECT_ROOT}/JUCE/modules" ]; then
        # Verify it has actual content (not just empty dirs)
        if [ -d "${PROJECT_ROOT}/JUCE/modules/juce_core" ]; then
            print_msg "$GREEN" "✓ JUCE framework found at JUCE/"

            # Try to get version if possible
            if [ -f "${PROJECT_ROOT}/JUCE/CMakeLists.txt" ]; then
                local juce_ver=$(grep -m1 "project(JUCE VERSION" "${PROJECT_ROOT}/JUCE/CMakeLists.txt" 2>/dev/null | sed 's/.*VERSION \([0-9.]*\).*/\1/' || echo "unknown")
                if [ "$juce_ver" != "unknown" ]; then
                    print_msg "$GREEN" "  JUCE version: $juce_ver"
                fi
            fi
            return 0
        fi
    fi

    print_warning "JUCE framework not found or incomplete"
    return 1
}

setup_juce() {
    print_msg "$YELLOW" "Setting up JUCE framework..."

    cd "$PROJECT_ROOT"

    # First, try git submodule if we're in a git repo
    if [ -d ".git" ]; then
        print_msg "$BLUE" "Attempting git submodule initialization..."
        if git submodule update --init --recursive 2>/dev/null; then
            if [ -d "${PROJECT_ROOT}/JUCE/modules/juce_core" ]; then
                print_msg "$GREEN" "✓ JUCE submodule initialized successfully"
                return 0
            fi
        fi
        print_msg "$YELLOW" "Git submodule method failed, will clone directly..."
    fi

    # If submodule didn't work (Perforce sync, corrupted state, etc.), clone directly
    if [ -d "${PROJECT_ROOT}/JUCE" ]; then
        print_msg "$YELLOW" "Removing incomplete JUCE directory..."
        rm -rf "${PROJECT_ROOT}/JUCE"
    fi

    print_msg "$BLUE" "Cloning JUCE framework from GitHub..."
    print_msg "$BLUE" "This may take a few minutes..."

    if git clone --depth 1 "$JUCE_GIT_URL" "${PROJECT_ROOT}/JUCE"; then
        print_msg "$GREEN" "✓ JUCE cloned successfully"
        return 0
    else
        error_exit "Failed to clone JUCE. Check your internet connection and try again."
    fi
}

# ============================================================================
# Optional Dependencies (LAME for MP3)
# ============================================================================

check_lame() {
    print_subheader "Checking LAME MP3 Encoder (optional)"

    case $PLATFORM in
        macos)
            if [ -f "${HOMEBREW_PREFIX}/lib/libmp3lame.dylib" ] || [ -f "/usr/local/lib/libmp3lame.dylib" ]; then
                print_msg "$GREEN" "✓ LAME library found (MP3 export enabled)"
                return 0
            fi
            ;;
        linux)
            if ldconfig -p 2>/dev/null | grep -q libmp3lame; then
                print_msg "$GREEN" "✓ LAME library found (MP3 export enabled)"
                return 0
            fi
            ;;
    esac

    print_warning "LAME not found - MP3 export will be disabled"
    print_msg "$YELLOW" "  Install with: brew install lame (macOS) or apt install libmp3lame-dev (Linux)"
    return 1
}

install_lame() {
    print_msg "$YELLOW" "Installing LAME MP3 encoder..."

    case $PLATFORM in
        macos)
            if command -v brew &> /dev/null; then
                brew install lame
                print_msg "$GREEN" "✓ LAME installed"
            fi
            ;;
        linux)
            case $PKG_MANAGER in
                apt)
                    sudo apt-get install -y libmp3lame-dev
                    ;;
                dnf)
                    sudo dnf install -y lame-devel
                    ;;
                pacman)
                    sudo pacman -S --noconfirm lame
                    ;;
            esac
            print_msg "$GREEN" "✓ LAME installed"
            ;;
    esac
}

# ============================================================================
# Setup Mode - First Time Environment Preparation
# ============================================================================

run_setup() {
    print_header "WaveEdit First-Time Setup"
    print_msg "$BLUE" "This will install all necessary dependencies for building WaveEdit."
    echo ""

    detect_platform

    # Platform-specific setup
    case $PLATFORM in
        macos)
            print_header "macOS Setup"

            # Xcode CLI Tools
            if ! check_xcode_cli_tools; then
                install_xcode_cli_tools
            fi

            # Homebrew (optional but recommended)
            if ! check_homebrew; then
                echo ""
                read -p "Install Homebrew? (recommended for CMake and LAME) [Y/n]: " -n 1 -r
                echo ""
                if [[ $REPLY =~ ^[Yy]$ ]] || [[ -z $REPLY ]]; then
                    install_homebrew
                fi
            fi

            # CMake
            if ! check_cmake; then
                install_cmake
            fi

            # Git (usually comes with Xcode CLI)
            if ! check_git; then
                install_git
            fi
            ;;

        linux)
            print_header "Linux Setup"

            # Compiler
            if ! check_compiler; then
                print_msg "$YELLOW" "Please install build-essential (apt) or base-devel (pacman)"
                error_exit "C++ compiler required"
            fi

            # CMake
            if ! check_cmake; then
                install_cmake
            fi

            # Git
            if ! check_git; then
                install_git
            fi

            # Linux dev libraries
            if ! check_linux_dependencies; then
                install_linux_dependencies
            fi
            ;;

        *)
            print_warning "Unsupported platform: $PLATFORM"
            print_msg "$YELLOW" "Please install CMake 3.15+ and a C++17 compiler manually."
            ;;
    esac

    # JUCE setup (all platforms)
    print_header "JUCE Framework Setup"
    if ! check_juce; then
        setup_juce
    fi

    # Optional: LAME for MP3 support
    print_header "Optional Dependencies"
    if ! check_lame; then
        echo ""
        read -p "Install LAME for MP3 export support? [Y/n]: " -n 1 -r
        echo ""
        if [[ $REPLY =~ ^[Yy]$ ]] || [[ -z $REPLY ]]; then
            install_lame
        fi
    fi

    # Setup complete
    print_header "Setup Complete!"
    print_msg "$GREEN" "✓ All prerequisites are installed."
    print_msg "$GREEN" "✓ You can now build WaveEdit with: ./build-and-run.command"
    echo ""
}

# ============================================================================
# Build Prerequisites Check (Quick Version)
# ============================================================================

check_prerequisites() {
    print_header "Checking Prerequisites"

    detect_platform

    local all_good=true

    # Check CMake
    if ! check_cmake; then
        all_good=false
    fi

    # Check compiler
    if ! check_compiler; then
        all_good=false
    fi

    # Check JUCE
    if ! check_juce; then
        # Try to set it up automatically
        setup_juce
        if ! check_juce; then
            all_good=false
        fi
    fi

    # Platform-specific checks
    case $PLATFORM in
        macos)
            if ! check_xcode_cli_tools; then
                all_good=false
            fi
            ;;
        linux)
            if ! check_linux_dependencies; then
                all_good=false
            fi
            ;;
    esac

    # Check optional deps (non-fatal)
    check_lame || true

    if [ "$all_good" = false ]; then
        echo ""
        print_msg "$RED" "═══════════════════════════════════════════════════════"
        print_msg "$RED" "  Some prerequisites are missing!"
        print_msg "$RED" "  Run './build-and-run.command setup' to install them."
        print_msg "$RED" "═══════════════════════════════════════════════════════"
        exit 1
    fi

    print_msg "$GREEN" "✓ All prerequisites satisfied"
}

# ============================================================================
# Clean Build Directory
# ============================================================================

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

# ============================================================================
# Configure with CMake
# ============================================================================

configure_cmake() {
    print_header "Configuring CMake ($BUILD_TYPE)"

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Configure with better error output
    if ! cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" 2>&1; then
        echo ""
        print_msg "$RED" "CMake configuration failed!"
        print_msg "$YELLOW" "Common fixes:"
        print_msg "$YELLOW" "  1. Run './build-and-run.command setup' to install dependencies"
        print_msg "$YELLOW" "  2. Run './build-and-run.command clean' then try again"
        print_msg "$YELLOW" "  3. Check that JUCE/ directory contains the framework"
        exit 1
    fi

    print_msg "$GREEN" "✓ CMake configuration successful"
}

# ============================================================================
# Build the Project
# ============================================================================

build_project() {
    print_header "Building WaveEdit ($BUILD_TYPE)"

    cd "$BUILD_DIR"

    # Determine number of parallel jobs
    case $PLATFORM in
        macos)
            JOBS=$(sysctl -n hw.ncpu)
            ;;
        linux)
            JOBS=$(nproc)
            ;;
        *)
            JOBS=4
            ;;
    esac

    print_msg "$BLUE" "Building with $JOBS parallel jobs..."

    if ! cmake --build . --config "$BUILD_TYPE" -j "$JOBS" 2>&1; then
        echo ""
        print_msg "$RED" "Build failed!"
        print_msg "$YELLOW" "Check the error messages above for details."
        print_msg "$YELLOW" "Common fixes:"
        print_msg "$YELLOW" "  1. Run './build-and-run.command clean' then try again"
        print_msg "$YELLOW" "  2. Check for missing source files in CMakeLists.txt"
        exit 1
    fi

    print_msg "$GREEN" "✓ Build successful"
}

# ============================================================================
# Find and Run the Executable
# ============================================================================

run_application() {
    print_header "Launching WaveEdit"

    cd "$BUILD_DIR"

    # Determine executable path based on platform
    local executable=""

    case $PLATFORM in
        macos)
            executable="WaveEdit_artefacts/${BUILD_TYPE}/WaveEdit.app/Contents/MacOS/WaveEdit"
            ;;
        linux)
            executable="WaveEdit_artefacts/${BUILD_TYPE}/WaveEdit"
            ;;
        *)
            error_exit "Unsupported platform: $PLATFORM"
            ;;
    esac

    if [ ! -f "$executable" ]; then
        print_msg "$RED" "Executable not found at: $executable"
        print_msg "$YELLOW" "Please build the project first with: ./build-and-run.command"
        exit 1
    fi

    print_msg "$GREEN" "✓ Found executable: $executable"
    print_msg "$BLUE" "Launching WaveEdit..."
    echo ""

    # Run the executable
    "$executable"
}

# ============================================================================
# Print Summary
# ============================================================================

print_summary() {
    echo ""
    print_header "Build Summary"
    echo "  Platform:        $PLATFORM ($ARCH)"
    echo "  Build Type:      $BUILD_TYPE"
    echo "  Build Directory: $BUILD_DIR"
    echo "  Project Root:    $PROJECT_ROOT"
    echo ""
    print_msg "$GREEN" "✓ WaveEdit is ready to use!"
    echo ""
}

# ============================================================================
# Main Execution Flow
# ============================================================================

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

    # Detect platform early
    detect_platform

    # Handle setup mode
    if [ "$DO_SETUP" = true ]; then
        run_setup
        exit 0
    fi

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

    # Ask user if they want to run the application — but only when
    # actually attached to a TTY and not running under CI. CI/automation
    # callers should not get blocked on an interactive prompt.
    if [ ! -t 0 ] || [ -n "$CI" ] || [ -n "$WAVEEDIT_NONINTERACTIVE" ]; then
        print_msg "$YELLOW" "Non-interactive environment — skipping launch."
        print_msg "$YELLOW" "Run './build-and-run.command run-only' to launch."
        return 0
    fi

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
