# WaveEdit TODO

Current priorities, roadmap, and known issues.
For completed work, see [CHANGELOG.md](CHANGELOG.md).

---

## Recently Completed

- ✅ **Color theme system — Phase 3 (2026-04-30)** —
  Final coverage of every visible surface, plus an accessibility
  theme and custom-theme support. All processing dialogs (Gain,
  Normalize, Fade In/Out, Parametric EQ, Graphical EQ, Head & Tail,
  Strip Silence, Looping Tools, Auto Region, New File, Go-to,
  Batch Export, Edit Region Boundaries, Offline Plugin) read their
  panel/border/text/grid/state tokens from the theme. Plugin chrome
  (Plugin Manager, Plugin Editor toolbar) follows the
  PluginChainWindow pattern: member colour constants converted to
  method accessors so they re-skin live. Toolbar buttons,
  ShortcutEditorPanel, KeyboardCheatSheetDialog, FadeCurvePreview,
  and the Settings panel itself are all themed. New built-in
  **High Contrast** theme: pure-black surfaces, neon-cyan accents,
  saturated state colours — added to ThemeManager and surfaced in
  the Settings picker. Settings → Display now has **Import...** and
  **Export...** buttons next to the picker; themes round-trip
  through a single JSON file (one `id`, one `name`, an 8-digit ARGB
  hex per token; reserved ids `dark` / `light` / `high-contrast`
  cannot be shadowed). Build + 372 tests stay green.
- ✅ **Color theme system — Phase 2 (2026-05-01)** —
  Extended the Phase 1 token vocabulary to every major UI surface:
  TabComponent (selected/hover/normal tab backgrounds, accent
  underline, text), PluginChainWindow (full re-skin via
  `applyThemeColours()` helper that re-applies child-component
  setColour calls on theme switch), PluginChainPanel (same pattern),
  RegionListPanel + MarkerListPanel (theme-driven row backgrounds,
  text colours), AutomationLanesPanel (16 namespace constants
  converted to theme-driven free functions; subscribes to
  ThemeManager for live re-skin), CommandPalette overlay
  (chrome + text, keeping per-category badge colours unchanged
  since they're workflow-specific), and the remaining
  WaveformDisplay paint sites (channel-label state colours
  routed through `warning` / `error` / `accent` tokens). All
  themed components register with ThemeManager and repaint live
  on switch. Settings picker note updated to reflect actual
  coverage. 372 groups, 0 failures. Phase 3 (processing dialogs
  + high-contrast theme + custom-theme JSON) filed in Backlog.
- ✅ **Color theme system — Phase 1 (2026-05-01)** —
  Typed `waveedit::Theme` token vocabulary (~18 semantic tokens) +
  `ThemeManager` singleton holding the active theme + settings
  persistence (`display.theme = "dark" | "light"`). Two built-in
  themes: **Dark** (existing visual identity, ported verbatim) and
  **Light** (neutral counterpart). Settings → Display tab gets a
  theme picker dropdown labelled with an explicit "Phase 1 — affects
  waveform area only" note so the partial coverage isn't a surprise.
  WaveformDisplay subscribes to the manager and re-skins live; the
  per-channel waveform colour fallback now reads from the theme so
  a fresh install on Light theme gets a Light-tuned waveform line
  without manually setting the override. 8 new unit tests cover token
  population, switching, persistence, and listener notification.
  364 → 372 groups. Phase 2 (re-theme the rest of the UI) filed
  in Backlog.
- ✅ **Time stretch + pitch shift via SoundTouch (2026-04-30)** —
  Two new Process menu items: **Time Stretch...** and **Pitch Shift...**
  SoundTouch (LGPL-2.1) bundled in Release builds (same shape as the
  existing LAME integration). New `Source/DSP/TimePitchEngine.{h,cpp}`
  is a thin wrapper over SoundTouch with planar↔interleaved
  conversion and chunked processing. Tempo range -50% to +500%;
  pitch range ±24 semitones. `TimePitchUndoAction` makes both
  operations undoable. 8 new unit tests (identity, ±2x tempo,
  ±1 octave pitch, stereo, empty input). NOTICE updated.
  356 → 364 groups. Follow-ups: real-time preview during the
  dialog, formant-preservation toggle.
- ✅ **Crash Recovery UI (2026-04-30)** —
  Per-file recovery on file open. When a user opens an audio file
  with a newer auto-save in `<settings>/autosave/`, a 3-button
  dialog (`Recover` / `Discard` / `Keep & Continue`) offers to
  restore the unsaved changes. "Recover" replaces the document's
  buffer with the auto-save's audio and marks dirty (force-Save to
  commit); "Discard" deletes them; "Keep & Continue" leaves them
  on disk for later. Saving a file evicts its auto-saves
  (superseded). Detection helpers extracted to UI-free
  `Source/Utils/AutoSaveRecovery.{h,cpp}`. 7 new unit tests cover
  detection (none / stale / fresh / multi-save / different-file),
  eviction-only-matches, and a smoke test against the real dir.
  347 → 354 groups.
