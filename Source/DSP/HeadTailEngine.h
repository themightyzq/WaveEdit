/*
  ==============================================================================

    HeadTailEngine.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Head & Tail processing engine for trimming silence, adding padding,
    and applying fades to audio files. Pure DSP -- no UI, no undo.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "HeadTailRecipe.h"

class HeadTailEngine
{
public:
    /**
     * Process an audio buffer according to a HeadTailRecipe.
     *
     * Pipeline order:
     * 1. Silence detection (if enabled)
     * 2. Head/tail trim (fixed and/or detected)
     * 3. Silence padding (prepend/append)
     * 4. Fade in/out
     *
     * @param input     Source audio buffer (not modified)
     * @param sampleRate Sample rate of the audio
     * @param recipe    Processing recipe
     * @param output    Receives the processed audio buffer
     * @return HeadTailReport with processing details
     */
    static HeadTailReport process(const juce::AudioBuffer<float>& input,
                                   double sampleRate,
                                   const HeadTailRecipe& recipe,
                                   juce::AudioBuffer<float>& output);

    /**
     * Detect content boundaries in a buffer using the recipe's detection settings.
     *
     * @param buffer     Audio buffer to analyze
     * @param sampleRate Sample rate
     * @param recipe     Recipe with detection parameters
     * @return Pair of (startSample, endSample) marking content boundaries
     */
    static std::pair<int64_t, int64_t> detectBoundaries(
        const juce::AudioBuffer<float>& buffer,
        double sampleRate,
        const HeadTailRecipe& recipe);

private:
    /**
     * Find the first and last non-silent samples in a buffer.
     *
     * @param buffer      Audio buffer to scan
     * @param sampleRate  Sample rate
     * @param thresholdDB Silence threshold in dB
     * @param mode        Peak or RMS detection
     * @param holdTimeMs  Minimum sustained signal duration to confirm detection
     * @return Pair of (firstNonSilent, lastNonSilent) sample indices,
     *         or (-1, -1) if all silent
     */
    static std::pair<int64_t, int64_t> findSilenceBoundaries(
        const juce::AudioBuffer<float>& buffer,
        double sampleRate,
        float thresholdDB,
        HeadTailRecipe::DetectionMode mode,
        float holdTimeMs);

    /** Convert milliseconds to sample count. */
    static int64_t msToSamples(float ms, double sampleRate)
    {
        return static_cast<int64_t>(ms * sampleRate / 1000.0);
    }
};
