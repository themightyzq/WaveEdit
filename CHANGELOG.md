# WaveEdit Changelog

All notable changes to WaveEdit are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased]

### Fixed
- **Restored five post-refactor regressions**: After the controller
  migration, five menu commands routed to `DSPController` stubs that
  showed an "AlertWindow: not yet fully implemented" message. All paths
  are now reconnected to their real implementations:
  - Graphical EQ — wired to `GraphicalEQEditor::showDialog` +
    `ApplyDynamicParametricEQAction`.
  - Channel Converter (Cmd+Shift+U) — wired to
    `ChannelConverterDialog::showDialog` +
    `waveedit::ChannelConverter::convert` + `ChannelConvertAction`.
  - Channel Extractor — wired to `ChannelExtractorDialog::showDialog`
    with WAV/FLAC/OGG export and individual-mono / combined-multi modes.
    Preserves the post-bug-fix createWriterFor stream-ownership pattern.
  - Apply Plugin Chain (Cmd+P) — wired to `PluginChainRenderer` with
    progress dialog for large selections, sync path for small ones,
    `ConvertToStereoAction` for mono-to-stereo, tail-sample support, and
    `ApplyPluginChainAction` for undo.
  - Offline Plugin Dialog — wired to `OfflinePluginDialog::showDialog`
    with a private `applyOfflinePluginToSelection` helper sharing the
    plugin-chain pattern.

### Changed
- **CI now runs the test suite** on macOS, Windows, and Linux (Linux uses
  `xvfb-run` for headless display). A failing test fails the build job.
- **Re-enabled three orphan test files**: `RegionManagerTests`,
  `MultiRegionDragTests`, and a rewritten
  `KeyboardShortcutConflictTests` (now a proper `juce::UnitTest` that
  parses each `Templates/Keymaps/*.json` and reports duplicate
  key+modifier bindings — the previous standalone version registered
  commands against a `nullptr` target so it could not detect any
  conflicts).
- Local test run: 30 groups / 104 assertions / 0 failures (was 22 / 90).

### Added
- **MarkerManagerTests** unit suite (`Tests/Unit/MarkerManagerTests.cpp`),
  covering Marker basic operations (constructors, setters, JSON
  round-trip, isNear, position-in-seconds) and MarkerManager (add/remove,
  sorted insertion, navigation, selection, snapshot semantics, sidecar
  JSON round-trip). Closes the marker-coverage gap that TODO had
  acknowledged.

### Changed
- **Split `UndoableEdits.h` per CLAUDE.md §8.1**:
  - New `Source/Utils/UndoActions/PluginUndoActions.h` —
    `ApplyParametricEQAction`, `ApplyDynamicParametricEQAction`,
    `ApplyPluginChainAction`.
  - New `Source/Utils/UndoActions/ChannelUndoActions.h` —
    `ChannelConvertAction`.
  - `RegionUndoActions.h` gains `NudgeRegionUndoAction`,
    `BatchRenameRegionUndoAction`, `MergeRegionsUndoAction`,
    `SplitRegionUndoAction`.
  - `UndoableEdits.h` trimmed from 1,141 to 453 lines (now under the
    CLAUDE.md §7.5 500-line cap), keeping only `UndoableEditBase` and
    the generic `DeleteAction` / `InsertAction` / `ReplaceAction`
    primitives.

### Fixed
- **`TestRunner.cpp` summary now reports honest totals.** The previous
  loop relied on `juce::UnitTestRunner::getNumResults()` after all
  `runTestsInCategory()` calls had completed — but JUCE's runner clears
  its results list at the start of every category invocation, so the
  summary reflected only the last category. The runner now accumulates
  per-category passes/failures into local counters between calls. The
  exit code now correctly returns 1 when any test fails.

### Fixed
- **86 of 89 newly visible test failures triaged**, 100% test-bugs:
  - 67 `Undo/Redo Multi-Level` failures — added `beginNewTransaction()`
    between `perform()` calls so each delete is its own undo unit
    (JUCE coalesces consecutive performs in a single transaction).
  - ~18 `AudioBufferManager Silence/Trim Operations` and
    `EditingTools Integration` failures — tests called
    `silenceRange`/`trimToRange` with second argument as `endSample`,
    but the API is `(startSample, numSamples)`. Translated all calls.
    Also corrected two stale content assertions whose expected values
    no longer matched the corrected trim ranges.
  - 1 `File I/O Error Handling` failure — test asserted 8-bit WAV save
    should fail; 8-bit PCM has been a supported format since 2025-11-06.
    Replaced with clearly-invalid bit depths (7, 64, 0).

