# CLAUDE.md - AI Agent Instructions for WaveEdit

## Project Overview

**WaveEdit** is a cross-platform, professional audio editor inspired by Sound Forge Pro. This is a JUCE-based C++ application designed for speed, precision, and keyboard-driven workflow. The primary goal is to create a streamlined, sample-accurate audio editing tool with minimal friction—no project files, no complex session management, just open → edit → save.

### Core Design Philosophy

1. **Speed is paramount**: Sub-1 second startup, instant file loading, 60fps waveform rendering
2. **Sample-accurate editing**: All operations must maintain sample-level precision
3. **Keyboard-first workflow**: Every action must have a keyboard shortcut
4. **Zero fluff**: No social media integration, no bloat, no unnecessary features
5. **Professional tool**: Built for audio engineers, podcasters, sound designers
6. **Sound Forge compatibility**: Default keyboard shortcuts match Sound Forge Pro

### Target Users

- Audio engineers who need quick edits
- Podcasters trimming episodes
- Sound designers cleaning up samples
- Game developers processing audio assets
- Anyone frustrated with Audacity's complexity or proprietary tools' cost

---

## Tech Stack Constraints

### Required Technologies

**Framework**: JUCE 7.x or later
- Use JUCE's audio engine (AudioFormatManager, AudioTransportSource, AudioThumbnail)
- Leverage built-in components where possible (avoid reinventing wheels)
- Follow JUCE coding patterns and best practices

**Language**: C++17 or later
- Modern C++ standards (auto, range-based loops, smart pointers)
- Avoid raw pointers except when interfacing with JUCE callbacks
- Use JUCE's reference-counted pointers (ReferenceCountedObjectPtr) where appropriate

**Build System**: CMake
- Cross-platform build configuration
- JUCE modules integrated via CMake
- Support for Windows, macOS, Linux

**License**: GPL v3
- All code must be GPL3 compatible
- Document any third-party dependencies and their licenses
- No proprietary code or assets

### Audio Processing Requirements

- **Sample rates**: Support 44.1kHz, 48kHz, 88.2kHz, 96kHz, 192kHz
- **Bit depths**: 16-bit, 24-bit, 32-bit float
- **Channels**: Mono, stereo, and multichannel (up to 8 channels)
- **File formats (MVP)**: WAV only (PCM, IEEE float)
- **File formats (Phase 2+)**: FLAC, MP3, OGG, AIFF

---

## Mandatory Coding Standards

### File Organization

```
WaveEdit/
├── Source/
│   ├── Main.cpp                    // Application entry point
│   ├── MainComponent.cpp/.h        // Top-level UI component
│   ├── Audio/
│   │   ├── AudioEngine.cpp/.h      // Playback, recording, transport
│   │   ├── AudioProcessor.cpp/.h   // DSP operations (fade, normalize, etc.)
│   │   └── AudioFileManager.cpp/.h // File I/O, format handling
│   ├── UI/
│   │   ├── WaveformDisplay.cpp/.h  // Waveform rendering component
│   │   ├── TransportControls.cpp/.h // Play/pause/stop buttons
│   │   ├── Meters.cpp/.h           // Peak/RMS level meters
│   │   ├── SettingsPanel.cpp/.h    // Preferences and shortcuts
│   │   └── CommandPalette.cpp/.h   // Cmd+K quick actions
│   ├── Commands/
│   │   ├── CommandManager.cpp/.h   // Central command registry
│   │   ├── KeyboardShortcuts.cpp/.h // Shortcut handling and customization
│   │   └── CommandIDs.h            // Enum of all command IDs
│   ├── Utils/
│   │   ├── Settings.cpp/.h         // Persistent settings (JSON)
│   │   ├── UndoManager.cpp/.h      // Multi-level undo/redo
│   │   └── Helpers.cpp/.h          // Utility functions
│   └── Assets/
│       └── (icons, fonts, etc.)
├── JuceLibraryCode/                // JUCE generated code
├── Builds/                         // Platform-specific builds
├── CMakeLists.txt
├── README.md
├── CLAUDE.md
├── TODO.md
└── LICENSE (GPL3)
```

### Naming Conventions

**Classes**: PascalCase
```cpp
class WaveformDisplay : public juce::Component { };
```

