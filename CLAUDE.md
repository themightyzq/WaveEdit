# CLAUDE.md - AI Agent Instructions for WaveEdit by ZQ SFX

## Company Information

**Developer**: ZQ SFX
**Copyright**: © 2025 ZQ SFX
**License**: GPL v3
**Project**: WaveEdit - Professional Audio Editor

All code files, documentation, and branding materials must reflect ZQ SFX ownership.

---

## 📄 Documentation Structure & Purpose

**MANDATORY**: These three documentation files have distinct purposes and must be maintained accordingly:

### **CLAUDE.md (This File)** - AI Operations Manual
**Purpose**: Instruction set and ruleset for AI agents to follow when working on this project.

**Contains**:
- Binding rules and architectural constraints
- Coding standards and mandatory patterns
- Task execution logic and quality control processes
- Design philosophy and technical requirements
- Architecture patterns and implementation examples
- Case studies and lessons learned from past issues

**Tone**: Authoritative, prescriptive. This is a contract, not a guide.

**What NOT to include**:
- ❌ Current project status or phase progress (belongs in TODO.md)
- ❌ Feature lists or roadmaps (belongs in TODO.md)
- ❌ Build instructions for end users (belongs in README.md)
- ❌ Installation guides (belongs in README.md)

**Analogy**: This is the law book. It tells AI agents what they must and must not do.

---

### **README.md** - Public Landing Page
**Purpose**: Introductory overview for humans encountering the project (users, contributors, evaluators).

**Contains**:
- Project description and goals
- Installation and build instructions
- Usage examples and quick start guide
- Feature highlights (what works now)
- File/folder structure overview
- Credits, license, and contact information

**Tone**: Informative and welcoming. For end users and potential contributors.

**Analogy**: This is the front door. It explains what the project is and how to use it.

---

### **TODO.md** - Developer Task Tracker
**Purpose**: Running list of tasks, phases, priorities, and project status.

**Contains**:
- Current project status and phase progress
- Task lists with priorities and time estimates
- Bug tracking and issue documentation
- Phase roadmaps and feature planning
- Code review results and quality metrics
- Backlog items and future enhancements

**Tone**: Pragmatic, developer-facing. Actionable tasks and status updates.

**Analogy**: This is the clipboard. Tracks work in progress, priorities, and next steps.

---

## Project Overview

**WaveEdit** is a cross-platform, professional audio editor by ZQ SFX, inspired by Sound Forge Pro. This is a JUCE-based C++ application designed for speed, precision, and keyboard-driven workflow. The primary goal is to create a streamlined, sample-accurate audio editing tool with minimal friction—no project files, no complex session management, just open → edit → save.

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

### Real-Time Buffer Updates During Playback

**CRITICAL KNOWLEDGE**: AudioTransportSource maintains internal audio buffers for smooth playback.

**Problem**: When you update the underlying audio buffer (e.g., during gain adjustment), the AudioTransportSource continues playing from its cached/buffered audio, NOT the newly updated buffer. This causes:
- Visual waveform updates work (you see the change)
- Audio doesn't change during playback (you don't hear the change)
- Edits only become audible after stopping and restarting playback

**Root Cause**: AudioTransportSource pre-buffers audio for smooth playback. Simply updating the buffer in MemoryAudioSource doesn't flush these internal buffers.

**Solution** ✅: Disconnect and reconnect the AudioTransportSource to flush internal buffers:

```cpp
bool AudioEngine::reloadBufferPreservingPlayback(const juce::AudioBuffer<float>& buffer,
                                                  double sampleRate, int numChannels)
{
    // 1. Capture current playback state
    bool wasPlaying = isPlaying();
    double currentPosition = getCurrentPosition();

    // 2. CRITICAL: Disconnect transport to flush internal buffers
    m_transportSource.setSource(nullptr);

    // 3. Update buffer with new audio data
    m_bufferSource->setBuffer(buffer, sampleRate, false);

    // 4. Reconnect transport - forces reading fresh audio from updated buffer
    m_transportSource.setSource(m_bufferSource.get(), 0, nullptr, sampleRate, numChannels);

    // 5. Restore playback position and restart if was playing
    if (wasPlaying)
    {
        m_transportSource.setPosition(currentPosition);
        m_transportSource.start();
    }

    return true;
}
```