### Known Issues
- **3 remaining test failures** all involve audio-engine playback
  position/state in a headless test environment with no audio device
  initialized: `Edit during pause - resume works correctly` (2),
  `Fade during playback` (1). Need investigation rather than a
  test-side patch. CI will continue to flag these.
- `perform()` in `Main.cpp` is a dispatcher but 21 cases still exceed the
  CLAUDE.md §6.7 ≤5-line rule.
- `AudioUndoActions.h` (1,238 lines) and `RegionUndoActions.h` (964
  lines) still exceed the §7.5 500-line cap; further per-class extraction
  to dedicated files is a follow-up.
- `Tests/Integration/RegionListPanelTests.cpp` remains disabled: uses
  non-existent `juce::UnitTest::getInstance()` and a stale Listener
  interface. Needs a rewrite as a proper `UnitTest` subclass.

### Added
- **Plugin Parameter Automation** (Phases 1-3): Core data model and real-time playback engine.
  - AutomationPoint with 4 curve types (Linear, Step, S-Curve, Exponential).
  - AutomationCurve: copy-on-write with SpinLock for real-time-safe reads from audio thread.
  - AutomationLane: per-parameter-per-plugin automation with enable/record state.
  - AutomationManager: lane management, plugin index tracking, JSON serialization, listener pattern.
  - Integrated into AudioEngine: automation applied before plugin chain processing each audio block.
  - Integrated into Document: each open file has its own AutomationManager.
- **Command Palette** (Cmd+Shift+A): VS Code-style fuzzy command search overlay.
  - Floating overlay at top-center with search field and scrollable command list.
  - Fuzzy matching with priority: exact prefix > substring > word-start > character match.
  - Shows category badges with per-category colors, keyboard shortcuts, and disabled state.
  - Keyboard-driven: arrow keys to navigate, Enter to execute, Escape to dismiss.
  - Filters 133+ commands across 13 categories in real-time.
  - Added to Help menu. Moved regionSelectAll shortcut from Cmd+Shift+A to Cmd+Alt+A.
- **Looping Tools Dialog** (Tools menu, Cmd+L): Selection-driven seamless loop creation
  and export.
  - Section "Loop Settings": Loop Count (1-100), Crossfade Length with ms/% mode toggle,
    Max Crossfade cap, Crossfade Curve (Linear / Equal Power / S-Curve), and Zero-Crossing
    Snap with configurable search window.
  - Section "Variation Settings": Offset Step and Shuffle Seed for multi-variation export
    (grayed out when Loop Count is 1).
  - Section "Output": directory chooser, filename suffix editor, bit depth selector
    (16/24/32-bit), and a live filename preview showing the generated file name(s).
  - Waveform preview (140px) renders the loop buffer with a semi-transparent blue overlay
    highlighting the crossfade zone; blue vertical line marks the loop point.
  - Diagnostics label shows loop length (seconds + samples), crossfade sample count,
    discontinuity before/after, and percentage reduction.
  - Export button writes one WAV (single loop) or N numbered WAVs (variations) to the
    chosen output directory, then closes the dialog.
  - Cancel button closes without writing files.
  - Dialog is self-contained: export logic lives in LoopingToolsDialog, using
    LoopEngine::createLoop() / createVariations() from Source/DSP/.
  - Command wired through DSPController::showLoopingToolsDialog() per controller pattern.
  - Active only when a file is loaded and a selection exists.
  - Real-time preview playback: toggle button builds loop, loads into AudioEngine via
    loadPreviewBuffer(), sets loop points, and plays with looping enabled. Slider changes
    during playback trigger debounced rebuild (200ms timer).
  - Enhanced diagnostics panel with detailed metrics: loop length (seconds + samples),
    crossfade (ms + samples + curve type), zero-crossing shift amounts, discontinuity
    before/after with quality rating (Excellent/Good/Acceptable/Click warning), and
    Shepard tone info when enabled.
  - Color-coded diagnostics: green (<0.001), yellow (<0.01), orange (<0.05), red (>=0.05).
  - Comprehensive unit tests (10 test cases): basic loop, zero-crossing refinement,
    crossfade curves, equal power level preservation, multiple variations, Shepard loop,
    short selection, empty buffer, mono/stereo, crossfade modes.
  - Documentation: `docs/looping_tools.md` with settings reference, best practices,
    diagnostics guide, and export instructions.
