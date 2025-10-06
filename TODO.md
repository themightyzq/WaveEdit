# WaveEdit - Project TODO

**Last Updated**: 2025-10-06
**Current Phase**: Phase 1 (Core Editor)
**Target**: MVP in 2 weeks, full feature set in 4-6 weeks

---

## Phase 1: Core Editor (Weeks 1-2) 🚧

**Goal**: Minimal viable audio editor - open, edit, save WAV files

### [Setup] Project Infrastructure ✅
- [x] Initialize Git repository
- [x] Set up CMake build configuration
- [x] Add JUCE as submodule or dependency
- [x] Create basic project structure (Source/, Builds/, etc.)
- [x] Configure cross-platform build (macOS, Windows, Linux)
- [x] Set up .gitignore for build artifacts
- [x] Add GPL v3 LICENSE file
- [x] Create initial CMakeLists.txt with JUCE integration

### [Core] Audio Engine
- [ ] Implement AudioFormatManager for WAV loading
- [ ] Set up AudioTransportSource for playback
- [ ] Configure AudioDeviceManager for output
- [ ] Implement basic playback state machine (stopped, playing, paused)
- [ ] Add audio file loading from filesystem
- [ ] Handle invalid/corrupted file errors gracefully
- [ ] Support 16-bit, 24-bit, and 32-bit float WAV files
- [ ] Support sample rates: 44.1kHz, 48kHz, 88.2kHz, 96kHz, 192kHz
- [ ] Handle mono and stereo files

### [Core] File I/O
- [ ] Implement file open dialog (Ctrl+O)
- [ ] Add drag & drop support for WAV files
- [ ] Implement Save (Ctrl+S) - overwrite original
- [ ] Implement Save As (Ctrl+Shift+S) - new file
- [ ] Add file close functionality (Ctrl+W)
- [ ] Display file properties (sample rate, bit depth, duration)
- [ ] Handle file access errors (read-only, permissions)
- [ ] Add recent files list (track last 10 files)

### [UI] Waveform Display
- [ ] Create WaveformDisplay component (extends juce::Component)
- [ ] Implement AudioThumbnail for waveform rendering
- [ ] Add horizontal scrollbar for navigation
- [ ] Implement zoom in/out functionality
- [ ] Display stereo waveforms (dual channel view)
- [ ] Show playback cursor position
- [ ] Highlight selected region visually
- [ ] Render waveform at 60fps (optimize for performance)
- [ ] Add time ruler with sample/time markers
- [ ] Handle window resize gracefully

### [UI] Transport Controls
- [ ] Create TransportControls component
- [ ] Add Play button (Space or F12)
- [ ] Add Pause button (Enter or Ctrl+F12)
- [ ] Add Stop button
- [ ] Add Loop toggle button (Q)
- [ ] Display playback state visually (icon/color changes)
- [ ] Show current position (time and samples)
- [ ] Add scrubbing support (click to jump to position)

### [Editing] Selection & Basic Editing
- [ ] Implement click-and-drag selection on waveform
- [ ] Show selection start/end times
- [ ] Add Select All (Ctrl+A) functionality
- [ ] Implement Cut (Ctrl+X) - remove and copy to clipboard
- [ ] Implement Copy (Ctrl+C) - copy selection to clipboard
- [ ] Implement Paste (Ctrl+V) - insert from clipboard at cursor
- [ ] Implement Delete (Delete key) - remove selection
- [ ] Handle paste into empty buffer (create new data window)
- [ ] Ensure sample-accurate selection boundaries

### [Editing] Undo/Redo
- [ ] Integrate juce::UndoManager
- [ ] Implement Undo (Ctrl+Z)
- [ ] Implement Redo (Ctrl+Shift+Z)
- [ ] Track edit operations (cut, paste, delete)
- [ ] Add undo for file modifications
- [ ] Display undo/redo state in UI (grayed out when unavailable)

### [UI] Main Window
- [ ] Create MainComponent as top-level container
- [ ] Add menu bar (File, Edit, View, Help)
- [ ] Implement File menu (New, Open, Save, Save As, Close, Exit)
- [ ] Implement Edit menu (Undo, Redo, Cut, Copy, Paste, Delete, Select All)
- [ ] Add window title showing current filename
- [ ] Handle window close events (prompt to save if modified)