**Methods**: camelCase
```cpp
void loadAudioFile(const juce::File& file);
```

**Member variables**: camelCase with `m_` prefix (optional but preferred)
```cpp
juce::AudioFormatManager m_formatManager;
juce::AudioThumbnail m_thumbnail;
```

**Constants**: UPPER_SNAKE_CASE or kPascalCase
```cpp
constexpr int MAX_UNDO_LEVELS = 100;
constexpr double kDefaultSampleRate = 44100.0;
```

**Enums**: PascalCase for enum name, UPPER_SNAKE_CASE for values
```cpp
enum class PlaybackState {
    STOPPED,
    PLAYING,
    PAUSED
};
```

### Code Style

**Indentation**: 4 spaces (no tabs)

**Braces**: Allman style (opening brace on new line)
```cpp
void someFunction()
{
    if (condition)
    {
        doSomething();
    }
}
```

**Comments**: Use `//` for single-line, `/* */` for multi-line
- Document all public methods with Doxygen-style comments
- Explain *why*, not *what* (code should be self-documenting for *what*)

```cpp
/**
 * Loads an audio file and prepares it for editing.
 *
 * @param file The audio file to load
 * @return true if successful, false if load failed
 */
bool loadAudioFile(const juce::File& file);
```

---

## Architecture Rules

### Component Hierarchy

```
MainComponent
├── WaveformDisplay
├── TransportControls
├── Meters
└── SettingsPanel (modal/overlay)
```

**Rules**:
- Keep UI components separate from audio logic
- Use JUCE's message thread for UI updates
- Never block the audio thread
- Use `juce::ChangeBroadcaster` / `juce::ChangeListener` for component communication

### Audio Thread Safety

**CRITICAL**: Never allocate memory, acquire locks, or do I/O on the audio thread.

**Audio thread** (real-time):
- `getNextAudioBlock()` callback
- Sample processing only
- Pre-allocated buffers

**Message thread** (UI):
- File I/O
- User interaction
- UI updates
- Memory allocation

**Background threads** (loading, processing):
- File loading
- Waveform thumbnail generation
- Non-real-time DSP (normalize, fade, etc.)

### Command System

All user actions must go through the centralized command system:

```cpp
enum CommandIDs
{
    fileNew = 0x1000,
    fileOpen,
    fileSave,
    fileSaveAs,
    editUndo,
    editRedo,
    editCut,
    editCopy,
    editPaste,
    // ... etc
};
```

**Benefits**:
- Single source of truth for all actions
- Easy keyboard shortcut binding
- Undo/redo integration
- Menu/button/shortcut consistency

### Settings Management

**Settings location** (platform-specific):
- macOS: `~/Library/Application Support/WaveEdit/`
- Windows: `%APPDATA%/WaveEdit/`
- Linux: `~/.config/WaveEdit/`

**Files**:
- `settings.json` - User preferences
- `keybindings.json` - Keyboard shortcuts
- `autosave/` - Auto-save temp files

