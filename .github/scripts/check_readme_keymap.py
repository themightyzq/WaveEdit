#!/usr/bin/env python3
"""Verify that README.md's keyboard shortcut table matches Default.json.

Fails CI if README documents a shortcut whose chord differs from the
binding in `Templates/Keymaps/Default.json`. This is the doc-test that
keeps the two from drifting apart.

The README is the user-visible source. Default.json is the runtime
source. They must agree for a documented shortcut.
"""
from __future__ import annotations

import json
import pathlib
import re
import sys


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
README = REPO_ROOT / "README.md"
KEYMAP = REPO_ROOT / "Templates" / "Keymaps" / "Default.json"

# Map command id (in keymap) -> its row label as it appears in README.
# Order: Cmd, Ctrl, Shift, Alt — match how JUCE's KeyPress::getTextDescription
# would render the chord. Keep this list in sync with README's table.
COMMAND_LABELS: dict[str, str] = {
    "fileNew": "New File",
    "fileOpen": "Open File",
    "fileSave": "Save",
    "fileSaveAs": "Save As",
    "fileProperties": "File Properties",
    "fileEditBWFMetadata": "Edit BWF Metadata",
    "filePreferences": "Preferences",
    "fileExit": "Quit",
    "fileBatchProcessor": "Batch Processor",
    "editSelectAll": "Select All",
    "editCut": "Cut",
    "editCopy": "Copy",
    "editPaste": "Paste",
    "editDelete": "Delete",
    "editSilence": "Silence Selection",
    "editTrim": "Trim (delete outside selection)",
    "editUndo": "Undo",
    "editRedo": "Redo",
    "playbackPlay": "Play",
    "playbackPause": "Pause",
    "playbackStop": "Stop",
    "playbackLoop": "Toggle Loop",
    "playbackLoopRegion": "Loop Region",
    "playbackRecord": "Record",
    # navigateLeft/Right/Start/End/PageLeft/PageRight/HomeVisible/EndVisible
    # are documented as combined rows in README (e.g. "Cmd+Left/Right"),
    # so they're verified manually rather than by exact-chord lookup.
    "navigateCenterView": "Center view on cursor",
    "navigateGoToPosition": "Go to Position",
    "viewZoomIn": "Zoom In",
    "viewZoomOut": "Zoom Out",
    "viewZoomSelection": "Zoom to Selection",
    "viewZoomToRegion": "Zoom to Region",
    "viewZoomFit": "Zoom to Fit",
    "viewZoomOneToOne": "Zoom 1:1",
    "snapCycleMode": "Cycle Snap Mode",
    "snapToggleZeroCrossing": "Toggle Zero-Crossing Snap",
    "viewCycleTimeFormat": "Cycle Time Format",
    "processGain": "Gain Dialog",
    "processIncreaseGain": "Increase Gain (+1 dB)",
    "processDecreaseGain": "Decrease Gain (-1 dB)",
    "processNormalize": "Normalize",
    "processFadeIn": "Fade In",
    "processFadeOut": "Fade Out",
    "processDCOffset": "DC Offset Removal",
    "processGraphicalEQ": "Graphical EQ",
    "toolsChannelConverter": "Channel Converter",
    "pluginShowChain": "Show Plugin Chain",
    "pluginApplyChain": "Apply Plugin Chain",
    "pluginShowAutomationLanes": "Show Automation Lanes",
    "regionAdd": "Add Region",
    "regionStripSilence": "Strip Silence to Regions",
    "regionShowList": "Region List Panel",
    "regionBatchRename": "Batch Rename",
    "regionExportAll": "Batch Export",
    "regionMerge": "Merge Regions",
    "regionSplit": "Split Region",
    "regionCopy": "Copy Regions",
    "regionPaste": "Paste Regions",
    "regionDelete": "Delete Region",
    "regionSelectAll": "Select All Regions",
    "regionSelectInverse": "Invert Region Selection",
    "markerAdd": "Add Marker",
    "markerShowList": "Marker List Panel",
    "markerNext": "Next Marker",
    "markerPrevious": "Previous Marker",
    "markerDelete": "Delete Marker",
    "viewAutoScroll": "Auto-Scroll",
    "viewAutoPreviewRegions": "Auto-Preview Regions",
    "viewSpectrumAnalyzer": "Spectrum Analyzer",
    "tabNext": "Next Tab",
    "tabPrevious": "Previous Tab",
    "tabClose": "Close Tab",
    "tabCloseAll": "Close All Tabs",
    "helpShortcuts": "Keyboard Shortcuts",
    "helpCommandPalette": "Command Palette",
    "fileEditiXMLMetadata": "Edit iXML Metadata",
    "editMarkSelectionStart": "Mark In",
    "editMarkSelectionEnd": "Mark Out",
    "processReverse": "Reverse",
    "processInvert": "Invert Polarity",
    "processResample": "Resample",
    "processTimeStretch": "Time Stretch",
    "processPitchShift": "Pitch Shift",
    "toolsChannelExtractor": "Channel Extractor",
    "toolsHeadTail": "Head & Tail Editor",
    "toolsLoopingTools": "Looping Tools",
    "generateInsertSilence": "Insert Silence",
    "generateTone": "Generate Tone",
    "generateNoise": "Generate Noise",
    "pluginOffline": "Offline Plugin Processing",
    "pluginBypassAll": "Bypass All Plugins",
    "automationToggleRecordArm": "Arm Automation Recording",
    "regionConvertToMarkers": "Convert Regions to Markers",
    "markerConvertToRegions": "Convert Markers to Regions",
    "viewToggleRegions": "Toggle Region Overlay",
    "toolbarCustomize": "Customize Toolbar",
    "toolbarReset": "Reset Toolbar",
}

