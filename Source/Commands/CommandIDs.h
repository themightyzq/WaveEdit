/*
  ==============================================================================

    CommandIDs.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

namespace CommandIDs
{
    /**
     * Application command IDs.
     * These are used throughout the application for menu items,
     * keyboard shortcuts, and toolbar buttons.
     */
    enum
    {
        // File Operations (0x1000 - 0x10FF)
        fileNew         = 0x1000,
        fileOpen        = 0x1001,
        fileSave        = 0x1002,
        fileSaveAs      = 0x1003,
        fileClose       = 0x1004,
        fileProperties  = 0x1005,
        fileExit        = 0x1006,
        filePreferences = 0x1007,  // Cmd+, - Preferences/Settings dialog
        fileEditBWFMetadata = 0x1008,  // Edit BWF metadata dialog
        fileEditiXMLMetadata = 0x1009,  // Edit iXML/SoundMiner metadata dialog

        // Edit Operations (0x2000 - 0x20FF)
        editUndo        = 0x2000,
        editRedo        = 0x2001,
        editCut         = 0x2002,
        editCopy        = 0x2003,
        editPaste       = 0x2004,
        editDelete      = 0x2005,
        editSelectAll   = 0x2006,
        editSilence     = 0x2007,  // Ctrl+L - Fill selection with silence
        editTrim        = 0x2008,  // Ctrl+T - Delete everything OUTSIDE selection

        // Playback Operations (0x3000 - 0x30FF)
        playbackPlay    = 0x3000,
        playbackPause   = 0x3001,
        playbackStop    = 0x3002,
        playbackLoop    = 0x3003,
        playbackRecord  = 0x3004, // Phase 2+
        playbackLoopRegion = 0x3005,  // Phase 4 - Loop selected region (Cmd+Shift+L)

        // View Operations (0x4000 - 0x40FF)
        viewZoomIn      = 0x4000,
        viewZoomOut     = 0x4001,
        viewZoomFit     = 0x4002,
        viewZoomSelection = 0x4003,
        viewZoomOneToOne  = 0x4004,
        viewCycleTimeFormat = 0x4005,  // Phase 3.5 - Cycle through time formats
        viewAutoScroll    = 0x4006,  // Phase 3 Tier 2 - Auto-scroll during playback (Cmd+Shift+F)
        viewZoomToRegion  = 0x4007,  // Phase 3.3 - Zoom to fit selected region with margins
        viewAutoPreviewRegions = 0x4008,  // Phase 3.4 - Auto-play regions when selected
        viewToggleRegions = 0x4009,  // Phase 4 - Toggle region visibility (Cmd+Shift+H)
        viewSpectrumAnalyzer = 0x400A,  // Cmd+Alt+S - Show/hide Spectrum Analyzer
        viewSpectrumFFTSize512 = 0x400B,  // Set FFT size to 512
        viewSpectrumFFTSize1024 = 0x400C,  // Set FFT size to 1024
        viewSpectrumFFTSize2048 = 0x400D,  // Set FFT size to 2048 (default)
        viewSpectrumFFTSize4096 = 0x400E,  // Set FFT size to 4096
        viewSpectrumFFTSize8192 = 0x400F,  // Set FFT size to 8192
        viewSpectrumWindowHann = 0x4010,  // Set window function to Hann (default)
        viewSpectrumWindowHamming = 0x4011,  // Set window function to Hamming
        viewSpectrumWindowBlackman = 0x4012,  // Set window function to Blackman
        viewSpectrumWindowRectangular = 0x4013,  // Set window function to Rectangular

        // Processing Operations (0x5000 - 0x50FF)
        processFadeIn   = 0x5000,
        processFadeOut  = 0x5001,
        processNormalize = 0x5002,
        processDCOffset = 0x5003,
        processGain     = 0x5004,  // Menu: Process → Gain... (precise gain entry)
        processIncreaseGain = 0x5005,  // Shift+Up (increase by +1 dB)
        processDecreaseGain = 0x5006,  // Shift+Down (decrease by -1 dB)
        processParametricEQ = 0x5007,  // Shift+E - 3-band Parametric EQ dialog
        processGraphicalEQ = 0x5008,  // Cmd+E - Graphical Parametric EQ editor

        // Navigation Operations (0x6000 - 0x60FF)
        navigateLeft         = 0x6000,  // Arrow left (uses current snap increment)
        navigateRight        = 0x6001,  // Arrow right (uses current snap increment)
        navigateStart        = 0x6004,  // Ctrl+Left (jump to start)
        navigateEnd          = 0x6005,  // Ctrl+Right (jump to end)
        navigatePageLeft     = 0x6006,  // Page Up (1s default)
        navigatePageRight    = 0x6007,  // Page Down
        navigateHomeVisible  = 0x6008,  // Home (first visible sample)
        navigateEndVisible   = 0x6009,  // End (last visible sample)
        navigateCenterView   = 0x600A,  // Period (.) - center on cursor/selection
        navigateGoToPosition = 0x600B,  // Cmd+G - Go To Position dialog

        // Selection Operations (0x7000 - 0x70FF)
        selectExtendLeft     = 0x7000,  // Shift+Left Arrow - extend selection left
        selectExtendRight    = 0x7001,  // Shift+Right Arrow - extend selection right
        selectExtendStart    = 0x7002,  // Shift+Home - extend to first visible
        selectExtendEnd      = 0x7003,  // Shift+End - extend to last visible
        selectExtendPageLeft = 0x7004,  // Shift+Page Up - extend selection left by page
        selectExtendPageRight= 0x7005,  // Shift+Page Down - extend selection right by page

        // Snap Operations (0x8000 - 0x80FF)
        snapCycleMode        = 0x8000,  // G key - toggle snap on/off (maintains last increment)
        snapToggleZeroCrossing = 0x8001, // Z key - quick toggle zero crossing
        snapPreferences      = 0x8002,  // Shift+G - open snap preferences

        // Help Operations (0x9000 - 0x90FF)
        helpAbout       = 0x9000,
        helpShortcuts   = 0x9001,

        // Tab Operations (0xA000 - 0xA0FF) - Phase 3 Multi-file support
        tabClose        = 0xA000,  // Cmd+W - Close current tab
        tabCloseAll     = 0xA001,  // Cmd+Shift+W - Close all tabs
        tabNext         = 0xA002,  // Ctrl+Tab - Next tab (moved from Cmd+Tab to avoid macOS App Switcher conflict)
        tabPrevious     = 0xA003,  // Ctrl+Shift+Tab - Previous tab (moved from Cmd+Shift+Tab to avoid macOS App Switcher conflict)
        tabSelect1      = 0xA004,  // Cmd+1 - Jump to tab 1
        tabSelect2      = 0xA005,  // Cmd+2 - Jump to tab 2
        tabSelect3      = 0xA006,  // Cmd+3 - Jump to tab 3
        tabSelect4      = 0xA007,  // Cmd+4 - Jump to tab 4
        tabSelect5      = 0xA008,  // Cmd+5 - Jump to tab 5
        tabSelect6      = 0xA009,  // Cmd+6 - Jump to tab 6
        tabSelect7      = 0xA00A,  // Cmd+7 - Jump to tab 7
        tabSelect8      = 0xA00B,  // Cmd+8 - Jump to tab 8
        tabSelect9      = 0xA00C,  // Cmd+9 - Jump to tab 9

        // Region Operations (0xB000 - 0xB0FF) - Phase 3 Tier 2
        regionAdd       = 0xB000,  // R - Create region from selection
        regionDelete    = 0xB001,  // Cmd+Shift+Delete - Delete region under cursor
        regionNext      = 0xB002,  // ] - Jump to and select next region
        regionPrevious  = 0xB003,  // [ - Jump to and select previous region
        regionSelectInverse = 0xB004,  // Cmd+Shift+I - Select everything NOT in regions
        regionSelectAll = 0xB005,  // Cmd+Shift+A - Select union of all regions
        regionStripSilence = 0xB006,  // Cmd+Shift+R - Auto-create regions from non-silent sections (Strip Silence)
        regionExportAll = 0xB007,  // Cmd+Shift+E - Export each region as separate file (Batch Export)
        regionShowList  = 0xB008,  // Cmd+Shift+M - Show/hide Region List Panel
        regionSnapToZeroCrossing = 0xB009,  // Phase 3.3 - Toggle region zero-crossing snap preference
        regionNudgeStartLeft = 0xB00A,  // Cmd+Shift+Left - Nudge region start boundary left by snap increment
        regionNudgeStartRight = 0xB00B,  // Cmd+Shift+Right - Nudge region start boundary right by snap increment
        regionNudgeEndLeft = 0xB00C,  // Shift+Alt+Left - Nudge region end boundary left by snap increment
        regionNudgeEndRight = 0xB00D,  // Shift+Alt+Right - Nudge region end boundary right by snap increment
        regionBatchRename = 0xB00E,  // Phase 3.4 - Batch rename multiple selected regions
        regionMerge = 0xB00F,  // Cmd+J - Merge selected adjacent regions
        regionSplit = 0xB010,  // Cmd+K - Split region at cursor position (moved from Cmd+R to avoid conflict with playbackRecord)
        regionCopy = 0xB011,  // Cmd+Shift+C - Copy region definitions to clipboard
        regionPaste = 0xB012,  // Cmd+Shift+V - Paste regions at cursor position

        // Marker Operations (0xC000 - 0xC0FF) - Phase 3.4
        markerAdd       = 0xC000,  // M - Add marker at cursor position
        markerDelete    = 0xC001,  // Cmd+Shift+Delete (when marker selected) - Delete selected marker
        markerNext      = 0xC002,  // Shift+] - Jump to next marker
        markerPrevious  = 0xC003,  // Shift+[ - Jump to previous marker
        markerShowList  = 0xC004   // Cmd+Shift+K - Show/hide Marker List Panel (moved from Cmd+M to avoid macOS Minimize Window conflict)
    };
}
