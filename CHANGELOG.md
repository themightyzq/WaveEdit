# WaveEdit Changelog

All notable changes to WaveEdit are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased]

### Changed
- **CommandHandler split for §7.5 file-size cap.** `Commands/
  CommandHandler.cpp` (1,876 lines → over the 1,500 cap) is now 877
  lines plus a new 1,026-line `CommandHandler_GetInfo.cpp` that hosts
  `getCommandInfo()` — the per-command switch that fills in menu
  titles, descriptions, categories, default keypresses, and active
  state. The constructor, `getAllCommands()`, and the `performCommand()`
  dispatch all stay in the main file. No behaviour changes; all 372
  test groups still pass.

- **UCSCategorySuggester data extracted from logic.**
  `Utils/UCSCategorySuggester.cpp` (1,827 lines → over the 1,500 cap)
  is now 201 lines: only the constructor, lookup methods, and scoring.
  The `initializeKeywordMappings()` data table — the full UCS v8.2.1
  category list, hand-rolled `m_mappings.push_back(...)` calls — moved
  to the already-existing `UCSCategorySuggester_generated.cpp`, which
  is now actually compiled. (The hand-written .cpp had a duplicate
  body that was shadowing the generated file; CMake was only building
  the hand-written one. They're now de-duplicated and the generated
  file is the single source of truth for the data.) The generated
  file (1,645 lines) is exempt from §7.5 as auto-generated data.

- **PluginManager split for §7.5 file-size cap.** `Plugins/
  PluginManager.cpp` (1,763 lines → over the 1,500 cap) is now 1,285
  lines plus a new 497-line `PluginManager_Persistence.cpp` hosting
  every disk-touching method: the plugin-list XML cache (`saveCache`,
  `loadCache`, `clearCache`, `getPluginCacheFile`), the blacklist
  text file (`saveBlacklist`, `loadBlacklist`, `addToBlacklist`,
  `removeFromBlacklist`, `isBlacklisted`, `getBlacklist`,
  `clearBlacklist`, `getBlacklistFile`), the custom-search-path
  text file (`get/setCustomSearchPaths`, `save/loadCustomSearchPaths`,
  `getCustomPathsFile`), the incremental-cache XML
  (`save/loadIncrementalCache`, `getIncrementalCacheFile`), the
  crash-notification queue (`getAndClearNewlyBlacklistedPlugins`,
  `hasNewlyBlacklistedPlugins`), and the default-blacklist seeding
  stub. Scanning thread, plugin-instance creation, and
  search/discovery APIs all stay in the main file. No behaviour
  changes; all 372 test groups still pass.

- **AudioEngine split for §7.5 file-size cap.** `Audio/AudioEngine.cpp`
  (1,537 lines → over the 1,500 cap) is now 1,226 lines plus a new
  336-line `AudioEngine_Preview.cpp` hosting the preview-system
  surface: entering/leaving preview mode, loading the preview buffer,
  the atomic per-processor setters (`setGainPreview`,
  `setParametricEQPreview`, `setDynamicEQPreview`, `setNormalizePreview`,
  `setFadePreview`, `setDCOffsetPreview`), the bypass / reset / disable
  helpers, and the selection-offset accounting. The audio callback,
  device management, file loading, transport, and level/solo/mute
  surfaces all stay in the main file. No behaviour changes; all 372
  test groups still pass.

- **GraphicalEQEditor split for §7.5 file-size cap.** `UI/
  GraphicalEQEditor.cpp` (1,526 lines → over the 1,500 cap) is now
  1,280 lines plus a new 266-line `GraphicalEQEditor_Presets.cpp`
  hosting the preset-management methods: `refreshPresetList`,
  `presetSelected`, `savePreset` + `doSavePreset` (with overwrite
  confirmation), and `deletePreset`. The visualisation, mouse /
  keyboard interaction, and DSP plumbing all stay in the main file.
  No behaviour changes; all 372 test groups still pass.

