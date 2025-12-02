/*
  ==============================================================================

    ParametricEQ.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

/**
 * 3-band Parametric EQ processor for professional audio editing.
 *
 * Architecture:
 * - Low Band: Shelf filter for boosting/cutting low frequencies
 * - Mid Band: Peaking filter for targeted frequency control
 * - High Band: Shelf filter for boosting/cutting high frequencies
 *
 * Each band has three parameters:
 * - Frequency: Center/cutoff frequency (Hz)
 * - Gain: Boost/cut amount (dB, -20 to +20)
 * - Q: Bandwidth/resonance (0.1 to 10.0)
 *
 * Thread Safety:
 * - applyEQ() is real-time safe (no allocations)
 * - Must call prepare() before first use
 * - Sample rate must be set via prepare()
 */
class ParametricEQ
{
public:
    /**
     * EQ band parameters.
     */
    struct BandParameters
    {
        float frequency = 1000.0f;  // Hz
        float gain = 0.0f;          // dB (-20 to +20)
        float q = 0.707f;           // Quality factor (0.1 to 10.0)
    };

    /**
     * Complete EQ configuration for all three bands.
     */
    struct Parameters
    {
        BandParameters low;   // Shelf filter (default: 100 Hz)
        BandParameters mid;   // Peak filter (default: 1000 Hz)
        BandParameters high;  // Shelf filter (default: 10000 Hz)

        /**
         * Create default neutral EQ parameters (0 dB gain on all bands).
         */
        static Parameters createNeutral();
    };

    ParametricEQ();
    ~ParametricEQ() = default;

    /**
     * Prepare the EQ processor for audio processing.
     *
     * MUST be called before first use of applyEQ().
     *
     * @param sampleRate The audio sample rate in Hz
     * @param maxSamplesPerBlock Maximum expected block size
     */
    void prepare(double sampleRate, int maxSamplesPerBlock);

    /**
     * Apply parametric EQ to an audio buffer (in-place processing).
     *
     * Thread Safety:
     * - This method is real-time safe (no heap allocations).
     * - MUST be called from a single thread only (not thread-safe for concurrent access).
     * - Coefficient updates are performed when parameters change.
     * - Frequency values are automatically clamped to valid range (20 Hz to Nyquist).
     *
     * @param buffer Audio buffer to process (will be modified)
     * @param params EQ parameters to apply
     */
    void applyEQ(juce::AudioBuffer<float>& buffer, const Parameters& params);

    /**
     * Get the current sample rate.
     *
     * @return Sample rate in Hz, or 0.0 if prepare() hasn't been called
     */
    double getSampleRate() const { return m_sampleRate; }

private:
    /**
     * Update IIR filter coefficients based on current parameters.
     *
     * @param params EQ parameters
     */
    void updateCoefficients(const Parameters& params);

    /**
     * Convert dB gain to linear gain factor.
     *
     * @param gainDB Gain in decibels
     * @return Linear gain factor
     */
    static float dbToLinearGain(float gainDB);

    // DSP state
    double m_sampleRate;
    int m_maxSamplesPerBlock;
    int m_lastNumChannels;  // Track channel count to avoid redundant prepare() calls

    // IIR filters for each band (stereo processing)
    using IIRFilter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;

    // Dual-mono processing (left and right channels)
    juce::dsp::ProcessorDuplicator<IIRFilter, Coefficients> m_lowShelf;
    juce::dsp::ProcessorDuplicator<IIRFilter, Coefficients> m_midPeak;
    juce::dsp::ProcessorDuplicator<IIRFilter, Coefficients> m_highShelf;

    // Cached parameters to avoid redundant coefficient updates
    Parameters m_currentParams;
    bool m_coefficientsNeedUpdate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParametricEQ)
};