**Key Insight**: The disconnect (`setSource(nullptr)`) / reconnect cycle flushes AudioTransportSource's internal buffers, forcing it to read fresh audio from the updated buffer source. Without this, the transport continues playing stale cached audio indefinitely.

**When to Use**: Any time you need to update the audio buffer during playback:
- Real-time gain adjustment
- Real-time effects processing
- Live parameter automation
- Buffer swapping during playback

**Thread Safety**: This method MUST be called from the message thread only (asserted in code).

**Implementation Files**:
- `Source/Audio/AudioEngine.h` - Method declaration
- `Source/Audio/AudioEngine.cpp` - Implementation (lines 408-491)

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

### Audio-Unit Navigation System

**CRITICAL ARCHITECTURE REQUIREMENT**: WaveEdit uses audio-unit based navigation and selection, NOT pixel-based.

**Why This Matters**:
- **Pixel-based selection** (WRONG): Selection accuracy depends on zoom level
  - At 10% zoom: 1 pixel = 100 samples → Poor accuracy
  - At 1000% zoom: 1 pixel = 1 sample → Perfect accuracy
  - **INCONSISTENT BEHAVIOR** - unprofessional

- **Audio-unit selection** (CORRECT): Selection accuracy is always sample-perfect
  - At any zoom level: Selection boundaries are exact sample positions
  - **CONSISTENT BEHAVIOR** - professional

**Implementation Pattern**:

```cpp
// WRONG - Pixel-based (DO NOT USE):
void mouseDown(const MouseEvent& event) {
    int x = event.x;  // Pixel position
    double time = xToTime(x);  // Quantized to pixel boundaries!
    setSelection(startTime, time);  // Accuracy depends on zoom
}

// CORRECT - Audio-unit based:
void mouseDown(const MouseEvent& event) {
    int x = event.x;  // Pixel position (approximate)
    double approxTime = xToTime(x);  // Convert to approximate time
    int64_t sample = timeToSample(approxTime, m_sampleRate);  // Exact sample
    int64_t snappedSample = snapToUnit(sample, m_snapMode, m_snapIncrement);  // Apply snap
    double exactTime = sampleToTime(snappedSample, m_sampleRate);  // Exact time
    setSelection(startTime, exactTime);  // Sample-accurate at ANY zoom!
}
```

**Required Components**:

1. **AudioUnits Namespace** (`Source/Utils/AudioUnits.h`)
   - Conversion functions: samples ↔ milliseconds ↔ seconds ↔ frames
   - Snap functions: snapToUnit(), snapToZeroCrossing()
   - Enum types: SnapMode, UnitType

2. **Navigation Preferences Class** (`Source/Utils/NavigationPreferences.h`)
   - Store user preferences (default increment, snap mode, frame rate)
   - Persist to settings.json

3. **WaveformDisplay Integration**
   - `navigateLeft/Right()` methods using audio-unit increments
   - `setSnapMode()` method to change snapping behavior
   - Mouse selection using audio-unit conversion pipeline

**Mandatory for Phase 1**: Navigation and selection MUST be audio-unit based before moving to Phase 2.

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

### Multi-File Architecture (Phase 3)

**CRITICAL REQUIREMENT**: WaveEdit must support loading and editing multiple audio files simultaneously with tab-based navigation.

**Why This Matters**:
- Professional workflow requires comparing/editing multiple files
- Copy/paste between files is essential for sound design
- Each file needs independent state (undo history, playback position, selection, regions)
- Tab system matches industry standards (Pro Tools, Reaper, Adobe Audition)

**Architecture Pattern**: Document/DocumentManager