- **DSPController split for §7.5 file-size cap.** `Controllers/
  DSPController.cpp` (1,602 lines → over the 1,500 cap) is now split
  into `DSPController.cpp` (1,297 lines: core + simple level
  operations — gain, normalize, fade, DC offset, silence, reverse,
  invert, trim) and a new `DSPController_Advanced.cpp` (1,061 lines:
  EQ dialogs, channel converter / extractor, plugin chain + offline
  plugin application, head & tail, looping tools, resample,
  time-stretch, pitch-shift). Both files are well under the cap. No
  behavior changes; all 372 test groups still pass.

### Added
- **Color theme system — Phase 3 (full coverage + High Contrast +
  custom JSON themes).** All remaining UI surfaces now route through
  the theme: every processing dialog (Gain, Normalize, Fade In/Out,
  Parametric EQ, Graphical EQ, Head & Tail, Strip Silence, Looping
  Tools, Auto Region), plugin chrome (Plugin Manager, Offline Plugin,
  Plugin Editor toolbar), Settings panel + tabs, the keyboard
  shortcut editor and cheat-sheet, the toolbar buttons, the fade
  curve preview, and the Edit Region Boundaries dialog. A new
  **High Contrast** built-in theme is available alongside Dark and
  Light — pure-black surfaces with neon-cyan accents, tuned for
  WCAG AAA legibility. Settings → Display now has **Import...** and
  **Export...** buttons next to the theme picker: themes round-trip
  to a single JSON file (one `id`, one `name`, an 8-digit ARGB hex
  value per token), so you can ship custom palettes without a
  rebuild. Reserved ids (`dark`, `light`, `high-contrast`) cannot
  be shadowed.

- **Color theme system — Phase 2 (most surfaces follow the theme).**
  Extends the Phase 1 token system to the major UI surfaces beyond
  the waveform area: tabs, plugin chain window + panel, region list
  panel, marker list panel, automation lanes panel, command palette,
  and the remaining WaveformDisplay paint sites (channel-label state
  colours). All these surfaces now subscribe to `ThemeManager` and
  re-skin live when the theme is switched. Settings picker note
  updated to reflect actual coverage — only some processing dialogs
  still appear in the original Dark colours (filed as Phase 3).

- **Color theme system — Phase 1 (Light + Dark).** Settings → Display
  has a new theme picker. Switching to **Light** instantly re-skins
  the waveform area: ruler, channel backgrounds, focus border, grid
  lines, and the default waveform line colour all adopt the active
  theme. Picker is labelled "Phase 1 — affects waveform area only"
  because the rest of the UI (dialogs, panels, toolbars) hasn't been
  routed through the new tokens yet — that's Phase 2. Theme choice
  persists across launches under `display.theme`. 8 new unit tests
  cover token population, switching, settings persistence, and
  listener notification.

- **Time Stretch and Pitch Shift.** Two new entries under the Process
  menu, backed by SoundTouch (LGPL-2.1). **Time Stretch...** changes
  tempo without affecting pitch (range -50% to +500%); **Pitch
  Shift...** shifts pitch without changing length (range ±24
  semitones). Both write a single undoable transaction; identity
  recipes are no-ops. SoundTouch is now bundled in Release builds
  alongside the existing LAME library — `brew install sound-touch`
  on macOS or the equivalent distribution package on Linux.
  NOTICE has the full LGPL attribution. 8 new unit tests cover
  identity / ±2× tempo / ±1 octave pitch / stereo / empty input.

- **Persistent waveform thumbnail cache.** The first time a file is
  opened the waveform is generated normally; on completion the binary
  thumbnail is written to `<settings>/thumbnail_cache/<hash>_<size>_
  <mtime>.thumb`. Subsequent opens of the same file load the cached
  thumbnail directly and skip audio decoding entirely — no more
  waiting for the waveform to render on large files. Cache invalidation
  is automatic (the filename includes size + mtime); stale entries
  are evicted when the cache exceeds 100 entries.

- **Selection edge handles are now grabbable.** Click within ~6 pixels
  of the selection's start or end line and drag — the selection
  resizes by pivoting around the opposite edge. Dragging past the
  anchor flips the active edge (start/end normalize automatically).
  Previously the yellow corner glyphs were purely decorative;
  clicking them just cleared the selection.

- **Per-channel waveform colors.** Right-click a channel label in the
  waveform display and choose **Set Channel Color...** to pick a
  custom colour for just that channel; the global "display.waveformColor"
  setting is the fallback when no override is set. Per-channel
  overrides persist as `display.waveformColor.ch<N>` in settings.
  **Reset Channel Color** clears the override. The live colour picker
  updates the rendering immediately.

