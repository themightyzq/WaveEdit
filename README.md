# WaveEdit

> A fast, sample-accurate, cross-platform audio editor inspired by Sound Forge Pro

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Built with JUCE](https://img.shields.io/badge/Built%20with-JUCE-00d4aa)](https://juce.com/)

---

## Overview

**WaveEdit** is a professional audio editor designed for speed, precision, and keyboard-driven workflow. Built with JUCE and inspired by the classic Sound Forge Pro, WaveEdit focuses on what matters most: getting your audio editing done fast without friction.

### Why WaveEdit?

- **Instant startup**: Sub-1 second cold start, no splash screens, no project files
- **Sample-accurate editing**: Professional-grade precision for all operations
- **Keyboard-first**: Every action has a shortcut (Sound Forge layout by default)
- **Fully customizable**: Remap any keyboard shortcut to match your workflow
- **Cross-platform**: Native builds for Windows, macOS, and Linux
- **Free and open source**: GPL v3 license, no proprietary lock-in

**Perfect for**:
- Audio engineers who need quick edits
- Podcasters trimming episodes
- Sound designers cleaning up samples
- Game developers processing audio assets
- Anyone who finds Audacity too complex or proprietary tools too expensive

---

## Features

### Current (Phase 1 - MVP)

- ✅ Open and save WAV files (16/24/32-bit, up to 192kHz)
- ✅ High-performance waveform display with smooth zoom and scroll
- ✅ Playback controls: play, pause, stop, loop selection
- ✅ Sample-accurate selection and editing
- ✅ Cut, copy, paste, delete operations
- ✅ Single-level undo/redo
- ✅ Sound Forge-compatible keyboard shortcuts

### Planned (Phase 2-4)

- 🔄 Customizable keyboard shortcuts with GUI editor
- 🔄 Peak/RMS level meters during playback
- 🔄 Basic DSP: fade in/out, normalize, DC offset removal
- 🔄 Multi-level undo (100+ levels)
- 🔄 Auto-save to temp location (user-configurable interval)
- 🔄 Numeric position entry (jump to exact sample/time)
- 🔄 Snap to zero-crossing
- 🔄 Command palette (Cmd/Ctrl+K for quick actions)
- 🔄 Recent files list with waveform thumbnails
- 🔄 Multi-format support (FLAC, MP3, OGG, AIFF)
- 🔄 Recording from audio input devices
- 🔄 Spectrum analyzer / FFT view
- 🔄 Batch processing
- 🔄 VST3 plugin hosting

---

## Installation

### Pre-built Binaries (Coming Soon)

Download the latest release for your platform from the [Releases](https://github.com/yourusername/waveedit/releases) page.

### Build from Source

**Prerequisites**:
- CMake 3.15 or later
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- JUCE 7.x (included as submodule)

**Clone and Build**:

```bash
# Clone the repository
git clone https://github.com/yourusername/waveedit.git
cd waveedit

# Initialize JUCE submodule
git submodule update --init --recursive

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Run (optional)
./WaveEdit_artefacts/Release/WaveEdit  # Linux/macOS
# or
.\WaveEdit_artefacts\Release\WaveEdit.exe  # Windows
```

**Platform-Specific Notes**:

**macOS**:
```bash
# Install Xcode command line tools if not already installed
xcode-select --install
```

**Linux (Ubuntu/Debian)**:
```bash
# Install dependencies
sudo apt-get update
sudo apt-get install build-essential cmake libasound2-dev \
    libjack-jackd2-dev libfreetype6-dev libx11-dev libxrandr-dev \
    libxinerama-dev libxcursor-dev libgl1-mesa-dev
```

**Windows**:
- Use Visual Studio 2017 or later
- Open the generated `.sln` file in `build/` directory
- Build from Visual Studio

---

## Usage

### Quick Start

1. **Open a file**: Drag & drop a WAV file onto the window, or use `Ctrl+O` (Cmd+O on Mac)
2. **Make a selection**: Click and drag on the waveform
3. **Edit**: Press `Delete` to remove, `Ctrl+X` to cut, `Ctrl+C` to copy
4. **Play**: Press `Space` to play/stop
5. **Save**: Press `Ctrl+S` to save (or `Ctrl+Shift+S` for Save As)

That's it! No project files, no import/export wizards, just open → edit → save.

### Keyboard Shortcuts (Default)

WaveEdit uses Sound Forge Pro keyboard shortcuts by default. All shortcuts are fully customizable.

#### File Operations
| Action | Shortcut |
|--------|----------|
| New Window | `Ctrl+N` (Cmd+N) |
| Open File | `Ctrl+O` |
| Save | `Ctrl+S` |
| Save As | `Ctrl+Shift+S` |
| Close Window | `Ctrl+W` |
| File Properties | `Alt+Enter` |

#### Navigation
| Action | Shortcut |
|--------|----------|
| Move cursor left/right | `Left/Right Arrow` |
| Jump to start/end | `Ctrl+Left/Right` |
| First/last visible sample | `Home/End` |
| Move 10% of view | `Page Up/Down` |
| Center cursor | `.` (period) |

#### Selection
| Action | Shortcut |
|--------|----------|
| Select all | `Ctrl+A` |
| Extend selection | `Shift+Left/Right` |
| Select to visible edge | `Shift+Home/End` |
| Snap to zero crossing | `Z` |

#### Editing
| Action | Shortcut |
|--------|----------|
| Cut | `Ctrl+X` |
| Copy | `Ctrl+C` |
| Paste | `Ctrl+V` |
| Delete | `Delete` |
| Undo | `Ctrl+Z` |
| Redo | `Ctrl+Shift+Z` |

#### Playback
| Action | Shortcut |
|--------|----------|
| Play/Stop | `Space` or `F12` |
| Play/Pause | `Enter` or `Ctrl+F12` |
| Play All | `Shift+Space` |
| Toggle Loop | `Q` |

#### Processing (Phase 2+)
| Action | Shortcut |
|--------|----------|
| Fade In | `Ctrl+Shift+I` |
| Fade Out | `Ctrl+Shift+O` |
| Normalize | `Ctrl+Shift+N` |

---

## Configuration

### Settings Location

WaveEdit stores settings in platform-specific locations:

- **macOS**: `~/Library/Application Support/WaveEdit/`
- **Windows**: `%APPDATA%/WaveEdit/`
- **Linux**: `~/.config/WaveEdit/`

### Configuration Files

**`settings.json`** - User preferences
```json
{
    "version": "1.0",
    "autoSave": {
        "enabled": true,
        "intervalMinutes": 5
    },
    "display": {
        "theme": "dark",
        "waveformColor": "00ff00"
    },
    "audio": {
        "sampleRate": 44100,
        "bitDepth": 16,
        "bufferSize": 512
    }
}
```

**`keybindings.json`** - Keyboard shortcuts
```json
{
    "version": "1.0",
    "profile": "Sound Forge Classic",
    "bindings": {
        "file.open": "Ctrl+O",
        "file.save": "Ctrl+S",
        "edit.undo": "Ctrl+Z",
        "playback.playPause": "Space"
    }
}
```

---

## Project Structure

```
WaveEdit/
├── Source/                     # C++ source code
│   ├── Main.cpp                # Application entry point
│   ├── MainComponent.cpp/.h    # Top-level UI component
│   ├── Audio/                  # Audio engine and processing
│   ├── UI/                     # UI components (waveform, transport, meters)
│   ├── Commands/               # Command system and keyboard shortcuts
│   └── Utils/                  # Settings, undo, helpers
├── JuceLibraryCode/            # JUCE generated code
├── Builds/                     # Platform-specific build files
├── CMakeLists.txt              # CMake build configuration
├── README.md                   # This file
├── CLAUDE.md                   # AI agent instructions
├── TODO.md                     # Project task list
└── LICENSE                     # GPL v3 license
```

---

## Development

### Tech Stack

- **Framework**: [JUCE 7.x](https://juce.com/) - Cross-platform C++ framework for audio applications
- **Language**: C++17
- **Build System**: CMake
- **Audio I/O**: JUCE's audio engine (CoreAudio on macOS, WASAPI on Windows, ALSA/JACK on Linux)
- **File Formats**: JUCE's AudioFormatManager (WAV, FLAC, MP3, OGG support)

### Contributing

Contributions are welcome! Please follow these guidelines:

1. **Check TODO.md** for current priorities
2. **Follow the coding style** in CLAUDE.md
3. **Write tests** for new features
4. **Update documentation** as needed
5. **Open an issue** before starting major work

**Coding Standards**:
- C++17 or later
- 4-space indentation
- Allman brace style
- PascalCase for classes, camelCase for methods
- Document all public methods

**Pull Request Process**:
1. Fork the repository
2. Create a feature branch (`feature/your-feature-name`)
3. Commit your changes (follow [Conventional Commits](https://www.conventionalcommits.org/))
4. Push to your fork
5. Open a pull request with a clear description

### Running Tests

```bash
# Build with tests enabled
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build .

# Run tests
ctest --output-on-failure
```

### Performance Benchmarks

Target performance metrics (measured on modern hardware):

- Startup time: <1 second
- File load (10min WAV): <2 seconds
- Waveform rendering: 60fps smooth scrolling
- Playback latency: <10ms
- Save time: <500ms

Use the built-in profiler or external tools (Instruments on macOS, Valgrind on Linux) to verify.

---

## Roadmap

### Phase 1: Core Editor ✅ (Weeks 1-2)
- File I/O (WAV only)
- Waveform display
- Basic playback
- Cut/copy/paste/delete
- Single-level undo

### Phase 2: Professional Features 🔄 (Week 3)
- Customizable keyboard shortcuts
- Peak meters
- Fade in/out, normalize
- Multi-level undo
- Auto-save

### Phase 3: Polish 🔄 (Week 4)
- Numeric position entry
- Snap to zero-crossing
- Shortcut cheat sheet
- Recent files list
- Command palette

### Phase 4: Extended Features 📅 (Month 2+)
- Multi-format support (FLAC, MP3, OGG)
- Recording from input
- Spectrum analyzer
- Batch processing
- VST3 plugin hosting

---

## FAQ

**Q: Why JUCE instead of Electron/Tauri?**
A: JUCE is purpose-built for audio applications. It provides sample-accurate timing, low-latency I/O, and native performance that web-based frameworks can't match.

**Q: Will you add multi-track editing?**
A: No. WaveEdit is a stereo/mono editor, not a DAW. Use Reaper, Ardour, or Audacity for multi-track work.

**Q: Can I use WaveEdit commercially?**
A: Yes! GPL v3 allows commercial use. You're free to use WaveEdit for any purpose, including commercial projects.

**Q: Will you add VST plugin support?**
A: Yes, VST3 hosting is planned for Phase 4. VST2 support depends on licensing availability.

**Q: How do I customize keyboard shortcuts?**
A: Phase 2 will include a settings panel with a shortcut editor. For now, you can manually edit `keybindings.json`.

**Q: Does WaveEdit support destructive editing?**
A: WaveEdit uses non-destructive editing with undo/redo. The original file is only overwritten when you explicitly save.

**Q: What about Windows 7/8 support?**
A: WaveEdit targets Windows 10+, macOS 10.13+, and modern Linux distributions. Older OS support is not planned.

---

## License

WaveEdit is licensed under the GNU General Public License v3.0.

This means:
- ✅ You can use WaveEdit for any purpose (personal, commercial, etc.)
- ✅ You can modify and distribute modified versions
- ✅ You must distribute source code with any binaries you share
- ✅ Derivative works must also be GPL v3

See [LICENSE](LICENSE) for full details.

---

## Credits

**Developed by**: [Your Name / Team]

**Built with**:
- [JUCE](https://juce.com/) - Cross-platform C++ framework
- Inspired by [Sound Forge Pro](https://www.magix.com/us/music-editing/sound-forge/) (no affiliation)

**Special thanks to**:
- The JUCE community for excellent documentation and support
- Sound Forge Pro for setting the standard in audio editing
- All contributors and testers

---

## Contact

- **Issues**: [GitHub Issues](https://github.com/yourusername/waveedit/issues)
- **Discussions**: [GitHub Discussions](https://github.com/yourusername/waveedit/discussions)
- **Email**: your.email@example.com

---

**Last Updated**: 2025-10-06
**Version**: 0.1.0-alpha (Phase 1 in progress)
