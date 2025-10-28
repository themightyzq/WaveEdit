# CLAUDE.md - AI Agent Instructions for WaveEdit

## Company Information
**Developer**: ZQ SFX  
**Copyright**: © 2025 ZQ SFX  
**License**: GPL v3  
**Project**: WaveEdit - Professional Audio Editor

---

## Documentation Structure

### CLAUDE.md (This File) - AI Operations Manual
**Purpose**: Binding rules and architectural constraints for AI agents.
**Contains**: Rules, patterns, requirements, architecture, case studies
**NOT**: Status updates (→TODO.md), installation guides (→README.md)

### README.md - Public Landing Page
**Purpose**: User-facing project overview and setup instructions

### TODO.md - Developer Task Tracker
**Purpose**: Current tasks, bugs, status, and roadmap

---

## Project Overview

**WaveEdit**: Cross-platform professional audio editor (Sound Forge-inspired). JUCE-based C++ application for speed and keyboard-driven workflow.

### Core Philosophy
1. **Speed**: <1s startup, instant loading, 60fps rendering
2. **Sample-accurate**: All operations maintain precision
3. **Keyboard-first**: Every action has a shortcut (Sound Forge defaults)
4. **Zero bloat**: No unnecessary features
5. **Professional**: Built for engineers/podcasters/designers

### UI Requirements
- **Icons over text** (standard transport symbols)
- **Minimal clutter** (dark theme default)
- **Accessibility**: Keyboard-navigable, tooltips, screen reader support, 44x44px minimum targets
- **Responsive**: 1024x768 to 4K, HiDPI support
- **Professional**: Flat modern design, monospace for numbers
- **Icons**: SVG only, 24x24px base, 2-3px stroke

---

## Tech Stack

**Required**:
- Framework: JUCE 7.x+
- Language: C++17+ (modern standards, smart pointers)
- Build: CMake (cross-platform)
- License: GPL v3

**Audio Specs**:
- Sample rates: 44.1-192kHz
- Bit depths: 16/24/32-bit float
- Channels: Mono/stereo/multi (≤8)
- Formats (MVP): WAV only (PCM, IEEE float)

---

## Mandatory Architecture Patterns

### Manager Pattern (REQUIRED)
Every subsystem MUST use singleton managers:
```cpp
class AudioBufferManager : public juce::DeletedAtShutdown {
    JUCE_DECLARE_SINGLETON(AudioBufferManager, true)
    // Manages ALL audio buffer lifecycle operations
};
```

**Required Managers**:
- `AudioBufferManager` - Buffer lifecycle
- `AudioEngine` - Playback/transport
- `DocumentManager` - Multi-document state
- `CommandManager` - Commands/shortcuts
- `SelectionManager` - Selection state
- `RegionManager` - Region handling
- `PluginManager` - VST3/AU hosting
- `UndoManager` - Undo/redo

### Component Hierarchy
```
MainWindow
├── MenuBarComponent
├── TabBarComponent (DocumentTabs)
├── EditorPanel
│   ├── TransportBar
│   ├── WaveformDisplay
│   ├── TimeRuler
│   └── StatusBar
└── FloatingWindows
```

### State Management Rules
1. **Single source of truth**: Each manager owns its domain
2. **No direct component communication**: Use listeners/broadcasters
3. **Document state isolation**: Per-document undo/selection/regions
4. **Thread safety**: Audio thread never blocks, use FIFO/lock-free

---

## Coding Standards

### Naming Conventions
- Classes: `PascalCase` (e.g., `AudioEngine`)
- Methods: `camelCase` (e.g., `processBlock`)
- Members: `m_camelCase` (e.g., `m_sampleRate`)
- Constants: `UPPER_SNAKE` (e.g., `MAX_CHANNELS`)

### Error Handling
- Use `juce::Result` for operations that can fail
- Never use exceptions in audio code
- Always check return values
- Provide meaningful error messages