- **Marker parity with regions.** Renaming and color changes from the
  Marker List Panel now go through the document UndoManager (Cmd+Z
  reverts). Click a marker's color cell to open a colour picker;
  changes apply live and are undoable. New Export.../Import... buttons
  in the panel write/read marker JSON files (import appends). Two new
  UndoableActions: `RenameMarkerUndoAction`, `ChangeMarkerColorUndoAction`.

- **Plugin chain preset save/load UI.** The Plugin Chain window
  toolbar now has a "Presets..." button that opens a popup menu with
  Save, Export to file, Import from file, and a list of every
  user-saved preset (click to load). Saved presets bundle the
  document's automation lanes when present, so loading a chain
  preset on a different file restores both the plugin states and
  the automation in one step. Delete is offered via a confirmation
  dialog.

- **Marquee select for automation points.** Drag from empty space in
  the curve view to draw a rectangle; every point inside becomes
  selected. Shift+drag adds to the existing selection instead of
  replacing it. Esc clears selection. Click without dragging still
  adds a point at the click position (same as before). 2 new unit
  tests cover non-additive and additive paths.

- **Crash recovery dialog.** When you open an audio file that has
  unsaved changes from a previous session (the app crashed, was
  force-quit, or you closed without saving), WaveEdit now offers to
  restore them. The detection is per-file: if any auto-save in
  `<settings>/autosave/` targets the file you're opening and is
  newer than the file on disk, a three-button dialog asks whether to
  **Recover** (replace the document's audio with the auto-save and
  mark dirty so you must Save to commit), **Discard** (delete the
  auto-saves), or **Keep & Continue** (leave them on disk for later).
  A normal save also evicts that file's auto-saves now. 7 new unit
  tests cover the detection algorithm and the eviction-on-save path.

- **Multi-select + copy/paste for automation points.** In the
  Automation Lanes panel, Cmd-click a point to add or remove it from
  the selection, Cmd+A to select every point in the lane, Cmd+C / Cmd+V
  to copy/paste (paste anchors at the current playhead, with relative
  offsets preserved across the copied set), and Delete to remove every
  selected point — each as one undo step. Dragging any selected point
  moves the whole selection by the same delta. Cross-lane copy/paste
  is supported (clipboard is process-global). Selected points render
  in orange. Selection auto-clears on lane lifecycle changes (add /
  remove / sidecar reload) but persists across point edits and
  recorder appends. 6 new unit tests.

- **Automation point edits are now undoable.** Click-to-add,
  drag-to-move, right-click delete, curve-type change, and "Clear All
  Points" in the Automation Lanes panel each become a single
  transaction on the document's UndoManager — `Cmd+Z` / `Cmd+Shift+Z`
  revert and replay the gesture. A whole drag is one undo step
  (snapshot at mouseDown, commit at mouseUp). Removing a lane while
  its point-edit history is still on the stack is safe — the undo
  entries inert-no-op rather than crash. 7 new unit tests for the
  skipFirstPerform pattern, per-gesture round-trip, and lane-removed
  safety.

- **Plugin Parameter Automation Phase 5 — visual lane editor**: a new
  Automation Lanes panel (`Plugins → Show Automation Lanes`,
  `Cmd+Alt+L`) lists every recorded lane for the current file with the
  parameter name, plugin name, point count, Enabled / Record toggles,
  and a Remove button. Each row's curve view spans the document
  timeline: click empty space to add a point, drag to move, right-click
  for Delete + Curve-to-Next-Point (Linear / Step / S-Curve /
  Exponential). A live playhead tracks transport during playback.

- **Plugin Parameter Automation Phase 4 — UI surface**: every
  PluginEditorWindow toolbar now has an "Automation" button. The popup
  menu lists every plugin parameter with a checkmark next to armed
  ones; clicking toggles arm and creates the lane on demand. Arming
  the first parameter also turns the global record-arm flag on, so
  first-time use "just works." A top-level menu item opens the lanes
  panel directly. Native-plugin right-click on individual knobs
  is still deferred — most third-party UIs swallow right-click before
  WaveEdit can see it; the toolbar button is the path that works for
  every editor.

