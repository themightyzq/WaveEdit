# CLAUDE.md - AI Agent Instructions for WaveEdit by ZQ SFX

## Company Information

**Developer**: ZQ SFX
**Copyright**: © 2025 ZQ SFX
**License**: GPL v3
**Project**: WaveEdit - Professional Audio Editor

All code files, documentation, and branding materials must reflect ZQ SFX ownership.

---

## 📄 Documentation Structure

**Single Source of Truth Principle**:
- **CLAUDE.md** (this file): AI instructions, architecture patterns, coding standards
- **README.md**: User guide, build instructions, keyboard shortcuts
- **TODO.md**: Current status, task tracking, roadmap

**Never duplicate content across files.** Link to the appropriate file instead.

---

## Project Overview

**WaveEdit** is a professional audio editor by ZQ SFX inspired by Sound Forge Pro. JUCE-based C++ application designed for speed, precision, and keyboard-driven workflow.

### Core Philosophy

1. **Speed**: Sub-1 second startup, 60fps waveform rendering
2. **Sample-accurate editing**: Maintain sample-level precision
3. **Keyboard-first**: Every action has a shortcut
4. **Zero fluff**: No social features, bloat, or unnecessary complexity
5. **Professional tool**: For audio engineers, podcasters, sound designers
6. **Streamlined UI**: Clean, minimal, accessible

### Target Users

Audio engineers, podcasters, sound designers, game developers - anyone who needs fast, accurate audio editing without DAW complexity.

---

## Tech Stack

**Framework**: JUCE 7.x+
**Language**: C++17+
**Build System**: CMake
**License**: GPL v3

**Audio Support**:
- Sample rates: 44.1kHz to 192kHz
- Bit depths: 16-bit, 24-bit, 32-bit float
- Channels: Mono, stereo, up to 8 channels
- Formats: WAV (primary), FLAC, MP3, OGG (future)

---

## Coding Standards

### File Organization

See actual codebase structure in `Source/` directory.

### Naming Conventions

- **Classes**: PascalCase (`WaveformDisplay`)
- **Methods**: camelCase (`loadAudioFile()`)
- **Members**: camelCase with `m_` prefix (`m_formatManager`)
- **Constants**: UPPER_SNAKE_CASE or kPascalCase
- **Enums**: PascalCase name, UPPER_SNAKE_CASE values

### Code Style

**Indentation**: 4 spaces (no tabs)
**Braces**: Allman style (opening brace on new line)
**Comments**: Doxygen for public methods, explain *why* not *what*

---

## Architecture Patterns

### Component Hierarchy

```
MainComponent
├── WaveformDisplay
├── TransportControls
├── Meters
└── SettingsPanel
```

**Rules**:
- UI components separate from audio logic
- JUCE message thread for UI updates
- Never block audio thread
- Use `ChangeBroadcaster`/`ChangeListener` for component communication

### Thread Safety

**CRITICAL**: Never allocate memory, acquire locks, or do I/O on audio thread.

**Audio thread** (real-time):
- `getNextAudioBlock()` callback only
- Sample processing
- Pre-allocated buffers

**Message thread** (UI):
- File I/O
- User interaction
- UI updates
- Memory allocation

**Background threads**:
- File loading
- Waveform thumbnail generation
- Non-real-time DSP

### Real-Time Buffer Updates

**Problem**: AudioTransportSource caches audio. Updating underlying buffer during playback doesn't flush the cache.

**Solution**: Disconnect and reconnect transport to flush buffers:

```cpp
// Preserve playback state
bool wasPlaying = isPlaying();
double position = getCurrentPosition();

// Flush internal buffers
m_transportSource.setSource(nullptr);
m_bufferSource->setBuffer(buffer, sampleRate, false);
m_transportSource.setSource(m_bufferSource.get(), 0, nullptr, sampleRate, numChannels);

// Restore state
if (wasPlaying) {
    m_transportSource.setPosition(position);
    m_transportSource.start();
}
```

See `AudioEngine::reloadBufferPreservingPlayback()` for full implementation.

### Command System

All user actions go through centralized command system in `CommandIDs.h`.

**Benefits**: Single source of truth, keyboard binding, undo/redo integration, consistency.

### Audio-Unit Navigation

**CRITICAL**: Navigation uses audio-unit based positioning (samples, milliseconds), NOT pixel-based.

**Why**: Sample-accurate at any zoom level. Pixel-based selection accuracy depends on zoom (unprofessional).

**Components**:
- `AudioUnits.h` - Conversion functions, snap modes
- `NavigationPreferences` - User preferences
- `WaveformDisplay` - Audio-unit aware navigation

### Settings Management

**Locations** (platform-specific):
- macOS: `~/Library/Application Support/WaveEdit/`
- Windows: `%APPDATA%/WaveEdit/`
- Linux: `~/.config/WaveEdit/`

**Files**: `settings.json`, `keybindings.json`, `autosave/`, `Keymaps/`

### Keyboard Template System

**Architecture**: Reaper-style template switching. Users select complete shortcut sets from dropdown.

**Built-in Templates**:
1. **Default**: Family-based organization (G=Gain, T=Time, R=Regions, M=Markers)
2. **WaveEdit-Classic**: Original Phase 1-2 layout
3. **Sound Forge**: Match Sound Forge Pro shortcuts
4. **Pro Tools**: Match Pro Tools shortcuts

**Template Locations**:
- Built-in: `WaveEdit.app/Contents/Resources/Keymaps/` (read-only)
- User: `~/Library/Application Support/WaveEdit/Keymaps/` (customizable)

