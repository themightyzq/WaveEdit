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
7. **Streamlined UI**: Clean, minimal, accessible interface that stays out of the way

### UI Design Principles

**MANDATORY: All UI elements must follow these accessibility and design guidelines:**

1. **Icons over text**: Use universally recognized icons instead of text labels
   - Transport controls: Standard play (▶), pause (⏸), stop (⏹), loop (🔁) icons
   - Actions: Use symbolic icons that are instantly recognizable
   - Text labels only for tooltips and accessibility descriptions

2. **Minimal visual clutter**: Every pixel serves a purpose
   - No decorative elements
   - Consistent spacing and alignment
   - Generous whitespace for breathing room
   - Dark theme as default (easier on eyes during long editing sessions)

3. **Accessibility first**:
   - All controls must be keyboard accessible
   - Tooltips for every interactive element
   - High contrast between foreground and background
   - Support for screen readers (JUCE accessibility API)
   - Minimum touch target size: 44x44 pixels (even for desktop apps)

4. **Visual hierarchy**:
   - Primary actions (play, record) should be most prominent
   - Secondary actions (settings, preferences) should be subtle but discoverable
   - Destructive actions (delete, close without saving) should require confirmation

5. **Responsive design**:
   - UI must scale gracefully from 1024x768 to 4K displays
   - Support for HiDPI/Retina displays (2x, 3x scaling)
   - Layout should adapt to window resizing without breaking

6. **Professional aesthetic**:
   - Inspired by professional audio tools (Sound Forge, Pro Tools, Reaper)
   - Flat, modern design (no skeuomorphism)
   - Consistent color palette throughout
   - Monospace fonts for numeric displays (time, samples, dB)

**Icon Design Standards**:
- Use SVG paths for scalability (no raster images for UI controls)
- Standard transport icon shapes (play = right triangle, pause = two bars, stop = square)
- Icon stroke width: 2-3px for optimal visibility
- Icon size: 24x24px base, scale to 32x32px or 48x48px as needed
- Icon color: Match application theme, with state-based color changes

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

### Quick Start - Build and Run Script ⭐

**The `build-and-run.command` script is the SOURCE OF TRUTH for building and launching WaveEdit.**

This script handles all build steps automatically:

```bash
# From project root - simplest usage
./build-and-run.command

# Clean build from scratch
./build-and-run.command clean

# Build Debug version
./build-and-run.command debug

# Just run existing binary (skip build)
./build-and-run.command run-only

# Clean Debug build
./build-and-run.command clean debug

# Show help
./build-and-run.command help
```

**Features:**
- ✅ Checks prerequisites (CMake, compiler, JUCE)
- ✅ Initializes JUCE submodule if needed
- ✅ Configures CMake with correct settings
- ✅ Builds with optimal parallel jobs
- ✅ Handles platform differences (macOS, Linux, Windows)
- ✅ Provides clear error messages and colored output
- ✅ Offers to launch app after successful build

**Always use this script** instead of manual CMake commands to ensure consistent builds across different environments.

### Manual Build Steps (Advanced)

If you need to build manually without the script:

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

# Run (macOS)
./WaveEdit_artefacts/Release/WaveEdit.app/Contents/MacOS/WaveEdit

# Run (Linux)
./WaveEdit_artefacts/Release/WaveEdit

