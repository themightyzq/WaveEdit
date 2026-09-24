# Classic Editor Keymap Reference

## Document Purpose

This document lists every keyboard shortcut in the **Classic Editor** keymap
template (`Templates/Keymaps/Classic.json`), mapped to WaveEdit commands, with
notes on the reasoning behind non-obvious bindings.

---

## Complete Command Mapping

### FILE OPERATIONS

| WaveEdit Command | Shortcut | Notes |
|-------------------|-------------|-------|
| fileNew | Cmd+N | Industry standard |
| fileOpen | Cmd+O | Industry standard |
| fileSave | Cmd+S | Industry standard |
| fileSaveAs | Cmd+Shift+S | Industry standard |
| fileClose | Cmd+W | Industry standard |
| fileProperties | Alt+Enter | Alt+Enter opens the properties/info dialog |
| fileExit | Cmd+Q | macOS/Linux standard |
| filePreferences | Cmd+, | macOS standard |

---

### EDIT OPERATIONS

| WaveEdit Command | Shortcut | Notes |
|-------------------|-------------|-------|
| editUndo | Cmd+Z | Universal |
| editRedo | Cmd+Shift+Z | Universal |
| editCut | Cmd+X | Universal |
| editCopy | Cmd+C | Universal |
| editPaste | Cmd+V | Universal |
| editDelete | Delete | Raw Delete key |
| editSelectAll | Cmd+A | Universal |
| editSilence | Cmd+L | Mnemonic: L |
| editTrim | Cmd+T | Trim to selection |

---

### PLAYBACK OPERATIONS

| WaveEdit Command | Shortcut | Notes |
|-------------------|-------------|-------|
| playbackPlay | Space | Universal transport control |
| playbackPause | Enter | Distinct key from Play/Stop |
| playbackStop | Escape | Explicit stop, distinct from Play/Pause |
| playbackLoop | Q | Mnemonic: quick loop |
| playbackRecord | Cmd+R | Mnemonic: Record |

**Design note**: this template gives Play/Pause/Stop distinct keys
(Space/Enter/Esc) rather than a single toggle key.

---

### VIEW/ZOOM OPERATIONS

| WaveEdit Command | Shortcut | Notes |
|-------------------|-------------|-------|
| viewZoomIn | Up Arrow | Arrow-key zoom |
| viewZoomOut | Down Arrow | Arrow-key zoom |
| viewZoomFit | Cmd+Down | Fit to window |
| viewZoomSelection | Cmd+Up | Zoom to current selection |
| viewZoomOneToOne | Cmd+0 | Zero = reset zoom |
| viewCycleTimeFormat | Shift+D | Mnemonic: Display |
| viewAutoScroll | Cmd+Shift+F | Follows the common pattern for auto-scroll |
| viewZoomToRegion | Cmd+E | Mnemonic: Expand |
| viewAutoPreviewRegions | Cmd+Shift+P | Preview mode toggle |

**Design philosophy**: this template uses arrow keys for intuitive zoom
(up = in, down = out).

---

### PROCESSING OPERATIONS

| WaveEdit Command | Shortcut | Notes |
|-------------------|-------------|-------|
| processGain | Shift+G | Opens the Gain dialog |
| processIncreaseGain | Shift+Up | +1 dB |
| processDecreaseGain | Shift+Down | -1 dB |
| processNormalize | Cmd+Shift+N | Mnemonic: Normalize |
| processFadeIn | Cmd+Shift+I | Mnemonic: In |
| processFadeOut | Cmd+Shift+O | Mnemonic: Out |
| processDCOffset | Cmd+Shift+D | Mnemonic: DC |

**Key idea**: mnemonic shortcuts throughout (I = In, O = Out, N = Normalize).

---

### NAVIGATION OPERATIONS

| WaveEdit Command | Shortcut | Notes |
|-------------------|-------------|-------|
| navigateLeft | Left Arrow | Universal |
| navigateRight | Right Arrow | Universal |
| navigateStart | Cmd+Left | Jump to file start |
| navigateEnd | Cmd+Right | Jump to file end |
| navigatePageLeft | Page Up | Universal |
| navigatePageRight | Page Down | Universal |
| navigateHomeVisible | Home | First visible sample |
| navigateEndVisible | End | Last visible sample |
| navigateCenterView | . (period) | Decimal point = center |
| navigateGoToPosition | Cmd+G | Mnemonic: Go To |

---

### SELECTION OPERATIONS