- ✅ **Plugin chain presets bundle automation (2026-04-30)** —
  `.wepchain` JSON gains an optional top-level `"automation"` block
  carrying `AutomationManager::toVar()` output. Empty managers omit
  the block to keep small presets small. New `PluginPresetManager`
  overloads (`savePreset` / `loadPreset` / `exportPreset` /
  `importPreset`) take an `AutomationManager&` alongside the chain.
  Existing chain-only signatures preserved unchanged — `BatchJob`
  keeps working without modification, and chain-only callers
  reading new files cleanly ignore the automation block.
  Loading a preset replaces both the chain and the manager
  (matches `chain.loadFromJson` "load = clear + add" semantics);
  loading a legacy preset clears the manager. New
  `PluginPresetManagerTests` (7 groups) covers round-trip,
  empty-skip, backwards compat both directions, file-not-found,
  and malformed JSON. 340 → 347 groups. Follow-up: a UI for
  plugin-chain preset save/load (none exists today; the format
  is now ready for one).
- ✅ **Automation multi-select + copy/paste (2026-04-30)** —
  Cmd-click to toggle a point's selection, Cmd+A to select all, Cmd+C/V
  to copy/paste (paste anchors at the playhead position; copies preserve
  relative offsets), Delete to remove selection — all single undo
  steps. Multi-drag moves every selected point by the dragged-point
  delta. Selected points render in orange. Cross-lane copy/paste
  works via the process-wide `AutomationClipboard` singleton.
  Selection auto-clears on lane lifecycle changes (add/remove/sidecar
  reload) but persists across point edits and recorder appends.
  6 new unit tests; 334 → 340 groups. Marquee/lasso area-select
  filed as follow-up.
- ✅ **Automation point undo/redo (2026-04-30)** —
  Each gesture in the AutomationLanesPanel curve view (click-to-add,
  drag-to-move, right-click delete, set curve type, clear lane) now
  becomes a single transaction on the document's UndoManager. New
  `AutomationCurveUndoAction` stores before/after point-list
  snapshots and looks up the lane by `(pluginIndex, parameterIndex)`
  so deleting a lane doesn't dangle pending undo entries — they
  inert-no-op gracefully. Cmd+Z / Cmd+Shift+Z work for these edits.
  Drag is one transaction (snapshot at mouseDown, commit at mouseUp).
  7 new unit tests cover skipFirstPerform, round-trip for each
  gesture type, and lane-removed safety. Total: 327 → 334 groups.
- ✅ **UndoActions §7.5 split (2026-04-30)** — `AudioUndoActions.h`
  (1,028 → 38 lines) and `RegionUndoActions.h` (964 → 35 lines) are
  now thin umbrella headers; the 25 individual undo-action classes
  live under `Source/Utils/UndoActions/` in 6 new sub-domain files,
  each well under the CLAUDE.md §7.5 500-line cap (max 381). All
  existing `#include` sites compile unchanged. Tests: 327 groups,
  0 failures (no behavior change).
- ✅ **Plugin Parameter Automation — UI lanes shipped (2026-04-30)** —
  AutomationLanesPanel + DocumentWindow with click-to-add /
  drag-to-move / right-click-to-delete-or-curve point editing.
  PluginEditorWindow toolbar gains an "Automation" button that lists
  every parameter and lets users toggle record-arm in one click.
  Cmd+Alt+L menu shortcut. Closes Phases 4-UI and 5; Phases 1-3 + 6
  were already in. Follow-ups: undo for point edits;
  per-knob right-click on native plugin UIs.
