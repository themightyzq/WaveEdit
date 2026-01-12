# WaveEdit Changelog

All notable changes to WaveEdit are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased]

### Added
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
  - Intelligent upmix/downmix algorithms preserving audio quality
  - Preset-based selection for common formats
  - Warnings for downmix operations that combine channels

### Fixed
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
- [TODO.md](TODO.md) - Current priorities and roadmap
- [CLAUDE.md](CLAUDE.md) - Architecture and coding standards