**API**: `KeymapManager.h` - Load/switch templates, import/export

### Multi-File Architecture

**Status**: ✅ Implemented (Phase 3)

**Pattern**: Document/DocumentManager

Each Document has:
- Independent AudioEngine
- Independent WaveformDisplay
- Independent UndoManager
- Independent RegionManager
- Independent playback/selection state

DocumentManager handles:
- Tab navigation
- Inter-file clipboard
- Save prompts on close

### Region System

**Status**: ✅ Implemented (Phase 3)

**Components**:
- `Region.h` - Data structure (name, start/end samples, color)
- `RegionManager.h` - Collection manager
- `AutoRegionCreator.h` - Strip Silence algorithm

**Persistence**: JSON sidecar files (`.wav.regions.json`)

**UI**: RegionDisplay, RegionListPanel, StripSilenceDialog

---

## Performance Targets

- **Startup**: <1 second
- **File load**: <2 seconds (10-minute WAV)
- **Rendering**: 60fps
- **Playback latency**: <10ms
- **Save**: <500ms

---

## Quality Assurance

### Mandatory Process

Before marking ANY task complete:

1. **Code Review** - Run `code-reviewer` agent, fix all issues
2. **Automated Testing** - Write tests, verify 100% pass rate
3. **Functional Testing** - Test complete workflow (open → edit → save)
4. **Manual Verification** - Build and test as user would
5. **Assessment** - Only mark complete if all steps pass

### Implementation Standards

**Proper Architecture Over Quick Hacks**:
- No global variables for state
- No bandaid solutions
- No TODOs for critical functionality

**Thread Safety**:
- Proper locking for shared data
- Never block audio thread

**Error Handling**:
- Handle ALL failure cases gracefully
- Log errors, show user-friendly messages

**Memory Management**:
- Use RAII (`std::unique_ptr`, JUCE reference counting)
- No raw `new`/`delete`

**Integration Over Isolation**:
- A component isn't "done" until user can use it end-to-end
- Verify: Open → Edit → Playback → Save

### Pre-Completion Verification Checklist

**For UI Features**:
- [ ] UI element appears when triggered
- [ ] Keyboard shortcut works
- [ ] Menu item present and clickable
- [ ] Buttons/controls wired to callbacks
- [ ] Callbacks execute (check console)
- [ ] No console errors

**For Keyboard Shortcuts**:
- [ ] Command ID in `getAllCommands()`
- [ ] Command info in `getCommandInfo()` with keypress
- [ ] Handler in `perform()` switch statement
- [ ] Actually triggers action in app

**For Data Features**:
- [ ] Data persists to disk
- [ ] Data loads correctly
- [ ] Changes visible in UI
- [ ] Undo/redo works
- [ ] No crashes or errors

**Never claim "ready to test" without personally verifying it works.**

---

## What to Avoid

### Anti-Patterns
- Project files (this is file-based, not a DAW)
- Social features (no sharing, cloud sync)
- UI bloat
- Blocking UI thread

### Features to Reject
- Multi-track editing (that's a DAW)
- MIDI support (audio-only)
- Video support (audio-only)
- Cloud storage
- Telemetry

### Performance Pitfalls
- Loading entire file into memory
- Re-rendering entire waveform on zoom
- Allocations in audio callback
- Synchronous I/O on message thread

---

## Build Instructions

**Use the build script** (SOURCE OF TRUTH):

```bash
./build-and-run.command          # Build and run
./build-and-run.command clean    # Clean build
./build-and-run.command debug    # Debug build
```

For manual CMake commands, see README.md build section.

---

## Case Studies

### Phase 3 Multi-File Integration Bugs (2025-10-14)

**Lesson**: Components implemented but not integrated end-to-end.

**Impact**: 6 critical bugs, 6 hours to fix.

**Root Cause**: Marked "complete" without testing complete workflow.

**Fix**: AudioBufferManager integration, command system null-safety.

**Takeaway**: NEVER mark complete without end-to-end testing.

### Phase 3 Tier 2 False Completion Claims (2025-10-16)

**Lesson**: Quality control process defined but not enforced.

**Impact**: 5 critical bugs, 100% completion claim was false.

**Root Cause**: Skipped all 5 QC steps (code review, testing, verification).

**Takeaway**: Process is useless if not followed. Enforce pre-completion checklist.

### Automated Testing Infrastructure Success (2025-10-15)

**Lesson**: Following QC process prevents bugs and saves time.

**Implementation**: Full 5-step process → 0 bugs, accurate documentation.

**Impact**: 8 hours to implement, saved 6+ hours debugging.

**Takeaway**: Proper QC upfront is faster than debugging later.

---

## Resources

**When Stuck**:
- JUCE docs: https://docs.juce.com/
- JUCE forum: https://forum.juce.com/
- Review existing code patterns
- Ask user for clarification

**Documentation**:
- Current status: See TODO.md
- User guide: See README.md
- Build instructions: See README.md

---

## Final Notes

This is a **professional audio tool**. Users expect reliability, speed, and precision.

Every decision should prioritize:
1. **Speed**: Fast startup, operations, workflow
2. **Accuracy**: Sample-perfect editing
3. **Usability**: Keyboard-driven, discoverable
4. **Simplicity**: No bloat, just editing

**When in doubt**: "Would Sound Forge Pro do this?" If yes, implement it. If no, skip it.

---

**Last Updated**: 2025-11-02 (Streamlined from 2440 → 1100 lines)
**Project Start**: 2025-10-06

> **Cross-References**:
> - **Status & Roadmap**: TODO.md
> - **User Guide**: README.md
> - **Architecture**: This file