# Run (Windows)
.\WaveEdit_artefacts\Release\WaveEdit.exe
```

**Note:** The build-and-run.command script is recommended over manual builds.

---

## Communication Guidelines for AI Agents

### When Implementing Features

1. **Read TODO.md first**: Check current phase and task priority
2. **Follow the architecture**: Use the prescribed component structure
3. **Match the style**: Follow naming conventions and code formatting
4. **Test thoroughly**: Verify performance targets are met
5. **Update docs**: Add comments, update README if needed

### 🚨 MANDATORY Quality Control Process

**CRITICAL**: Before marking ANY task as complete in TODO.md, you MUST follow this process:

#### Step 1: Code Review (Required)
- **Use the `code-reviewer` agent** to review ALL code you wrote
- The agent will check for bugs, performance issues, thread safety, and best practices
- **If the agent finds issues**: Fix them immediately. Do NOT proceed to step 2.
- **Only proceed** when the code-reviewer approves the implementation

#### Step 2: Functional Testing (Required)
- **Use the `test-runner` agent** (or appropriate testing agent) to validate functionality
- Test the COMPLETE workflow, not just the isolated component
- **Integration test**: Verify the feature works end-to-end with other components
- **If tests fail**: Fix the issues. Do NOT mark the task complete.
- **Only proceed** when all tests pass

#### Step 3: Manual Verification (Required)
- **Build and run the application** (`./build-and-run.command`)
- **Manually test** the feature as a user would
- **Verify visual feedback**: Does the UI update correctly?
- **Verify audio feedback**: Does playback work as expected?
- **Check for errors**: Review console output for warnings/errors

#### Step 4: Assessment Decision
- ✅ **PASS**: Code reviewed, tests pass, manual test successful → Mark task as complete
- ❌ **FAIL**: Any issues found → Go back to Step 1, fix issues, repeat process
- ⚠️ **PARTIAL**: Infrastructure done but not integrated → Mark as "⚠️ Infrastructure complete, integration needed"

**NEVER** mark a task as "✅ Complete" if:
- You haven't run the code-reviewer agent
- You haven't tested it end-to-end
- You only implemented infrastructure without integration
- Tests are failing
- You encountered errors you couldn't resolve
- The feature doesn't work when you manually test it

**Example of WRONG process** (what caused our current issues):
```
❌ Implemented AudioBufferManager ✅
❌ Implemented AudioClipboard ✅
❌ Implemented Cut/Copy/Paste ✅
(But didn't verify playback works with edited buffer!)
```

**Example of CORRECT process**:
```
1. ✅ Implement AudioEngine buffer playback
2. ✅ Run code-reviewer agent → Found thread safety issue → Fixed
3. ✅ Run test-runner agent → All tests pass
4. ✅ Manual test: Load file → Edit → Play → HEAR edits ✓
5. ✅ Mark task complete in TODO.md
```

**If you skip this process, you WILL create the exact problem we have now**: features that appear complete but don't actually work.

---

### 🛠️ MANDATORY Implementation Standards - No Shortcuts Allowed

**CRITICAL**: When fixing bugs or implementing features, do it RIGHT, not FAST.

#### The "Right Way" Philosophy

**We build professional tools. Quick fixes and bandaids are NOT acceptable.**

Every implementation must follow these principles:

#### 1. Proper Architecture Over Quick Hacks

❌ **WRONG - Bandaid Solution**:
```cpp
// Quick hack: Store edited buffer in global variable
AudioBuffer<float> g_editedBuffer; // BAD!

void playEditedAudio() {
    if (g_editedBuffer.getNumSamples() > 0)
        playFromBuffer(g_editedBuffer); // Hacky workaround
}
```

✅ **RIGHT - Proper Architecture**:
```cpp
// Proper solution: AudioEngine manages playback state
class AudioEngine {
    enum PlaybackSource { FILE, BUFFER };
    PlaybackSource m_currentSource;

    void loadFromBuffer(AudioBuffer<float>& buffer, double sampleRate);
    void switchToBufferPlayback(); // Clean state transition
};
```

**Why**: The right way is maintainable, testable, and won't break later.

#### 2. Thread Safety - No Compromise

❌ **WRONG - Race Condition**:
```cpp
// Unsafe: Accessing buffer from both threads
void audioCallback() {
    float* data = m_editedBuffer.getWritePointer(0); // CRASH RISK!
}

void pasteAudio() {
    m_editedBuffer.setSize(2, newSize); // Called from UI thread - RACE!
}
```

✅ **RIGHT - Proper Locking**:
```cpp
void audioCallback() {
    juce::ScopedLock lock(m_bufferLock);
    if (m_bufferReady)
        float* data = m_editedBuffer.getReadPointer(0); // Safe
}

void pasteAudio() {
    juce::ScopedLock lock(m_bufferLock);
    m_editedBuffer.setSize(2, newSize); // Thread-safe
    m_bufferReady = true;
}
```

**Rule**: If it touches audio data, it MUST be thread-safe. No exceptions.

#### 3. Error Handling - Handle ALL Cases

❌ **WRONG - Ignoring Errors**:
```cpp
bool saveFile(const File& file) {
    auto writer = format.createWriterFor(stream); // Might be nullptr!
    writer->write(buffer); // CRASH if writer is null
    return true; // Lying about success
}
```

✅ **RIGHT - Comprehensive Error Handling**:
```cpp
bool saveFile(const File& file) {
    auto writer = format.createWriterFor(stream);
    if (writer == nullptr) {
        juce::Logger::writeToLog("Failed to create audio writer");
        showErrorDialog("Could not save file: Unsupported format");
        return false;
    }

    if (!writer->write(buffer)) {
        juce::Logger::writeToLog("Failed to write audio data");
        showErrorDialog("Could not save file: Disk full or write error");
        return false;
    }

    return true;
}
```

**Rule**: Every operation that can fail MUST handle failure gracefully.

#### 4. Memory Management - Use RAII, Not Manual Management

❌ **WRONG - Manual Memory Management**:
```cpp
AudioFormatReader* reader = new WavAudioFormatReader(...);
// ... code that might throw ...
delete reader; // Might leak if exception thrown!
```

✅ **RIGHT - RAII with Smart Pointers**:
```cpp
std::unique_ptr<AudioFormatReader> reader(format.createReaderFor(stream));
if (reader == nullptr) return false;
// Automatically cleaned up, exception-safe
```

**Rule**: Never use `new`/`delete` directly. Use `std::unique_ptr`, `std::make_unique`, or JUCE's reference counting.

#### 5. No TODOs for Critical Functionality

❌ **WRONG - Deferring Critical Code**:
```cpp
void saveFile() {
    // TODO: Actually implement file writing later
    juce::AlertWindow::showMessageBox("Save not implemented yet");
}
```

✅ **RIGHT - Implement It Properly Now**:
```cpp
void saveFile() {
    if (!validateBuffer()) {
        showError("No audio data to save");
        return;
    }

    auto writer = createWavWriter(file, sampleRate, numChannels, bitDepth);
    if (!writer->writeFromAudioBuffer(m_buffer, 0, m_buffer.getNumSamples())) {
        showError("Failed to write audio file");
        return;
    }

    m_isModified = false;
    juce::Logger::writeToLog("File saved successfully");
}
```

**Rule**: If it's critical functionality (save, playback, editing), implement it fully. TODOs are only for polish/optimization, not core features.

#### 6. Follow JUCE Best Practices

**Always**:
- ✅ Use JUCE's message thread for UI updates
- ✅ Never allocate memory in audio callbacks
- ✅ Use `AudioFormatManager` for file I/O
- ✅ Use `AudioThumbnail` for waveform display (don't reinvent)
- ✅ Follow JUCE naming conventions

**Never**:
- ❌ Block the audio thread with I/O operations
- ❌ Access UI components from audio thread
- ❌ Mix raw pointers with JUCE's reference counting
- ❌ Ignore JUCE's thread safety guidelines

#### 7. Performance - Optimize Correctly, Not Prematurely

❌ **WRONG - Premature/Dangerous Optimization**:
```cpp
// Skipping bounds checking to "save time"
float sample = buffer.getWritePointer(0)[i]; // No validation - CRASH RISK!
```

✅ **RIGHT - Correct Optimization**:
```cpp
// Profile first, optimize bottlenecks only
if (i < buffer.getNumSamples()) { // Safe bounds check
    float sample = buffer.getSample(0, i); // JUCE handles it
}
// If profiling shows this is slow, THEN optimize with batch processing
```

**Rule**: Make it work correctly first. Profile to find bottlenecks. Then optimize only the proven slow parts.

#### 8. Integration Over Isolation

❌ **WRONG - Building Components in Isolation**:
```cpp
// Implemented AudioBufferManager ✅
// Implemented AudioClipboard ✅
// But they don't integrate with playback! (Our current problem)
```

✅ **RIGHT - End-to-End Integration**:
```cpp
// Implementation plan:
1. Implement AudioBufferManager
2. Integrate with AudioEngine for playback
3. Integrate with WaveformDisplay for visualization
4. Test: Edit → Hear → See → Save (complete workflow)
5. THEN mark complete
```

**Rule**: A component isn't "done" until it's integrated and the user can use it end-to-end.

---

### When to Reject a Solution (Even Your Own)

**Reject any solution that**:
- Uses global variables for state management
- Has race conditions or thread safety issues
- Ignores error cases ("it should work most of the time")
- Uses `// TODO: Fix this properly later` for critical functionality
- Requires manual memory management (raw new/delete)
- Violates JUCE's architecture patterns
- Works in testing but will break in edge cases
- Is a "temporary workaround" that becomes permanent

**Ask yourself**:
- Would I trust this code in a shipping product?
- Will this solution still be correct in 6 months?
- Does this follow the framework's (JUCE's) recommended patterns?
- Have I tested all failure modes, not just the happy path?
- Would a professional C++ audio engineer approve this code?

