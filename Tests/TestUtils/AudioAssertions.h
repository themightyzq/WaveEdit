/*
  ==============================================================================

    AudioAssertions.h
    Created: 2025-10-15
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Sample-accurate assertion helpers for audio buffer testing.
    Provides floating-point comparison with appropriate tolerance.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace AudioAssertions
{
    // Default tolerance for floating-point audio comparisons
    // 32-bit float has ~7 decimal digits of precision
    constexpr float DEFAULT_SAMPLE_TOLERANCE = 0.0001f;  // -80 dB

    /**
     * Asserts two audio buffers are bit-identical (exact sample match)
     *
     * @param buffer1  First buffer to compare
     * @param buffer2  Second buffer to compare
     * @param message  Error message if assertion fails
     * @return true if buffers match exactly
     */
    inline bool expectBuffersEqual(const juce::AudioBuffer<float>& buffer1,
                                     const juce::AudioBuffer<float>& buffer2,
                                     const juce::String& message = "Buffers should be equal")
    {
        if (buffer1.getNumChannels() != buffer2.getNumChannels())
        {
            DBG("FAIL: " << message << " - Channel count mismatch: "
                << buffer1.getNumChannels() << " vs " << buffer2.getNumChannels());
            return false;
        }

        if (buffer1.getNumSamples() != buffer2.getNumSamples())
        {
            DBG("FAIL: " << message << " - Sample count mismatch: "
                << buffer1.getNumSamples() << " vs " << buffer2.getNumSamples());
            return false;
        }

        for (int channel = 0; channel < buffer1.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < buffer1.getNumSamples(); ++sample)
            {
                const float value1 = buffer1.getSample(channel, sample);
                const float value2 = buffer2.getSample(channel, sample);

                if (value1 != value2)
                {
                    DBG("FAIL: " << message << " - Sample mismatch at channel " << channel
                        << ", sample " << sample << ": " << value1 << " vs " << value2);
                    return false;
                }
            }
        }

        return true;
    }

    /**
     * Asserts two audio buffers are approximately equal within tolerance
     * Use this for DSP operations where floating-point error accumulates
     *
     * @param buffer1   First buffer to compare
     * @param buffer2   Second buffer to compare
     * @param tolerance Maximum allowed difference per sample
     * @param message   Error message if assertion fails
     * @return true if buffers match within tolerance
     */
    inline bool expectBuffersNearlyEqual(const juce::AudioBuffer<float>& buffer1,
                                          const juce::AudioBuffer<float>& buffer2,
                                          float tolerance = DEFAULT_SAMPLE_TOLERANCE,
                                          const juce::String& message = "Buffers should be nearly equal")
    {
        if (buffer1.getNumChannels() != buffer2.getNumChannels())
        {
            DBG("FAIL: " << message << " - Channel count mismatch");
            return false;
        }

        if (buffer1.getNumSamples() != buffer2.getNumSamples())
        {
            DBG("FAIL: " << message << " - Sample count mismatch");
            return false;
        }

        int mismatchCount = 0;
        float maxError = 0.0f;

        for (int channel = 0; channel < buffer1.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < buffer1.getNumSamples(); ++sample)
            {
                const float value1 = buffer1.getSample(channel, sample);
                const float value2 = buffer2.getSample(channel, sample);
                const float error = std::abs(value1 - value2);

                if (error > tolerance)
                {
                    if (mismatchCount == 0)  // Log first mismatch
                    {
                        DBG("FAIL: " << message << " - First mismatch at channel " << channel
                            << ", sample " << sample << ": " << value1 << " vs " << value2
                            << " (error: " << error << ", tolerance: " << tolerance << ")");
                    }
                    mismatchCount++;
                }

                maxError = std::max(maxError, error);
            }
        }

        if (mismatchCount > 0)
        {
            DBG("FAIL: " << message << " - " << mismatchCount << " samples exceeded tolerance. Max error: " << maxError);
            return false;
        }

        return true;
    }

    /**
     * Asserts buffer contains only silence (all samples are zero)
     *
     * @param buffer    Buffer to check
     * @param tolerance Maximum allowed absolute value
     * @param message   Error message if assertion fails
     * @return true if buffer is silent
     */
    inline bool expectSilence(const juce::AudioBuffer<float>& buffer,
                               float tolerance = DEFAULT_SAMPLE_TOLERANCE,
                               const juce::String& message = "Buffer should be silent")
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const float value = buffer.getSample(channel, sample);
                if (std::abs(value) > tolerance)
                {
                    DBG("FAIL: " << message << " - Non-silent sample at channel " << channel
                        << ", sample " << sample << ": " << value);
                    return false;
                }
            }
        }

        return true;
    }

    /**
     * Asserts buffer peak level matches expected value
     *
     * @param buffer    Buffer to analyze
     * @param expectedPeak Expected peak amplitude
     * @param tolerance Allowed error in peak detection
     * @param message   Error message if assertion fails
     * @return true if peak matches
     */
    inline bool expectPeakLevel(const juce::AudioBuffer<float>& buffer,
                                 float expectedPeak,
                                 float tolerance = DEFAULT_SAMPLE_TOLERANCE,
                                 const juce::String& message = "Peak level mismatch")
    {
        float actualPeak = 0.0f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const float channelPeak = buffer.getMagnitude(channel, 0, buffer.getNumSamples());
            actualPeak = std::max(actualPeak, channelPeak);
        }

        if (std::abs(actualPeak - expectedPeak) > tolerance)
        {
            DBG("FAIL: " << message << " - Expected peak: " << expectedPeak
                << ", Actual peak: " << actualPeak << " (error: " << std::abs(actualPeak - expectedPeak) << ")");
            return false;
        }

        return true;
    }

    /**
     * Asserts buffer RMS level matches expected value
     *
     * @param buffer    Buffer to analyze
     * @param expectedRMS Expected RMS amplitude
     * @param tolerance Allowed error in RMS calculation
     * @param message   Error message if assertion fails
     * @return true if RMS matches
     */
    inline bool expectRMSLevel(const juce::AudioBuffer<float>& buffer,
                                float expectedRMS,
                                float tolerance = DEFAULT_SAMPLE_TOLERANCE,
                                const juce::String& message = "RMS level mismatch")
    {
        float actualRMS = 0.0f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const float channelRMS = buffer.getRMSLevel(channel, 0, buffer.getNumSamples());
            actualRMS = std::max(actualRMS, channelRMS);
        }

        if (std::abs(actualRMS - expectedRMS) > tolerance)
        {
            DBG("FAIL: " << message << " - Expected RMS: " << expectedRMS
                << ", Actual RMS: " << actualRMS << " (error: " << std::abs(actualRMS - expectedRMS) << ")");
            return false;
        }

        return true;
    }

    /**
     * Asserts buffer has zero DC offset
     *
     * @param buffer    Buffer to analyze
     * @param tolerance Maximum allowed DC offset
     * @param message   Error message if assertion fails
     * @return true if DC offset is below tolerance
     */
    inline bool expectNoDCOffset(const juce::AudioBuffer<float>& buffer,
                                   float tolerance = DEFAULT_SAMPLE_TOLERANCE,
                                   const juce::String& message = "Buffer has DC offset")
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            double sum = 0.0;
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                sum += buffer.getSample(channel, sample);
            }

            const float dcOffset = static_cast<float>(sum / buffer.getNumSamples());

            if (std::abs(dcOffset) > tolerance)
            {
                DBG("FAIL: " << message << " - Channel " << channel
                    << " has DC offset: " << dcOffset);
                return false;
            }
        }

        return true;
    }

    /**
     * Computes hash of buffer contents for change detection
     * Useful for undo/redo testing - verify buffer state is restored exactly
     *
     * @param buffer Buffer to hash
     * @return 64-bit hash value
     */
    inline int64_t hashBuffer(const juce::AudioBuffer<float>& buffer)
    {
        int64_t hash = 0;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const float* data = buffer.getReadPointer(channel);
            const int numSamples = buffer.getNumSamples();

            for (int sample = 0; sample < numSamples; ++sample)
            {
                // Simple hash: XOR with rotated hash and sample bits
                union { float f; int32_t i; } u;
                u.f = data[sample];
                hash ^= (static_cast<int64_t>(u.i) << (sample % 32));
                hash = (hash << 1) | (hash >> 63);  // Rotate left
            }
        }

        return hash;
    }

    /**
     * Asserts gain change is bit-accurate
     * Verifies buffer2 = buffer1 * gainFactor
     *
     * @param original   Original buffer
     * @param processed  Buffer after gain applied
     * @param gainFactor Expected gain multiplier
     * @param tolerance  Allowed floating-point error
     * @param message    Error message if assertion fails
     * @return true if gain was applied correctly
     */
    inline bool expectGainApplied(const juce::AudioBuffer<float>& original,
                                    const juce::AudioBuffer<float>& processed,
                                    float gainFactor,
                                    float tolerance = DEFAULT_SAMPLE_TOLERANCE,
                                    const juce::String& message = "Gain not applied correctly")
    {
        if (original.getNumChannels() != processed.getNumChannels() ||
            original.getNumSamples() != processed.getNumSamples())
        {
            DBG("FAIL: " << message << " - Buffer size mismatch");
            return false;
        }

        for (int channel = 0; channel < original.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < original.getNumSamples(); ++sample)
            {
                const float expected = original.getSample(channel, sample) * gainFactor;
                const float actual = processed.getSample(channel, sample);
                const float error = std::abs(expected - actual);

                if (error > tolerance)
                {
                    DBG("FAIL: " << message << " - Sample mismatch at channel " << channel
                        << ", sample " << sample << ": expected " << expected
                        << ", got " << actual << " (error: " << error << ")");
                    return false;
                }
            }
        }

        return true;
    }

} // namespace AudioAssertions
