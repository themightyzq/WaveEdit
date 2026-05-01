/*
  ==============================================================================

    RegionUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Umbrella header for region undo actions.

    This file used to contain every region undo action in WaveEdit
    (~960 lines). Per CLAUDE.md §7.5 it has been split per sub-domain:

      - RegionLifecycleUndoActions.h  — Add, Delete, Rename, BatchRename,
                                        ChangeRegionColor
      - RegionEditUndoActions.h       — Resize, Nudge, Merge (pair),
                                        MultiMerge, Split
      - RegionDerivationUndoActions.h — StripSilence, Retrospective,
                                        MarkersToRegions

    Existing call sites that include this umbrella keep working without
    change.

  ==============================================================================
*/

#pragma once

#include "RegionLifecycleUndoActions.h"
#include "RegionEditUndoActions.h"
#include "RegionDerivationUndoActions.h"