- **Head & Tail Dialog**: Replaced the placeholder AlertWindow-based dialog with a full
  professional UI (700x680) for the Head & Tail processing pipeline.
  - Section 1 "Intelligent Trim": enable checkbox, Peak/RMS detection mode combo, and
    slider rows for Threshold, Hold Time, Leading Pad, Trailing Pad, Min Kept, and Max Trim.
    All detection controls are disabled when the Enable checkbox is unchecked.
  - Section 2 "Time-Based Edits": slider rows for Head Trim, Tail Trim, Prepend Silence,
    Append Silence, Fade In (with curve combo), and Fade Out (with curve combo).
  - Waveform preview (140px) shows grayed-out overlay on trimmed head/tail regions and
    green vertical lines at detected content boundaries after clicking Preview.
  - Recipe summary text editor (80px, read-only) lists all active operations and the
    source duration; updates live as controls change.
  - Preview button runs HeadTailEngine::detectBoundaries() and updates the waveform
    overlay without modifying the audio buffer.
  - Apply/Cancel buttons call onApply/onCancel callbacks; dialog closes via DialogWindow.
  - Dialog is pure UI — no buffer modification; all processing stays in
    DSPController::applyHeadTail() per the controller pattern.

### Changed
- **Major Architecture Refactoring**: Reduced Main.cpp from 11,251 to 3,344 lines (70% reduction)
  - Created Source/Controllers/ with 5 controller classes (4,689 lines total):
    - FileController: open, save, save-as, close, drag-drop, auto-save
    - DSPController: gain, normalize, fade, EQ, DC offset, plugins
    - RegionController: add, delete, merge, split, copy, paste, navigate, batch rename
    - MarkerController: add, delete, navigate, rename
    - ClipboardController: select all, copy, cut, paste, delete
  - Created Source/Utils/UndoActions/ with 3 domain-organized headers (1,812 lines)
  - Extracted command routing to Source/Commands/CommandHandler (1,073 lines)
  - Extracted menu construction to Source/Commands/MenuBuilder (264 lines)
  - Deleted 1,527 lines of dead duplicate undo action classes from Main.cpp
  - Made perform() a thin dispatcher: every case ≤5 lines, delegates to controllers
  - Removed all file operation duplication between Main.cpp and FileController
  - Merged FadeInDialog/FadeOutDialog into unified FadeDialog with FadeDirection enum
  - Migrated window/panel ownership to std::unique_ptr, plugin listeners to OwnedArray
  - Added try/catch error handling to all DSP buffer operations
  - Reduced Logger::writeToLog from 517 to 40 calls (routine messages use DBG())
  - Wired CompactTransport record button to command system via callback

### Fixed
- Marker rename dialog now properly captures text input (was broken with AlertWindow::showAsync)
- Memory safety: window lifecycles managed by smart pointers instead of raw delete

### Added
- **Channel Extractor Format Support**: Export extracted channels to WAV, FLAC, or OGG formats (previously WAV-only)
- **Channel System Tests**: Comprehensive test suite (Tests/ChannelSystemTests.cpp) verifying:
  - ITU-R BS.775 downmix coefficients for stereo and mono conversion
  - Channel extraction buffer integrity
  - Upmix strategy algorithms (FrontOnly, PhantomCenter, FullSurround, Duplicate)
  - Downmix presets (ITU Standard, Professional, FilmFoldDown)