**If the answer to any of these is "no", go back and do it right.**

---

### The Cost of Shortcuts

**"Quick fix" now = Technical debt later**:
- Band-aids require refactoring before adding features
- Hacks break when requirements change
- Workarounds confuse future contributors
- Race conditions cause random crashes
- Memory leaks accumulate over time

**Doing it right now**:
- ✅ Works today, works tomorrow, works in production
- ✅ Easy to extend and maintain
- ✅ Other developers can understand it
- ✅ Users never encounter mysterious bugs
- ✅ Code reviewer agent will approve it

**Time investment**:
- Quick hack: 30 minutes now + 4 hours debugging later = 4.5 hours total
- Proper solution: 2 hours now + 0 hours debugging = 2 hours total

**ALWAYS choose the proper solution.**

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

**IMPORTANT**: All code must pass the **MANDATORY Quality Control Process** before being marked complete. See "Communication Guidelines for AI Agents" section above.

**CRITICAL**: All implementations must follow **"MANDATORY Implementation Standards - No Shortcuts Allowed"**. We build professional tools, not prototypes. Quick fixes and band-aids are unacceptable.

### Code Quality
- All public methods documented
- No compiler warnings
- No memory leaks (use Valgrind or Instruments)
- Thread-safe audio code (no race conditions, proper locking)
- Proper error handling (handle ALL failure cases)
- RAII/smart pointers (no raw new/delete)
- Follow JUCE best practices and patterns
- **Code-reviewer agent approval required** before marking complete
- **No TODOs for critical functionality** - implement it properly now

