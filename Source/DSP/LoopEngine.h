/*
  ==============================================================================

    LoopEngine.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Core loop creation engine using zero-crossing search and crossfade
    blending to produce seamless audio loops. Pure DSP -- no UI, no undo.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "LoopRecipe.h"
#include <vector>

class LoopEngine
{
public:
    /**
     * Create a seamless loop from a region of the source buffer.
     *
     * The engine extracts the selection, optionally refines boundaries to
     * zero-crossings, then crossfades the tail into the head so the loop
     * point is smooth.
     *
     * @param sourceBuffer  Source audio data
     * @param sampleRate    Sample rate in Hz
     * @param startSample   Selection start (inclusive)
     * @param endSample     Selection end (exclusive)
     * @param recipe        Crossfade and zero-crossing settings
     * @return LoopResult with the processed loop buffer and diagnostics
     */
    static LoopResult createLoop(
        const juce::AudioBuffer<float>& sourceBuffer,
        double sampleRate,
        int64_t startSample,
        int64_t endSample,
        const LoopRecipe& recipe);

    /**
     * Create multiple loop variations from a single selection by applying
     * configurable offsets (and optional shuffle) to the start/end points.
     *
     * @param sourceBuffer  Source audio data
     * @param sampleRate    Sample rate in Hz
     * @param startSample   Base selection start
     * @param endSample     Base selection end
     * @param recipe        Settings including loopCount and offsetStepMs
     * @return Vector of LoopResult, one per variation
     */
    static std::vector<LoopResult> createVariations(
        const juce::AudioBuffer<float>& sourceBuffer,
        double sampleRate,
        int64_t startSample,
        int64_t endSample,
        const LoopRecipe& recipe);

    /**
     * Create a Shepard tone loop — an auditory illusion of continuously
     * rising (or falling) pitch using two crossfaded pitch-ramped layers.
     *
     * Uses resampling-based pitch shifting (works well for 0.5-2 semitones
     * on tonal/ambient content).
     */
    static LoopResult createShepardLoop(
        const juce::AudioBuffer<float>& sourceBuffer,
        double sampleRate,
        int64_t startSample,
        int64_t endSample,
        const LoopRecipe& recipe);

    /**
     * Measure the sample discontinuity at the loop point of a buffer.
     * Returns the maximum absolute difference between the last and first
     * sample across all channels.
     */
    static float measureDiscontinuity(const juce::AudioBuffer<float>& buffer);

    /**
     * Compute crossfade gain pair for a given position t in [0, 1].
     * Public for testability.
     *
     * @param t      Normalised position (0 = start of crossfade, 1 = end)
     * @param curve  Crossfade curve type
     * @return Pair of (fadeOut gain, fadeIn gain)
     */
    static std::pair<float, float> getCrossfadeGains(
        float t, LoopRecipe::CrossfadeCurve curve);

private:
    /**
     * Search for the nearest zero-crossing (minimum amplitude) around
     * targetSample within +/- searchRadius.
     */
    static int64_t refineToZeroCrossing(
        const juce::AudioBuffer<float>& buffer,
        int64_t targetSample,
        int searchRadius);

    /**
     * Apply a crossfade blend: tail fades out while head fades in.
     * All three buffers must have the same channel count and sample count.
     */
    static void applyCrossfade(
        const juce::AudioBuffer<float>& tailRegion,
        const juce::AudioBuffer<float>& headRegion,
        juce::AudioBuffer<float>& output,
        LoopRecipe::CrossfadeCurve curve);

    /**
     * Linear interpolation sample read from buffer at a fractional position.
     */
    static float lerpSample(const juce::AudioBuffer<float>& buffer,
                            int channel, float position);

    /** Convert milliseconds to sample count. */
    static int64_t msToSamples(float ms, double sampleRate);
};
