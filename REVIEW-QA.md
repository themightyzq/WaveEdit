# REVIEW-QA.md — Adversarial QA Sweep (Pass 1)

**Reviewer:** Mira Kovač (staff QA, adversarial discovery)
**Date:** 2026-05-29
**Scope:** Full source attack surface (247 files, ~97k LOC). Contract = CLAUDE.md.
**Method:** 8 parallel adversarial hunters over independent slices (audio/DSP, file I/O & persistence, regions & markers, clipboard & multi-doc, commands & keymap, UI dialog validation, plugins & batch, concurrency & resources). Highest-impact Criticals were re-verified by hand at line level before listing.

> **Status legend:**
> `CONFIRMED✓` = I read the code paths myself and the defect holds.
> `REPORTED` = hunter supplied concrete file:line evidence; reasoning is sound but I did not personally re-verify every step.
> `SUSPECTED` = no concrete repro, or the hunter's reasoning was internally shaky — needs confirmation before fixing.

**NOTHING in this file has been fixed.** This is discovery only. Awaiting triage decision.

---

## Tally

| Severity | Count |
|---|---|
| Critical | 18 |
| High | 27 |
| Medium | 21 |
| Low | 13 |

Recurring root-cause themes (these are systemic, not isolated):
- **Sample-vs-seconds and int64-vs-int unit confusion** at controller→engine boundaries (C1, C2, H2, H17, plus resampler).
- **Audio-thread reads non-atomic shared state** written by the message thread — EQ params, automation lanes, raw analyzer/editor pointers, buffer swaps (C5–C9).
- **Undo actions store raw indices / raw buffer references** that go stale after a later add/delete/reload (H8–H13, C13).
- **Non-atomic file writes** (audio, region sidecar, iXML) — crash mid-write = data loss (C10, C11, H21).
- **Channel-count / sample-rate mismatch on paste** silently drops or corrupts data (C12, H5).
- **`callAsync([this]...)` capturing objects that can be destroyed before the callback fires** — UAF across doc close (C7, C16, H19, H24).
- **Leftover `std::cerr` debug spew** ships in release on the paste hot path (M-tier, repeated).

---

# CRITICAL

### C1. Trim to Selection keeps the WRONG range — every trim is corrupt `CONFIRMED✓`
- **What breaks:** "Trim to Selection" retains audio past the selection end; on some buffer lengths it silently does nothing.
- **Repro:** 100-sample file, select [20,80). Trim. Result = samples [20,100) (80 samples kept) instead of [20,80) (60 samples).
- **Location:** `Source/Controllers/DSPController.cpp:1226-1233` passes `endSample` into `TrimUndoAction`'s `numSamples` parameter; `TrimUndoAction::perform` calls `trimToRange(startSample, numSamples)` (`RangeUndoActions.h:143`), which keeps `numSamples` starting at `startSample` (`AudioBufferManager.cpp:453`).
- **Why:** Absolute end index supplied where a *count* is expected. Also, validation `startSample + numSamples > getNumSamples()` then frequently fails → trim silently returns false (no-op) for selections in the back half of the file.
- **Severity:** Critical — data loss on a core edit, every time.

### C2. Marker jump uses wrong units twice — playhead flies to ~12 hours `CONFIRMED✓`
- **What breaks:** Jump-to-next/previous-marker seeks to a nonsensical position; on files shorter than the bogus time it silently clamps to end / does nothing. Every marker jump is broken.
- **Repro:** Add a marker anywhere. Press the jump shortcut.
- **Location:** `Source/Controllers/MarkerController.cpp:136,153,170,187` (and 220, 603). `getCurrentPosition()` returns `double` *seconds* (`AudioEngine.h:201`), truncated into `int64_t currentSample` and passed to `getNextMarkerIndex()` which compares against marker positions stored in *samples* (`Marker.h:45`). Then `setPosition(marker->getPosition())` passes a *sample count* into `setPosition(double seconds)` (`AudioEngine.h:208`).
- **Why:** Two independent seconds⇄samples confusions on the same path.
- **Severity:** Critical — core navigation feature non-functional.

### C3. Region paste is not undoable — pasted regions are unrecoverable `REPORTED`
- **What breaks:** Paste regions from clipboard, then Cmd+Z — nothing happens. Paste then Save = permanent.
- **Location:** `Source/Controllers/RegionController.cpp:569-617` calls `regionManager.addRegion(newRegion)` directly, never `getUndoManager().perform(...)`.
- **Severity:** Critical — silent un-undoable state change (also violates §10.4).