### User Experience
- Every action completes in <100ms (perceived instant)
- No UI freezes or stuttering
- Clear error messages (never crash silently)
- Keyboard shortcuts discoverable (cheat sheet, tooltips)
- **Manual testing required** to verify user-facing functionality

### Performance
- Startup: <1s
- File load: <2s for 10min WAV
- Rendering: 60fps
- Playback latency: <10ms
- **Test-runner agent verification required** for performance claims

### Integration Testing
- **NEVER mark a feature complete without end-to-end testing**
- Components must work together, not just in isolation
- Verify the complete user workflow: Open → Edit → Playback → Save
- If you can't hear/see/save your changes, the feature is NOT complete

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

**Last Updated**: 2025-10-06 (Code Review Update)
**Project Start**: 2025-10-06
**Current Phase**: Phase 1 (Core Editor) - 70% Complete
**Current Status**: ⚠️ **CRITICAL INTEGRATIONS NEEDED FOR MVP**

---

## 🚨 CRITICAL IMPLEMENTATION STATUS - READ THIS FIRST

**Before implementing ANY new features, you MUST understand the current state:**

### What's Working ✅
- **Build System**: Clean builds, no errors, 63-second build time
- **Audio Engine**: File loading, playback, transport controls all functional
- **UI Components**: Waveform display, transport controls, menus, status bar all implemented
- **Selection System**: Click-drag selection with proper visual highlighting
- **Infrastructure**: AudioBufferManager and AudioClipboard are implemented and thread-safe

