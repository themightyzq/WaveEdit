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
     *               Range: -60dB to +20dB (silent to 10x amplification)
     * @return true if successful, false if buffer is invalid
     *
     * Thread Safety: Safe to call from any thread (no shared state)
     * Performance: O(n) where n = buffer size * channels
     */
    static bool applyGain(juce::AudioBuffer<float>& buffer, float gainDB);

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

    //==============================================================================
    // Fade Operations

    /**
     * Applies a linear fade-in to an audio buffer.
     * Gradually increases volume from 0 to 1 over the buffer duration.
     *
     * @param buffer Audio buffer to process (modified in place)
     * @param numSamples Number of samples to fade (0 = entire buffer)
     * @return true if successful, false if buffer is invalid
     *
     * Thread Safety: Safe to call from any thread
     * Performance: O(n)
     */
    static bool fadeIn(juce::AudioBuffer<float>& buffer, int numSamples = 0);

    /**
     * Applies a linear fade-out to an audio buffer.
     * Gradually decreases volume from 1 to 0 over the buffer duration.
     *
     * @param buffer Audio buffer to process (modified in place)
     * @param numSamples Number of samples to fade (0 = entire buffer)
     * @return true if successful, false if buffer is invalid
     *
     * Thread Safety: Safe to call from any thread
     * Performance: O(n)
     */
    static bool fadeOut(juce::AudioBuffer<float>& buffer, int numSamples = 0);

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