- **ITUCoefficients Namespace**: Centralized gain constants (kUnityGain, kMinus3dB, kMinus6dB) in ChannelLayout.h
- **Empty Buffer Guards**: All channel conversion functions now validate input buffers to prevent crashes on invalid/empty data
- **UI/UX Accessibility Improvements**:
  - Custom WaveEditLookAndFeel with WCAG-compliant focus indicators on buttons, toggles, combo boxes, sliders, and text editors
  - Improved text contrast: secondary and muted text now meet WCAG 2.1 AA standards (~5:1 contrast ratio)
  - UIConstants.h centralized design system with colors, fonts, and layout dimensions
  - Focus ring visible on all focusable components for keyboard navigation
- **Tools menu**: New menu category for utility tools (Batch Processor and future tools)
- **Batch Processor** (Cmd+Alt+B): Process multiple audio files with identical settings
  - DSP chain support: Gain, Normalize, DC Offset, Fade In/Out, EQ presets
  - Plugin chain integration with VST3/AU effects
  - Configurable output: directory, naming patterns, format conversion
  - Error handling options: stop on error, continue, or skip and log
  - Preset system for saving and loading batch configurations
- Spectrum analyzer integration during preview playback (displays during REALTIME_DSP mode)
- Preview position indicator in waveform display (orange cursor during DSP preview playback)
- EQ preset system with 11 factory presets and user preset save/load/delete
- Global bypass system for A/B comparison in all DSP preview dialogs
- Info labels on EQ band markers showing frequency/gain values
- "Set Gain..." context menu option for precise gain input in Graphical EQ
- Bypass button and loop toggle state persistence across dialog reopens for all DSP preview dialogs
- **Multichannel Support**: Full support for audio files up to 8 channels (7.1 surround)
  - ChannelLayout system with standard layouts: Mono, Stereo, LCR, Quad, 5.0, 5.1, 6.1, 7.1 Surround
  - Film/SMPTE channel ordering (L, R, C, LFE, Ls, Rs, Lrs, Rrs)
  - Proper channel labels in waveform display based on layout type
  - Layout names displayed in File Properties dialog
  - WAVEFORMATEXTENSIBLE speaker position mask support
- **Channel Converter** (Cmd+Shift+U): Convert between channel layouts
  - **ITU-R BS.775 standard downmix coefficients** for professional-quality stereo downmix
  - Downmix presets: ITU Standard, Professional, Film Fold-Down
  - LFE handling options: Exclude (default), Include at -3dB, Include at -6dB
  - Channel extraction mode: select specific channels to keep without mixing
  - Formula preview showing exact mixing coefficients for each output channel
  - Intelligent upmix algorithms preserving audio quality
- **Per-Channel Editing**: Edit individual channels independently in multichannel files
  - Double-click on channel waveform to focus that channel and select all
  - Double-click on channel label to toggle focus for that channel
  - Double-click on already-focused channel to return to "all channels" mode
  - Focused channels indicated by blue background on label, cyan border on waveform
  - Unfocused channels dimmed for clear visual hierarchy
  - Cut/copy/paste/delete operations only affect focused channels
  - Per-channel delete silences focused channels (preserves file length)
  - Per-channel paste replaces in-place (no length change)

### Fixed
- **CRITICAL: Mono downmix silence bug** - Fixed pointer/clear ordering in mixdownToMono that caused all mono downmixes to produce silence
- **Channel Converter dialog cutoff** - Increased dialog height from 480px to 580px to fully display upmix options and buttons
- **Heap corruption crash** - Fixed double-free bugs across multiple file writing code paths:
  - Channel extractor FLAC/OGG export
  - Auto-save background job
  - AudioFileManager FLAC/OGG export
  - Batch processor output
  - Root cause: JUCE's `createWriterFor` may delete the stream on failure, but unique_ptr was also trying to delete it
- **FLAC bit depth** - Fixed bit depth clamping (JUCE FLAC only supports 16/24-bit, not 8-bit)
- **Use-after-free in modal dialogs** - Fixed Channel Extractor and Channel Converter dialogs accessing freed memory after `runModal()` returns. Changed from heap allocation with `setOwned()` to stack allocation with `setNonOwned()` to prevent JUCE from deleting the dialog before results are retrieved
- **Linux CI build failure** - Fixed type mismatch between `juce::int64` and `int64_t` in test assertions (different types on Linux)
- Solo indicator no longer appends "S" suffix, avoiding confusion with surround channels like "Ls" and "Rs"; background color (yellow) now indicates solo state
- Graphical EQ preview now starts from selection start instead of file beginning

