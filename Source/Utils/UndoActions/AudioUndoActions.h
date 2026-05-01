/*
  ==============================================================================

    AudioUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Umbrella header for audio-buffer undo actions.

    This file used to contain every audio undo action in WaveEdit (~1000
    lines). Per CLAUDE.md §7.5 it has been split per sub-domain:

      - LevelUndoActions.h     — Gain, Normalize, DC offset removal
      - RangeUndoActions.h     — Silence, Trim
      - TransformUndoActions.h — Reverse, Invert, Resample, Head/Tail

    Channel-shape actions (ConvertToStereo, SilenceChannels,
    ReplaceChannels) moved to ChannelUndoActions.h alongside
    ChannelConvertAction; include that header directly if you need
    them and not the rest.

    Existing call sites that include this umbrella keep working without
    change.

  ==============================================================================
*/

#pragma once

#include "LevelUndoActions.h"
#include "RangeUndoActions.h"
#include "TransformUndoActions.h"
#include "ChannelUndoActions.h"