```cpp
// Document.h - Represents a single audio file with all associated state
class Document
{
public:
    Document(const juce::File& file);
    ~Document();

    // File information
    juce::String getFilename() const;
    juce::File getFile() const;
    bool isModified() const;
    void setModified(bool modified);

    // Audio engine (per-document playback/editing)
    AudioEngine& getAudioEngine();
    WaveformDisplay& getWaveformDisplay();
    RegionManager& getRegionManager();

    // Undo/redo (independent per file)
    juce::UndoManager& getUndoManager();

    // Playback state (independent per file)
    double getPlaybackPosition() const;
    void setPlaybackPosition(double position);

    // Selection state (independent per file)
    SelectionRange getSelection() const;
    void setSelection(const SelectionRange& range);

private:
    juce::File m_file;
    bool m_isModified;
    std::unique_ptr<AudioEngine> m_audioEngine;
    std::unique_ptr<WaveformDisplay> m_waveformDisplay;
    std::unique_ptr<RegionManager> m_regionManager;
    juce::UndoManager m_undoManager;
    double m_playbackPosition;
    SelectionRange m_selection;
};

// DocumentManager.h - Manages multiple open documents with tab interface
class DocumentManager
{
public:
    DocumentManager();
    ~DocumentManager();

    // Document lifecycle
    Document* createDocument(const juce::File& file);
    Document* openDocument(const juce::File& file);
    bool closeDocument(int index);
    bool closeAllDocuments();

    // Tab management
    Document* getCurrentDocument();
    int getCurrentDocumentIndex() const;
    void setCurrentDocumentIndex(int index);
    int getNumDocuments() const;
    Document* getDocument(int index);

    // Inter-file clipboard (copy/paste between documents)
    void copyToInterFileClipboard(const juce::AudioBuffer<float>& buffer, double sampleRate);
    bool pasteFromInterFileClipboard(Document* targetDoc);
    bool hasInterFileClipboard() const;

    // Tab navigation shortcuts
    void selectNextDocument();    // Cmd+Option+Right
    void selectPreviousDocument(); // Cmd+Option+Left
    void selectDocumentByIndex(int index); // Cmd+1 through Cmd+9

private:
    juce::OwnedArray<Document> m_documents;
    int m_currentDocumentIndex;

    // Inter-file clipboard
    juce::AudioBuffer<float> m_interFileClipboard;
    double m_interFileClipboardSampleRate;
    bool m_hasInterFileClipboard;
};
```

**Implementation Requirements**:

1. **Tab Component** (`Source/UI/TabComponent.h/.cpp`)
   - Visual tab bar at top of window
   - Close button on each tab (X icon)
   - Modified indicator (• dot) when file has unsaved changes
   - Right-click menu: Close, Close Others, Close All, Save, Save As
   - Drag-to-reorder tabs (optional Phase 4)

2. **State Isolation**
   - Each Document has independent AudioEngine instance
   - Each Document has independent WaveformDisplay component
   - Each Document has independent UndoManager (100 actions per file)
   - Each Document has independent RegionManager
   - Switching tabs swaps UI components and audio routing

3. **Inter-File Clipboard**
   - Separate clipboard for copy/paste between files
   - Preserves sample rate information for resampling on paste
   - Cmd+Shift+C: Copy to inter-file clipboard
   - Cmd+Shift+V: Paste from inter-file clipboard

4. **Tab Navigation Shortcuts**
   - Cmd+Option+Right: Next tab
   - Cmd+Option+Left: Previous tab
   - Cmd+1 through Cmd+9: Jump to tab 1-9
   - Cmd+W: Close current tab (with save prompt if modified)

5. **Save Prompt on Close**
   - If document is modified, show save dialog before closing
   - "Save", "Don't Save", "Cancel" options
   - Apply to tab close, window close, quit application

**Files to Create**:
- `Source/Utils/Document.h/.cpp` - Document class
- `Source/Utils/DocumentManager.h/.cpp` - DocumentManager class
- `Source/UI/TabComponent.h/.cpp` - Tab bar UI component

**Files to Modify**:
- `Source/MainComponent.h/.cpp` - Integrate TabComponent and DocumentManager
- `Source/Commands/CommandIDs.h` - Add tab-related command IDs
- `Source/Main.cpp` - Add tab navigation shortcuts

**Estimated Time**: 12-16 hours

---

### Region System Architecture (Phase 3)

**CRITICAL REQUIREMENT**: WaveEdit must support named regions/markers for organizing audio content and enabling advanced workflows.