- ✅ **Architecture Refactoring complete** — Main.cpp now satisfies every
  CLAUDE.md §6.7 / §8.1 requirement:
  - Main.cpp: 11,251 → **281 lines** (97% reduction)
  - `MainComponent` (and the `SelectionInfoPanel` /
    `CallbackDocumentWindow` helpers) extracted to
    `Source/MainComponent.h`. Main.cpp is now just
    `WaveEditApplication` + `main()`.
  - **9 controllers**: File, DSP, Region, Marker, Clipboard, Playback,
    Recording, Dialog, Plugin.
  - CommandHandler + MenuBuilder extracted; the 648-line `perform()`
    switch lives in `Commands/CommandHandler.cpp` per §8.1, with
    `MainComponent::perform()` reduced to a one-line forward and
    `CommandHandler` declared a `friend` of `MainComponent`.
  - UndoActions split per §8.1 across 6 domain headers under
    `Utils/UndoActions/` (Audio, Fade, Region, Marker, Plugin, Channel).
    `UndoableEdits.h` trimmed to base + generic primitives.
  - **Every `perform()` case is now ≤5 lines.**
  - File operation duplication eliminated; FadeInDialog / FadeOutDialog
    merged into FadeDialog; smart-pointer ownership; try/catch on all
    DSP operations; logger cleaned 517 → 50 project-wide.
  - Five post-refactor regressions (Graphical EQ, Channel Converter,
    Channel Extractor, Apply Plugin Chain, Offline Plugin) all
    restored.
  - Real bug fixed: `AudioEngine::reloadBufferPreservingPlayback()`
    now preserves the cursor across paused-state buffer reloads
    (previously snapped to 0 when `isPlaying()` was false).
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
- ✅ **Looping Tools** - Selection-driven seamless loop creation (nvk_LOOPMAKER-style)
  - LoopEngine + LoopRecipe: zero-crossing search, crossfade (linear/equal power/S-curve)
  - Multi-variation generation with offset stepping and shuffle
  - Shepard Tone mode: resampling-based pitch ramp, two-layer crossfade
  - Real-time preview playback with debounced rebuild
  - Color-coded diagnostics: discontinuity, zero-crossing, crossfade metrics
  - 10 comprehensive unit tests, docs at `docs/looping_tools.md`
  - Entry: Tools menu > Looping Tools... (Cmd+L)
- ✅ **Command Palette** - Quick access to all commands (Cmd+Shift+A)
  - Fuzzy search across 133+ commands, keyboard-driven with shortcuts shown
  - Category badges with per-category colors, disabled command dimming
- ✅ **Head & Tail Processing Tool** - Intelligent detection + time-based edits
  - `HeadTailRecipe` settings + `HeadTailEngine` (`Source/DSP/`)
  - Detection: peak/RMS threshold, hold time, leading/trailing pad
  - Time-based ops: head/tail trim, prepend/append silence, fade in/out
  - Full UI dialog (700×680) with waveform preview, recipe summary,
    before/after duration display
  - DSPController-routed apply path with undo support
    (`HeadTailUndoAction`)
  - 8 unit tests covering exact sample counts, multichannel, edge cases
  - Entry: Tools menu → Head & Tail...
  - Future: Batch Processor integration (engine is already reusable)

---

## Active Tasks

### Up Next
*(Empty.)* The previous large in-flight feature — Plugin Parameter
Automation Phases 1–6 — is complete, with all follow-ups either
shipped or filed under **Backlog → Medium Priority**. Pick the next
item from the Backlog when planning a session.

---

## User-Facing Issues (End-User Review 2026-04-29)

End-user expert review found documentation, install, and UX gaps that
materially affect first-run trust and adoption. Severity per finding;
work top-down.

### Blockers
- ✅ **README keyboard shortcuts table is largely wrong** (resolved
  2026-04-29) — README §Keyboard Shortcuts rebuilt from
  `Templates/Keymaps/Default.json`; doc-tested against keymap.

### Resolved / Downgraded
- ✅ **No usable end-user install path** — downgraded to minor;
  scope was wrong. Audience is dev-friendly; tagged Releases from
  GitHub Actions + a documented Gatekeeper/SmartScreen first-run
  workaround is the right shape. Workflow + README updated
  2026-04-29 (push a `v*` tag → Release publishes automatically).
  Code-signing not needed.

### Major
- ✅ **Plain `S` silences the selection** (resolved 2026-04-29) —
  rebound to `Alt+Shift+S` in Default keymap; README updated.
- ✅ **`cd waveedit` after `git clone WaveEdit.git` fails on
  case-sensitive filesystems** (resolved 2026-04-29) — `cd WaveEdit`.
- ✅ **README claims 4 keymap templates, only 3 ship** (resolved
  2026-04-29) — README/source comments now consistently list
  Default, Sound Forge, Pro Tools.
- ✅ **README lists `Cmd+Alt+B` for both Edit BWF Metadata and Batch
  Processor** (resolved 2026-04-29) — Batch Processor moved to
  `Cmd+B` in Default keymap and CommandHandler fallback; README
  updated.
