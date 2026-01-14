# WaveEdit TODO

Current priorities, roadmap, and known issues.
For completed work, see [CHANGELOG.md](CHANGELOG.md).

---

## Recently Completed

- ✅ **Multichannel Support + Channel Converter** - Full N-channel audio pipeline (1-8 channels)
  - ChannelLayout system with Mono, Stereo, LCR, Quad, 5.0-7.1 Surround support
  - Film/SMPTE channel ordering, proper channel labels in waveform display
  - WAVEFORMATEXTENSIBLE speaker position masks
  - Channel Converter dialog (Cmd+Shift+U) with:
    - ITU-R BS.775 standard downmix coefficients (professional quality)
    - Downmix presets: ITU Standard, Professional, Film Fold-Down
    - LFE handling options (Exclude, -3dB, -6dB)
    - Channel extraction mode for selecting specific channels
    - Formula preview showing exact mixing coefficients
  - Solo/mute indicated by background color (no text suffix to avoid surround label confusion)
- ✅ **Per-Channel Editing** - Edit individual channels independently
  - Double-click channel waveform or label to focus that channel
  - Double-click again on focused channel to return to "all channels" mode
  - Focused channel shows blue label background + cyan waveform border
  - Cut/copy/paste/delete only affect focused channels
  - Per-channel delete silences (preserves length); per-channel paste replaces in-place
- ✅ **Batch Processor** - Process multiple files with DSP chain, EQ presets, plugin chains
- ✅ **Universal Preview System** (Phase 6 complete)
- ✅ **VST3/AU Plugin Hosting** - Full implementation with scanning, chain, presets
- ✅ **Export Options** - Sample rate/bit depth conversion, batch region export
- ✅ **EQ Systems** - 3-band Parametric EQ + 20-band Graphical EQ with presets
- ✅ **Auto-Save** - Configurable interval with dirty file tracking

---

## Active Tasks

### Up Next
- **Looping Tools (nvk_LOOPMAKER-style)**: Selection-driven seamless loop creation
  - **Phase 1 - Discovery**: Audit selection model, preview engine, and processing tool UI patterns
  - **Phase 2 - Core Loop Engine**:
    - Zero-crossing search and boundary refinement near loop points
    - Crossfade engine (linear, equal power, custom curves)
    - Configurable crossfade length (proportional % or absolute ms)
    - Fallback to minimum amplitude when zero-crossing unavailable
  - **Phase 3 - UI Dialog**:
    - Entry: Tools menu → Looping Tools... (also context menu on selection/region)
    - Settings: loop count (N), crossfade length/curve/max, search window size
    - Position controls: offset slider, second snap, shuffle seed for variations
    - Output naming: prefix/suffix, separator, numbering with leading zeros
  - **Phase 4 - Multi-Variation Generation**:
    - Create N seamless loops from single selection with controllable spacing
    - Shuffle variation positions with repeatable seed
    - Output as separate files (file-based workflow)
  - **Phase 5 - Shepard Tone Mode**:
    - Two-layer pitch ramp with crossfade (up/down illusion)
    - Controls: semitones, ramp shape, optional pitch algorithm selector
    - Loop length constraint: original length ÷ 2^loops
  - **Phase 6 - Preview & Polish**:
    - Real-time preview toggle (looped playback with current settings)
    - Diagnostics panel: chosen samples, zero-crossing found, discontinuity metrics
  - **Phase 7 - Testing & Docs**:
    - Unit tests: zero-crossing search, crossfade curves, discontinuity metrics
    - Integration tests: synthetic buffers with known clicks → verify reduction
    - Manual QA: ambience, tonal, percussive loops, Shepard tone
    - Create `docs/looping_tools.md` with settings, best practices, presets
  - Reference: Inspired by nvk_LOOPMAKER 2 workflows (behavioral only, no code sharing)

