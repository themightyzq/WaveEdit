# WaveEdit by ZQ SFX

> A fast, sample-accurate, cross-platform audio editor inspired by Sound Forge Pro

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Built with JUCE](https://img.shields.io/badge/Built%20with-JUCE-00d4aa)](https://juce.com/)
[![Build Status](https://github.com/themightyzq/WaveEdit/actions/workflows/build.yml/badge.svg)](https://github.com/themightyzq/WaveEdit/actions/workflows/build.yml)

**Developer**: ZQ SFX
**Copyright**: © 2025 ZQ SFX
**License**: GPL v3

---

## Overview

**WaveEdit** is a professional audio editor designed for speed, precision, and keyboard-driven workflow. Built with JUCE and inspired by Sound Forge Pro, WaveEdit focuses on getting your audio editing done fast without friction.

### Why WaveEdit?

- **Instant startup**: Sub-1 second cold start, no splash screens, no project files
- **Sample-accurate editing**: Professional-grade precision
- **Keyboard-first**: Every action has a shortcut (Sound Forge layout by default)
- **Fully customizable**: Remap any keyboard shortcut
- **Cross-platform**: Native builds for Windows, macOS, and Linux
- **Free and open source**: GPL v3 license

**Perfect for**: Audio engineers, podcasters, sound designers, game developers

---

## Quick Start

### Launch the App

**Already built?**
```bash
# macOS
open ./build/WaveEdit_artefacts/Release/WaveEdit.app

# OR use the script
./build-and-run.command run-only
```

**Not built yet?**
```bash
./build-and-run.command
```

### Basic Usage

1. **Open**: Drag & drop WAV file or press `Cmd+O`
2. **Select**: Click and drag on waveform
3. **Edit**: `Delete`, `Cmd+X` (cut), `Cmd+C` (copy), `Cmd+V` (paste)
4. **Play**: Press `Space`
5. **Save**: Press `Cmd+S`

No project files, no import wizards. Just open → edit → save.

---

## Features

### What Works Now

**Core Editing**:
- ✅ Multi-file support with tab-based UI
- ✅ Cut, copy, paste, delete
- ✅ Per-channel editing (double-click to focus individual channels)
- ✅ Undo/redo (100 levels per file)
- ✅ Drag & drop multiple files
- ✅ Save/Save As with error handling

**Playback**:
- ✅ Play, pause, stop, loop
- ✅ Selection-bounded playback
- ✅ Real-time level meters (peak, RMS, clipping detection)
- ✅ Real-time spectrum analyzer (FFT-based frequency visualization)

**DSP Operations**:
- ✅ Gain adjustment (±1dB with `Shift+Up/Down`)
- ✅ 3-band Parametric EQ (Low/Mid/High shelves)
- ✅ 20-band Graphical EQ (Bell, Shelf, Cut, Notch, Bandpass filters with real-time curve)
- ✅ Normalize (0dB peak)
- ✅ Fade in/out with 4 curve types (Linear, Exponential, Logarithmic, S-Curve) and visual preview
- ✅ DC offset removal
- ✅ Silence selection
- ✅ Trim (delete outside selection)

**Regions** 🆕:
- ✅ Create, rename, delete, navigate
- ✅ Region list panel with search/filter
- ✅ Batch rename (pattern/find-replace/prefix-suffix)
- ✅ Batch export (each region as separate WAV)
- ✅ Merge/split/copy/paste regions
- ✅ Edit boundaries with snap to zero crossings

**Metadata** 🆕:
- ✅ BWF (Broadcast Wave Format) support
- ✅ iXML metadata with UCS v8.2.1 categories (753 mappings)
- ✅ SoundMiner Extended fields (FXName, Description, Keywords, Designer)
- ✅ File Properties dialog (`Cmd+Enter`) with UCS suggestions
- ✅ Persistent metadata embedded in WAV files

**Navigation**:
- ✅ Sample-accurate selection at any zoom
- ✅ Snap modes: Off, Samples, Ms, Seconds, Frames, Zero
- ✅ Keyboard navigation honors snap mode
- ✅ Go to position (6 time formats supported)

**Keyboard Shortcuts**:
- ✅ Sound Forge Pro compatibility (default)
- ✅ Fully customizable with GUI editor
- ✅ 4 built-in templates (Default, Classic, Sound Forge, Pro Tools)
- ✅ Import/export custom templates

**Quality**:
- ✅ Automated test suite (47 assertions, 100% pass rate)
- ✅ Sub-1 second startup, 60fps rendering
- ✅ <10ms waveform updates, <10ms playback latency

> **Status**: Production-quality for core editing workflows.

**Spectrum Analyzer** 🆕:
- ✅ Real-time FFT visualization during playback
- ✅ Configurable FFT size (512, 1024, 2048, 4096, 8192 samples)
- ✅ Multiple windowing functions (Hann, Hamming, Blackman, Rectangular)
- ✅ Logarithmic frequency scale (20Hz-20kHz) with peak hold
- ✅ Color gradient visualization (blue → green → yellow → red for magnitude)
- ✅ Toggle with `Cmd+Alt+S` or View → Spectrum Analyzer
- ✅ Configure FFT size and window function from View menu submenus

**Batch Processor** 🆕:
- ✅ Process multiple audio files with identical DSP settings (`Cmd+Alt+B`)
- ✅ DSP chain: Gain, Normalize, DC Offset, Fade In/Out, EQ presets
- ✅ Plugin chain support (apply VST3/AU effect chains)
- ✅ Output settings: directory, naming patterns, sample rate/bit depth conversion
- ✅ Error handling: stop on error, continue, or skip and log
- ✅ Save/load batch presets for recurring workflows

### What's Next

Planned features:
- Additional DSP operations (reverb, compressor, noise reduction)
- More export formats
- Plugin preset management improvements

---

## Installation

### Pre-Built Binaries (Recommended for End Users)

Automated builds are available for macOS, Windows, and Linux. Download the latest release from the [Actions](https://github.com/themightyzq/WaveEdit/actions/workflows/build.yml) page (artifacts available for 30 days).

**Pre-built binaries include**:
- ✅ All audio codecs (WAV, FLAC, OGG, MP3)
- ✅ LAME MP3 encoder bundled (no installation required)
- ✅ Ready to run - just extract and launch

### Build from Source (For Developers)

**Prerequisites**:
- CMake 3.15+
- C++17 compiler
- JUCE 7.x (included as submodule)
- **LAME library** (for MP3 encoding support)

**Quick build** (recommended):
```bash
git clone https://github.com/themightyzq/WaveEdit.git
cd waveedit
./build-and-run.command
```

**Additional options**:
```bash
./build-and-run.command              # Build and launch
./build-and-run.command run-only     # Launch without building
./build-and-run.command clean        # Clean build
./build-and-run.command debug        # Debug build
./build-and-run.command help         # Show all options
```

**Manual build** (if you prefer CMake commands directly):
```bash
git clone https://github.com/themightyzq/WaveEdit.git
cd waveedit
git submodule update --init --recursive

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Launch
open ./WaveEdit_artefacts/Release/WaveEdit.app  # macOS
./WaveEdit_artefacts/Release/WaveEdit           # Linux
.\WaveEdit_artefacts\Release\WaveEdit.exe       # Windows
```

**Developer dependencies**:

macOS:
```bash
# Xcode command line tools
xcode-select --install

# LAME MP3 encoder (required for MP3 support)
brew install lame
```

Linux (Ubuntu/Debian):
```bash
# Build tools and audio dependencies
sudo apt-get install build-essential cmake libasound2-dev \
    libjack-jackd2-dev libfreetype6-dev libx11-dev libxrandr-dev \
    libxinerama-dev libxcursor-dev libgl1-mesa-dev

# LAME MP3 encoder (required for MP3 support)
sudo apt-get install libmp3lame-dev
```

Windows:
- Visual Studio 2017+ (open generated `.sln` file)
- LAME MP3 encoder: Download from https://lame.sourceforge.io/

**Note for developers**: LAME is only required for building from source. Release builds automatically bundle LAME, so end users don't need to install it separately.

---

## Testing

**Build and run tests**:
```bash
cmake --build build --target WaveEditTests
./build/WaveEditTests_artefacts/Debug/WaveEditTests
```

**Test output**:
```
╔══════════════════════════════════════════════════════════════╗
║         WaveEdit Automated Test Suite by ZQ SFX             ║
╚══════════════════════════════════════════════════════════════╝

Total test groups: 13
Total assertions: 47
Passed: 47
Failed: 0

✅ All tests PASSED
```

**Test infrastructure**:
- Unit tests: Individual components (AudioEngine, AudioBufferManager, etc.)
- Integration tests: Components working together
- End-to-end tests: Complete workflows (open → edit → save)

---

## Keyboard Shortcuts

All shortcuts are customizable. Default layout matches Sound Forge Pro.

### File Operations
| Action | Shortcut |
|--------|----------|
| New File | `Cmd+N` |
| Open File | `Cmd+O` |
| Save | `Cmd+S` |
| Save As | `Cmd+Shift+S` |
| Close Window | `Cmd+W` |
| File Properties | `Cmd+Enter` |
| Edit BWF Metadata | `Cmd+Alt+B` |
| Preferences | `Cmd+,` |

### Navigation
| Action | Shortcut |
|--------|----------|
| Move cursor | `Left/Right` (honors snap mode) |
| Jump to start/end | `Cmd+Left/Right` |
| Center cursor | `.` |
| Go to position | `Cmd+G` |
| Cycle snap mode | `G` |

### Selection
| Action | Shortcut |
|--------|----------|
| Select all | `Cmd+A` |
| Extend selection | `Shift+Left/Right` |

### Editing
| Action | Shortcut |
|--------|----------|
| Cut | `Cmd+X` |
| Copy | `Cmd+C` |
| Paste | `Cmd+V` |
| Delete | `Delete` |
| Silence | `Ctrl+L` |
| Trim | `Ctrl+T` |
| Undo | `Cmd+Z` |
| Redo | `Cmd+Shift+Z` |

### Playback
| Action | Shortcut |
|--------|----------|
| Play/Stop | `Space` |
| Play/Pause | `Enter` |
| Toggle Loop | `L` |
| Loop Region | `Cmd+Shift+L` |
| Record | `Cmd+R` |
| Auto-scroll | `Cmd+Shift+F` |

### Processing
| Action | Shortcut |
|--------|----------|
| Batch Processor | `Cmd+Alt+B` |
| Parametric EQ | `Shift+E` |
| Gain Dialog | `Cmd+Shift+G` |
| Increase Gain | `Shift+Up` |
| Decrease Gain | `Shift+Down` |
| Normalize | `Ctrl+Shift+N` |
| Fade In | `Ctrl+Shift+I` |
| Fade Out | `Ctrl+Shift+O` |
| DC Offset Removal | `Cmd+Shift+D` |

### Regions
| Action | Shortcut |
|--------|----------|
| Add Region | `R` |
| Region List Panel | `Cmd+Shift+R` |
| Batch Rename | `Cmd+Shift+B` |
| Batch Export | `Cmd+Alt+R` |
| Merge Regions | `Cmd+J` |
| Split Region | `Cmd+K` |
| Copy Regions | `Cmd+Alt+C` |
| Paste Regions | `Cmd+Alt+V` |
| Edit Boundaries | Right-click → Edit Boundaries |

### Markers
| Action | Shortcut |
|--------|----------|
| Add Marker | `M` |
| Marker List Panel | `Cmd+Shift+K` |
| Next Marker | `Shift+]` |
| Previous Marker | `Shift+[` |

### Tabs
| Action | Shortcut |
|--------|----------|
| Next Tab | `Ctrl+Tab` |
| Previous Tab | `Ctrl+Shift+Tab` |
| Close Tab | `Cmd+W` |
| Select Tab 1-9 | `Cmd+1` to `Cmd+9` |

> **Note**: `Cmd` key on macOS = `Ctrl` key on Windows/Linux

---

## Configuration

**Settings location**:
- macOS: `~/Library/Application Support/WaveEdit/`
- Windows: `%APPDATA%/WaveEdit/`
- Linux: `~/.config/WaveEdit/`

**Configuration files**:
- `settings.json` - User preferences (audio device, display, auto-save)
- `keybindings.json` - Keyboard shortcuts
- `Keymaps/` - Custom keyboard templates

---

## Development

### Tech Stack

- **Framework**: [JUCE 7.x](https://juce.com/)
- **Language**: C++17
- **Build System**: CMake
- **Audio I/O**: JUCE audio engine (CoreAudio/WASAPI/ALSA)

### Contributing

1. Open an issue before major work
2. Follow coding standards below
3. Write tests for new features
4. Update documentation

**Coding standards**:
- C++17 or later
- 4-space indentation, Allman braces
- PascalCase classes, camelCase methods
- Document all public methods

**Pull request process**:
1. Fork the repository
2. Create feature branch (`feature/your-feature`)
3. Follow [Conventional Commits](https://www.conventionalcommits.org/)
4. Push to your fork
5. Open pull request with clear description

### Performance Targets

- Startup: <1 second
- File load (10min WAV): <2 seconds
- Rendering: 60fps
- Playback latency: <10ms
- Save: <500ms

---

## FAQ

**Q: Why JUCE instead of Electron/Tauri?**
A: JUCE is purpose-built for audio with sample-accurate timing and low-latency I/O that web frameworks can't match.

**Q: Will you add multi-track editing?**
A: No. WaveEdit is a stereo/mono editor, not a DAW. Use Reaper or Ardour for multi-track.

**Q: Can I use WaveEdit commercially?**
A: Yes! GPL v3 allows commercial use.

**Q: How do I customize keyboard shortcuts?**
A: Open Preferences (`Cmd+,`) → Keyboard Shortcuts tab.

**Q: Does WaveEdit support destructive editing?**
A: Non-destructive with undo/redo. Original file only overwritten when you save.

---

## License

GNU General Public License v3.0

- ✅ Use for any purpose (personal, commercial)
- ✅ Modify and distribute
- ✅ Must distribute source code with binaries
- ✅ Derivative works must also be GPL v3

See [LICENSE](LICENSE) for full details.

---

## Credits

**Developed by**: ZQ SFX

**Built with**:
- [JUCE](https://juce.com/) - Cross-platform C++ framework
- Inspired by [Sound Forge Pro](https://www.magix.com/us/music-editing/sound-forge/)

**Thanks to**:
- JUCE community
- Sound Forge Pro for setting the standard
- All contributors and testers

---

## Contact

- **Issues**: [GitHub Issues](https://github.com/themightyzq/WaveEdit/issues)
- **Discussions**: [GitHub Discussions](https://github.com/themightyzq/WaveEdit/discussions)

---

**Version**: 0.1.0-dev
**Last Updated**: 2026-01-14
