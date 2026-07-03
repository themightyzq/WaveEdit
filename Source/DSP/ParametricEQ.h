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
 * Thread Safety (C2 fix):
 * - setParameters() is message-thread only: it builds the IIR coefficients
 *   (which allocates) and publishes them into a pending slot under a lock.
 * - applyEQ() is real-time safe: it try-locks, copies pending coefficient
 *   VALUES element-wise into the live filter state (no allocation), and
 *   processes. On lock contention the block is skipped (inaudible).
 * - Must call prepare() (message thread) before first use.
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
     * Publish new EQ parameters (message thread ONLY -- allocates).
     *
     * Builds the three biquad coefficient sets off the audio thread and
     * stages them for the next applyEQ() call to adopt without allocating.
     *
     * @param params EQ parameters to apply
     */
    void setParameters(const Parameters& params);

    /**
     * Apply parametric EQ to an audio buffer (in-place processing).
     *
     * Real-time safe: no heap allocation, try-lock only (skips the block on
     * contention with setParameters()). Buffers with more than MAX_CHANNELS
     * channels are passed through untouched.
     *
     * @param buffer Audio buffer to process (will be modified)
     */
    void applyEQ(juce::AudioBuffer<float>& buffer);

    /** Maximum channels supported (matches AudioEngine / ChannelLayout). */
    static constexpr int MAX_CHANNELS = 8;

    /**
     * Get the current sample rate.
     *
     * @return Sample rate in Hz, or 0.0 if prepare() hasn't been called
     */
    double getSampleRate() const { return m_sampleRate; }

private:
    /**
     * Build the three pending coefficient sets from parameters.
     * Message thread only (allocates). Caller must NOT hold m_lock.
     *
     * @param params EQ parameters
     */
    void buildPendingCoefficients(const Parameters& params);

    /** Copy pending coefficient VALUES into the live filter states.
        Audio thread; caller holds m_lock; performs no allocation. */
    void adoptPendingCoefficients();

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

    // IIR filters for each band (stereo processing)
    using IIRFilter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;

    // Dual-mono processing (left and right channels)
    juce::dsp::ProcessorDuplicator<IIRFilter, Coefficients> m_lowShelf;
    juce::dsp::ProcessorDuplicator<IIRFilter, Coefficients> m_midPeak;
    juce::dsp::ProcessorDuplicator<IIRFilter, Coefficients> m_highShelf;

    // Cross-thread coefficient hand-off (C2). The message thread builds
    // pending coefficient objects in setParameters(); the audio thread adopts
    // their VALUES under a try-lock in applyEQ(). The Ptrs themselves are only
    // ever assigned/released on the message thread.
    juce::CriticalSection m_lock;
    Coefficients::Ptr m_pendingLow, m_pendingMid, m_pendingHigh;
    bool m_pendingReady = false;
    bool m_hasCoefficients = false;   // States initialised with real values yet?

    // Cached parameters to short-circuit redundant setParameters() calls
    // (message thread only).
    Parameters m_currentParams;
    bool m_haveParams = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParametricEQ)
};