- **Head & Tail Processing Tool**: Intelligent detection + time-based edits (batch-ready)
  - **Phase 1 - Discovery**: Audit selection model, fade/trim utilities, existing processing dialogs
  - **Phase 2 - Core Recipe Engine** (implement first, no UI):
    - `HeadTailRecipe` settings type with enabled flags per operation
    - Operation list with computed order:
      - `DetectTrimOp` → threshold/RMS detection, returns start/end frame indices
      - `FixedTrimOp(headMs, tailMs)` → deterministic trim amounts
      - `SilencePadOp(preMs, postMs)` → prepend/append silence
      - `FadeOp(inMs, outMs, curve)` → apply fade curves
    - Single `applyRecipe(audio, recipe)` function returning processed buffer + report
    - All operations frame-aligned for multichannel safety
  - **Phase 3 - Intelligent Detection**:
    - Mode: Peak threshold OR RMS threshold with window
    - Threshold (dBFS), hold time (ms), search window (ms)
    - Leading pad (ms), trailing pad (ms)
    - "If not found" behavior (skip, fail, use defaults)
    - Safety: minimum kept duration, max trim amount
  - **Phase 4 - Time-Based Operations**:
    - Head Trim (ms): remove exactly X ms from start (clamped to file length)
    - Tail Trim (ms): remove exactly X ms from end
    - Head Fade In (ms): apply fade-in to first X ms (after any head trim)
    - Tail Fade Out (ms): apply fade-out to last X ms (after any tail trim)
    - Prepend Silence (ms): insert silence at beginning
    - Append Silence (ms): insert silence at end
  - **Phase 5 - Order of Operations**:
    - Default order: 1) Detection trim → 2) Fixed trims → 3) Add silence → 4) Apply fades
    - Order Mode dropdown: Standard (fixed) or Advanced (user reorderable)
    - Clear documentation of processing order
  - **Phase 6 - UI Dialog**:
    - Entry: Tools menu → Head & Tail... (or context menu on selection)
    - Two tabs/sections: "Intelligent Trim" and "Time-based Edits"
    - Recipe Summary readout showing computed operations
    - Before/after duration display, computed trim points, total ms removed/added
    - Preview with looped playback if feasible
  - **Phase 7 - Testing & Docs**:
    - Unit tests: exact sample counts for trim/silence/fade operations
    - Combined recipe tests: detect + fixed trim + append silence + fade out
    - Edge cases: file shorter than trim, fade longer than file, detection removes all
    - Create `docs/head_tail_trimming.md` with examples and best practices
  - Architecture: Reusable engine for future Batch Processor integration

- **Plugin Parameter Automation**: Record and playback parameter changes in plugin chain
  - Investigate automation lane UI approach
  - Handle automation curve interpolation
  - Persist automation data with plugin presets

---

## Backlog

### High Priority
- **DSP Operations**: Time stretch, pitch shift, reverse, invert, resample
- **Markers to Regions**: Convert markers to regions and vice versa

### Medium Priority
- **UI/UX Polish**:
  - Full color theme system (currently only waveform/selection colors)
  - Waveform color options per channel
  - Selection handles for easier resizing
- **Marker Feature Parity**:
  - Marker editing (currently limited vs. regions)
  - Marker search/filter in panel
  - Marker export/import

### Low Priority
- **Command Palette**: Quick access to all commands (Cmd+Shift+P style)
- **Crash Recovery UI**: Recovery dialog for auto-saved sessions (auto-save works, UI needed)
- **Background Thumbnail Cache**: Pre-generate waveform caches for faster reopening

---

## Known Issues

### Non-Blocking
- Some VST3 plugins may require multiple load attempts (workaround: retry in Offline Plugin dialog)
- Large file thumbnails may take 1-2 seconds to fully render
- Occasional toolbar highlight flicker on rapid mouse movement

### Code Quality
- Several `// TODO` comments in codebase need resolution
- Marker system test coverage is minimal (vs. comprehensive region tests)
- Some error messages could be more user-friendly

---

## Test Coverage

### Current Tests
- **Region System**: Well-tested
  - `Tests/RegionManagerTests.cpp` (563 lines)
  - `Tests/RegionListPanelTests.cpp` (391 lines)
  - `Tests/MultiRegionDragTests.cpp`
- **Channel System**: Comprehensive ITU coefficient and conversion tests
  - `Tests/ChannelSystemTests.cpp` (650+ lines, 25 test groups)
  - Verifies downmix coefficients (stereo→mono, 5.1→stereo)
  - Tests upmix strategies (FrontOnly, PhantomCenter, FullSurround, Duplicate)
  - Validates channel extraction buffer integrity
  - Edge case handling (empty buffers, single sample, clipping prevention)
- **Unit tests**: `./build/WaveEditTests_artefacts/Release/WaveEditTests`
- Manual QA checklist in CLAUDE.md Section 10

### Areas Needing Tests
- Marker system (no dedicated test file)
- Multi-file clipboard operations
- Plugin chain state persistence
- Undo/redo with large operations

---

## Links

- [CHANGELOG.md](CHANGELOG.md) - Completed work and version history
- [README.md](README.md) - User guide and keyboard shortcuts
- [CLAUDE.md](CLAUDE.md) - Architecture and AI instructions
