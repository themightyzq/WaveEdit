# WaveEdit — End-User Review

> **Status (2026-05-29): RESOLVED — historical snapshot.** Every finding below
> was fixed: Broken (B1 Finder open, B2 box-glyphs, B3 double-ampersand),
> Missing (M1 file-type registration, M2 empty-state Open button), Confusing
> (C1 duplicate menu bar, C2 doubled message, C3 macOS menu conventions),
> Recommended (R1 CI ASCII guard — `scripts/check_rendered_ascii.py`, R2 README
> label), and Nice-to-have (N1 accessible names, N2 tab compression). The ASCII
> guard also surfaced a stray `©` and a "Version 1.0" mismatch in the About box,
> both fixed. See CHANGELOG [Unreleased] and CLAUDE.md §6.12 / §6.13. Residuals
> (N1 not yet screen-reader-tested; generated UCS keyword mojibake) are tracked
> in TODO.md → Known Issues. Kept as the record of what the review found.

Reviewed as a demanding sound-designer / audio-engineer (the persona CLAUDE.md targets: "Would a Sound Forge user find this intuitive and robust?"). macOS, Release build (`0.1.0`), launched and clicked through. Findings are weighted to user-visible impact. I ran the app; I did not read source except to pin down exact locations for a few findings.

**What I could verify:** install/build docs, launch, empty state, the full menu structure (native + in-window), opening a file via Finder/argument, the theme/glyph rendering, version consistency, config/uninstall docs.
**What I could NOT verify (no scripted way to drive native dialogs / mouse-drag):** the `Cmd+O` file picker, drag-and-drop onto the window, actual DSP/playback output, and a real screen-reader pass. Where I couldn't test, I say so.

---

## Broken — told it works, but doesn't

### B1. You can't open an audio file from Finder (double-click / "Open With" / drag-to-icon)
- **Symptom:** Launching WaveEdit *with a file* (double-click a WAV in Finder, "Open With → WaveEdit", or drag a file onto the app/Dock icon) opens the app to an empty "No audio file loaded" window. The file never loads, and there's no error.
- **Where:** Verified by `open -a WaveEdit.app moans_backup.wav` → empty window. The app bundle's `Info.plist` has **no `CFBundleDocumentTypes`**, so WaveEdit doesn't even appear in Finder's "Open With" list or claim `.wav`.
- **Severity:** major
- **Fix:** Register `CFBundleDocumentTypes` for `wav/flac/ogg/mp3` and handle the macOS open-document event / `JUCEApplication::anotherInstanceStarted` + `openFile` to load the passed path.

### B2. Unreadable box-glyphs still appear in menus, dialogs, and panels
- **Symptom:** Several visible labels render as missing-glyph boxes (the exact problem reported earlier): the menu items **"Regions → Markers"** and **"Markers → Regions"**; Looping Tools buttons **"▶ Preview"** and **"■ Stop Preview"**; the Progress dialog status **"Working…"**; the Region/Marker panel empty-state lines **"No regions yet — …" / "No markers yet — …"** (em-dash); the plugin-scan **"⏱"** timer icon; a **"→"** in the Batch Processor.
- **Where:** `CommandHandler_GetInfo.cpp:891,899`; `LoopingToolsDialog.cpp:392,829,890,906`; `ProgressDialog.cpp:101`; `RegionListPanel.cpp:1159`; `MarkerListPanel.cpp:663`; `PluginScanDialogs.cpp:586`; `BatchProcessorDialog.cpp:956`. These use **byte-escape literals** (`"\xe2\x80\xa6"`), which the previous ASCII sweep's raw-byte search could not see — so they were missed.
- **Severity:** major (it's the user's reported symptom, still present, and the CHANGELOG now claims "all rendered text is now ASCII" — which is currently false).
- **Fix:** Replace the byte-escaped Unicode with ASCII (`->`, `...`, `--`, `>`/`#` for play/stop, drop the timer glyph); re-run the sweep with a grep that also matches `\xNN` escape forms.

### B3. "Head && Tail…" shows a literal double ampersand
- **Symptom:** The Tools menu item reads **"Head && Tail…"** instead of "Head & Tail…".
- **Where:** `CommandHandler_GetInfo.cpp:630` (string is `"Head && Tail..."`; JUCE's mac menu doesn't collapse the `&&` mnemonic escape).
- **Severity:** minor
- **Fix:** Use a single `&` ("Head & Tail...").

---

## Missing — reasonably expected, not present

### M1. No OS file-type integration
- **Symptom:** WaveEdit isn't offered as an app to open audio files, has no recent-files jump list from the Dock, and can't be set as the default `.wav` handler.
- **Where:** Bundle `Info.plist` (no document types / UTI declarations).
- **Severity:** major (every comparable editor — Audacity, ocenaudio, Sound Forge, WaveLab — registers audio types; a sound designer opens files from Finder/Soundminer constantly).
- **Fix:** Declare document types + handle open-document (pairs with B1).

