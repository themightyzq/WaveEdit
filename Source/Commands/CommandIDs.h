/*
  ==============================================================================

    CommandIDs.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

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

        // Edit Operations (0x2000 - 0x20FF)
        editUndo        = 0x2000,
        editRedo        = 0x2001,
        editCut         = 0x2002,
        editCopy        = 0x2003,
        editPaste       = 0x2004,
        editDelete      = 0x2005,
        editSelectAll   = 0x2006,

        // Playback Operations (0x3000 - 0x30FF)
        playbackPlay    = 0x3000,
        playbackPause   = 0x3001,
        playbackStop    = 0x3002,
        playbackLoop    = 0x3003,
        playbackRecord  = 0x3004, // Phase 2+

        // View Operations (0x4000 - 0x40FF)
        viewZoomIn      = 0x4000,
        viewZoomOut     = 0x4001,
        viewZoomFit     = 0x4002,
        viewZoomSelection = 0x4003,
        viewZoomOneToOne  = 0x4004,

        // Processing Operations (0x5000 - 0x50FF)
        processFadeIn   = 0x5000,
        processFadeOut  = 0x5001,
        processNormalize = 0x5002,
        processDCOffset = 0x5003,

        // Help Operations (0x9000 - 0x90FF)
        helpAbout       = 0x9000,
        helpShortcuts   = 0x9001
    };
}