# Commands that ARE bound in Default.json but are documented in the README as
# combined/range rows (e.g. "Cmd+Left/Right", "Cmd+1 ... Cmd+9") rather than
# one exact-chord row each. They are deliberately NOT exact-chord checked, but
# they must still be accounted for so the two-way completeness test below can
# assert that EVERY bound command is either exact-checked or explicitly listed
# here. Adding a new bound command forces a choice: give it a README row +
# COMMAND_LABELS entry, or list it here as an intentional combined row.
MANUAL_ROWS: set[str] = {
    # Navigation range rows ("Left/Right", "Cmd+Left/Right", "Home/End", ...)
    "navigateLeft", "navigateRight", "navigateStart", "navigateEnd",
    "navigatePageLeft", "navigatePageRight",
    "navigateHomeVisible", "navigateEndVisible",
    # Selection-extend range rows ("Shift+Left/Right", "Shift+Home/End", ...)
    "selectExtendLeft", "selectExtendRight",
    "selectExtendStart", "selectExtendEnd",
    "selectExtendPageLeft", "selectExtendPageRight",
    # Tab selection ("Cmd+1 ... Cmd+9")
    "tabSelect1", "tabSelect2", "tabSelect3", "tabSelect4", "tabSelect5",
    "tabSelect6", "tabSelect7", "tabSelect8", "tabSelect9",
    # Region nudge range rows ("Cmd+Alt+Left/Right", "Shift+Alt+Left/Right")
    "regionNudgeStartLeft", "regionNudgeStartRight",
    "regionNudgeEndLeft", "regionNudgeEndRight",
    # Region prev/next combined row ("] / [")
    "regionNext", "regionPrevious",
}

MOD_ORDER = ["cmd", "ctrl", "shift", "alt"]
MOD_LABEL = {"cmd": "Cmd", "ctrl": "Ctrl", "shift": "Shift", "alt": "Alt"}


def chord(entry: dict) -> str:
    parts = [MOD_LABEL[m] for m in MOD_ORDER if m in entry.get("modifiers", [])]
    parts.append(entry["key"])
    return "+".join(parts)


ROW_RE = re.compile(r"^\|\s*([^|]+?)\s*\|\s*`([^`]+)`\s*\|", re.MULTILINE)


def parse_readme_rows(text: str) -> dict[str, str]:
    """Build a {row-label -> chord} map from README's tables.

    Combined-row entries like ``| ... | `Cmd+Left/Right` |`` end up in
    the map verbatim; the test simply doesn't reference those labels.
    """
    rows: dict[str, str] = {}
    for m in ROW_RE.finditer(text):
        label, chord_str = m.group(1), m.group(2)
        rows[label] = chord_str
    return rows


def main() -> int:
    keymap = json.loads(KEYMAP.read_text())["shortcuts"]
    readme_rows = parse_readme_rows(README.read_text())

    failures: list[str] = []

    # --- Two-way completeness: every BOUND command in Default.json must be
    # either exact-chord documented (COMMAND_LABELS) or an intentional
    # combined row (MANUAL_ROWS). This is the direction that used to let 14+
    # bound shortcuts go silently undocumented (finding UX9/UX28).
    bound_commands = [c for c in keymap if not c.startswith("_comment")]
    documented = set(COMMAND_LABELS) | MANUAL_ROWS
    for cmd in bound_commands:
        if cmd not in documented:
            failures.append(
                f"{cmd}: bound in Default.json (chord `{chord(keymap[cmd])}`) "
                f"but NOT documented in README (add a row + COMMAND_LABELS "
                f"entry, or list it in MANUAL_ROWS if it is a combined row)"
            )

    for cmd, label in COMMAND_LABELS.items():
        if cmd not in keymap:
            failures.append(f"{cmd}: not in Default.json (label '{label}')")
            continue
        expected_chord = chord(keymap[cmd])
        readme_chord = readme_rows.get(label)
        if readme_chord is None:
            failures.append(
                f"{cmd}: README has no row labeled '{label}' "
                f"(expected chord `{expected_chord}`)"
            )
            continue
        if readme_chord != expected_chord:
            failures.append(
                f"{cmd}: README row '{label}' says `{readme_chord}` "
                f"but Default.json says `{expected_chord}`"
            )

    if failures:
        sys.stderr.write(
            f"FAIL: README.md and Default.json disagree on "
            f"{len(failures)} shortcut(s):\n\n"
        )
        for f in failures:
            sys.stderr.write(f"  - {f}\n")
        sys.stderr.write(
            "\nFix README.md's Keyboard Shortcuts table to match "
            "Templates/Keymaps/Default.json (or vice versa).\n"
        )
        return 1

    bound_total = len([c for c in keymap if not c.startswith("_comment")])
    print(
        f"OK: README ⇆ Default.json agree on {len(COMMAND_LABELS)} "
        f"exact-chord shortcuts; all {bound_total} bound commands are "
        f"documented ({len(MANUAL_ROWS)} as combined rows)."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