### M2. Empty state has no clickable way in
- **Symptom:** The opening screen is a large empty area with the text "No audio file loaded — press Cmd+O or drag in a file". There's no **Open** button or visible drop target to click.
- **Where:** Main window, no-document state.
- **Severity:** minor
- **Fix:** Add an "Open File…" button (and/or a dashed drop-zone) in the empty state; most editors do.

---

## Confusing — works, but causes friction or self-doubt

### C1. Two identical menu bars on macOS
- **Symptom:** The standard macOS menu bar at the top of the screen (Apple · WaveEdit · File · Edit · … · Help) is fully populated, **and** the same File/Edit/View/…/Help bar is drawn again inside the window. Two menu bars, one redundant, eating a row of vertical space.
- **Where:** Top-of-screen menu bar vs. in-window menu row (visible in the empty-state window).
- **Severity:** minor
- **Fix:** On macOS, use only the native menu bar; don't also render the in-window `MenuBarComponent` (keep in-window for Windows/Linux).

### C2. "No audio file loaded" is shown twice at once
- **Symptom:** With no file open, the message appears both centered in the waveform area **and** in the status bar simultaneously.
- **Where:** Main window empty state + status bar.
- **Severity:** polish
- **Fix:** Show it in one place (keep the centered one; let the status bar show neutral/ready text).

### C3. Windows-isms in the Mac menus
- **Symptom:** The File menu has **"Exit"** (Mac uses "Quit", already present under the WaveEdit app menu), and **"Preferences…"** lives under **File** rather than the **WaveEdit** app menu where Mac users look for it.
- **Where:** File menu (`Application → Preferences... / Exit`).
- **Severity:** minor
- **Fix:** On Mac, put Preferences in the app menu (and confirm `Cmd+,` opens it); drop the redundant "Exit".

---

## Recommended — would meaningfully raise quality

### R1. Make the prior "glyph" fix actually complete and verifiable
- **Symptom:** The codebase claims (CHANGELOG, CLAUDE.md §6.12) that rendered strings are ASCII, but B2 shows they aren't. A user who hit this once will hit it again and lose trust.
- **Severity:** major
- **Fix:** Add a check (grep or test) that fails on any non-ASCII in drawn strings **including `\xNN` escape forms**, so this can't silently regress.

### R2. Confirm/standardize the Preferences shortcut and Help discoverability
- **Symptom:** README lists "Keyboard Shortcuts Reference — `Cmd+/`" but the Help menu item is just "Keyboard Shortcuts" (no visible accelerator in the menu dump). Minor naming/shortcut drift makes the reference feel less trustworthy.
- **Where:** README "Help" table vs. Help menu.
- **Severity:** minor
- **Fix:** Show the accelerator in the menu item and match the README label/wording.

---

## Nice-to-have — lower-priority polish

### N1. Icon-only toolbar/transport buttons have no accessible names
- **Symptom:** A VoiceOver user would hear unlabeled buttons. (Tooltips exist but aren't exposed as accessibility names.)
- **Severity:** polish (niche for this category, but free trust)
- **Fix:** Set `setTitle()`/accessibility names on icon-only buttons.

### N2. File-tab overflow with many open files
- **Symptom:** Tabs are fixed-width and scroll rather than compress; with many long-named SFX files open you scroll a lot. (Names now truncate with `...`, which is good.)
- **Severity:** polish
- **Fix:** Compress tab widths when the bar overflows, like most editors.

---

## Promises vs. Reality (spot checks)
- "Sub-1 second cold start" — **plausible**; the window appeared effectively instantly.
- "Open: Drag & drop WAV file or press Cmd+O" — **partially verified**. Finder/Open-With does **not** work (B1). I could not script-test drag-onto-window or the Cmd+O picker, so those are unverified-but-claimed.
- Screenshots referenced in README (`Docs/screenshots/01–05`) — **present and correct**.
- Version numbers (CMake `0.1.0`, README footer `0.1.0`, bundle `0.1.0`, CHANGELOG `0.1.0`) — **consistent**. Good.
- Config/uninstall docs (Application Support paths, log location, legacy-path migration, `rm -rf` uninstall) — **present and unusually thorough**. Good trust signal.

---

## Top User Frustrations (ordered by churn likelihood)

1. **Can't open files from Finder (B1/M1).** A sound designer's #1 muscle-memory action — double-click a WAV, or "Open With" from Soundminer/Finder — does nothing. This alone will make people assume the app is broken on first contact.
2. **Unreadable boxes in menus and dialogs (B2).** The very thing that was reported as fixed is still visible in the menu ("Regions → Markers"), Looping Tools, the progress dialog, and the panels users see on day one. Reads as unfinished.
3. **Two menu bars + Mac-isms (C1/C3).** A duplicated menu bar and an "Exit" item / misplaced Preferences immediately signal "this wasn't built by a Mac person," undermining the "professional tool" positioning.
4. **No obvious way in from an empty window (M2).** New users land on a black void; the only instruction is a one-line hint. A clickable Open/drop-zone would convert the first 5 seconds from doubt to action.
