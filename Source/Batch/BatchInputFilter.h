/*
  ==============================================================================

    BatchInputFilter.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include "../Audio/AudioFileManager.h"

namespace waveedit
{

/**
 * Wildcard filter for batch-processor input files: everything WaveEdit can
 * open, per AudioFileManager (the single source of truth -- includes AIFF
 * everywhere and *.m4a on macOS).
 */
inline juce::String batchInputWildcard()
{
    return AudioFileManager::getSupportedExtensions();
}

/** True if the file's extension is accepted as batch-processor input. */
inline bool isBatchInputFile(const juce::File& file)
{
    return AudioFileManager::isSupportedAudioFile(file);
}

} // namespace waveedit
