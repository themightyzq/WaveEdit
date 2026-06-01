# WaveEdit — Design Craft Review

> **Status (2026-05-29): RESOLVED — historical snapshot.** Nearly all findings
> below were fixed in the follow-up polish round. All 6 Critical (C1–C6) and the
> High-Impact items (H1–H18) are addressed, along with N1–N4, N6, N8, N9, N12.
> Theming is now driven end-to-end by the LookAndFeel + ThemeManager, color
> literals swept to tokens, fonts/padding on the UIConstants scale, and all
> rendered strings are ASCII (see CHANGELOG [Unreleased] and CLAUDE.md §6.11 /
> §6.12). Known not-fully-done: a few domain-visualisation colours (spectrum /
> EQ-curve) remain literals by design; N5 (tab overflow) and N7 (screen-reader
> accessible names) were left as future work. This file is kept as the record
> of what the review found, not a live task list.

Native JUCE desktop audio editor. The web checklist (WCAG/ARIA/breakpoints) is mapped to native equivalents: LookAndFeel + colour tokens for CSS, JUCE keyboard focus for ARIA, the §6.8 dialog-footer protocol for layout consistency. Contrast ratios below are computed against WCAG AA (4.5:1 text, 3:1 UI).

Scope: the craft pass. Findings are sorted Critical / High Impact / Nice to Have. Each is independently actionable.

---

## Critical

### C1. Light and High Contrast themes are broken — only the waveform re-skins
`ThemeManager` (Dark/Light/HighContrast) exists and broadcasts changes, but per its own note only waveform surfaces are routed through it (`Source/UI/WaveEditTheme.h:50-52`). `WaveEditLookAndFeel` never references `ThemeManager` — it hardcodes every control colour from `UIConstants.h` dark literals (`Source/UI/WaveEditLookAndFeel.cpp:16-34`). On top of that there are **196 raw `juce::Colour(0xff…)` literals across `Source/UI/`** painting backgrounds, text, and borders unconditionally dark.
**Effect:** switching to Light or High Contrast leaves the entire UI dark except the waveform box — a half-broken, obviously-unfinished state. High Contrast, which is advertised as the accessibility theme, never reaches the controls that need it.
**Why it matters:** ships a feature (theme picker) that visibly doesn't work; the one accessibility affordance is inert. Either route the LookAndFeel + paint paths through `ThemeManager` or remove Light/HighContrast from the picker until they work.

### C2. SettingsPanel text turns invisible on Light theme
Every section label in SettingsPanel is forced `juce::Colours::white` (`Source/UI/SettingsPanel.cpp:461,543,549,609,616,688`) and the panel never subscribes to `ThemeManager`. The Settings panel is *where the user switches themes* — switch to Light and the labels you just used to do it vanish against the light panel.
**Why it matters:** the most embarrassing possible instance of C1 — the control surface for theming breaks itself.

### C3. GraphicalEQ control points are grabbable 20px away from where they're drawn
The mouse handlers reserve a 40px footer (`BUTTON_HEIGHT+MARGIN`) when mapping clicks to the EQ curve (`Source/UI/GraphicalEQEditor.cpp:355-356,389-390,860-861`), but `paint()`/`resized()` reserve 60px (`BUTTON_HEIGHT+FOOTER_SPACING+AXIS_LABEL_HEIGHT`, lines `261,311`). The hit-test coordinate space disagrees with the rendered curve by 20px vertically.
**Why it matters:** a flagship feature where you drag points on a curve — and the points respond at the wrong vertical position. Functional defect, not cosmetic.

### C4. RecordingDialog spams modal dialogs when the buffer fills
`timerCallback` fires every 33ms and shows an async modal warning whenever `isBufferFull()` is true, with no latch (`Source/UI/RecordingDialog.cpp:292-295`). Once the buffer is full it stacks a new "buffer full" modal ~30×/second.
**Why it matters:** the app becomes unusable mid-record; the user can't dismiss dialogs faster than they spawn.

### C5. FilePropertiesDialog reserves space for section headers it never draws
`resized()` reserves `ROW_HEIGHT+SPACING` blocks for "File Information", "Audio Information", etc. (`Source/UI/FilePropertiesDialog.cpp:204,213,222,232`), but `paint()` draws nothing there (`176-177`). The result is the properties list broken up by unexplained empty gaps with no headings.
**Why it matters:** the dialog reads as broken/half-rendered; the information hierarchy the gaps imply is simply missing.