### C4. `importMarkers` and marker drag/inline-rename bypass UndoManager `REPORTED`
- **What breaks:** Bulk marker import (50 markers) cannot be undone; dragging a marker or renaming/recoloring it in the waveform view cannot be undone.
- **Location:** `MarkerController.cpp:772-779` (import), `:319-355` (drag move — comment literally says "no undo for move in Phase 1"), `:236-258` (inline rename), `:260-282` (color change). The `MarkerListPanel` paths *do* go through undo; the waveform-view paths do not — inconsistent and lossy.
- **Severity:** Critical — destructive, irreversible on a common interaction.

### C5. EQ parameter struct: audio thread reads a non-atomic struct mid-write `CONFIRMED✓ (found by 2 hunters)`
- **What breaks:** Crash / heap corruption / NaN into filters when the EQ dialog is open during playback and the user drags a band or adds/removes one.
- **Repro:** Open Graphical EQ, start playback, drag a band rapidly (or add a band).
- **Location:** message-thread write `AudioEngine_Preview.cpp:176-185`; audio-thread read `AudioEngine.cpp:1038-1056`. `m_pendingParametricEQParams` / `m_pendingDynamicEQParams` are plain structs containing `std::vector<BandParameters>` (`AudioEngine.h:949`). The `juce::Atomic<bool> …Changed` flag synchronizes only the flag, not the vector payload — torn read with heap reallocation.
- **Severity:** Critical — data race on a container, realistic workflow.

### C6. `DynamicParametricEQ::applyEQ` iterates `m_bandStates` (audio thread) without the lock that `setParameters/addBand/removeBand` hold `REPORTED`
- **What breaks:** Vector reallocation on band add invalidates iterators/pointers the audio thread is mid-loop on → crash or corruption.
- **Location:** read `DynamicParametricEQ.cpp:107-145` (no lock); write under `m_parameterLock` at `:58-89`.
- **Severity:** Critical — same class as C5, distinct object.

### C7. `AutomationManager::m_lanes` vector mutated (message thread) while iterated (audio thread) `REPORTED`
- **What breaks:** UAF / iterator invalidation when a lane is added/removed/cleared during playback.
- **Location:** read in `applyAutomation()` `AutomationManager.cpp:131-153`; writes at `:60,69,103,109,119`. The header's "lock-free" claim only covers `AutomationCurve` point lists (COW); the `m_lanes` vector itself and the `lane.enabled/pluginIndex/...` fields have **no** synchronization.
- **Severity:** Critical.

### C8. Raw `m_spectrumAnalyzer` / `m_graphicalEQEditor` pointers dereferenced on audio thread; component can be deleted mid-callback `CONFIRMED✓ (found by 2 hunters)`
- **What breaks:** Close the Spectrum Analyzer or Graphical EQ while audio plays → UAF in `audioDeviceIOCallbackWithContext`.
- **Location:** setters (message thread, non-atomic) `AudioEngine.cpp:812-820`; reads (audio thread) `:1145-1160`. TOCTOU between null-check and dereference; the component is destroyed before the engine pointer is cleared.
- **Severity:** Critical — audio thread runs at interrupt priority, cannot be preempted safely.

### C9. `RecordingEngine::audioDeviceStopped()` calls `sendChangeMessage()` from the device-teardown thread `REPORTED`
- **What breaks:** `ChangeBroadcaster` internal state races with message-thread `sendChangeMessage()`; possible corruption/deadlock on device disconnect or system sleep while recording. Also the 1-hour buffer-full path (`appendToRecordingBuffer:305`) stores IDLE but never notifies the UI, so the dialog never learns recording stopped.
- **Location:** `RecordingEngine.cpp:197-203` → `:94`; buffer-full at `:305`.
- **Severity:** Critical.

### C10. Main WAV save is not atomic — crash/disk-full mid-write destroys the original `REPORTED`
- **What breaks:** `saveAsWav` writes directly onto the target path, then the iXML pass deletes+rewrites the whole file again. Crash/power-loss/disk-full at any point leaves a truncated or zero-byte file with the original already gone.
- **Location:** `AudioFileManager.cpp:228` (`createOutputStream` on target), `Document.cpp:315-324` (second full rewrite). No temp-file + atomic rename — yet `ThumbnailDiskCache.cpp:72` does it correctly, so the pattern exists in-repo.
- **Severity:** Critical — data loss.