| WaveEdit Command | Shortcut | Notes |
|-------------------|-------------|-------|
| selectExtendLeft | Shift+Left | Universal |
| selectExtendRight | Shift+Right | Universal |
| selectExtendStart | Shift+Home | Extend to file start |
| selectExtendEnd | Shift+End | Extend to file end |
| selectExtendPageLeft | Shift+Page Up | Universal |
| selectExtendPageRight | Shift+Page Down | Universal |

---

### SNAP OPERATIONS

| WaveEdit Command | Shortcut | Notes |
|-------------------|-------------|-------|
| snapCycleMode | T | Mnemonic: Toggle |
| snapToggleZeroCrossing | Z | Mnemonic: Zero crossing |

---

### TAB OPERATIONS (Multi-file Support)

| WaveEdit Command | Shortcut | Notes |
|-------------------|-------------|-------|
| tabClose | Cmd+W | Browser/OS standard |
| tabCloseAll | Cmd+Shift+W | Browser/OS standard |
| tabNext | Cmd+Tab | Browser/OS standard |
| tabPrevious | Cmd+Shift+Tab | Browser/OS standard |
| tabSelect1-9 | Cmd+1-9 | Browser/OS standard |

**Note**: this template has no traditional tab interface to draw from, so tab
switching follows modern browser conventions that users already expect.

---

### REGION OPERATIONS

| WaveEdit Command | Shortcut | Notes |
|-------------------|-------------|-------|
| regionAdd | R | Mnemonic: Region |
| regionDelete | Cmd+Delete | Destructive action requires modifier |
| regionNext | ] | Bracket = boundary |
| regionPrevious | [ | Bracket = boundary |
| regionSelectInverse | Cmd+Shift+I | Inverse selection |
| regionSelectAll | Cmd+Shift+A | Variant of Select All |
| regionStripSilence | Cmd+Shift+U | Mnemonic: Un-silence |
| regionExportAll | Cmd+Shift+E | Mnemonic: Export |
| regionShowList | Cmd+M | Mnemonic: Marker/region list |
| regionSnapToZeroCrossing | Shift+Z | Variant of snap toggle |
| regionNudgeStartLeft | Cmd+Shift+Left | Boundary nudging |
| regionNudgeStartRight | Cmd+Shift+Right | Boundary nudging |
| regionNudgeEndLeft | Shift+Alt+Left | Boundary nudging |
| regionNudgeEndRight | Shift+Alt+Right | Boundary nudging |
| regionBatchRename | Cmd+Shift+R | Rename operation |
| regionMerge | Cmd+J | Photoshop-inspired (Join layers) |
| regionSplit | Cmd+Shift+S | Split operation |
| regionCopy | Cmd+Shift+C | Variant of Copy |
| regionPaste | Cmd+Shift+V | Variant of Paste |

**Design note**: bracket keys `[` `]` are used for region navigation
(intuitive: brackets = boundaries).

---

### MARKER OPERATIONS

| WaveEdit Command | Shortcut | Notes |
|-------------------|-------------|-------|
| markerAdd | M | Mnemonic: Marker |
| markerDelete | Cmd+Shift+Delete | Destructive action, requires modifier |
| markerNext | Shift+] | Related to region navigation |
| markerPrevious | Shift+[ | Related to region navigation |
| markerShowList | Cmd+Shift+K | Marker list panel |

**Design note**: markers are distinguished from regions by the Shift modifier
on the same bracket keys.

---

### HELP OPERATIONS

| WaveEdit Command | Shortcut | Notes |
|-------------------|-------------|-------|
| helpAbout | Cmd+Shift+A | About dialog |
| helpShortcuts | Cmd+/ | macOS "Show Help Menu" convention |

---

## Design Philosophy

- **Arrow-key centric**: Up/Down for zoom, Left/Right for navigation.
- **Mnemonic shortcuts**: I = In, O = Out, R = Region, M = Marker.
- **Explicit transport**: separate keys for Play (Space), Pause (Enter), and
  Stop (Esc).
- **Bracket navigation**: `[` `]` for region boundaries.
- Cmd modifiers are used on destructive operations to prevent single-key
  accidents.
- Universal shortcuts (Cmd+Z, Cmd+S, Cmd+C, etc.) are preserved for muscle
  memory.

---

## Template Files

- **Classic.json** ("Classic Editor") - the shortcuts documented above.
- **Session.json** ("Session Style") - an alternate mapping familiar to
  editors coming from multitrack DAWs.
- **Default.json** - WaveEdit's own hybrid template, optimized for
  discoverability by new users.

---

**Document Version**: 2.0
**Author**: WaveEdit Development Team
**License**: GPL v3
**Copyright**: (c) 2025 ZQ SFX