### C6. Region and Marker list panels render a blank box when empty
`getNumRows()` returns the filtered count and `paint()` only fills the background — there is no empty-state message for "no regions / no markers" or for "search matched nothing" (`Source/UI/RegionListPanel.cpp:549-552,717`; `Source/UI/MarkerListPanel.cpp:299-302`). PluginChainPanel and AutomationLanesPanel do this correctly (`m_emptyLabel`) — copy that pattern.
**Why it matters:** two of the core side panels look empty/broken on first run and after a no-match search; the user gets no guidance on how to create the first region/marker.

---

## High Impact

### H1. Error/warning text fails WCAG AA contrast
`kStatusError = 0xffff0000` (pure red) on the `0xff2b2b2b` background is **3.54:1** — below the 4.5:1 AA text threshold. It's the standard error/warning text colour, used in NormalizeDialog gain warnings (`Source/UI/NormalizeDialog.cpp:316`), SaveAsOptionsPanel (`169`), RecordingDialog status (`382`), and `UIConstants.h:117`. Darken the background usage or use a lighter red (e.g. `#ff5c5c` clears 4.5:1).
**Why it matters:** error messages — the text users most need to read — are the least legible.

### H2. Accent blue fails AA when used as text
`kAccentPrimary = 0xff4a90d9` is **4.24:1** on the main background (`UIConstants.h:73`). It's fine as a focus ring (UI component, 3:1) but fails as link/value/emphasis text. Audit text uses of the accent and lighten for text contexts.

### H3. The type scale is theory, not practice — 10+ live font sizes
`UIConstants.h` defines a clean scale (18/16/14/12/10 + 11 mono) but **no audited dialog or panel includes or uses it.** Sizes hardcoded in the wild: 9, 10, 11, 12, **13**, **13.5**, 14, 16, 18, **20**, **24** (`GoToPositionDialog.cpp:63`=20, `RecordingDialog.cpp:117`=24, `ShortcutEditorPanel.cpp:121`=20, `GraphicalEQEditor.cpp:697`=9, `AutomationLanesPanel.cpp:921`=13.5). The bolded sizes aren't even on the intended scale.
**Why it matters:** hierarchy stops doing work when there are eleven sizes — adjacent dialogs disagree on what a "title" or "body" size is. Route everything through `ui::kFont*` and delete the off-scale sizes.

### H4. Five near-identical dark grays; two differ by one value
Background literals in use: `0xff2b2b2b` (kBackgroundPrimary), `0xff2a2a2a` (kBackgroundSecondary, also used as dialog panel bg everywhere), `0xFF2D2D30` (`CustomizableToolbar.cpp:49`), `0xff1a1a1a` (`Meters.cpp:174`, time readouts), `0xff202020` (`AutomationLanesPanel.cpp:242`). `kBackgroundPrimary` vs `kBackgroundSecondary` differ by 1/255 — an imperceptible distinction masquerading as a semantic token.
**Why it matters:** where a `2d2d30` toolbar sits above a `2a2a2a` transport there's a visible seam (`CustomizableToolbar.cpp:49` vs `CompactTransport.cpp:244`). Collapse to 2–3 deliberate surface levels.

### H5. The same "no file" state is worded three different ways
`m_noFileLabel` says "No file open" (`Source/MainComponent.h:358`); the status bar says "No file loaded - Press Cmd+O to open or drag & drop a WAV file" (`MainComponent.h:1050`); the waveform says "No audio file loaded" (`Source/UI/WaveformDisplay_Render.cpp:56`). Three strings, one condition.
**Why it matters:** copy written without a reader in mind; pick one phrasing and one place to say it.

### H6. Dialog padding is inconsistent across the app
`kDialogPadding=15` is ignored; dialogs hardcode their own inset: Gain/EQ `reduced(15)`, Normalize/Fade/StripSilence/GoTo `reduced(20)`, HeadTail/Looping `kMargin=16`. Four different outer paddings.
**Why it matters:** dialogs that open from the same menu feel like they came from different apps. Use the token.

### H7. §6.8 dialog-footer protocol is violated by several dialogs
The standard is `Preview→Bypass→Loop | Cancel→Apply`. Violations: StripSilenceDialog and HeadTailDialog use a centered 3-button row (Preview/Apply/Cancel, width 100, no Bypass/Loop) (`StripSilenceDialog.cpp:262-268`, `HeadTailDialog.cpp:568-575`); ParametricEQDialog injects a center **Reset** button and drops the entire left group when no engine is present (`ParametricEQDialog.cpp:354,371`); GoToPositionDialog reverses to Go-left/Cancel-right (`GoToPositionDialog.cpp:181-183`).
**Why it matters:** the footer is the one piece of chrome users hit on every process — inconsistency here is felt constantly.