**Format**: JSON (use JUCE's `juce::var` and `juce::DynamicObject`)

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

---

## Sound Forge Compatibility

### Default Keyboard Shortcuts

**MANDATORY**: Implement these shortcuts as defaults. User can customize later.

**File Operations**
- `Ctrl+N` (Cmd+N on Mac): New data window
- `Ctrl+O`: Open file
- `Ctrl+S`: Save
- `Ctrl+Shift+S`: Save As
- `Ctrl+W`: Close window
- `Alt+Enter`: File properties

**Navigation**
- `Left/Right Arrow`: Move cursor one pixel
- `Ctrl+Left/Right`: Jump to start/end
- `Home/End`: First/last visible sample
- `Page Up/Down`: Move 10% of view
- `.` (period): Center cursor in view

**Selection**
- `Ctrl+A`: Select all
- `Shift+Left/Right`: Extend selection by pixel
- `Shift+Home/End`: Select to visible edge
- `T`: Snap to grid (future feature)
- `Z`: Snap to zero crossing

**Editing**
- `Ctrl+X`: Cut
- `Ctrl+C`: Copy
- `Ctrl+V`: Paste
- `Delete`: Delete selection
- `Ctrl+Z`: Undo
- `Ctrl+Shift+Z`: Redo

**Playback**
- `Space` or `F12`: Play/Stop
- `Enter` or `Ctrl+F12`: Play/Pause
- `Shift+Space`: Play all
- `Q`: Toggle loop
- `Ctrl+R`: Record (Phase 2)

**Processing**
- `Ctrl+N`: Normalize (conflict with New - resolve in Phase 2)
- `Ctrl+Shift+I`: Fade In
- `Ctrl+Shift+O`: Fade Out

### Shortcut Customization

Users must be able to:
1. View all shortcuts in a searchable list
2. Click to rebind any shortcut
3. See conflict warnings
4. Export/import keybindings (JSON)
5. Reset to defaults

---

## Performance Targets

### Startup Time
- **Target**: <1 second cold start (no splash screen)
- **Measure**: From launch to UI ready
- **Strategy**: Lazy-load plugins, defer non-critical initialization

### File Loading
- **Target**: <2 seconds for 10-minute WAV (44.1kHz stereo)
- **Measure**: From file selection to waveform displayed
- **Strategy**: Stream loading, progressive rendering

### Waveform Rendering
- **Target**: 60fps smooth scrolling and zooming
- **Measure**: Frame time <16ms
- **Strategy**: Multi-resolution thumbnails, GPU rendering (future)

### Playback Latency
- **Target**: <10ms output latency
- **Measure**: Time from play button to audio output
- **Strategy**: Low buffer sizes, JUCE's audio device settings

### Save Time
- **Target**: <500ms for typical files
- **Measure**: From save command to file written
- **Strategy**: Background thread, async I/O

---

## Feature Implementation Priority

### Phase 1: Core Editor (Weeks 1-2)
**Goal**: Minimal viable audio editor

**Must Have**:
1. File open (WAV only, drag & drop + file picker)
2. Waveform display (zoom, scroll, cursor)
3. Playback controls (play, pause, stop)
4. Selection (click & drag)
5. Cut, copy, paste, delete
6. Save / Save As
7. Single-level undo/redo

**UI Components**:
- WaveformDisplay with scrollbar
- Transport controls (play/pause/stop buttons)
- Time/sample position display

**Technical Requirements**:
- Use `juce::AudioFormatManager` for file I/O
- Use `juce::AudioThumbnail` for waveform rendering
- Use `juce::AudioTransportSource` for playback
- Implement basic `juce::UndoManager` integration

### Phase 2: Professional Features (Week 3)
**Goal**: Keyboard shortcuts, meters, basic effects

**Features**:
1. Customizable keyboard shortcuts (settings panel)
2. Peak meters during playback
3. Fade in/out on selection
4. Normalize entire file or selection
5. Multi-level undo (100 levels)
6. Auto-save to temp location
7. Export/import keybindings

**UI Components**:
- SettingsPanel with keybinding editor
- Meters component (peak + RMS)
- Effects parameter dialogs (if needed)

**Technical Requirements**:
- Settings stored in `~/.config/WaveEdit/keybindings.json`
- Auto-save every N minutes (user configurable)
- Effect processing on background thread

### Phase 3: Polish (Week 4)
**Goal**: Professional workflow features

**Features**:
1. Numeric position entry (jump to sample/time)
2. Snap to zero-crossing toggle
3. Keyboard shortcut cheat sheet (overlay)
4. Recent files list with waveform thumbnails
5. Command palette (Cmd/Ctrl+K for quick actions)
6. Zoom presets (fit, selection, 1:1)

**UI Components**:
- Numeric entry fields for cursor position
- Overlay panel for shortcut cheat sheet
- Command palette modal

### Phase 4: Extended Features (Month 2+)
**Goal**: Multi-format, recording, analysis

**Features**:
1. Multi-format support (FLAC, MP3, OGG, AIFF)
2. Recording from audio input
3. Spectrum analyzer / FFT view
4. Batch processing (apply effect to multiple files)
5. VST3 plugin hosting (future)
6. Pitch shift / time stretch (future)

---

## What to Avoid

### Anti-Patterns
- **Don't create project files**: This is a file-based editor, not a DAW
- **Don't add social features**: No sharing, posting, cloud sync
- **Don't bloat the UI**: Keep it clean and functional
- **Don't ignore keyboard users**: Every action needs a shortcut
- **Don't block the UI thread**: Always use background threads for I/O

### Features to Reject
- Multi-track editing (that's a DAW, not our scope)
- MIDI support (audio-only editor)
- Video support (audio-only)
- Cloud storage integration (local files only)
- Telemetry or analytics (privacy-first)

### Performance Pitfalls
- Never load entire file into memory (stream large files)
- Don't re-render entire waveform on every zoom (use cached thumbnails)
- Avoid allocations in audio callback (pre-allocate buffers)
- Don't use synchronous file I/O on message thread (use async or background thread)

---

## Testing Requirements

### Unit Tests
- Audio processing functions (fade, normalize, DC offset removal)
- File I/O (load, save, format detection)
- Undo/redo system
- Selection logic

### Integration Tests
- Full workflow: open → edit → save
- Keyboard shortcut handling
- Settings persistence

### Manual Testing Checklist
- [ ] Startup time <1 second
- [ ] Load 10-minute WAV <2 seconds
- [ ] Smooth 60fps waveform scrolling
- [ ] Zero-latency playback start
- [ ] All default shortcuts work
- [ ] Shortcuts customizable in settings
- [ ] Auto-save works and restores
- [ ] No audio glitches during playback
- [ ] Undo/redo works for all operations

---

## Build Instructions (for AI reference)

### Prerequisites
- CMake 3.15+
- JUCE 7.x (via git submodule or system install)
- C++17 compiler (GCC, Clang, MSVC)

### Build Steps
```bash
# Clone repo
git clone https://github.com/yourusername/waveedit.git
cd waveedit

# Initialize JUCE submodule (if using submodules)
git submodule update --init --recursive

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Run
./WaveEdit  # or WaveEdit.exe on Windows
```

---

## Communication Guidelines for AI Agents

### When Implementing Features

1. **Read TODO.md first**: Check current phase and task priority
2. **Follow the architecture**: Use the prescribed component structure
3. **Match the style**: Follow naming conventions and code formatting
4. **Test thoroughly**: Verify performance targets are met
5. **Update docs**: Add comments, update README if needed

### When Stuck

1. **Check JUCE docs**: https://docs.juce.com/
2. **Search JUCE forum**: https://forum.juce.com/
3. **Review existing code**: Look for similar patterns in the codebase
4. **Ask the user**: If truly blocked, ask for clarification

### When Suggesting Changes

1. **Justify with performance data**: "This improves startup time by 200ms"
2. **Align with philosophy**: "This makes the keyboard workflow faster"
3. **Consider scope**: "This is Phase 2 feature, should we defer?"
4. **Propose alternatives**: "We could use X or Y, here are tradeoffs"

---

## Quality Standards

### Code Quality
- All public methods documented
- No compiler warnings
- No memory leaks (use Valgrind or Instruments)
- Thread-safe audio code

### User Experience
- Every action completes in <100ms (perceived instant)
- No UI freezes or stuttering
- Clear error messages (never crash silently)
- Keyboard shortcuts discoverable (cheat sheet, tooltips)

### Performance
- Startup: <1s
- File load: <2s for 10min WAV
- Rendering: 60fps
- Playback latency: <10ms

---

## Version Control

### Commit Messages
Follow conventional commits:
```
feat: Add waveform zoom controls
fix: Resolve playback glitch on Windows
perf: Optimize thumbnail rendering (60fps achieved)
docs: Update keyboard shortcuts in README
```

### Branching
- `main`: Stable, always builds
- `develop`: Integration branch for features
- `feature/*`: Individual features
- `bugfix/*`: Bug fixes

### Pull Requests
- Reference TODO.md task
- Include performance measurements if relevant
- Add screenshots for UI changes

---

## Final Notes

This is a **professional audio tool**, not a toy. Users expect reliability, speed, and precision. Every decision should prioritize:

1. **Speed**: Fast startup, fast operations, fast workflow
2. **Accuracy**: Sample-perfect editing, no quality loss
3. **Usability**: Keyboard-driven, discoverable, customizable
4. **Simplicity**: No bloat, no distractions, just editing

When in doubt, ask: "Would Sound Forge Pro do this?" If yes, implement it. If no, skip it.

---

**Last Updated**: 2025-10-06
**Project Start**: 2025-10-06
**Current Phase**: Phase 1 (Core Editor)