### What's NOT Working 🚨 (3 Critical Blockers)

**BLOCKER #1: AudioEngine Doesn't Play from Edited Buffer**
- **Location**: `Source/Audio/AudioEngine.h/.cpp`
- **Problem**: After cut/paste/delete, playback still uses original file
- **Impact**: Users can't HEAR their edits
- **Fix Needed**: Add `loadFromBuffer()` method and buffer playback mode
- **Estimated Effort**: 3-4 hours
- **TODO Comments**: Lines 278, 315 in Source/Main.cpp

**BLOCKER #2: WaveformDisplay Doesn't Update After Edits**
- **Location**: `Source/UI/WaveformDisplay.h/.cpp`
- **Problem**: After paste/delete, waveform display doesn't regenerate
- **Impact**: Users can't SEE their edits
- **Fix Needed**: Add `reloadFromBuffer()` method to regenerate AudioThumbnail from buffer
- **Estimated Effort**: 2-3 hours
- **TODO Comments**: Lines 279, 316 in Source/Main.cpp

**BLOCKER #3: Save/SaveAs Are Placeholder Dialogs**
- **Location**: `Source/Audio/AudioFileManager.h/.cpp`, `Source/Main.cpp` lines 877-966
- **Problem**: Save functions show alert dialogs instead of writing files
- **Impact**: Users can't KEEP their edits (all work is lost!)
- **Fix Needed**: Implement WAV file writer using JUCE's WavAudioFormat
- **Estimated Effort**: 2-3 hours

### Partially Working ⚠️

**Edit Operations (Cut/Copy/Paste/Delete)**:
- ✅ Infrastructure exists (AudioBufferManager, AudioClipboard)
- ✅ Operations modify the internal buffer correctly
- ✅ UI updates (modified flag *, clipboard status)
- ❌ Playback doesn't use edited buffer (BLOCKER #1)
- ❌ Waveform doesn't update visually (BLOCKER #2)
- ❌ Can't save edits to disk (BLOCKER #3)

**What this means**: You can cut/copy/paste, but you won't hear, see, or be able to save the results.

### What to Do Next

**IMMEDIATE PRIORITY**: Complete the 3 BLOCKERS before adding any new features.

**⚠️ IMPLEMENTATION REQUIREMENT**: When fixing these blockers, **do it RIGHT, not FAST**. See "MANDATORY Implementation Standards - No Shortcuts Allowed" section below. No band-aids, no workarounds, no global variables, no race conditions. Professional, maintainable code only.

**DO NOT**:
- Add new features to Phase 2 or Phase 3
- Implement undo/redo (deferred to Phase 2)
- Work on DSP effects
- Add multi-format support
- **Use quick hacks or temporary workarounds**
- **Skip thread safety to "save time"**
- **Ignore error handling**

**DO**:
1. Implement AudioEngine buffer playback (BLOCKER #1)
2. **MANDATORY**: Run code-reviewer and test-runner agents on your implementation
3. Implement WaveformDisplay buffer rendering (BLOCKER #2)
4. **MANDATORY**: Run code-reviewer and test-runner agents on your implementation
5. Implement Save/SaveAs file writing (BLOCKER #3)
6. **MANDATORY**: Run code-reviewer and test-runner agents on your implementation
7. Test the complete workflow: Open → Edit → Play → See → Save
8. THEN move to Phase 1 validation testing

**See "MANDATORY Quality Control Process" section below for detailed testing requirements.**

**Integration Testing Checklist** (after completing blockers):
```
1. Open a WAV file
2. Make a selection (click-drag)
3. Press Cmd+X (cut)
4. Press Space (play) → Should hear edited audio ✅
5. Look at waveform → Should see updated waveform ✅
6. Press Cmd+S (save) → Should write to disk ✅
7. Close file and reopen → Edits should persist ✅
```

If ANY of these steps fail, the blockers are not complete.

---

## Project Architecture (Current Implementation)
