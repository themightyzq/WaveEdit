# WaveEdit TODO

Current priorities, roadmap, and known issues.
For completed work, see [CHANGELOG.md](CHANGELOG.md).

---

## Priority: Finalize Universal Preview System

**Phase 6** - Remaining polish items to complete the preview system:

- [x] Spectrum analyzer integration during preview playback
- [x] Bypass button state persistence across dialog reopens
- [x] Loop toggle state persistence across dialog reopens
- [ ] Preview position indicator in waveform display

---

## Active Tasks

### In Progress
- Universal Preview System Phase 6 finalization (see above)

### Up Next
- **VST3 Plugin Hosting**: Load and use third-party VST3 plugins in the plugin chain
  - Investigate JUCE AudioPluginHost approach
  - Handle plugin state persistence
  - Add plugin parameter automation

---

## Backlog

### High Priority
- **DSP Operations**: Time stretch, pitch shift, reverse, invert, resample
- **Batch Processing**: Process multiple files with same settings
- **Markers to Regions**: Convert markers to regions and vice versa

### Medium Priority
- **UI/UX Polish**:
  - Customizable color themes
  - Waveform color options per channel
  - Selection handles for easier resizing
- **Export Options**:
  - Sample rate conversion on export
  - Bit depth conversion on export
  - Export selected regions as separate files

### Low Priority
- **Command Palette**: Quick access to all commands (Cmd+Shift+P style)
- **Background Thumbnails**: Pre-generate waveform caches for faster reopening
- **Session Recovery**: Auto-save and recover from crashes

---

## Known Issues

### Non-Blocking
- Some VST3 plugins may require multiple load attempts (workaround: retry in Offline Plugin dialog)
- Large file thumbnails may take 1-2 seconds to fully render
- Occasional toolbar highlight flicker on rapid mouse movement

### Code Quality
- Several `// TODO` comments in codebase need resolution
- Test coverage for region operations is incomplete
- Some error messages could be more user-friendly

---

## Test Coverage

### Infrastructure
- Unit tests: `./build/WaveEditTests_artefacts/Debug/WaveEditTests`
- Integration tests planned but not yet implemented
- Manual QA checklist in CLAUDE.md Section 10

### Areas Needing Tests
- Region merge/split edge cases
- Multi-file clipboard operations
- Plugin chain state persistence
- Undo/redo with large operations

---

## Links

- [CHANGELOG.md](CHANGELOG.md) - Completed work and version history
- [README.md](README.md) - User guide and keyboard shortcuts
- [CLAUDE.md](CLAUDE.md) - Architecture and AI instructions
