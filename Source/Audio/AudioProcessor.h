/*
  ==============================================================================

    AudioProcessor.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Centralized audio processing for DSP operations.
    All audio processing algorithms are collected here for maintainability.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>

/**
 * Fade curve types for fade in/out operations.
 * Provides different sonic characteristics for crossfades and automation.
 */
enum class FadeCurveType
{
    LINEAR,         // Constant rate of change (default, existing behavior)
    EXPONENTIAL,    // Slow start, fast end (x²)
    LOGARITHMIC,    // Fast start, slow end (1 - (1-x)²)
    S_CURVE         // Smooth start and end (smoothstep: 3x² - 2x³)
};

/**
 * Audio processing utilities for WaveEdit.
 *
 * This class provides static methods for common DSP operations:
 * - Gain adjustment
 * - Normalization
 * - Fade in/out
 * - DC offset removal
 *
 * All methods are thread-safe and operate on JUCE AudioBuffer objects.
 * Performance optimized for real-time processing.
 */
class AudioProcessor
{
public:
    //==============================================================================
    // Gain and Level Operations

    /**
     * Applies gain adjustment to an audio buffer.
     * Converts dB to linear gain and applies to all channels.
     *
     * @param buffer Audio buffer to process (modified in place)
     * @param gainDB Gain in decibels (negative = quieter, positive = louder)
     *               No hard limits (warns on extreme values < -100dB or > +40dB)
     * @return true if successful, false if buffer is invalid
     *
     * Thread Safety: Safe to call from any thread (no shared state)
     * Performance: O(n) where n = buffer size * channels
     */
    static bool applyGain(juce::AudioBuffer<float>& buffer, float gainDB);

    /**
     * Applies gain adjustment to a specific range in an audio buffer.
     * Converts dB to linear gain and applies to specified sample range across all channels.
     *
     * @param buffer Audio buffer to process (modified in place)
     * @param gainDB Gain in decibels (negative = quieter, positive = louder)
     * @param startSample Starting sample index (0-based)
     * @param numSamples Number of samples to process (or -1 for rest of buffer)
     * @return true if successful, false if buffer is invalid or range is out of bounds
     *
     * Thread Safety: Safe to call from any thread (no shared state)
     * Performance: O(n) where n = numSamples * channels
     */
    static bool applyGainToRange(juce::AudioBuffer<float>& buffer, float gainDB,
                                 int startSample, int numSamples = -1);

    /**
     * Normalizes audio buffer to a target peak level.
     * Finds peak across all channels and scales proportionally.
     *
     * @param buffer Audio buffer to process (modified in place)
     * @param targetDB Target peak level in dB (default: 0dB = maximum without clipping)
     *                 Range: -60dB to 0dB
     * @return true if successful, false if buffer is invalid or silent
     *
     * Thread Safety: Safe to call from any thread
     * Performance: O(2n) - one pass to find peak, one pass to apply gain
     */
    static bool normalize(juce::AudioBuffer<float>& buffer, float targetDB = 0.0f);

    /**
     * Gets the peak level of an audio buffer in dB.
     * Scans all channels and returns the maximum absolute value.
     *
     * @param buffer Audio buffer to analyze
     * @return Peak level in dB, or -INFINITY if buffer is empty/silent
     *
     * Thread Safety: Safe to call from any thread
     * Performance: O(n)
     */
    static float getPeakLevelDB(const juce::AudioBuffer<float>& buffer);

    /**
     * Calculate RMS (Root Mean Square) level of an audio buffer.
     *
     * RMS provides a measure of perceived loudness, unlike peak which measures
     * absolute maximum amplitude. RMS is calculated as sqrt(mean(x²)).
     *
     * @param buffer Audio buffer to analyze
     * @return RMS level in decibels (dBFS), or -INFINITY if buffer is silent
     *
     * Thread Safety: Safe to call from any thread
     * Performance: O(n) where n = total samples
     */
    static float getRMSLevelDB(const juce::AudioBuffer<float>& buffer);

    //==============================================================================
    // Fade Operations

    /**
     * Applies a fade-in to an audio buffer with selectable curve type.
     * Gradually increases volume from 0 to 1 over the buffer duration.
     *
     * @param buffer Audio buffer to process (modified in place)
     * @param numSamples Number of samples to fade (0 = entire buffer)
     * @param curve Fade curve type (default: LINEAR for backward compatibility)
     * @return true if successful, false if buffer is invalid
     *
     * Thread Safety: Safe to call from any thread
     * Performance: O(n) - efficient per-sample calculation, no allocations
     */
    static bool fadeIn(juce::AudioBuffer<float>& buffer, int numSamples = 0,
                      FadeCurveType curve = FadeCurveType::LINEAR);

    /**
     * Applies a fade-out to an audio buffer with selectable curve type.
     * Gradually decreases volume from 1 to 0 over the buffer duration.
     *
     * @param buffer Audio buffer to process (modified in place)
     * @param numSamples Number of samples to fade (0 = entire buffer)
     * @param curve Fade curve type (default: LINEAR for backward compatibility)
     * @return true if successful, false if buffer is invalid
     *
     * Thread Safety: Safe to call from any thread
     * Performance: O(n) - efficient per-sample calculation, no allocations
     */
    static bool fadeOut(juce::AudioBuffer<float>& buffer, int numSamples = 0,
                       FadeCurveType curve = FadeCurveType::LINEAR);

    //==============================================================================
    // DC Offset Operations

    /**
     * Removes DC offset from an audio buffer.
     * DC offset causes waveform asymmetry and affects dynamics processing.
     *
     * @param buffer Audio buffer to process (modified in place)
     * @return true if successful, false if buffer is invalid
     *
     * Thread Safety: Safe to call from any thread
     * Performance: O(2n) - one pass to measure offset, one pass to correct
     */
    static bool removeDCOffset(juce::AudioBuffer<float>& buffer);

    //==============================================================================
    // Utility Functions

    /**
     * Converts decibels to linear gain.
     *
     * @param db Gain in decibels
     * @return Linear gain factor (1.0 = unity gain)
     */
    static float dBToLinear(float db)
    {
        return std::pow(10.0f, db / 20.0f);
    }

    /**
     * Converts linear gain to decibels.
     *
     * @param linear Linear gain factor
     * @return Gain in decibels, or -INFINITY if linear <= 0
     */
    static float linearToDB(float linear)
    {
        if (linear <= 0.0f)
            return -std::numeric_limits<float>::infinity();
        return 20.0f * std::log10(linear);
    }

    /**
     * Clamps a buffer's samples to the valid range [-1.0, 1.0].
     * Prevents clipping by hard-limiting out-of-range samples.
     *
     * @param buffer Audio buffer to process (modified in place)
     * @return Number of samples that were clipped (for warning purposes)
     *
     * Thread Safety: Safe to call from any thread
     * Performance: O(n)
     */
    static int clampToValidRange(juce::AudioBuffer<float>& buffer);

private:
    // Private constructor - this is a utility class (static methods only)
    AudioProcessor() = delete;
    ~AudioProcessor() = delete;

    JUCE_DECLARE_NON_COPYABLE(AudioProcessor)
};