### [Shortcuts] Keyboard Handling
- [ ] Set up juce::ApplicationCommandManager
- [ ] Define all Phase 1 command IDs (see CLAUDE.md)
- [ ] Bind Sound Forge default shortcuts (hardcoded for now)
- [ ] Implement navigation shortcuts (arrows, Home/End, Page Up/Down)
- [ ] Test all shortcuts on macOS (Cmd vs Ctrl)
- [ ] Test all shortcuts on Windows and Linux

### [Testing] Phase 1 Validation
- [ ] Test: Open 10-minute WAV file in <2 seconds
- [ ] Test: Smooth 60fps waveform scrolling
- [ ] Test: Playback starts with <10ms latency
- [ ] Test: All keyboard shortcuts work
- [ ] Test: Cut/copy/paste operations are sample-accurate
- [ ] Test: Undo/redo works for all operations
- [ ] Test: No memory leaks (run with Valgrind or Instruments)
- [ ] Test: Graceful error handling for corrupted files
- [ ] Test: Save overwrites file correctly
- [ ] Test: Startup time <1 second

---

## Phase 2: Professional Features (Week 3) 📅

**Goal**: Keyboard shortcuts customization, meters, basic effects, auto-save

### [Settings] Configuration System
- [ ] Create Settings class for persistent config
- [ ] Implement JSON serialization/deserialization (juce::var)
- [ ] Set up platform-specific settings directory
  - [ ] macOS: ~/Library/Application Support/WaveEdit/
  - [ ] Windows: %APPDATA%/WaveEdit/
  - [ ] Linux: ~/.config/WaveEdit/
- [ ] Create settings.json structure (see CLAUDE.md)
- [ ] Create keybindings.json structure
- [ ] Load settings on startup
- [ ] Save settings on exit or change

### [Shortcuts] Customizable Keyboard Shortcuts
- [ ] Create SettingsPanel component (modal overlay)
- [ ] Add "Preferences" menu item (Ctrl+,)
- [ ] List all available commands in scrollable table
- [ ] Show current shortcut binding for each command
- [ ] Implement "Click to Rebind" functionality
- [ ] Detect keyboard input for new binding
- [ ] Check for conflicts with existing shortcuts
- [ ] Display conflict warnings to user
- [ ] Add "Reset to Defaults" button
- [ ] Implement Export keybindings (save JSON file)
- [ ] Implement Import keybindings (load JSON file)
- [ ] Add search/filter for commands list
- [ ] Save keybindings.json on changes

### [UI] Level Meters
- [ ] Create Meters component
- [ ] Implement peak level metering during playback
- [ ] Add RMS level display
- [ ] Show clip indicators (peak >0dBFS)
- [ ] Add peak hold markers (decay after 1 second)
- [ ] Display meters for stereo (left/right channels)
- [ ] Update meters at 30fps (audio thread → UI thread)
- [ ] Add meter reset button