### H8. Dialogs aren't keyboard-operable, in a keyboard-first product
CLAUDE.md §2 makes keyboard-first a core principle, but only GoToPositionDialog and NewFileDialog wire Enter=confirm / Esc=cancel and grab initial focus. The other ~8 processing/metadata dialogs (Normalize, Fade, StripSilence, HeadTail, ParametricEQ, GraphicalEQ, BWF, iXML, ChannelConverter…) set no default focus, no default (Enter) button, and no Esc-cancel; tab order is implicit construction order.
**Why it matters:** every process requires reaching for the mouse to dismiss — directly contradicts the product's stated workflow.

### H9. Selected rows use the border colour, not the selection token
RegionListPanel, MarkerListPanel, and PluginChainPanel fill the selected row with `theme.border` (`RegionListPanel.cpp:360`, `MarkerListPanel.cpp:140`, `PluginChainPanel.cpp:505`) — a low-contrast separator colour. A `theme.selection` token exists and only ShortcutEditorPanel uses it (`ShortcutEditorPanel.cpp:236`).
**Why it matters:** "what's selected" is the primary state in a list; right now it's barely visible. Selected-row text is also hardcoded `Colours::white` (`RegionListPanel.cpp:750`), invisible on Light theme.

### H10. BWF/iXML field validation is dead code
`setInputRestrictions(maxChars)` hard-caps input length (`BWFEditorDialog.cpp:65-69`, `iXMLEditorDialog.cpp:70`), so the "N / 256 chars → turns red" overflow indicator can never fire (`BWFEditorDialog.cpp:312`, `iXMLEditorDialog.cpp:473`). Meanwhile BWF date/time fields are only length-restricted and accept malformed values like "99-99-9999" silently (`BWFEditorDialog.cpp:68-69,278`), and iXML's "ALL CAPS"/"Title Case" hints are never enforced (`iXMLEditorDialog.cpp:100-101,324`).
**Why it matters:** the validation UI is decorative; invalid metadata is written without warning.

### H11. Transport toggles have no "on" state; record has no active indicator
Loop buttons in both transports are `setClickingTogglesState(true)` but the icon colour never changes when active (`TransportControls.cpp:138`, `CompactTransport.cpp:200`), and the recording indicator is unimplemented (`CompactTransport.cpp:251-252` "would go here"). State relies on JUCE's faint default tint.
**Why it matters:** the user can't tell at a glance whether loop is engaged or whether they're recording — core transport feedback.

### H12. Time readouts use a proportional font and jitter
The transport time display updates ~20×/s in a proportional 16px font (`TransportControls.cpp:144`) and 11px in CompactTransport (`CompactTransport.cpp:206`) — digits change width as they tick, so the readout shifts horizontally. `kFontMonospace` exists for exactly this and isn't used. The CompactTransport time label also cycles format on click with zero affordance (no cursor change, no tooltip) (`CompactTransport.cpp:210-211`).
**Why it matters:** a jittering clock reads as cheap; a hidden click target is undiscoverable.

### H13. Icon system is three divergent hand-coded sets
Transport icons are drawn paths on a 24px grid (`TransportControls.cpp:28-99`), CompactTransport on 16px (`CompactTransport.cpp:28-146`), and ToolbarButton on 16px (`ToolbarButton.cpp:251-469`) — with inconsistent stroke weights (1.5 vs 2.0) and mixed fill-vs-stroke metaphors (filled triangle here, stroked outline there). ToolbarButton picks the glyph by fragile substring match: `containsIgnoreCase("dc")` matches any command containing "dc", `("new")`/`("open")` over-match, and unmatched commands all fall back to an identical generic circle (`ToolbarButton.cpp:406-468`).
**Why it matters:** inconsistent iconography is the clearest "vibe-coded" tell; the substring dispatch silently assigns wrong/duplicate icons.

### H14. CommandPalette doesn't respond to the mouse and hides its overflow
The palette is keyboard-only — there's no `mouseDown`/row hit-testing, so clicking a result does nothing (`CommandPalette.cpp:218-236`), and it pages through results with no visible scrollbar so the user can't tell more exist (`117-123`). The 17-entry category colour table is sprawl: `edit`/`toolbar`/default all resolve to gray, and `playback` (`0xff55cc77`) vs `region` (`0xff66cc66`) are indistinguishable greens (`CommandPalette.cpp:451-469`).
**Why it matters:** a command palette is a showcase surface (every Linear/Raycast user expects click + scroll); this one quietly ignores half its inputs.

### H15. Meters: stereo-only, linear color thresholds, no idle state
The meter only ever draws "L"/"R" (`Meters.cpp:183-205`) so mono files are mislabeled and >2ch is unsupported; the color thresholds compare *linear* level not dB (`Meters.cpp:233-238`), so the green zone visually dominates and the yellow/red transition is compressed near the top; and there's no distinct idle/"no signal" state — inactive metering looks identical to true silence (`Meters.cpp:270`).
**Why it matters:** the meter is the always-visible signal-health indicator; mislabeling and a misleading color curve undermine trust in it.