- **`AutomationCurve::setPointCurve(int, CurveType)`**: change a single
  point's interpolation curve without touching its time/value, kept on
  the same publish path as moves.

- **AutomationLanesPanel test suite** (7 groups) covering the new
  setPointCurve API, panel listener wiring (rows track lane count
  through add / remove / clear), and lifetime safety
  (manager outliving panel destruction).

- **Plugin Parameter Automation Phase 4**: record plugin parameter
  changes during playback. New `Plugins → Automation → Arm Automation
  Recording` toggle. Once armed, lanes whose `isRecording` flag is set
  capture user-driven knob movements into the AutomationCurve while
  the transport is playing; the existing Phase 3 playback path then
  reproduces them on the next pass. Off-thread parameter callbacks
  are bounced onto the message thread via AsyncUpdater, and rapid
  knob drags are coalesced by a per-lane debounce (default 20 ms,
  value-change threshold). Audio thread is untouched — JUCE's plain
  `setValue()` doesn't notify listeners, so playback can't feed back
  into recording.

- **Plugin Parameter Automation Phase 6**: per-document persistence
  of automation lanes via `<file>.<ext>.automation.json` sidecar
  next to the audio file (mirrors the existing region/marker sidecar
  pattern). Recorded automation now survives between sessions.
  Document::saveToFile / openFile call the round-trip; empty managers
  skip writing so we don't litter the disk for un-automated files.

### Changed
- **Plugin chain preset format now bundles automation lanes.** The
  `.wepchain` JSON has gained an optional top-level `"automation"`
  block carrying the document's AutomationManager state. Empty
  managers omit the block (small presets stay small). New
  `PluginPresetManager` overloads accept an `AutomationManager`
  alongside the chain; existing chain-only callers (BatchJob and
  any external file readers) keep working unchanged and cleanly
  ignore the automation block when reading newer files. Loading a
  preset through the automation-aware API replaces both the chain
  and the manager. 7 new unit tests cover round-trip, empty-skip,
  bidirectional backwards compatibility, and error paths.

- **UndoActions split for §7.5 compliance**: `AudioUndoActions.h`
  (1,028 lines) and `RegionUndoActions.h` (964 lines) — both over
  the CLAUDE.md §7.5 500-line cap — are now thin umbrella headers
  that re-export from sub-domain files. New: `LevelUndoActions.h`
  (Gain/Normalize/DCOffset), `RangeUndoActions.h` (Silence/Trim),
  `TransformUndoActions.h` (Reverse/Invert/Resample/HeadTail),
  `RegionLifecycleUndoActions.h` (Add/Delete/Rename/Color/
  BatchRename), `RegionEditUndoActions.h` (Resize/Nudge/Merge/
  MultiMerge/Split), `RegionDerivationUndoActions.h` (StripSilence/
  Retrospective/MarkersToRegions). `ChannelUndoActions.h` absorbed
  the channel-shape actions (`ConvertToStereoAction`,
  `SilenceChannelsUndoAction`, `ReplaceChannelsAction`) that were
  always misfiled under Audio. All callers using
  `#include "AudioUndoActions.h"` or `#include "RegionUndoActions.h"`
  keep working unchanged. Largest UndoActions file is now 381 lines.
  No behavior change.

- **Test suite grew from 308 → 364 groups** across the entire
  Unreleased cycle (~207,000 assertions, all passing). New test files:
  `AutomationRecorderTests`, `AutomationLanesPanelTests`,
  `PluginPresetManagerTests`, `CrashRecoveryTests`, `TimePitchEngineTests`.

### Fixed
- **Marker rename in the Marker List Panel never persisted.** The
  panel called the listener with the new name but the listener
  override dropped the argument, so `Marker::setName` was never
  invoked. Fixed by routing the new name through
  `MarkerController::handleMarkerListMarkerRenamed` which now also
  wraps the rename in an UndoableAction.

---

## [0.1.0] - 2026-04-29