**Why This Matters**:
- Professional workflow for organizing long files (podcasts, voiceovers, music)
- Essential for batch export (export each region as separate file)
- Industry standard feature (Sound Forge, Pro Tools, Reaper all have regions)
- "Select inverse" enables advanced editing (e.g., delete all silence, keep only dialog)

**Region Data Structure**:

```cpp
// Region.h - Represents a named audio region with start/end positions
class Region
{
public:
    Region(const juce::String& name, int64_t startSample, int64_t endSample);

    juce::String getName() const;
    void setName(const juce::String& name);

    int64_t getStartSample() const;
    void setStartSample(int64_t sample);

    int64_t getEndSample() const;
    void setEndSample(int64_t sample);

    int64_t getLengthInSamples() const;
    double getLengthInSeconds(double sampleRate) const;

    juce::Colour getColor() const;
    void setColor(const juce::Colour& color);

    juce::var toJSON() const;
    static Region fromJSON(const juce::var& json);

private:
    juce::String m_name;
    int64_t m_startSample;
    int64_t m_endSample;
    juce::Colour m_color;
};

// RegionManager.h - Manages collection of regions for a document
class RegionManager
{
public:
    RegionManager();
    ~RegionManager();

    // Region lifecycle
    void addRegion(const Region& region);
    void removeRegion(int index);
    void removeAllRegions();

    // Region access
    int getNumRegions() const;
    Region* getRegion(int index);
    const Region* getRegion(int index) const;

    // Region navigation
    int findRegionAtSample(int64_t sample) const;
    void selectRegion(int index);
    void selectInverseOfRegions(); // Select everything EXCEPT regions

    // Persistence (JSON sidecar files)
    bool saveToFile(const juce::File& audioFile) const;
    bool loadFromFile(const juce::File& audioFile);

    // Auto-region creation (Strip Silence)
    void autoCreateRegions(const juce::AudioBuffer<float>& buffer,
                           double sampleRate,
                           float thresholdDB,
                           float minRegionLengthMs,
                           float minSilenceLengthMs,
                           float preRollMs,
                           float postRollMs);

private:
    juce::Array<Region> m_regions;
    int m_selectedRegionIndex;
};
```

**Persistence Format** (JSON Sidecar File):

```json
// example.wav -> example.wav.regions.json
{
    "version": "1.0",
    "audioFile": "example.wav",
    "sampleRate": 44100,
    "regions": [
        {
            "name": "Intro",
            "startSample": 0,
            "endSample": 132300,
            "color": "ff0000"
        },
        {
            "name": "Verse 1",
            "startSample": 132300,
            "endSample": 264600,
            "color": "00ff00"
        }
    ]
}
```

**Auto-Region Creation Algorithm** (Strip Silence):

```cpp
void RegionManager::autoCreateRegions(const juce::AudioBuffer<float>& buffer,
                                       double sampleRate,
                                       float thresholdDB,
                                       float minRegionLengthMs,
                                       float minSilenceLengthMs,
                                       float preRollMs,
                                       float postRollMs)
{
    // Convert parameters to samples
    float threshold = juce::Decibels::decibelsToGain(thresholdDB);
    int minRegionSamples = (int)(minRegionLengthMs * sampleRate / 1000.0);
    int minSilenceSamples = (int)(minSilenceLengthMs * sampleRate / 1000.0);
    int preRollSamples = (int)(preRollMs * sampleRate / 1000.0);
    int postRollSamples = (int)(postRollMs * sampleRate / 1000.0);

    // Algorithm:
    // 1. Scan buffer for sections above threshold (non-silent)
    // 2. Detect silence gaps (below threshold for min silence duration)
    // 3. Create regions for non-silent sections
    // 4. Filter out regions shorter than min length
    // 5. Add pre/post-roll margins
    // 6. Name regions automatically ("Region 1", "Region 2", etc.)

    // Implementation details in Source/Audio/AutoRegionCreator.cpp
}
```

**UI Components**:

1. **Region Display** (`Source/UI/RegionDisplay.h/.cpp`)
   - Colored bars above/below waveform
   - Region names overlay
   - Click to select region
   - Double-click to rename region
   - Right-click menu: Rename, Delete, Change Color, Export