### C11. iXML save destroys the BWF `bext` chunk on every round-trip `REPORTED`
- **What breaks:** Open a WAV with BWF metadata, edit iXML, save → BWF `bext` fields gone when reopened in any BWF tool.
- **Location:** `Document.cpp:303-308` sets `metadata["iXML"]` (a key JUCE's WAV writer ignores — confirmed dead), then `:324` calls `appendiXMLChunk`, which strips every chunk except `fmt`/`data` (`AudioFileManager.cpp:545-554`) — including the `bext` JUCE just wrote.
- **Note:** I verified the double-write call structure in `Document.cpp` firsthand; the strip behavior is per the hunter's read of `appendiXMLChunk`.
- **Severity:** Critical — silent metadata corruption.

### C12. Channel-mismatch paste silently drops audio AND leaves a phantom undo entry `REPORTED (found by 2 hunters)`
- **What breaks:** Copy stereo, paste into a mono (or 4/8-ch) file → `insertAudio` returns false (channel-count guard), paste silently fails, no ErrorDialog. `beginNewTransaction("Paste")` already ran, so the undo stack holds a no-op "Paste" entry that can later mis-fire.
- **Location:** `ClipboardController.cpp:341-350`, `:311`; `insertAudio` guard `AudioBufferManager.cpp:225-228`; `InsertAction::perform` ignores the false return (`UndoableEdits.h:339-349`). No channel normalization before constructing the action.
- **Severity:** Critical — silent data loss + undo-history corruption.

### C13. `StripSilenceUndoAction` stores `const AudioBuffer&` → dangling after file reload `REPORTED`
- **What breaks:** The action holds a reference to the document's live buffer; load a different file (buffer replaced), then undo → reads freed memory.
- **Location:** `Source/Utils/UndoActions/RegionDerivationUndoActions.h:57,127`. (Note: the dialog currently wires `RetrospectiveStripSilenceUndoAction`, which is safe — but `StripSilenceUndoAction` exists and is a live landmine.)
- **Severity:** Critical — UAF if exercised.

### C14. `m_previousDocument` raw pointer dangles after the previous document is closed `REPORTED`
- **What breaks:** Close the document held in `m_previousDocument` without switching away first; next document switch calls `m_previousDocument->getAudioEngine().stop()` on freed memory.
- **Location:** `MainComponent.h:609-611,624-626,637`; `documentRemoved()` at `:670` never nulls `m_previousDocument`.
- **Severity:** Critical — UAF.

### C15. Resampling on Save-As re-reads input from sample 0 every chunk → corrupt audio `CONFIRMED✓`
- **What breaks:** Save-As with any sample-rate change, on files longer than 4096 output samples, produces garbage after the first chunk.
- **Location:** `AudioFileManager.cpp:885-925`. `sourceData` (`:891`) is set once and never advanced inside the `while` loop; additionally `samplesProcessed += samplesGenerated` conflates `LagrangeInterpolator::process`'s input-consumed return value with the output write offset (`targetData + samplesProcessed`, loop bound `targetNumSamples`).
- **Severity:** Critical — silent audio corruption on a standard export path.

### C16. Plugin/batch `callAsync([this]…)` UAF across document close `REPORTED`
- **What breaks:** `PluginChain::notifyChainChanged()` posts `[this]{ sendChangeMessage(); }`; close the document before it fires → write to freed `PluginChain`. Same pattern: `ChainWindowListener` raw `PluginChain*`/`AudioEngine*` (`PluginController.cpp:56-88`), `BatchProcessorEngine` notifications capturing `this` with queued messages outliving the dialog (`BatchProcessorEngine.cpp:222-244`).
- **Location:** `PluginChain.cpp:604-608`; `ChainWindowListener.h:52-54`; `BatchProcessorEngine.cpp:222-244`.
- **Severity:** Critical — UAF on realistic close-during-async flows.

### C17. Batch region export truncates `int64`→`int` → negative buffer size / crash on long regions `REPORTED`
- **What breaks:** A region longer than `INT_MAX` samples (or, more practically, JUCE's `AudioBuffer` int limit reached on long-form files) wraps negative → JUCE assert / crash, or silent failure.
- **Location:** `RegionExporter.cpp:190` `static_cast<int>(endSample - startSample)`, used at `:206,211`. Same class: `AudioBufferManager.cpp:56` narrows `lengthInSamples` to int with no guard.
- **Severity:** Critical (data loss / crash on large files). *Practical trigger threshold is large; included as Critical for the corruption class, downgrade to High if large-file support is out of scope.*

### C18. `PluginManager` singleton (`DeletedAtShutdown`) is `new`'d and never deleted → scan cache lost, threads not joined `REPORTED`
- **What breaks:** Destructor (`stopThread`, `saveCache`, `saveIncrementalCache`) never runs on clean exit. Run a scan, quit → cache missing next launch; scan threads not joined; pending `callAsync` fires into destroyed UI.
- **Location:** `PluginManager.cpp:272-276` (`static PluginManager* instance = new PluginManager();` with no matching delete, defeating the `DeletedAtShutdown` contract).
- **Severity:** Critical (resource/lifecycle; data-loss-adjacent for the scan cache).

---

# HIGH

### H1. `replaceRange` is non-atomic: delete succeeds, insert fails → buffer left short with a gap `REPORTED (2 hunters)`
- **What breaks:** Cross-document paste via `replaceRange` into a channel-mismatched doc deletes the target range, then `insertAudio` fails → buffer permanently missing samples, no rollback.
- **Location:** `AudioBufferManager.cpp:270-299` — no lock across the two sub-ops; channel check fires only after delete commits.
- **Severity:** High — silent data loss.

### H2. `loopRegion()` feeds raw samples to seconds-based `setSelection()` `CONFIRMED✓`
- **What breaks:** Loop Region highlights a selection spanning the whole file (or clamped to end) — visually wrong every time. Engine loop points are correct; only the selection display is broken.
- **Location:** `PlaybackController.cpp:109-110` passes `region->getStartSample()/getEndSample()` into `setSelection(double seconds,…)` (`WaveformDisplay.h:222`), while lines 106-107 correctly divide by sample rate for the loop points.
- **Severity:** High — misleading core feedback.

### H3. `ParametricEQ` hard-prepared for 2 channels — channels 2-7 pass through unprocessed `REPORTED`
- **Location:** `ParametricEQ.cpp:68` `spec.numChannels = 2;`, `:74` `m_lastNumChannels = 2;` — the re-prepare condition can never fire for >2ch. Apply EQ to a 4-ch file → only ch 0,1 affected, silent phase/level mismatch.
- **Severity:** High — silent wrong result on multichannel.

### H4. `DynamicParametricEQ::applyEQ` structurally capped at 2 channels `REPORTED`
- **Location:** `DynamicParametricEQ.cpp:130` `jmin(numChannels, 2)`; `BandState` holds `std::array<IIRFilter,2>`. Multichannel EQ silently bypasses ch 2-7.
- **Severity:** High.

### H5. Per-channel paste with mismatched channel counts silently drops/partial-writes `REPORTED`
- **What breaks:** Clipboard has more channels than focused channels → extras silently ignored; fewer → some focused channels left unmodified. No warning.
- **Location:** `ClipboardController.cpp:265-306`; `ChannelUndoActions.h:309-330`. No reconciliation between `pasteAudio.getNumChannels()` and popcount(`focusedChannels`).
- **Severity:** High — silent wrong data.

### H6. FadeProcessor divides by zero when fade duration is 0 → NaN flood in output `REPORTED`
- **What breaks:** `setFadePreview(true, 0, 0.0f, true)` (0 ms) → first audio callback computes `0.0f/0.0f = NaN`, every output sample × NaN.
- **Location:** `AudioEngine.h:829` (guard at `:826` protects only the modulo, not the division).
- **Severity:** High (Critical if it reaches hardware/recording).

### H7. Loading a file runs an O(N) NaN/Inf scan on the message thread — multi-second UI freeze `REPORTED`
- **Location:** `AudioEngine.cpp:376-387`. 1-hr 96k stereo = ~690M floats scanned before connecting the buffer. Violates §9 (<2 s load, no blocking on message thread). Asymmetric: absent from `reloadBufferPreservingPlayback`.
- **Severity:** High.

### H8–H13. Undo actions store stale integer indices / position-based removal `REPORTED`
A whole family: an index captured at action-creation time points at a *different* item after any later add/delete shifts the list, so undo mutates the wrong region/marker.
- **H8** Region rename/color/resize/nudge/batch-rename: `RegionLifecycleUndoActions.h:176,245,320`, `RegionEditUndoActions.h:34,119`.
- **H9** Marker rename/color: `MarkerUndoActions.h:195,256`.
- **H10** `MultiMergeRegionsUndoAction::undo` re-inserts at original indices without compensating for prior insertions → wrong order for non-contiguous selections: `RegionEditUndoActions.h:287-296`.
- **H11** Regions↔markers conversion undo removes "last N" entries — wrong items if more were added before undo: `RegionDerivationUndoActions.h:248`, `MarkerUndoActions.h:168`.
- **Repro (representative):** 3 regions; rename middle to "TEST"; add a region before it; undo rename → wrong region renamed.
- **Severity:** High — undo corrupts data under ordinary edit sequences.

### H14. Region auto-numbering reuses numbers after delete → duplicate names → batch-export overwrite `REPORTED`
- **What breaks:** Create 001/002/003, delete 001, create again → second "003"; batch export with name-in-filename silently overwrites.
- **Location:** `RegionController.cpp:80-81` (`getNumRegions()+1` as the name). Same root affects undo/redo of Add Region.
- **Severity:** High — silent export data loss.

### H15. Batch export: no filename-collision dedup `REPORTED`
- **What breaks:** "Take 1" and "Take/1" both sanitize to "Take_1"; second WAV silently overwrites the first; success count says 2, only 1 file exists.
- **Location:** `RegionExporter.cpp:98` (sanitize only, no dedup suffix, no existence check).
- **Severity:** High.

### H16. Batch export accepts Windows-reserved names (CON, PRN, NUL, COM1…) `REPORTED`
- **Location:** `RegionExporter.cpp:88-163` — no reserved-name filter; `createOutputStream` returns null on Windows → that region silently fails to export.
- **Severity:** High (Windows). Note: per §3 supported platforms include Windows.

### H17. `GoToPosition` NaN/Inf input → UB via `(int64_t)NaN` cast `REPORTED`
- **What breaks:** No `setInputRestrictions`; typing/pasting "NaN"/"1e999" → NaN passes `< 0` guards (all NaN comparisons false) → `(int64_t)NaN` is UB; infinity → `INT64_MIN` delivered as a position.
- **Location:** `GoToPositionDialog.cpp:299-313`.
- **Severity:** High (UB; potential crash/corrupt playhead).

### H18. `GainDialog` / `Normalize` accept NaN gain (paste-bypass) → NaN written to buffer `REPORTED`
- **What breaks:** `std::stof("NaN")` returns NaN, passes `[-100,100]` range check (NaN comparisons false); applied to buffer. On macOS, paste can bypass `setInputRestrictions`. Normalize RMS on a zero-length selection yields `+inf` gain pushed to the audio thread (`NormalizeDialog.cpp:398-405`, `m_currentRMSDB` inits to `-inf`).
- **Location:** `GainDialog.cpp:264-278`; `NormalizeDialog.cpp:293,398-405`, `NormalizeDialog.h:184`.
- **Severity:** High (NormalizeDialog +inf path is arguably Critical — corrupts the preview stream).

### H19. `RecordingApplyListener` captures raw `Document*` that can close mid-recording `REPORTED`
- **Repro:** Open file A, "insert at cursor", start recording, close file A, stop recording → UAF at `RecordingController.cpp:56`.
- **Location:** `RecordingController.cpp:32-44,164`.
- **Severity:** High — UAF, realistic flow.

### H20. `MemoryAudioSource::getNextAudioBlock` holds a blocking `CriticalSection` the message thread also takes for a deep buffer copy `REPORTED`
- **What breaks:** Editing a large file calls `setBuffer()` (deep `makeCopyOf`, tens of ms) under `m_lock`; the audio callback blocks on the same lock → audible dropout. §6.4 forbids blocking locks on the audio thread.
- **Location:** `AudioEngine.cpp:38-65` (setBuffer) vs `:84-157` (callback).
- **Severity:** High — audio glitch on every large-file edit.

### H21. Region sidecar JSON not written atomically `REPORTED`
- **What breaks:** `replaceWithText` truncates then writes; crash mid-write → empty `.regions.json` → all regions silently vanish on next open.
- **Location:** `RegionManager.cpp:550` (and holds `m_lock` across the disk write, `:528` — stalls all region access on slow drives).
- **Severity:** High.

### H22. Region/marker JSON deserialization has no validation `REPORTED`
- **What breaks:** Missing/wrong-type `startSample`/`endSample` → silent `[0,0]` region; `start>end` not unswapped (bypasses ctor swap) → negative-length region, breaks `getInverseRanges`; out-of-buffer offsets from a version-mismatch sidecar accepted and shown.
- **Location:** `Region.cpp:123-138` (`fromJSON`); `RegionManager.cpp:582-591` (no bounds check on load).
- **Severity:** High — malformed/old sidecar corrupts region state.

### H23. `AutomationRecorder` parameter dispatch re-enters on the audio thread (lock + async post) `REPORTED`
- **What breaks:** `applyAutomation()` calls `param->setValue()` (`AutomationManager.cpp:151`), which synchronously fires `parameterValueChanged()` → acquires `m_pendingLock` (CriticalSection) and `triggerAsyncUpdate()` on the audio thread (`AutomationRecorder.cpp:241-257`). §6.4 violation; priority inversion / deadlock risk.
- **Severity:** High.

### H24. `BatchJob::applyPluginChain` busy-waits up to 30s on a `callAsync` that the modal batch dialog can block `REPORTED`
- **What breaks:** Worker thread spins on `chainCreated.load()` (`Thread::sleep(10)`, no `threadShouldExit()` check) waiting for a message-thread `callAsync`; if the message thread is blocked in a modal loop → 30 s stall then silent skip of the plugin chain.
- **Location:** `BatchJob.cpp:320-337`.
- **Severity:** High.

### H25. Batch "include tail" silently discards the tail `REPORTED`
- **What breaks:** `result.processedBuffer` has `numSamples+tail`, but `m_buffer` is never resized and the copy clamps to `m_buffer.getNumSamples()` → reverb/delay tail dropped despite the user requesting it.
- **Location:** `BatchJob.cpp:378-385`.
- **Severity:** High — silent wrong output.

### H26. File drop accepts only `.wav`; .flac/.mp3/.ogg silently ignored `REPORTED`
- **What breaks:** Drag a FLAC onto the window → nothing happens, no error — yet the Open dialog advertises all four formats.
- **Location:** `FileController.cpp:459`.
- **Severity:** High — broken core affordance, inconsistent with file chooser.

### H27. Keymap: 15 wired commands can never get a shortcut + a dangling keymap name `REPORTED`
- **What breaks:** 15 registered commands (`toolsChannelConverter/Extractor`, `toolsHeadTail`, `toolsLoopingTools`, `processReverse/Invert/Resample/TimeStretch/PitchShift`, `markerConvertToRegions`, `regionConvertToMarkers`, `pluginOffline`, `toolbarCustomize/Reset`, `fileBatchProcessor`) are absent from `KeymapManager`'s name maps → `getKeyPress` returns empty → no `addDefaultKeypress`. Cmd+L (Looping Tools) etc. never bind. Separately, `Default.json:356` references `processChannelConverter`, a command ID that no longer exists (renamed to `toolsChannelConverter`) → shortcut silently dropped. Violates §6.7 ("every command must have a default keypress").
- **Location:** `KeymapManager.cpp:599-735,754-833`; `Templates/Keymaps/Default.json:356`.
- **Severity:** High — keyboard-first design broken for the Tools/Process families.

---

# MEDIUM

### M1. `replaceRange` ships `std::cerr` debug spew on the paste hot path `REPORTED (2 hunters)`
- 8 `std::cerr` lines fire on every replace-range/paste in **release** builds (`DBG()` is stripped, `cerr` is not). `AudioBufferManager.cpp:273-298`. Also present in `DynamicParametricEQ.cpp:250-303,538-580`. Violates §6.10. *(Quick win; quarantine candidate.)*

### M2. `beginNewTransaction` called before validation → failed actions linger in undo history `REPORTED`
- `ClipboardController.cpp:240,311` open a transaction before channel/position checks; a failed `perform()` leaves a phantom action that can mis-fire on later undo. Pattern repeats wherever transactions precede validation.

### M3. `EditRegionBoundariesDialog` uses stale `m_totalSamples` after a destructive edit shrinks the buffer `REPORTED`
- Dialog open → user cuts audio (buffer shrinks) → OK applies boundaries beyond the live buffer; `ResizeRegionUndoAction` doesn't revalidate. `EditRegionBoundariesDialog.cpp:44`, `RegionController.cpp:1092-1128`.

### M4. AutoSave filename collision between same-named files in different dirs `REPORTED`
- `autosave_<stem>_<ts>.wav` keyed on stem only → `/A/kick.wav` and `/B/kick.wav` collide; recovery offers the wrong file's audio. `FileController.cpp:503`, `AutoSaveRecovery.cpp:22-23`.

### M5. AutoSave cleanup mis-groups stems containing underscores `REPORTED`
- `cleanupOldAutoSaves` tokenizes on `_` and takes `parts[1]` as the stem → `my_kick_01.wav` grouped as "my"; unrelated files deleted. `FileController.cpp:537-542`.

### M6. Crash recovery deletes all autosaves before validating the recovered buffer `REPORTED`
- `loadIntoBuffer` can return true on a truncated/zero-sample file; no `getNumSamples()>0` check before deleting the autosaves → user gets silence and the backups are gone. `FileController.cpp:769-790`. Also nonstandard-sample-rate autosaves fail `isValidAudioFile` and recovery silently no-ops (`:769`, `AudioFileManager.cpp:90-101`).

### M7. iXML writer does not entity-escape field text → malformed XML drops all iXML on reload `REPORTED`
- `iXMLMetadata.cpp:91-141` uses `createTextElement(x)->getText()` (raw, unescaped). A field containing `&`/`<`/`>` → unparseable chunk → `fromXMLString` fails → all iXML silently lost on next open. Repro: Project = "Test & Demo".

### M8. `appendiXMLChunk` mixed unsigned chunk-size arithmetic — overflow on crafted/corrupt WAV `REPORTED`
- `offset + 8 + parsedChunkSize > fileSize` mixes `uint32_t`/`size_t`; near-`UINT32_MAX` size fields can wrap and pass the guard → out-of-range `write`. `AudioFileManager.cpp:538-557,655-683`. SUSPECTED on 64-bit, confirmed-shape on 32-bit.

### M9. `FilePropertiesDialog` hardcodes `Colours::white/lightgrey` + bg `0xff2a2a2a`, no ThemeManager listener `REPORTED (2 hunters)`
- All 23 labels invisible on Light theme; dialog never re-themes. §6.11 violation. `FilePropertiesDialog.cpp:44,76,109,197,546`. Also `Font(15.0f)` violates the type scale.

### M10. `GraphicalEQEditor` extensive hardcoded chrome/visualization colours + font sizes `REPORTED`
- `Colours::white/cyan/grey`, `0xcc1e1e1e` info box, `darkred` preview button instead of `ui::kButtonPreviewActive`; control-point/grid labels invisible on Light/High-Contrast. `GraphicalEQEditor.cpp:282,592-739,981-988,1210`. §6.11.

### M11. `ChannelConverterDialog` renders runtime bullet `charToString(0x2022)` — non-ASCII glyph CI can't catch `REPORTED`
- `ChannelConverterDialog.cpp:310,320-460`. §6.12 violation; renders as a box in some typefaces/remote sessions. **The CI scanner misses it** because the glyph is produced at runtime from an int literal, not a source byte/escape — a real gap in `check_rendered_ascii.py`. (BWF/iXML metadata text shown in `FilePropertiesDialog.cpp:401-454` has the same runtime non-ASCII display risk.)

### M12. `GainDialog` footer is 30px, not anchored to bottom — violates §6.8 `REPORTED`
- `GainDialog.cpp:234` uses `removeFromTop(30)` without the push-to-bottom idiom; Normalize/Fade use 40px. Visual inconsistency across processing dialogs.

### M13. `HeadTailDialog` Preview updates only the waveform overlay — no audio, no Bypass/Loop `REPORTED`
- `HeadTailDialog.cpp:356-399`. §6.8/§10.2 implied audio preview is absent; user can't hear the trim before applying.

### M14. `ParametricEQDialog` Reset button placed inside the left (audio-control) group `REPORTED`
- `ParametricEQDialog.cpp:384-398` — 4-element left group breaks §6.8 footer convention.

### M15. `FadeDialog` applies a 0 ms fade on a zero-length selection — silent no-op consuming an undo slot `REPORTED`
- `FadeDialog.cpp:260-270,308`. No guard / no "selection required" message.

### M16. `AutomationLanesPanel` row reuse shows wrong lane after a non-tail lane removal `REPORTED`
- Pooled rows keep their original `m_laneIndex` after a `removeLane` shifts the vector → each row reads the next lane's data (off-by-one): wrong names + record-arming the wrong lane. `AutomationLanesPanel.cpp:919,966`.

### M17. `RecordingEngine::appendToRecordingBuffer` silently drops samples on `tryEnter()` failure `REPORTED`
- Whenever the message thread holds `m_bufferLock` (stop/clear), audio samples near the stop boundary are dropped with no flag → silent gap in the recording. `RecordingEngine.cpp:284-289`. Always occurs at stop boundary, not just a race.

### M18. `RecordingEngine` allocates a full 1-hour buffer on every device start `REPORTED`
- ~1.3–2.8 GB at pro rates, under `ScopedLock`, even when not recording (fires on every SR/buffer-size change). OOM/stall risk. `RecordingEngine.cpp:179-195`. (`m_tempRecordingBuffer` is allocated but unused — dead.)

### M19. Thumbnail cache can serve a stale waveform after same-mtime overwrite `REPORTED`
- Cache key = mtime+size; FAT (2 s) / same-second saves collide → wrong waveform shown for new content. Also rapid double-`loadFile` saves a thumbnail under the wrong path. `WaveformDisplay.cpp:135 vs 908,857-914`.

### M20. `PluginChain::processPendingDeletes()` runs VST3 destructors under `m_pendingDeleteMutex` `REPORTED`
- VST3 dtors can block/call back into host; concurrent `publishChain()` taking the same mutex can deadlock. `PluginChain.cpp:144-158`.

### M21. `isBlacklisted()` substring containment → over-broad matching `REPORTED`
- `fileOrIdentifier.contains(entry) || entry.contains(fileOrIdentifier)` blacklists whole directories / all plugins sharing a manufacturer substring. `PluginManager_Persistence.cpp:282-292`.

---

# LOW

- **L1.** `MemoryAudioSource::setNextReadPosition` reads `m_buffer.getNumSamples()` without `m_lock` — brief over/under-clamp window during a buffer swap; corrected on next callback. `AudioEngine.cpp:160-162`. *(2 hunters; SUSPECTED race.)*
- **L2.** `loadFromFile` narrows `int64` length to `int` with no guard → silent truncation >~11.9 h @192k, no error logged. `AudioBufferManager.cpp:56`.
- **L3.** `NewFileDialog` allows multi-GB allocations (e.g. 1 h @192k stereo = 5.5 GB) with no available-memory check; only caps duration at 36000 s. `NewFileDialog.cpp:253-262`.
- **L4.** `GoToPosition` Samples mode uses `getLargeIntValue()` → "1e5" silently becomes 1 (wrong nav, no error). `GoToPositionDialog.cpp:292`.
- **L5.** Several short-lived modal dialogs cache `setColour()` without ThemeManager listener (GoToPosition `:65-112`, BatchExport `:99-101`, LoopingTools crossfade `0xff4488ff` `:64,78`). §6.11; low impact because modal/short-lived.
- **L6.** `playbackPlay`/`playbackPause` register the same keypress 2–3× (copy-paste); intended Space/Shift+Space alternates were never wired. `CommandHandler_GetInfo.cpp:262-280`.
- **L7.** Orphan CommandIDs declared+named but never registered (`snapPreferences` 0x8002, `pluginAddPlugin` 0xD001) — a custom keymap binding them does nothing. `CommandIDs.h:120,176`.
- **L8.** `regionDelete` enabled whenever a file is loaded (not "region selected") → Cmd+Delete routes to region delete with nothing selected, relying on controller no-op. `CommandHandler_GetInfo.cpp:720-725`.
- **L9.** Doc-comment shortcut drift: `CommandIDs.h` comments disagree with `Default.json` for `regionShowList`, `regionExportAll`, `regionStripSilence`, `viewZoomSelection`, `editSilence`. Header is widely trusted as the spec. `CommandIDs.h:47,149-151,164`.
- **L10.** Corrupt user keymap loaded post-scan resets to an *empty* shortcut set with no fallback to Default and no error surfaced. `KeymapManager.cpp:368-380`.
- **L11.** Region transaction names ("Add Region: <name>") embed user-typed names into Edit-menu items → non-ASCII region name = §6.12 violation in a rendered string (CI catches new source literals, not runtime-built menu text). `RegionController.cpp:85-86,126,925`.
- **L12.** Thumbnail cache eviction sorts by mtime; FAT 2 s resolution makes the sort unstable → can evict newer entries. `ThumbnailDiskCache.cpp:89-104`.
- **L13.** `PluginChainRenderer::processBlock` uses a function-`static int blockCounter` — write-write race if offline render and realtime preview overlap; also `Document::~Document` omits `clearUndoHistory()`, leaving member-destruction-order risk if a doc is destroyed without `closeFile()`. `PluginChainRenderer.cpp:406-410`; `Document.cpp:75-84`.

---

# Areas swept that held up (one line each)

- **No duplicate numeric CommandID values; no two Default.json chords map to the same command** (verified programmatically by the keymap hunter).
- **macOS menu is NOT duplicated** — `setMenuBar` is correctly `#if !JUCE_MAC`-guarded (`Main.cpp:236-243`) and `setMacMainMenu` used on Mac (`MainComponent.h:391`). §6.13 compliant.
- **CommandPalette** rebuilds its list per `show()`, checks `isActive` before invoking, skips itself, handles empty-query filtering. No defect.
- **`scripts/check_rendered_ascii.py` passes** (exit 0) on the current tree for source-literal non-ASCII — but see M11 for its runtime-glyph blind spot.

---

## Triage requested

Before Pass 2, please mark each item **FIX NOW / ROADMAP / DROP**. My recommended fix-now set (smallest patches, largest user-facing correctness impact, all CONFIRMED):

1. **C1** Trim wrong-range (one-arg fix + boundary test)
2. **C2 / H2** seconds⇄samples confusion (marker jump + loop selection)
3. **C15** resampler input-pointer not advanced (Save-As corruption)
4. **C12 / H1 / H5** channel-mismatch paste (guard + user-facing error before transaction)
5. **C10 / H21 / C11** atomic writes for WAV + region sidecar, and the iXML/BWF double-write
6. **M1** strip the `std::cerr` spew (trivial, ships in release)

The concurrency Criticals (C5–C9, C16) are real but each needs a small design decision (lock vs lock-free vs SafePointer) — I'd want to confirm the chosen synchronization approach with you rather than pick unilaterally. The undo-stale-index family (H8–H13) likely shares one root fix (switch from stored indices to stable region/marker IDs) and is worth scoping as a single roadmap item.