### H16. Debug `std::cerr` logging runs on every layout
`PluginChainPanel::resized()` writes `[PLUGINROW]` lines to stderr on every layout pass (`Source/UI/PluginChainPanel.cpp:157-166`, with `#include <iostream>` at `18`), violating logging protocol §6.10.
**Why it matters:** noise on every resize, shipped to release; remove before completion.

### H17. KeyboardCheatSheet is hand-maintained and already stale
The cheat-sheet builds its list from a manual `commandIDs.add(...)` block (`KeyboardCheatSheetDialog.cpp:271-372`) that already omits commands with display names — `regionSelectInverse`, `regionSelectAll`, `markerShowList`. Any new command must be added here by hand or it silently won't appear. There's also no on-canvas "?" affordance to discover it; it opens only via the `helpShortcuts` command.
**Why it matters:** the shortcut reference — the thing that teaches a keyboard-first app — is incomplete and silently drifts.

### H18. RecordingDialog presents device controls that do nothing
The input-device, sample-rate, and channel combo boxes are no-ops marked "Future:" (`RecordingDialog.cpp:256-270`).
**Why it matters:** controls that look functional but silently ignore input erode trust; disable or hide until wired.

---

## Nice to Have

### N1. List-panel row striping uses opposite parity
RegionListPanel stripes `%2==1` while MarkerListPanel stripes `%2==0` (`RegionListPanel.cpp:722` vs `MarkerListPanel.cpp:311`) — the two adjacent panels shade opposite rows. Pick one.

### N2. Region color swatch looks clickable but is dead
The swatch cell renders like the interactive one in MarkerListPanel but RegionListPanel's `cellClicked` only handles the name column (`RegionListPanel.cpp:803-810`) — clicking the swatch does nothing.

### N3. Marker color picker pops up in the wrong place
The color-picker CallOutBox anchors to `m_table.getScreenBounds()` (the whole table) instead of the clicked cell (`MarkerListPanel.cpp:427`), so it appears detached from what was clicked.

### N4. ErrorDialog can't distinguish errors from warnings and can't be copied
Errors reuse the WarningIcon (`ErrorDialog.cpp:135`), the "collapsible details" are just appended text not an expander (`46-50`), and there's no way to copy the technical detail string.

### N5. Tabs scroll instead of compressing; long names squeezed not ellipsized
Tabs are fixed `kMaxTabWidth` and never shrink when many are open (`TabComponent.cpp:37`) — you scroll rather than see all tabs. Long filenames are fit with `drawFittedText(...,1)` (`TabComponent.cpp:110`), which scales text down rather than middle-truncating while preserving the extension.

### N6. Button hover feedback is near-invisible
`drawButtonBackground` lightens by only `brighter(0.1f)` on hover (`WaveEditLookAndFeel.cpp:48`) — barely perceptible on the dark surface.

### N7. No screen-reader names; icon-only buttons rely on tooltips
There is no `AccessibilityHandler`/accessible-name usage in the codebase; the 51 `setTooltip` calls are not exposed as accessible names by JUCE. Icon-only transport/toolbar buttons are unlabeled to assistive tech. (Lower priority for a pro desktop tool, but the gap is total.)

### N8. SaveAsOptions leaves ragged gaps per format
Format-specific rows (bit-depth/quality/metadata) are hidden by visibility but still consume layout height in `resized()` (`SaveAsOptionsPanel.cpp:387-406`), so compressed formats leave the WAV rows as empty vertical gaps and vice versa. Reflow on format change.

### N9. ChannelConverter allows a no-op conversion
Selecting the "(Current)" same-channel preset shows "No conversion needed" but Apply stays enabled and runs anyway (`ChannelConverterDialog.cpp:273,457`). Disable Apply for the no-op case.

### N10. Deprecated `juce::Font(float)` ctor papered over with pragmas
`UIConstants.h:211-269` wraps every font helper in `-Wdeprecated-declarations` suppression rather than migrating to `FontOptions`; the codebase mixes `FontOptions` (newer files) and `juce::Font(...)` (Gain/Normalize/SaveAs) inconsistently. Migrate and drop the suppressions.

### N11. Validation glyphs use raw Unicode
GoToPositionDialog uses `✓`/`✗` (`GoToPositionDialog.cpp:349,357`) which depend on the rendering font having the glyphs; a drawn check/cross or a colored dot is safer.

### N12. `#include <iostream>` in UI files
`GraphicalEQEditor.cpp:20` and `SaveAsOptionsPanel` pull in `<iostream>` in GUI code — dead/inappropriate include (related to the debug logging in H16).