- ✅ **NOTICE file omits bundled LAME/codecs** (resolved 2026-04-29)
  — full attribution for LAME (LGPL-2.1), libFLAC, libvorbis,
  Steinberg VST3 SDK in NOTICE.
- ✅ **No application log file or crash-dump location** (resolved
  2026-04-29) — `juce::FileLogger` installed in
  `WaveEditApplication::initialise()` (logs to
  `~/Library/Logs/WaveEdit/WaveEdit.log` and platform equivalents).
  `juce::SystemStats::setApplicationCrashHandler` writes a
  timestamped backtrace to
  `~/Library/Logs/WaveEdit/crashes/crash-YYYY-MM-DD_HH-MM-SS.txt`
  on fatal signals. Verified by sending `kill -SEGV` to a running
  build — file appears with version, OS, CPU info, and full
  backtrace. README §Configuration documents both paths.
- ✅ **Screenshots in README** (resolved 2026-04-29) — five views
  shipping under `Docs/screenshots/`: main window with selection,
  Region List panel, 3-Band Parametric EQ, Spectrum Analyzer, and
  Batch Processor. README §Screenshots has captions for each.

### Minor
- ✅ **Settings location documented in README is wrong** (resolved
  2026-04-29; consolidation done 2026-04-29) — `Settings.cpp` now
  writes to `~/Library/Application Support/WaveEdit/` on macOS
  (matches Keymaps/Toolbars/Presets); first launch under the new
  build copies anything from the legacy `~/Library/WaveEdit/` into
  the new path and leaves the old folder as a backup. README
  §Configuration documents the unified layout.
- ✅ **`Cmd+W` is documented as both "Close Window" and "Close Tab"**
  (resolved 2026-04-29) — README now uses "Close Tab" only.
- ✅ **No `--help` / `--version` on the binary** (resolved 2026-04-29)
  — `handleCommandLineFlags()` in `Source/Main.cpp` intercepts
  `-h`/`--help`/`-v`/`--version` before any GUI initialization.
- ✅ **No uninstall instructions** (resolved 2026-04-29) —
  README §Configuration lists every directory + plist key with a
  ready-to-paste `rm -rf` block.
- ✅ **`./build-and-run.command` hangs on interactive prompt in
  non-TTY environments** (resolved 2026-04-29) — script now
  short-circuits when stdin isn't a TTY, when `CI` is set, or when
  `WAVEEDIT_NONINTERACTIVE=1`.

### Polish
- ✅ **`build.log` committed at repo root + 1 real warning** (resolved
  2026-04-29) — `build.log` is already gitignored via `*.log` (was
  never tracked); `unused variable 'wasPlaying'` in
  `Utils/UndoActions/AudioUndoActions.h:72` fixed with
  `[[maybe_unused]]`.
- ✅ **`KeymapManager.h:31` source comment still references 4
  templates** (resolved 2026-04-29) — comment now lists exactly
  Default, Sound Forge, Pro Tools.
- ✅ **README and Default.json can drift again** (resolved
  2026-04-29) — `scripts/check_readme_keymap.py` parses both files
  and fails on any mismatch; CI runs it on every push as the
  `doc-checks` job in `.github/workflows/build.yml`.

---

## Backlog


### Medium Priority
- **Plugin Parameter Automation — Native plugin editor right-click**.
  *Scoping required before code.* The "Automation" toolbar button on
  PluginEditorWindow is today's workaround. Going beyond requires:
    - a research spike on how to intercept right-click for native
      VST3/AU UIs (they swallow the event before our component sees
      it — possible angles: swizzling JUCE's `VST3PluginInstance`
      wrapper, OS-level event monitor with accessibility permission,
      or instrumenting only the generic `juce::GenericAudioProcessorEditor`);
    - even with intercept, mapping screen coordinates to a parameter
      requires per-plugin metadata that VST3/AU don't expose.
  Realistic outcome: a spike that ends in "this doesn't work cleanly
  for third-party plugins" with the toolbar button staying as the
  answer. Authorize the spike specifically — don't pick it up
  expecting it to ship a feature.
- **Time stretch / pitch shift — follow-ups**.
  - Real-time preview during the dialog (universal preview system
    integration; would need a SoundTouch instance running in the
    audio callback). The current dialogs use AlertWindow without
    preview — works, but doesn't match the §6.8 dialog footer
    protocol.
  - Formant-preservation toggle. SoundTouch's pitch shift smears
    formants ("chipmunky" on vocals at large semitone shifts). A
    formant-correction layer is its own research piece.
