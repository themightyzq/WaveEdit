# WaveEdit by ZQ SFX - TODO

**Last Updated**: 2025-11-04 (Metadata System Complete)
**Company**: ZQ SFX (© 2025)
**Philosophy**: Feature-driven development - ship when ready, not before

---

## Current Status

### What's Working ✅

**Core Editing**:
- Multi-file tabs with full keyboard navigation
- Cut, copy, paste, delete, undo/redo (100 levels per file)
- Gain adjustment, normalize, fade in/out, DC offset removal, silence, trim

**Playback**:
- Play, pause, stop, loop with selection-bounded playback
- Real-time level meters (peak, RMS, clipping)
- Auto-scroll during playback (toggle with Cmd+Shift+F)

**Regions**:
- Create, rename, delete, navigate, multi-select
- Region list panel with search/filter, sortable columns
- Batch rename (pattern/find-replace/prefix-suffix)
- Batch export (each region as separate WAV)
- Merge/split/copy/paste with undo support
- Edit boundaries with snap to zero crossings

**Markers**:
- Add markers at cursor (M key)
- Navigate between markers (Shift+[/])
- Rename, delete, color picker
- Full menu integration

**Navigation**:
- Sample-accurate selection at any zoom level
- Snap modes: Off, Samples, Ms, Seconds, Frames, Zero
- Go to position (Cmd+G) with 6 time formats
- Cycle time format display (Shift+G)

**Keyboard**:
- 90+ shortcuts, fully customizable
- 4 built-in templates (Default, Classic, Sound Forge, Pro Tools)
- Conflict detection, import/export
- Searchable cheat sheet (F1 or Cmd+/)

**Metadata** (NEW):
- BWF (Broadcast Wave Format) support
- iXML metadata with UCS v8.2.1 categories (753 mappings)
- SoundMiner Extended fields (FXName, Description, Keywords, Designer)
- File Properties dialog (Cmd+Enter) with UCS suggestions
- Persistent metadata embedded in WAV files

**Quality**:
- Automated test suite: 47 assertions, 100% pass rate
- Sub-1 second startup, 60fps rendering
- <10ms waveform updates, <10ms playback latency
- Zero P0/P1 bugs

### Known Issues

**None** - All critical bugs resolved.

### Bug Fixes (Recent)

**2025-10-28 - Region Workflow Fixes**:
- ✅ Copy Regions now respects selection (was copying all)
- ✅ Merge Regions now allows gaps (was too strict)
- ✅ Split Region shortcut changed to Cmd+T (resolved conflict)
- ✅ Multi-select now functional (Cmd+Click, Shift+Click)
- ✅ Undo/redo now works for multi-region merge

**2025-10-20 - Region Display Sync**:
- ✅ Region bars stay aligned during zoom/scroll operations

**2025-10-17 - Keyboard Shortcuts**:
- ✅ Cmd+1 conflict resolved (viewZoomOneToOne → Cmd+0)
- ✅ Cmd+W conflict resolved (tabClose only)
- ✅ Shift+G conflict resolved (processGain → Cmd+Shift+G)

---

## Active Tasks

### In Progress 🔄

**None** - Focus on code review and cleanup first.

### Up Next

See Backlog section for prioritized features.

---

## Backlog (Prioritized)

### High Priority

**BWF Metadata Integration** ✅ **COMPLETE**:
- ✅ Infrastructure complete (BWFMetadata utility class)
- ✅ Integrated with Document::loadFile() and Document::saveFile()
- ✅ Display BWF metadata in File Properties dialog (Alt+Enter)
- ✅ BWF editor UI (File → Edit BWF Metadata...)

**Multi-Format Support** (6-8 hours):
- FLAC codec integration (libFLAC, BSD license)
- MP3 codec integration (LAME, LGPL license)
- OGG Vorbis codec integration (BSD license)
- Format detection and transcoding on save

