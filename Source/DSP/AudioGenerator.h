/*
  ==============================================================================

    AudioGenerator.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

/**
 * Offline signal generator for the Generate menu: tones and noise.
 *
 * Stateful: the oscillator carries phase and the pink-noise filter carries
 * per-channel state, so successive calls join seamlessly (no click at buffer
 * boundaries). One-shot fills of an already-sized buffer -- not a real-time
 * audio-thread processor.
 *
 * Thread safety: message-thread only (called from controllers / dialog
 * preview). No locks, no atomics; not audio-thread safe.
 *
 * Usage:
 * @code
 *   AudioGenerator gen;
 *   gen.prepare(sampleRate);
 *   juce::AudioBuffer<float> buf(numChannels, numSamples);
 *   gen.generateTone(buf, AudioGenerator::Waveform::Sine, 1000.0, 0.25f);
 * @endcode
 */
class AudioGenerator
{
public:
    enum class Waveform
    {
        Sine,
        Square,
        Saw,
        Triangle
    };

    enum class NoiseType
    {
        White,
        Pink
    };

    AudioGenerator() = default;

    /** Sets the sample rate and clears oscillator/noise state. */
    void prepare(double sampleRate);

    /** Clears oscillator phase and per-channel pink-noise filter state. */
    void reset();

    /**
     * Fills every channel with an identical tone. Phase is carried across
     * calls so the waveform is continuous.
     *
     * @param buffer     Target (already sized to the desired length/channels)
     * @param waveform   Sine / Square / Saw / Triangle
     * @param frequencyHz Tone frequency in Hz
     * @param amplitude  Linear peak amplitude in [0, 1]
     */
    void generateTone(juce::AudioBuffer<float>& buffer, Waveform waveform,
                      double frequencyHz, float amplitude);

    /**
     * Fills each channel with independent (decorrelated) noise.
     *
     * @param buffer    Target (already sized)
     * @param noiseType White or Pink
     * @param amplitude Linear peak amplitude in [0, 1]
     */
    void generateNoise(juce::AudioBuffer<float>& buffer, NoiseType noiseType,
                       float amplitude);

    /**
     * Seeds the noise generator for reproducible output (tests). When set,
     * channel c uses seed (baseSeed + c). Without a seed, noise is seeded
     * from the system random at prepare().
     */
    void setNoiseSeed(juce::int64 baseSeed);

private:
    // Per-channel Paul Kellet pink-noise filter state (7 taps).
    struct PinkState
    {
        float b[7] = { 0, 0, 0, 0, 0, 0, 0 };
    };

    double m_sampleRate = 44100.0;
    double m_phase = 0.0;                 // tone phase in [0, 1)
    std::vector<PinkState> m_pinkState;   // one per channel

    bool m_hasSeed = false;
    juce::int64 m_noiseSeed = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioGenerator)
};