First tagged release. Pre-built binaries for macOS, Windows, and
Linux are published to the
[Releases page](https://github.com/themightyzq/WaveEdit/releases/tag/v0.1.0)
by the CI release workflow. Binaries are unsigned; the README
documents the first-run workaround for Gatekeeper / SmartScreen.

### Added
- **Tagged release publishing**: pushing a `v*` tag now triggers a
  GitHub Actions job that downloads the macOS / Windows / Linux
  build artifacts and publishes them as a Release with first-run
  instructions in the body.
- **Application log file**: `juce::FileLogger` writes
  `juce::Logger::writeToLog()` traffic to
  `~/Library/Logs/WaveEdit/WaveEdit.log` (macOS),
  `%APPDATA%\WaveEdit\WaveEdit.log` (Windows), and
  `~/.config/WaveEdit/WaveEdit.log` (Linux). Path documented in
  README §Configuration.
- **Crash backtrace capture**: on a fatal signal,
  `juce::SystemStats::setApplicationCrashHandler` writes a single
  timestamped record (time, version, OS, CPU, full stack
  backtrace) to `~/Library/Logs/WaveEdit/crashes/`. Complements the
  OS crash dialog rather than replacing it.
- **CLI flags**: `--help` and `--version` (and short forms `-h` /
  `-v`) print and exit before any GUI initialization.
- **Status bar**: "No file loaded — Press Cmd+O…" hint now uses
  `Cmd` on macOS and `Ctrl` elsewhere.
- **README ⇆ keymap drift CI test**:
  `.github/scripts/check_readme_keymap.py` runs on every push and
  fails the build if `README.md`'s shortcut table disagrees with
  `Templates/Keymaps/Default.json`.
- **README §Screenshots**: main window with selection, Region List
  panel, 3-Band Parametric EQ, Spectrum Analyzer, Batch Processor.

### Changed
- **macOS user state consolidated under one folder.** Pre-2026-04-29
  builds wrote `settings.json` and the plugin scan cache into
  `~/Library/WaveEdit/` while keymaps / toolbars / batch presets
  lived under `~/Library/Application Support/WaveEdit/`. New builds
  use the canonical Application Support path for everything.
  `Settings::migrateLegacySettingsIfNeeded()` runs once on first
  launch under the new build, copies anything from the legacy
  directory into the new one, and leaves the legacy folder in
  place as a backup. The migration is logged so users can confirm
  before deleting the old directory by hand.
- **Default keymap**: `editSilence` rebound from plain `S` to
  `Shift+Alt+S` (no longer destructive without a modifier).
  `fileBatchProcessor` assigned `Cmd+B` (was colliding with
  `fileEditBWFMetadata` on `Cmd+Alt+B`).
- **README §Keyboard Shortcuts** rebuilt from the keymap JSON. Nine
  documented shortcuts were silently wrong (e.g., `Cmd+G` was
  documented as Go-To-Position but actually runs Normalize); they
  now match the actual bindings, and the new CI doc-test prevents
  the table from drifting out of sync again.
- **README §Configuration** rewritten to describe the actual
  on-disk layout per platform, document the macOS migration, list
  the application log + crash-report paths, and include a complete
  uninstall block.
- **README §Installation** points at GitHub Releases instead of
  Actions artifacts; documents Gatekeeper / SmartScreen first-run
  workaround for the unsigned binaries.
- **NOTICE** updated with full attribution for bundled LAME
  (LGPL-2.1), libFLAC, libvorbis, and the Steinberg VST3 SDK.
- **`build-and-run.command`** now skips the interactive launch
  prompt when stdin isn't a TTY, when `CI` is set, or when
  `WAVEEDIT_NONINTERACTIVE=1`.
- **Source comments and identifiers**: `WaveEdit Classic` keymap
  references removed from `KeymapManager.h` (only Default / Sound
  Forge / Pro Tools ship). Settings.cpp / Settings.h doc comments
  match the runtime path.

### Fixed
- **README install instruction** `cd waveedit` (lowercase) →
  `cd WaveEdit` so it works on case-sensitive filesystems.
- **`unused variable 'wasPlaying'` warning** in
  `Utils/UndoActions/AudioUndoActions.h` (debug-only locals are now
  `[[maybe_unused]]`).

### Changed (architecture round 3)
- **Main.cpp now satisfies CLAUDE.md §8.1 size rule.** Extracted
  `MainComponent`, `SelectionInfoPanel`, and `CallbackDocumentWindow`
  into `Source/MainComponent.h`. Main.cpp shrank from **2,851 → 281
  lines** and now contains only `WaveEditApplication` and `main()`.
- **`perform()` moved into `CommandHandler.cpp` per §8.1.** The 648-line
  switch statement that the audit flagged as belonging in
  `Commands/CommandHandler.cpp` is now there as
  `CommandHandler::performCommand(MainComponent& mc, const InvocationInfo&)`.
  `MainComponent::perform()` is a one-line forward.
  `CommandHandler` is declared a `friend` of `MainComponent` so it can
  reach the controller members it dispatches to.
- **MainComponent.h (1,959 lines) exceeds the §7.5 500-line .h cap and
  CommandHandler.cpp (1,806 lines) exceeds the §7.5 1,500-line .cpp
  cap.** These are honest tradeoffs: closing them would require
  splitting `MainComponent` into multiple smaller classes (a much
  larger refactor) and per-class extraction of the controllers each
  command case touches. Tracked under TODO.md.

### Fixed
- **Audio engine: paused buffer reload no longer snaps cursor to 0.**
  `AudioEngine::reloadBufferPreservingPlayback()` previously captured
  and restored playback position only when `isPlaying()` was true. An
  edit while paused therefore lost the cursor. Fix matches the
  CLAUDE.md §6.5 protocol: capture position unconditionally, restore
  it unconditionally (clamped to the new buffer length), only call
  `start()` when playback was actually running.
- **Three remaining test failures resolved.** With the audio-engine
  position fix, `Edit during pause - resume` (×2) and `Fade during
  playback` (×1, plus a stale single-sample expectation that was
  swapped for a half-vs-half magnitude check) all pass.
  Local non-CI run: 308 groups / 184,702 assertions / 0 failures.
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

### Changed (architecture round 2)
- **Four new domain controllers** under `Source/Controllers/`:
  - `PlaybackController` — `togglePlayback` / `stopPlayback` /
    `pausePlayback` / `toggleLoop` / `loopRegion`. Owns the
    selection-vs-cursor-vs-start playback decision tree and the
    always-clear-stale-loop-points invariant.
  - `RecordingController` — `handleRecordCommand` plus the previously
    inline `RecordingDialog::Listener` (~100 lines of buffer-splice
    logic for punch-in inserts and new-document creation).
  - `DialogController` — application-level dialog launchers
    (`showAboutDialog`, `showKeyboardShortcutsDialog`,
    `showBatchProcessorDialog`).
  - `PluginController` — `showPluginManagerDialog`,
    `showPluginChainPanel`, owns the OwnedArray of
    `ChainWindowListener` and runs the document-closed teardown.
- **`perform()` now satisfies CLAUDE.md §6.7** — every case is at most
  5 lines (was 21 violations, worst was `pluginClearCache` at 37
  lines). Bodies that did real work moved to MainComponent helpers
  (`handleUndo`, `handleRedo`, `showGoToPositionDialog`,
  `toggleRegionVisibility`, etc.) or to controllers.
- **Main.cpp: 3,318 → 2,851 lines.** Still over the §8.1 <2,000
  target (the 648-line `perform()` switch is the remaining bulk;
  its compliance with the ≤5-line rule means each case is already a
  one-line delegation — moving the dispatch to `CommandHandler.cpp`
  per §8.1 would require extracting `MainComponent` into its own
  header file, deferred as a follow-up).
- **`FileController::createNewFile()`** — handleNewFileCommand body
  (35 inline lines in Main.cpp) moved verbatim to FileController.

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
- **Split FadeIn/FadeOut into `FadeUndoActions.h`**.
  AudioUndoActions.h: 1,238 → 1,027 lines (still over the cap;
  further per-domain splits remain in TODO).

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
- `MainComponent.h` (1,959 lines) exceeds the §7.5 500-line .h cap and
  `Commands/CommandHandler.cpp` (1,806 lines) exceeds the 1,500-line
  .cpp cap. Both come from concentrating the application's command and
  UI surface in one place; further splitting requires breaking
  `MainComponent` into smaller classes (separate Spectrum/Toolbar/
  RegionPanel-window controllers) and grouping the command dispatch
  per domain.
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
