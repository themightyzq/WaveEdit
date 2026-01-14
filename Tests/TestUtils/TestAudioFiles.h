/*
  ==============================================================================

    TestAudioFiles.h
    Created: 2025-10-15
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Test audio data generators for automated testing.
    Provides various synthetic audio signals for comprehensive test coverage.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace TestAudio
{
    /**
     * Creates a sine wave test signal
     *
     * @param frequency      Frequency in Hz
     * @param amplitude      Peak amplitude (0.0 to 1.0)
     * @param sampleRate     Sample rate in Hz
     * @param durationSeconds Duration in seconds
     * @param numChannels    Number of channels (1=mono, 2=stereo)
     * @return AudioBuffer containing the generated sine wave
     */
    inline juce::AudioBuffer<float> createSineWave(double frequency,
                                                     float amplitude,
                                                     double sampleRate,
                                                     double durationSeconds,
                                                     int numChannels = 1)
    {
        const int numSamples = static_cast<int>(durationSeconds * sampleRate);
        juce::AudioBuffer<float> buffer(numChannels, numSamples);

        const double angleIncrement = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        double angle = 0.0;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float value = amplitude * static_cast<float>(std::sin(angle));

            for (int channel = 0; channel < numChannels; ++channel)
            {
                buffer.setSample(channel, sample, value);
            }

            angle += angleIncrement;
            if (angle >= juce::MathConstants<double>::twoPi)
                angle -= juce::MathConstants<double>::twoPi;
        }

        return buffer;
    }

    /**
     * Creates a square wave test signal
     * Useful for testing click/pop detection
     */
    inline juce::AudioBuffer<float> createSquareWave(double frequency,
                                                       float amplitude,
                                                       double sampleRate,
                                                       double durationSeconds,
                                                       int numChannels = 1)
    {
        const int numSamples = static_cast<int>(durationSeconds * sampleRate);
        juce::AudioBuffer<float> buffer(numChannels, numSamples);

        const int samplesPerHalfCycle = static_cast<int>(sampleRate / (2.0 * frequency));

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float value = ((sample / samplesPerHalfCycle) % 2 == 0) ? amplitude : -amplitude;

            for (int channel = 0; channel < numChannels; ++channel)
            {
                buffer.setSample(channel, sample, value);
            }
        }

        return buffer;
    }

    /**
     * Creates digital silence (all zeros)
     * Essential for testing silence detection and null operations
     */
    inline juce::AudioBuffer<float> createSilence(double sampleRate,
                                                    double durationSeconds,
                                                    int numChannels = 1)
    {
        const int numSamples = static_cast<int>(durationSeconds * sampleRate);
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        buffer.clear();
        return buffer;
    }

    /**
     * Creates white noise
     * Useful for testing RMS calculations and metering
     */
    inline juce::AudioBuffer<float> createWhiteNoise(float amplitude,
                                                       double sampleRate,
                                                       double durationSeconds,
                                                       int numChannels = 1,
                                                       int seed = 12345)
    {
        const int numSamples = static_cast<int>(durationSeconds * sampleRate);
        juce::AudioBuffer<float> buffer(numChannels, numSamples);

        juce::Random random(seed);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            for (int sample = 0; sample < numSamples; ++sample)
            {
                // Generate random value in range [-amplitude, +amplitude]
                const float value = amplitude * (2.0f * random.nextFloat() - 1.0f);
                buffer.setSample(channel, sample, value);
            }
        }

        return buffer;
    }

    /**
     * Creates a DC offset test signal
     * Useful for testing DC offset detection and removal
     */
    inline juce::AudioBuffer<float> createDCOffset(float dcValue,
                                                     double sampleRate,
                                                     double durationSeconds,
                                                     int numChannels = 1)
    {
        const int numSamples = static_cast<int>(durationSeconds * sampleRate);
        juce::AudioBuffer<float> buffer(numChannels, numSamples);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            for (int sample = 0; sample < numSamples; ++sample)
            {
                buffer.setSample(channel, sample, dcValue);
            }
        }

        return buffer;
    }

    /**
     * Creates a sine wave with DC offset
     * Useful for testing DC offset removal with real audio content
     */
    inline juce::AudioBuffer<float> createSineWithDC(double frequency,
                                                       float amplitude,
                                                       float dcOffset,
                                                       double sampleRate,
                                                       double durationSeconds,
                                                       int numChannels = 1)
    {
        auto buffer = createSineWave(frequency, amplitude, sampleRate, durationSeconds, numChannels);

        // Add DC offset to all samples
        for (int channel = 0; channel < numChannels; ++channel)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                buffer.addSample(channel, sample, dcOffset);
            }
        }

        return buffer;
    }

    /**
     * Creates an impulse (single non-zero sample)
     * Useful for testing impulse response and zero-crossing detection
     */
    inline juce::AudioBuffer<float> createImpulse(float amplitude,
                                                    int impulsePosition,
                                                    double sampleRate,
                                                    double durationSeconds,
                                                    int numChannels = 1)
    {
        const int numSamples = static_cast<int>(durationSeconds * sampleRate);
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        buffer.clear();

        if (impulsePosition >= 0 && impulsePosition < numSamples)
        {
            for (int channel = 0; channel < numChannels; ++channel)
            {
                buffer.setSample(channel, impulsePosition, amplitude);
            }
        }

        return buffer;
    }

    /**
     * Creates a linear ramp (useful for fade testing)
     * Ramps from startAmplitude to endAmplitude over duration
     */
    inline juce::AudioBuffer<float> createLinearRamp(float startAmplitude,
                                                       float endAmplitude,
                                                       double sampleRate,
                                                       double durationSeconds,
                                                       int numChannels = 1)
    {
        const int numSamples = static_cast<int>(durationSeconds * sampleRate);
        juce::AudioBuffer<float> buffer(numChannels, numSamples);

        const float amplitudeStep = (endAmplitude - startAmplitude) / static_cast<float>(numSamples - 1);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            for (int sample = 0; sample < numSamples; ++sample)
            {
                const float value = startAmplitude + amplitudeStep * static_cast<float>(sample);
                buffer.setSample(channel, sample, value);
            }
        }

        return buffer;
    }

} // namespace TestAudio