### [Processing] Basic DSP Effects
- [ ] Implement Fade In (Ctrl+Shift+I)
  - [ ] Linear fade algorithm
  - [ ] Apply to selection or entire file
  - [ ] Run on background thread (don't block UI)
  - [ ] Show progress bar for long operations
- [ ] Implement Fade Out (Ctrl+Shift+O)
  - [ ] Linear fade algorithm
  - [ ] Apply to selection or entire file
- [ ] Implement Normalize (Ctrl+Shift+N)
  - [ ] Find peak sample in selection
  - [ ] Calculate gain to reach 0dBFS (or user-specified level)
  - [ ] Apply gain to all samples
  - [ ] Option: normalize to -0.1dB to prevent clipping
- [ ] Implement DC Offset Removal
  - [ ] Calculate average DC offset
  - [ ] Subtract from all samples
- [ ] Add effect parameter dialogs (if needed)
- [ ] Ensure all effects are undoable

### [Editing] Multi-Level Undo
- [ ] Increase undo history to 100 levels
- [ ] Optimize memory usage for large undo buffers
- [ ] Add undo history viewer (list of operations)
- [ ] Implement "Undo to" functionality (jump to specific state)
- [ ] Clear undo history on file close

### [Core] Auto-Save
- [ ] Add auto-save preference toggle (on/off)
- [ ] Add interval selection (1min, 5min, 10min)
- [ ] Create autosave/ subdirectory in settings folder
- [ ] Implement background auto-save timer
- [ ] Save to temp file with original filename + timestamp
- [ ] Auto-recover on next launch if crash detected
- [ ] Clean up old auto-save files (keep last 5)
- [ ] Display auto-save status in UI (last saved timestamp)

### [Testing] Phase 2 Validation
- [ ] Test: All shortcuts customizable and save correctly
- [ ] Test: Conflict detection works
- [ ] Test: Export/import keybindings preserves settings
- [ ] Test: Peak meters update in real-time
- [ ] Test: Fade in/out produces smooth, click-free results
- [ ] Test: Normalize reaches exactly 0dBFS (or target level)
- [ ] Test: Auto-save creates files at correct intervals
- [ ] Test: Auto-recover restores unsaved work after crash
- [ ] Test: Undo history handles 100+ operations without slowdown

---

## Phase 3: Polish (Week 4) 📅

**Goal**: Professional workflow features - numeric entry, snap-to-zero, cheat sheet, command palette

### [UI] Numeric Position Entry
- [ ] Add editable time display field (HH:MM:SS.mmm format)
- [ ] Add editable sample position field
- [ ] Implement "Go To" dialog (Ctrl+G)
- [ ] Support time input formats: samples, seconds, HH:MM:SS
- [ ] Jump to entered position on Enter key
- [ ] Validate input (reject invalid times/samples)
- [ ] Highlight current cursor position in fields

### [Editing] Snap to Zero-Crossing
- [ ] Add "Snap to Zero" toggle (Z key)
- [ ] Implement zero-crossing detection algorithm
- [ ] Adjust selection boundaries to nearest zero-crossing
- [ ] Visual indicator when snap is active
- [ ] Apply snap to selection start/end independently
- [ ] Disable snap when user holds Shift (override)

### [UI] Keyboard Shortcut Cheat Sheet
- [ ] Create cheat sheet overlay component (modal)
- [ ] Trigger with `?` key or Help menu
- [ ] Display all shortcuts organized by category
- [ ] Show current user bindings (not just defaults)
- [ ] Add search/filter functionality
- [ ] Print-friendly layout option
- [ ] Close on Esc or outside click

### [UI] Recent Files
- [ ] Store last 10 opened files in settings.json
- [ ] Display in File → Recent Files menu
- [ ] Show file path and last modified date
- [ ] Generate waveform thumbnails for recent files
- [ ] Cache thumbnails in settings directory
- [ ] Clear recent files option
- [ ] Remove invalid/deleted files from list on launch

### [UI] Command Palette
- [ ] Create CommandPalette component (modal overlay)
- [ ] Trigger with Cmd/Ctrl+K
- [ ] Implement fuzzy search for commands
- [ ] Display matching commands as user types
- [ ] Show keyboard shortcut next to each command
- [ ] Execute command on Enter or click
- [ ] Close on Esc
- [ ] Track recently used commands (show at top)

### [UI] Zoom Presets
- [ ] Implement "Fit to Window" (Ctrl+0)
- [ ] Implement "Zoom to Selection" (Ctrl+E)
- [ ] Implement "Zoom 1:1" (view all samples) (Ctrl+1)
- [ ] Add zoom in/out buttons or shortcuts (Ctrl +/-)
- [ ] Display current zoom level percentage
- [ ] Smooth zoom animation (optional, if performant)

### [Rendering] Waveform Optimization
- [ ] Implement multi-resolution waveform cache
- [ ] Generate thumbnail at multiple zoom levels (LOD)
- [ ] Stream large files in chunks (don't load all into memory)
- [ ] Use background thread for thumbnail generation
- [ ] Cache rendered waveform images for fast redraw
- [ ] Optimize paint() method for 60fps

### [Testing] Phase 3 Validation
- [ ] Test: Numeric entry jumps to correct position
- [ ] Test: Snap to zero-crossing eliminates clicks on edits
- [ ] Test: Cheat sheet displays all current shortcuts
- [ ] Test: Recent files load correctly and thumbnails match
- [ ] Test: Command palette fuzzy search finds commands
- [ ] Test: Zoom presets work correctly
- [ ] Test: Waveform maintains 60fps even with large files (30+ min)

---

## Phase 4: Extended Features (Month 2+) 📅

**Goal**: Multi-format support, recording, analysis, batch processing

### [Core] Multi-Format Support
- [ ] Add FLAC support (lossless compression)
  - [ ] Use JUCE's FlacAudioFormat
  - [ ] Test encode/decode quality
- [ ] Add MP3 support
  - [ ] Use JUCE's MP3AudioFormat or lame encoder
  - [ ] Add bitrate options (128, 192, 256, 320 kbps)
  - [ ] Handle ID3 tags (metadata)
- [ ] Add OGG Vorbis support
  - [ ] Use JUCE's OggVorbisAudioFormat
  - [ ] Add quality options
- [ ] Add AIFF support (Apple format)
- [ ] Update file open dialog to show all supported formats
- [ ] Add format selector in Save As dialog
- [ ] Handle format-specific options (bitrate, compression)

### [Core] Recording from Input
- [ ] Implement recording from audio input device (Ctrl+R)
- [ ] Create recording UI (level meter, timer)
- [ ] Add input device selector (dropdown)
- [ ] Show live waveform during recording
- [ ] Add clipping indicators for input
- [ ] Stop recording on Ctrl+R or Stop button
- [ ] Auto-create new data window with recorded audio
- [ ] Support mono and stereo recording

### [UI] Spectrum Analyzer
- [ ] Create SpectrumAnalyzer component
- [ ] Implement FFT analysis (use juce::dsp::FFT)
- [ ] Display frequency spectrum (0-22kHz for 44.1kHz)
- [ ] Update at 15-30fps during playback
- [ ] Add logarithmic frequency scale
- [ ] Add dB scale for amplitude
- [ ] Option to freeze display (F key)
- [ ] Display peak frequency value

### [UI] Spectrogram View (Optional)
- [ ] Create Spectrogram component
- [ ] Render time-frequency heatmap
- [ ] Color map: blue (low) → red (high)
- [ ] Synchronize with waveform scrolling
- [ ] Toggle between waveform and spectrogram view

### [Processing] Batch Processing
- [ ] Create batch processing dialog
- [ ] Allow selection of multiple files
- [ ] Choose effect/process to apply
- [ ] Set output directory and format
- [ ] Show progress bar for batch operation
- [ ] Run on background thread
- [ ] Log errors for failed files
- [ ] Add batch normalize, batch convert, batch fade

### [Processing] Advanced DSP
- [ ] Implement Pitch Shift
  - [ ] Use librubberband or soundtouch library
  - [ ] Preserve formants option
  - [ ] Semitone and cent adjustments
- [ ] Implement Time Stretch
  - [ ] Independent tempo change (no pitch shift)
  - [ ] Quality options (fast, balanced, high quality)
- [ ] Implement Noise Reduction
  - [ ] Capture noise profile from selection
  - [ ] Apply reduction with adjustable threshold
- [ ] Implement EQ (3-band or parametric)

### [Plugins] VST3 Hosting (Future)
- [ ] Integrate VST3 SDK
- [ ] Implement plugin scanner (find installed VST3s)
- [ ] Create plugin host component
- [ ] Display plugin GUI (editor window)
- [ ] Route audio through plugin chain
- [ ] Save plugin settings with project (or as preset)
- [ ] Add plugin bypass/enable toggle
- [ ] Support multiple plugin instances

### [Testing] Phase 4 Validation
- [ ] Test: FLAC files load and save without quality loss
- [ ] Test: MP3 encoding at various bitrates
- [ ] Test: Recording captures input without dropouts
- [ ] Test: Spectrum analyzer displays accurate frequencies
- [ ] Test: Batch processing completes without errors
- [ ] Test: Pitch shift preserves audio quality
- [ ] Test: VST3 plugins load and process audio correctly

---

## Backlog / Future Ideas 💡

### Nice-to-Have Features
- [ ] Region markers (named sections)
- [ ] Playlist/cutlist (arrange multiple sections)
- [ ] Script editor for automation (JavaScript or Lua)
- [ ] MIDI trigger support (start playback via MIDI)
- [ ] Network rendering (offload heavy processing to server)
- [ ] Cloud sync for settings/shortcuts (optional, privacy-first)
- [ ] Plugin presets library (share user presets)
- [ ] Video file audio extraction (FFmpeg integration)
- [ ] Loudness normalization (EBU R128)
- [ ] Advanced metering (LUFS, true peak)

### UI Improvements
- [ ] Themes (light, dark, custom colors)
- [ ] Customizable toolbar layouts
- [ ] Detachable panels (meters, spectrum on separate window)
- [ ] Touch screen support (pinch-to-zoom, swipe)
- [ ] High-DPI display support (Retina, 4K)

### Platform-Specific
- [ ] macOS: Touch Bar support
- [ ] macOS: Apple Silicon optimization (ARM builds)
- [ ] Windows: ASIO driver support (low-latency)
- [ ] Linux: JACK auto-connect on launch

### Documentation
- [ ] User manual (online docs or PDF)
- [ ] Video tutorials (YouTube channel)
- [ ] API documentation (for plugin developers)
- [ ] Contributing guide (detailed)

---

## Bugs / Known Issues 🐛

_None yet - project just started!_

**Bug Tracking**:
- Use GitHub Issues for bug reports
- Label bugs by severity: critical, high, medium, low
- Assign to milestone (Phase 1, 2, 3, etc.)

---

## Performance Optimization Tasks ⚡

### Startup Time
- [ ] Profile startup sequence (identify bottlenecks)
- [ ] Lazy-load JUCE modules where possible
- [ ] Defer plugin scanning to background thread
- [ ] Cache font loading, UI resources

### File Loading
- [ ] Stream large files instead of full memory load
- [ ] Use memory-mapped files for read-only access
- [ ] Generate thumbnails on background thread
- [ ] Cache thumbnails for recent files

### Waveform Rendering
- [ ] Pre-render waveform at multiple zoom levels (LOD)
- [ ] Use double buffering for smooth redraws
- [ ] Clip rendering to visible region only
- [ ] Consider GPU acceleration (OpenGL/Metal/Vulkan) for very long files

### Playback
- [ ] Minimize audio callback processing time
- [ ] Pre-allocate all buffers (no allocations in callback)
- [ ] Use lock-free data structures for real-time thread communication
- [ ] Optimize buffer size for latency vs. CPU usage

### Memory Usage
- [ ] Profile memory usage with Valgrind or Instruments
- [ ] Fix any leaks detected
- [ ] Use smart pointers to manage lifetime
- [ ] Limit undo history size (cap at 100 or user-defined)

---

## Release Checklist 📦

### Pre-Release (Before 1.0)
- [ ] All Phase 1 & 2 features complete and tested
- [ ] No critical bugs
- [ ] Cross-platform builds working (macOS, Windows, Linux)
- [ ] Documentation complete (README, user guide)
- [ ] License file included (GPL v3)
- [ ] Code comments and cleanup
- [ ] Performance targets met (see CLAUDE.md)

### Release Process
- [ ] Tag version in Git (e.g., v1.0.0)
- [ ] Build release binaries for all platforms
- [ ] Code sign macOS and Windows binaries (if applicable)
- [ ] Create installers (.dmg, .exe, .deb, .AppImage)
- [ ] Generate release notes (changelog)
- [ ] Upload to GitHub Releases
- [ ] Announce on social media, forums, etc.

### Post-Release
- [ ] Monitor for bug reports
- [ ] Triage and prioritize issues
- [ ] Plan next release (1.1, 1.2, etc.)

---

## Notes & Reminders 📝

- **Keep it simple**: Don't add features that aren't in the roadmap without discussing first
- **Speed matters**: Every decision should prioritize performance
- **Test frequently**: Don't let bugs accumulate
- **Document as you go**: Comments, README updates, etc.
- **Ask for help**: JUCE community is helpful, don't hesitate to ask

---

**Last Updated**: 2025-10-06
**Next Review**: After Phase 1 completion