**Spectrum Analyzer** (8-10 hours):
- FFT-based frequency display
- Real-time analysis during playback
- Configurable FFT size, windowing function
- Zoom and frequency axis display

### Medium Priority

**Recording from Input** (6-8 hours):
- Audio input device selection
- Recording UI (levels, status, elapsed time)
- Live monitoring during recording
- Auto-create new document for recording

**Additional DSP Operations**:
- Reverb / Convolution reverb (4-6 hours)
- EQ (3-band parametric, 3-4 hours)
- Compressor/Limiter (4-5 hours)
- Time stretch / Pitch shift (requires Rubber Band Library, 8-10 hours)

**Batch Processing** (4-6 hours):
- Apply operation to multiple files
- Progress indicator, cancel button
- File list with checkboxes
- Operation selector dropdown

### Low Priority / Future

**VST3 Plugin Hosting** (Phase 5, 20+ hours):
- VST3 SDK integration (GPL v3 compatible)
- Plugin scanner and manager
- Plugin UI hosting
- Parameter automation

**Command Palette** (Cmd+K, 3-4 hours):
- Fuzzy search for all commands
- Keyboard-driven command execution
- Recent commands history

**Recent Files Thumbnails** (2-3 hours):
- Generate waveform thumbnails for recent files
- Show in file picker or separate panel

---

## Test Coverage

**Current Status**: 47 assertions, 100% pass rate across 13 test groups

**Test Infrastructure**:
- Unit tests: AudioEngine, AudioBufferManager, RegionManager, BWFMetadata
- Integration tests: Playback + editing, multi-document state
- End-to-end tests: Complete workflows (open → edit → save)

**Run tests**:
```bash
./build/WaveEditTests_artefacts/Debug/WaveEditTests
```

**Coverage Gaps** (non-critical):
- Additional DSP operation tests (fade curves, DC offset algorithm)
- Keyboard shortcut conflict detection (currently manual)
- UI interaction tests (requires GUI test framework)

---

## Development Notes

### Architecture

See [CLAUDE.md](CLAUDE.md) for architecture patterns, coding standards, and quality assurance process.

**Key patterns**:
- Document/DocumentManager for multi-file support
- Region/RegionManager for region management
- UndoableAction for all editing operations
- AudioUnits namespace for sample-accurate navigation

### Performance Targets

- Startup: <1 second
- File load (10min WAV): <2 seconds
- Rendering: 60fps
- Playback latency: <10ms
- Save: <500ms

### Quality Standards

**Before marking any task complete**:
1. Code review agent approval
2. Automated tests written and passing
3. Functional testing (complete workflow)
4. Manual verification (build and test as user)
5. Assessment (all steps pass)

See CLAUDE.md "Quality Assurance" section for details.

---

## Changelog

### 2025-11-04 - Metadata Persistence & Recent Files Fixes (Complete)
- ✅ Fixed iXML metadata persistence (CRITICAL: file was being appended instead of overwritten)
- ✅ Implemented Settings::setLastFileDirectory() and getLastFileDirectory()
- ✅ File browser now remembers last opened directory across sessions
- ✅ Recent files now populate correctly (added to all file open code paths)
- ✅ Fixed Close command to properly close document/tab with unsaved warning
- ✅ Rewrote appendiXMLChunk() to parse chunks selectively (only copy fmt/data)
- ✅ Added file.deleteFile() before FileOutputStream to prevent append behavior
- Bug fixes:
  - Main.cpp (lines 2843, 2890, 2973, 3089) - Added recent files tracking
  - Main.cpp (line 2867) - File chooser uses saved directory
  - Main.cpp (lines 3158-3203) - Close command fixed
  - AudioFileManager.cpp (lines 456-561) - Fixed file truncation bug
  - Settings.h (lines 106-118) - Directory memory API
  - Settings.cpp (lines 310-337) - Directory persistence implementation

