/*
  ==============================================================================

    ProgressCallback.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <functional>
#include <juce_core/juce_core.h>

/**
 * Callback function type for reporting progress during long operations.
 *
 * @param progress Value from 0.0 to 1.0 representing completion percentage
 * @param status Optional status message to display
 * @return true to continue processing, false to request cancellation
 *
 * Thread Safety: This callback may be invoked from a background thread.
 * Implementations must be thread-safe if they update shared state.
 */
using ProgressCallback = std::function<bool(float progress, const juce::String& status)>;