2. **Region List Panel** (`Source/UI/RegionListPanel.h/.cpp`)
   - List of all regions with start/end times
   - Click to jump to region
   - Rename inline editing
   - Drag to reorder (visual only, doesn't change region positions)

3. **Strip Silence Dialog** (`Source/UI/StripSilenceDialog.h/.cpp`)
   - Threshold slider (dB)
   - Min region length slider (ms)
   - Min silence length slider (ms)
   - Pre/post-roll sliders (ms)
   - Preview button (highlight regions without creating)
   - Create button (apply auto-region creation)

**Keyboard Shortcuts**:
- `M`: Add region at current selection
- `Shift+M`: Delete region under cursor
- `Cmd+M`: Open region list panel
- `Alt+Left/Right`: Jump to previous/next region
- `Cmd+Shift+I`: Select inverse of regions (everything NOT in regions)
- `Cmd+Shift+R`: Strip Silence dialog (auto-create regions)

**Files to Create**:
- `Source/Utils/Region.h/.cpp` - Region data structure
- `Source/Utils/RegionManager.h/.cpp` - Region collection manager
- `Source/Audio/AutoRegionCreator.h/.cpp` - Strip Silence algorithm
- `Source/UI/RegionDisplay.h/.cpp` - Visual region indicators
- `Source/UI/RegionListPanel.h/.cpp` - Region list UI
- `Source/UI/StripSilenceDialog.h/.cpp` - Auto-region creation dialog

**Estimated Time**: 18-24 hours total
- Basic region system: 8-10 hours
- Region navigation with "select inverse": 4-6 hours
- Auto-region creation / Strip Silence: 6-8 hours

---

### Open Source Library Integration (Phase 4+)

**CRITICAL REQUIREMENT**: WaveEdit leverages open source libraries for advanced features while maintaining GPL v3 license compatibility and proper attribution.

**GPL v3 Compatible Licenses**:
- ✅ GPL v2 (compatible with GPL v3)
- ✅ GPL v3 (same license)
- ✅ LGPL (Lesser GPL - compatible)
- ✅ BSD (2-clause, 3-clause) - permissive
- ✅ MIT - permissive
- ✅ Apache 2.0 - permissive
- ❌ GPL v3 with additional restrictions - NOT compatible
- ❌ Proprietary licenses - NOT compatible

**Approved Open Source Libraries**:

| Library | License | Purpose | Integration Phase |
|---------|---------|---------|------------------|
| **libsamplerate (SRC)** | BSD-2-Clause | High-quality sample rate conversion | Phase 4 |
| **FFTW** | GPL v2+ | Fast Fourier Transform (spectrum analyzer) | Phase 4 |
| **Kiss FFT** | BSD-3-Clause | Lightweight FFT alternative to FFTW | Phase 4 |
| **libebur128** | MIT | EBU R128 loudness metering (broadcast standard) | Phase 4 |
| **RNNoise** | BSD-3-Clause | ML-based noise suppression | Phase 4 |
| **Rubber Band Library** | GPL v2+ | High-quality pitch/time stretching | Phase 4 |
| **LAME** | LGPL | MP3 encoding | Phase 2-3 |
| **libFLAC** | BSD-3-Clause | FLAC lossless codec | Phase 2-3 |
| **libvorbis** | BSD-3-Clause | Ogg Vorbis codec | Phase 2-3 |
| **Opus** | BSD-3-Clause | Opus codec | Phase 3-4 |
| **VST3 SDK** | GPL v3 | VST3 plugin hosting | Phase 5 |

**Attribution Requirements**:

1. **NOTICE File** (`NOTICE` in project root)
   - List all open source libraries used
   - Include library name, version, license, copyright holder
   - Include link to library homepage/repository

2. **About Dialog** (Help → About WaveEdit)
   - "Third-Party Licenses" button
   - Show scrollable text with all library attributions
   - Link to full license texts in Licenses/ directory

3. **Licenses Directory** (`Licenses/` in project root)
   - Full license text for each library (e.g., `Licenses/libsamplerate.txt`)
   - Bundle with installer/application package

4. **Source File Headers**
   - If directly embedding library code (not linking), preserve original headers
   - Add comment indicating library name and license

**NOTICE File Example**:

```
WaveEdit by ZQ SFX
Copyright © 2025 ZQ SFX
Licensed under GPL v3

This software includes the following open source libraries:

---

libsamplerate (Secret Rabbit Code)
Version: 0.2.2
Copyright: © 2002-2021 Erik de Castro Lopo
License: BSD-2-Clause
Homepage: http://www.mega-nerd.com/SRC/

---

FFTW - Fastest Fourier Transform in the West
Version: 3.3.10
Copyright: © 2003, 2007-2011 Matteo Frigo, © 2003, 2007-2011 Massachusetts Institute of Technology
License: GPL v2 or later
Homepage: http://www.fftw.org/

---

[... additional libraries ...]
```

**Integration Guidelines**:

1. **CMake Integration**:
   ```cmake
   # Find system-installed library first
   find_package(SampleRate QUIET)
   if(NOT SampleRate_FOUND)
       # Fall back to bundled version
       add_subdirectory(ThirdParty/libsamplerate)
   endif()

   target_link_libraries(WaveEdit PRIVATE SampleRate::samplerate)
   ```

2. **Avoid Direct Code Embedding**:
   - Prefer linking against system-installed libraries (Linux package managers)
   - Bundle pre-compiled libraries for Windows/macOS (with attribution)
   - Only embed source code if library is small and unmaintained

3. **License Compatibility Check**:
   - Before adding any new library, verify license compatibility
   - Check for additional restrictions (e.g., "GPL v3 with exceptions")
   - Document decision in NOTICE file

4. **Update Attribution on Every Upgrade**:
   - When upgrading library version, update NOTICE file
   - Update copyright years if changed
   - Re-verify license hasn't changed (rare but possible)

**Files to Create**:
- `NOTICE` - Open source library attributions (root directory)
- `Licenses/` - Directory containing full license texts
- `Source/UI/AboutDialog.h/.cpp` - About dialog with "Third-Party Licenses" button

**Files to Modify**:
- `CMakeLists.txt` - Add library dependencies
- `README.md` - Document library dependencies for building from source

**Estimated Time**: 2-3 hours per library integration (CMake setup, testing, attribution)

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

**Navigation (Audio-Unit Based)** ⚠️ **MANDATORY FOR PHASE 1**
- `Left/Right Arrow`: Move cursor by time increment (default: 10ms, user-configurable)
- `Shift+Left/Right`: Move cursor by large increment (default: 100ms, user-configurable)
- `Ctrl+Left/Right`: Jump to start/end of file
- `Home/End`: Jump to first/last visible sample in current view
- `Page Up/Down`: Move by page increment (default: 1 second, user-configurable)
- `.` (period): Center cursor in view

**CRITICAL REQUIREMENT**: Navigation must use **audio-unit based positioning** (samples, milliseconds, frames, seconds), NOT pixel-based positioning. Selection boundaries and cursor positions must remain sample-accurate regardless of zoom level.

**Selection (Sample-Accurate)** ⚠️ **MANDATORY FOR PHASE 1**
- `Ctrl+A`: Select all
- `Shift+Left/Right`: Extend selection by time increment (respects snap mode)
- `Shift+Home/End`: Extend selection to visible edge
- `T`: Toggle snap mode (samples/milliseconds/seconds/frames/grid)
- `Z`: Snap to zero crossing

**CRITICAL REQUIREMENT**: Selection must be **sample-accurate at any zoom level**. Mouse-based selection should convert pixel positions to exact sample positions using the audio-unit snapping system.

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
- `Shift+G`: Gain adjustment dialog (Sound Forge compatible)
- `Shift+Up`: Increase gain by 1 dB (quick adjustment)
- `Shift+Down`: Decrease gain by 1 dB (quick adjustment)
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
**Goal**: Minimal viable audio editor with professional navigation

**Must Have**:
1. File open (WAV only, drag & drop + file picker)
2. Waveform display (zoom, scroll, cursor)
3. **Audio-unit based navigation system** ⚠️ **CRITICAL**
   - Arrow key navigation (10ms default increment)
   - Shift+Arrow large navigation (100ms default increment)
   - Page Up/Down navigation (1s default increment)
   - Jump to start/end (Ctrl+Left/Right)
   - Snap modes: Off, Samples, Milliseconds, Seconds, Frames, Grid, ZeroCrossing
4. **Sample-accurate selection** ⚠️ **CRITICAL**
   - Click & drag selection (sample-accurate at any zoom level)
   - Keyboard selection refinement (Shift+arrows)
   - Selection must use audio-unit conversion, NOT pixel quantization
5. Playback controls (play, pause, stop, loop)
6. Cut, copy, paste, delete
7. Save / Save As
8. Multi-level undo/redo (100 levels minimum)

**UI Components**:
- WaveformDisplay with scrollbar
- Transport controls (play/pause/stop/loop buttons)
- Time/sample position display with snap mode indicator
- Selection info panel (start/end/duration in multiple units)

**Technical Requirements**:
- Use `juce::AudioFormatManager` for file I/O
- Use `juce::AudioThumbnail` for waveform rendering
- Use `juce::AudioTransportSource` for playback
- Implement `AudioUnits` namespace for sample-accurate conversions
- Implement `juce::UndoManager` with 100-level history
- **Navigation must be audio-unit based, NOT pixel-based**
- **Selection must remain sample-accurate regardless of zoom level**

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

#### Step 2: Automated Testing (Required) ⚠️ **NEW MANDATORY REQUIREMENT**
- **Write automated tests BEFORE marking feature complete**
- Tests go in `Tests/Unit/`, `Tests/Integration/`, or `Tests/EndToEnd/`
- **Unit tests**: Test individual components in isolation (AudioEngine, AudioBufferManager, etc.)
- **Integration tests**: Test components working together (playback + editing, multi-document state)
- **End-to-end tests**: Test complete user workflows (open → edit → save → verify)
- **Run tests**: `./build/WaveEditTests_artefacts/Debug/WaveEditTests`
- **If tests fail**: Fix the issues. Do NOT mark the task complete.
- **Coverage target**: All critical code paths must have automated tests
- **Only proceed** when all tests pass

#### Step 3: Functional Testing (Required)
- **Use the `test-runner` agent** (or appropriate testing agent) to validate functionality
- Test the COMPLETE workflow, not just the isolated component
- **Integration test**: Verify the feature works end-to-end with other components
- **If tests fail**: Fix the issues. Do NOT mark the task complete.
- **Only proceed** when all tests pass

#### Step 4: Manual Verification (Required)
- **Build and run the application** (`./build-and-run.command`)
- **Manually test** the feature as a user would
- **Verify visual feedback**: Does the UI update correctly?
- **Verify audio feedback**: Does playback work as expected?
- **Check for errors**: Review console output for warnings/errors

#### Step 5: Assessment Decision
- ✅ **PASS**: Code reviewed, automated tests written and passing, functional tests pass, manual test successful → Mark task as complete
- ❌ **FAIL**: Any issues found → Go back to Step 1, fix issues, repeat process
- ⚠️ **PARTIAL**: Infrastructure done but not integrated → Mark as "⚠️ Infrastructure complete, integration needed"

**NEVER** mark a task as "✅ Complete" if:
- You haven't run the code-reviewer agent
- **You haven't written automated tests** ⚠️ **NEW**
- **Automated tests are failing** ⚠️ **NEW**
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
❌ No automated tests written
❌ Didn't verify playback works with edited buffer!
```

**Example of CORRECT process** (updated with automated testing requirement):
```
1. ✅ Implement AudioEngine buffer playback
2. ✅ Run code-reviewer agent → Found thread safety issue → Fixed
3. ✅ Write automated tests:
   - Tests/Unit/AudioEngineTests.cpp (buffer loading, playback state)
   - Tests/Integration/PlaybackIntegrationTests.cpp (edit + playback)
4. ✅ Run automated tests → ./build/WaveEditTests_artefacts/Debug/WaveEditTests → All pass
5. ✅ Run test-runner agent → All functional tests pass
6. ✅ Manual test: Load file → Edit → Play → HEAR edits ✓
7. ✅ Mark task complete in TODO.md
```

**If you skip this process, you WILL create the exact problem we have now**: features that appear complete but don't actually work.

**Automated Testing Benefits**:
- Catches regressions when refactoring
- Documents expected behavior
- Enables confident code changes
- Reduces manual testing time
- Prevents shipping broken features

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

**Last Updated**: 2025-10-15 (Automated Testing Infrastructure Complete)
**Project Start**: 2025-10-06
**Current Phase**: Phase 3.5 ✅ **COMPLETE** (All P0 UX gaps fixed)
**Current Status**: ✅ **USER MVP - Ready for Limited Public Beta**
**Code Quality**: ⭐⭐⭐⭐⭐ **9.5/10 - PRODUCTION GRADE (A+)**
**Test Coverage**: ✅ **Infrastructure Complete** - Automated testing framework operational
**Next Steps**: Write comprehensive test suite for existing features (Phase 4 work)

---

## 📍 Project Status

**For current project status, phase progress, code quality metrics, and task tracking, see [TODO.md](TODO.md).**

This file (CLAUDE.md) contains AI instructions, architecture patterns, coding standards, and lessons learned. It does NOT track project status or roadmaps.

---

## 📚 Case Studies & Lessons Learned

### Case Study: Phase 3 Multi-File Integration Bugs (2025-10-14)

**Critical Lesson**: Integration Over Isolation (validates CLAUDE.md rule #8)

**Context**: The Phase 3 multi-file refactoring (completed 2025-10-13) introduced 6 critical bugs that broke ALL functionality. This case study demonstrates why complete end-to-end integration is mandatory.

**What Happened**: After implementing the multi-document architecture (Document, DocumentManager, TabComponent classes), the system appeared to work (files opened, tabs appeared, UI rendered) but ALL editing operations failed. User testing immediately revealed catastrophic failures.

**Root Cause Pattern**: Incomplete integration - components were implemented but not connected end-to-end.

**Critical Bugs**:

1. **Bug #1: All menus blank, no keyboard shortcuts**
   - **Root Cause**: `getCommandInfo()` had early return `if (!doc) return;` preventing ALL command info from being set
   - **Impact**: Complete UI failure - menus had no text, shortcuts never registered
   - **Fix**: Removed early return, changed 247 occurrences to null-safe pattern `doc && doc->method()`
   - **Lesson**: Never use early returns that prevent essential initialization

2. **Bug #2: Cannot open files**
   - **Root Cause**: `perform()` had early return blocking document-independent commands (fileOpen, fileExit)
   - **Impact**: Users could not open ANY files - app was unusable
   - **Fix**: Removed early return, added per-command null checks for 40+ commands
   - **Lesson**: Distinguish between document-independent and document-dependent operations

3. **Bug #6: ALL editing broken (MOST CRITICAL)**
   - **Root Cause**: `Document.loadFile()` loaded AudioEngine (playback) and WaveformDisplay (visual) but NEVER loaded AudioBufferManager (editing)
   - **Symptom**: "Invalid selection: beyond duration (max=0.000000)" - buffer was empty!
   - **Impact**: Copy, cut, paste, gain, normalize, fade - EVERYTHING failed
   - **Fix**: Added ONE line: `m_bufferManager.loadFromFile(file, m_audioEngine.getFormatManager())`
   - **Lesson**: This is EXACTLY what CLAUDE.md rule #8 warns against - "A component isn't 'done' until it's integrated and the user can use it end-to-end"

**Analysis**:
- **Pattern**: All 6 bugs were caused by incomplete integration
- **Appearance vs Reality**: System "worked" visually (files opened, tabs appeared) but was fundamentally broken
- **Time to Fix**: ~6 hours (investigation + fixes + testing)
- **Prevention**: MANDATORY Quality Control Process must be followed

**Key Takeaway**: **NEVER** mark a feature complete without end-to-end testing. If the user cannot complete their workflow (open → edit → save → hear changes), the feature is NOT done.

**This case study must be referenced when**:
- Implementing any new architecture or refactoring
- Reviewing code that claims to be "complete"
- Evaluating whether to merge a feature branch

**Files Modified** (for reference):
- `Source/Main.cpp` (lines 890-1438) - Command system fixes
- `Source/Utils/Document.cpp` (lines 93-99) - AudioBufferManager integration

---

## Project Architecture (Current Implementation)