### 2025-11-03 - BWF Metadata Editor UI (Complete)
- ✅ Implemented BWFEditorDialog UI component
- ✅ Editable fields for all BWF metadata (description, originator, dates, etc.)
- ✅ Character limit validation and visual feedback (256/32/32 chars)
- ✅ "Set Current" button for automatic date/time stamping
- ✅ Apply/OK/Cancel buttons with proper callback behavior
- ✅ Integrated with command system (CommandIDs::fileEditBWFMetadata)
- ✅ Added to File menu: "File → Edit BWF Metadata..."
- ✅ Document marked as modified when metadata changes
- ✅ Automated tests written and passing (5 test cases)
- ✅ All 47 existing tests still pass
- Files:
  - Source/UI/BWFEditorDialog.h (new, 160 lines)
  - Source/UI/BWFEditorDialog.cpp (new, 340 lines)
  - Source/Commands/CommandIDs.h (added fileEditBWFMetadata)
  - Source/Main.cpp (integration: includes, commands, menu)
  - Tests/Unit/BWFEditorDialogTests.cpp (new, 5 test cases)
  - CMakeLists.txt (build system integration)

### 2025-11-02 - BWF Metadata Integration (Complete)
- ✅ Integrated BWF metadata loading in Document::loadFile()
- ✅ Implemented Document::saveFile() with BWF metadata support
- ✅ Automatic default metadata creation for files without BWF chunks
- ✅ Timestamp updates on save (origination date/time)
- ✅ Added BWF display to File Properties dialog (Alt+Enter)
- ✅ Shows BWF Description, Originator, Origination Date/Time
- ✅ All existing tests pass (47 assertions, 0 failures)
- Files:
  - Document.h (lines 35, 199-207, 235-242, 272-273)
  - Document.cpp (lines 24, 150-160, 188-261)
  - FilePropertiesDialog.h (lines 124-126, 141-143)
  - FilePropertiesDialog.cpp (dialog size, BWF section, labels, loading)
- Remaining: BWF editor UI for editing metadata fields

### 2025-11-02 - Documentation Streamlining
- Streamlined CLAUDE.md from 2440 → 400 lines (84% reduction)
- Streamlined README.md from 680 → 396 lines (42% reduction)
- Streamlined TODO.md from 843 → current lines (focus on tasks, not history)
- Removed arbitrary phase percentages, timelines, and redundant process docs
- Established "Single Source of Truth" principle across documentation

### 2025-10-28 - Region Workflow Bug Fixes
- Fixed Copy Regions (was copying all instead of selected)
- Fixed Merge Regions (now allows gaps between regions)
- Fixed Split Region shortcut conflict (Cmd+T)
- Fixed Multi-select (Cmd+Click, Shift+Click now work)
- Fixed Undo/redo for multi-region merge operations
- Added thread safety to RegionManager (production-grade)

### 2025-10-20 - Region Display Sync Fix
- Fixed region bars falling out of alignment during zoom/scroll
- Added onVisibleRangeChanged callbacks to 9 critical methods
- User-verified working

### 2025-10-17 - Keyboard Shortcut Conflicts Resolved
- Cmd+0 for viewZoomOneToOne (was Cmd+1, conflicted with tab selection)
- Cmd+W for tabClose only (fileClose no longer has shortcut)
- Cmd+Shift+G for processGain (was Shift+G, conflicted with viewCycleTimeFormat)

### 2025-10-16 - Phase 3.5 Complete
- Keyboard shortcut customization UI complete
- Keyboard cheat sheet dialog complete
- Template system with 4 built-in templates
- All conflicts resolved, code quality 9.5/10

### 2025-10-14 - Phase 3 Complete
- Multi-file architecture with tabs
- Independent undo/redo per file (100 levels each)
- Inter-file clipboard
- All critical bugs fixed

### 2025-10-13 - Phase 2 Complete
- Gain adjustment, normalize, fade in/out
- Level meters during playback
- Code review 9/10 - production ready

---

**For architecture and coding standards, see [CLAUDE.md](CLAUDE.md)**
**For user guide and keyboard shortcuts, see [README.md](README.md)**