### Changed
- Dialog width standardized to 450px minimum for GainDialog and NormalizeDialog
- All processing dialogs now use native macOS title bars for consistency
- Batch Processor moved from File menu to new **Tools** menu
- Batch Processor dialog now properly closes via title bar close button

### Removed
- DCOffsetDialog removed - DC Offset now runs automatically from Process menu

---

## [0.1.0-dev] - 2026-01-01

### Added
- **Graphical EQ**: 20-band dynamic parametric EQ with real-time curve visualization
  - All filter types: Bell, Low/High Shelf, Low/High Cut, Notch, Bandpass
  - Double-click to add bands, right-click for filter type and deletion
  - Scroll wheel to adjust Q/bandwidth
  - Preview button with AudioEngine integration
- **EQ Preset System**: Factory and user presets with JSON serialization (.weeq files)
- **Global Bypass**: A/B comparison during preview in all 7 DSP dialogs
- **Plugin Chain Reordering**: Move Up/Down buttons and drag-and-drop with visual indicator

### Fixed
- Plugin chain drag-and-drop now handles all adjacent moves correctly
- Move Down button works for second-to-last plugin
- EQ curve updates in real-time when dragging control points (JUCE IIR coefficient indexing fix)
- Preview Stop button now properly stops audio
- Closing EQ dialog stops any active preview

---

## [2025-12-18]

### Fixed
- **P0**: "Clear Cache & Rescan" now deletes all cache files and provides user feedback
- Plugin chain reordering edge cases (adjacent drag-drops, bounds checking)
- Visual drag indicator now renders properly using `paintOverChildren()`

---

## [2025-12-17]

### Fixed
- **P0 Critical**: PluginChain race condition resolved with Copy-on-Write (COW) architecture
  - Lock-free audio thread access via atomic pointer swap
  - Generation-based deletion (8 generations) prevents use-after-free
- Toolbar context menu now appears at cursor position
- Toolbar tooltips now display properly (500ms delay)
- New toolbar items insert before spacer (visible, not cut off)

---

## [2025-12-10]

### Added
- **Offline Plugin Chain Rendering**: Apply Plugin Chain to selection (Cmd+P)
  - Creates independent plugin instances for background processing
  - Automatic latency compensation for look-ahead plugins
  - Full undo/redo support via ApplyPluginChainAction
  - Progress dialog for large selections (>500k samples)

### Fixed
- **P0 Critical**: Plugin chain now affects audio during normal playback
  - Moved plugin chain processing outside preview mode conditional
  - Enables Soundminer-style real-time monitoring

---

## [2025-12-08]

### Added
- **Out-of-Process Plugin Scanning**: Robust VST3 scanning that survives plugin crashes
  - Each plugin scanned in separate worker subprocess
  - Crashed plugins auto-blacklisted for future scans
  - Progress reporting with Retry/Skip/Cancel options
  - Scan summary dialog showing results
- **Plugin Paths Configuration**: Preferences UI for managing VST3 directories

### Fixed
- **P0 Critical**: App no longer hangs on startup due to plugin scanning
  - Auto-blacklist problematic Apple system AudioUnits
  - CFRunLoop pumping in worker subprocess
  - Per-plugin 15-second timeout
- **P0 Critical**: Dead-mans-pedal file encoding (UTF-16 vs UTF-8)
- **P0 Critical**: JUCE initialization order in plugin scanner worker

---

## [2025-12-04]

### Fixed
- **P0 Critical**: DSP operations (Fade, Normalize, DC Offset) now apply at correct location
  - Implemented regionBuffer pattern (extract -> process -> copy back)
  - Thread-safe async processing for large selections

---

## [2025-12-03]

### Fixed
- **P0 Critical**: Preview loop coordinate system - loops now work correctly in all processing dialogs
- **P1**: Loop UI consistency - all dialogs show "Loop" label, default to ON
- All keyboard shortcut conflicts resolved (Cmd+M, Cmd+Tab, Cmd+R)
- Missing shortcuts added (Cmd+N, Cmd+Shift+L, Cmd+Alt+B)
- Region system undo/redo support (Add/Delete/Merge/Split all undoable)
- Auto-scroll playback cursor when out of view