### Memory Management
- Smart pointers everywhere (`std::unique_ptr`, `juce::ReferenceCountedObjectPtr`)
- RAII for all resources
- No naked `new`/`delete`
- Preallocate audio buffers

### JUCE-Specific
- Inherit from appropriate bases (`Component`, `AudioProcessor`)
- Use JUCE containers over STL in UI code
- Leverage built-in components (avoid reinventing)
- Follow JUCE threading model

---

## Quality Control Process (MANDATORY)

**Before marking ANY feature complete, ALL must be YES**:

1. ✅ Code review agent run and issues fixed?
2. ✅ Automated tests written and passing?
3. ✅ Application builds without warnings?
4. ✅ Manual testing as user would?
5. ✅ Keyboard shortcuts verified?
6. ✅ Menu items present and functional?
7. ✅ Console checked for errors?
8. ✅ End-to-end workflow verified?
9. ✅ Documentation matches implementation?

**If ANY answer is NO, feature is NOT complete.**

---

## Integration Rules

### New Features MUST
1. Register in appropriate manager
2. Add to command system with shortcuts
3. Create menu item (if user-facing)
4. Implement undo/redo
5. Update status bar
6. Add automated tests
7. Update documentation

### Audio Processing Requirements
- Never allocate in audio callback
- Use lock-free structures for thread communication
- Process in blocks (512-2048 samples)
- Support variable buffer sizes
- Handle discontinuities gracefully

---

## Testing Requirements

**Minimum Coverage**:
- Unit tests for each manager
- Integration tests for workflows
- End-to-end tests for user scenarios
- Performance benchmarks for audio code

**Test Execution**: Must pass before marking complete

---

## Common Anti-Patterns (AVOID)

### Infrastructure ≠ Integration
**Wrong**: "Created class, feature done"
**Right**: Class + manager registration + UI + shortcuts + tests

### False Completion
**Wrong**: Mark complete without QC process
**Right**: Follow ALL 9 QC steps first

### Documentation Drift
**Wrong**: Update code, ignore docs
**Right**: Code and docs updated together

---

## Case Studies

### Phase 3 Multi-File Bugs (Lesson: Test Integration)
**Issue**: Infrastructure existed but wasn't connected
**Impact**: 6 critical bugs, 6 hours debugging
**Prevention**: Always verify end-to-end workflow

### Phase 3 Tier 2 False Completion
**Issue**: Marked complete without QC process
**Found**: 5 bugs in 45min review (shortcuts conflicts, missing menus)
**Prevention**: ENFORCE 9-step QC checklist

### Testing Infrastructure Success ✅
**Followed Process**: All 5 QC steps → 47 test groups → 100% pass
**Result**: Zero post-release bugs, saved 6+ hours
**Lesson**: Proper QC upfront prevents debugging later

---

## Build Instructions

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Test
./build/Tests/WaveEdit_Tests

# Run
./build/WaveEdit
```

---

## File Structure

```
WaveEdit/
├── Source/
│   ├── Audio/          # Audio processing
│   ├── Components/     # UI components
│   ├── Managers/       # Singleton managers
│   ├── Utils/          # Utilities
│   └── Main.cpp        # Application entry
├── Tests/              # Automated tests
├── Resources/          # Assets
└── CMakeLists.txt     # Build config
```

---

## Performance Targets

- Startup: <1 second
- File load: <2 seconds for 1-hour stereo
- Waveform render: 60fps minimum
- Latency: <10ms roundtrip
- Memory: <500MB for 1-hour file

---

## Current Implementation Status

See TODO.md for current phase and task status.

---

## Critical Reminders

1. **NEVER** mark features complete without QC process
2. **ALWAYS** test integration, not just infrastructure
3. **ENFORCE** manager pattern for all subsystems
4. **MAINTAIN** documentation accuracy with code
5. **VERIFY** keyboard shortcuts have no conflicts

**Remember**: Features aren't done until they work for users end-to-end.