- **UI for plugin-chain preset save/load**: shipped 2026-04-30
  via the Plugin Chain window's "Presets..." button. The Batch
  Processor's preset path could also expose Save/Export from its
  own UI for symmetry — currently it only loads.
---

## Known Issues

### Non-Blocking
- Some VST3 plugins may require multiple load attempts (workaround: retry in Offline Plugin dialog)
- Large file thumbnails may take 1-2 seconds to fully render
- Occasional toolbar highlight flicker on rapid mouse movement

### Code Quality
- Main.cpp: **281 lines** ✅ (was 3,318; CLAUDE.md §8.1 <2,000 cap met).
- `perform()` lives in `Commands/CommandHandler.cpp` per §8.1, with
  every case ≤5 lines per §6.7. ✅
- 9 controllers: File, DSP, Region, Marker, Clipboard, Playback,
  Recording, Dialog, Plugin. ✅
- Remaining file-size violations (CLAUDE.md §7.5):
  - `.cpp` >1,500: `WaveformDisplay.cpp` 2,799,
    `BatchProcessorDialog.cpp` 2,465, `Commands/CommandHandler.cpp` 1,876
    (gained the `perform()` switch), `UCSCategorySuggester.cpp` 1,827,
    `PluginManager.cpp` 1,763, `Audio/AudioEngine.cpp` 1,537.
    Recently brought under cap (2026-05-01):
      - `Controllers/DSPController.cpp` 1,602 → 1,297 + 1,061-line
        `DSPController_Advanced.cpp` (EQ, channel, plugin, head-tail,
        loop, time-pitch, resample).
      - `UI/GraphicalEQEditor.cpp` 1,526 → 1,280 + 266-line
        `GraphicalEQEditor_Presets.cpp` (refresh / select / save /
        delete preset flows).
    (`UCSCategorySuggester_generated.cpp` is auto-generated — exempt.)
  - `.h` >500: `MainComponent.h` 1,959, `Audio/AudioEngine.h` 989,
    `UI/WaveformDisplay.h` 748, `Utils/AudioUnits.h` 530.
    (`AudioUndoActions.h` and `RegionUndoActions.h` were split into
    sub-domain files on 2026-04-30 and are now umbrella headers.)
  Closing these requires breaking up the underlying classes (e.g.
  splitting `MainComponent` into per-panel controllers, splitting
  `WaveformDisplay` into rendering/interaction sub-classes) — bigger
  refactors than the rule itself.
- Some error messages could be more user-friendly.

---

## Test Coverage

### Current Tests
- **Test runner**: `./build/WaveEditTests_artefacts/Release/WaveEditTests`
  reports **364 groups / ~207,000 assertions / 0 failures** locally
  (CI is similar with device-dependent groups skipped via
  `WAVEEDIT_CI=1`). New test files this round:
  `Tests/Unit/AutomationLanesPanelTests.cpp`,
  `Tests/Unit/PluginPresetManagerTests.cpp`,
  `Tests/Unit/CrashRecoveryTests.cpp`,
  `Tests/Unit/TimePitchEngineTests.cpp`.
- **Region System** — fully covered in build:
  - `Tests/Unit/RegionManagerTests.cpp`
  - `Tests/Integration/MultiRegionDragTests.cpp`
  - `Tests/Integration/RegionListBatchRenameTests.cpp`
  - `Tests/Integration/RegionListPanelTests.cpp` — rewritten as a
    proper `juce::UnitTest`, 9 groups passing.
- **Channel System**: `Tests/ChannelSystemTests.cpp` (650+ lines, 25 groups)
- **DSP**: `Tests/Unit/{HeadTailEngine,LoopEngine,FadeCurveTypes}Tests.cpp`
- **File I/O / multi-doc / clipboard**: integration tests in build
- Manual QA checklist in CLAUDE.md §10

### Areas Needing Tests
- Plugin chain state persistence
- Undo/redo with large operations
  (Marker system: covered by `Tests/Unit/MarkerManagerTests.cpp`.
   Plugin Parameter Automation: covered by
   `Tests/Unit/AutomationRecorderTests.cpp` and
   `Tests/Unit/AutomationLanesPanelTests.cpp`.)

---

## Links

- [CHANGELOG.md](CHANGELOG.md) - Completed work and version history
- [README.md](README.md) - User guide and keyboard shortcuts
- [CLAUDE.md](CLAUDE.md) - Architecture and AI instructions