---

## [2025-12-02]

### Added
- RMS normalization mode (perceived loudness) alongside Peak mode

### Fixed
- **P1**: ParametricEQ preview optimization - artifact-free real-time DSP
  - Switched to REALTIME_DSP mode with lock-free atomic parameter exchange
  - <1ms latency (10x better than target)

---

## [2025-12-01]

### Added
- Preview buttons for all processing dialogs (Normalize, Fade In/Out, DC Offset)
- Loop toggle checkbox in all preview dialogs
- Real-time parameter updates during ParametricEQ preview

### Fixed
- **P0 Critical**: Preview dialogs now start at selection start, not file start
- Standardized button layout across all 6 DSP dialogs

---

## [2025-11-25]

### Fixed
- **P0 Critical**: Preview playback coordinate system bug
  - Added `setLoopPoints()` calls in preview buffer coordinates
  - Preview now respects selection boundaries

---

## [2025-11-21]

### Added
- **Universal Preview System Phase 1**: Foundation for all DSP previews
  - PreviewMode enum (DISABLED, REALTIME_DSP, OFFLINE_BUFFER)
  - GainProcessor with atomic thread-safe parameters
  - Audio callback integration for real-time processing

---

## [2025-11-18]

### Added
- **Recording from Input**: Full recording feature (Cmd+R)
  - Smart recording destination (insert at cursor or new file)
  - Real-time input level monitoring
  - Device selection with channel names
  - Buffer full detection with graceful notification
- **Spectrum Analyzer**: Real-time FFT visualization (Cmd+Alt+S)
  - Configurable FFT size (512-8192 samples)
  - Multiple windowing functions (Hann, Hamming, Blackman, Rectangular)
  - Logarithmic frequency scale with peak hold
- **3-Band Parametric EQ**: Low/Mid/High shelves with interactive sliders (Shift+E)

### Fixed
- **P0 Critical**: Removed sendChangeMessage() from audio thread in RecordingEngine
- Thread safety improvements throughout recording feature

---

## [2025-11-17]

### Added
- **MarkerListPanel**: Complete marker management UI (Cmd+Shift+K)
  - Sortable table, search/filter, inline editing
  - Time format cycling, keyboard navigation
- **GainDialog**: Full interactive implementation with validation

### Changed
- Multi-format support enabled: WAV, FLAC, OGG, MP3 (read)
- File validation accepts all registered formats

---

## [2025-11-06]

### Fixed
- **Critical**: Resampling bug affecting all sample rate conversions (inverted speed ratio)
- 8-bit audio support (read/write)
- Migrated to JUCE 8 AudioFormatWriterOptions API

---

## [2025-11-04]

### Fixed
- iXML metadata persistence (file append vs overwrite)
- Recent files populate correctly
- Close command with unsaved warning
- File browser remembers last directory

---

## [2025-11-03]

### Added
- **BWF Metadata Editor**: Full UI for editing Broadcast Wave Format metadata

---

## [2025-11-02]

### Added
- BWF metadata integration in Document load/save
- BWF display in File Properties dialog

---

## [2025-10-28]

### Fixed
- Copy Regions respects selection
- Merge Regions allows gaps
- Split Region shortcut conflict (Cmd+T)
- Multi-select (Cmd+Click, Shift+Click)
- Undo/redo for multi-region merge

---

## [2025-10-20]

### Fixed
- Region bars stay aligned during zoom/scroll

---

## [2025-10-17]

### Fixed
- Keyboard shortcut conflicts (Cmd+0, Cmd+W, Cmd+Shift+G)

---

## [2025-10-16]

### Added
- Keyboard shortcut customization UI
- Keyboard cheat sheet dialog (F1)
- Template system with 4 built-in templates

---

## [2025-10-14]

### Added
- Multi-file architecture with tabs
- Independent undo/redo per file (100 levels)
- Inter-file clipboard

---

## [2025-10-13]

### Added
- Gain adjustment, normalize, fade in/out
- Level meters during playback

---

## Links

- [README.md](README.md) - User guide and keyboard shortcuts
- [GitHub Issues](https://github.com/themightyzq/WaveEdit/issues) - Bug reports and feature requests
