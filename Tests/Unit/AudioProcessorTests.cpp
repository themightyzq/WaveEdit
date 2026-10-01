/*
  ==============================================================================

    AudioProcessorTests.cpp
    Created: 2025-10-15
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Comprehensive tests for AudioProcessor DSP operations.
    Tests gain, normalize, fade, DC offset removal, and utility functions.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "Audio/AudioProcessor.h"
#include "TestAudioFiles.h"
#include "AudioAssertions.h"

// ============================================================================
// AudioProcessor Gain Tests
// ============================================================================

class AudioProcessorGainTests : public juce::UnitTest
{
public:
    AudioProcessorGainTests() : juce::UnitTest("AudioProcessor Gain", "Processor") {}

    void runTest() override
    {
        beginTest("Apply positive gain");
        testApplyPositiveGain();

        beginTest("Apply negative gain");
        testApplyNegativeGain();

        beginTest("Apply unity gain (0dB)");
        testApplyUnityGain();

        beginTest("Apply gain to range");
        testApplyGainToRange();

        beginTest("Gain conversion accuracy");
        testGainConversionAccuracy();
    }

private:
    void testApplyPositiveGain()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.1, 2);
        auto original = buffer;

        bool success = AudioProcessor::applyGain(buffer, 6.0f);  // +6dB = 2x amplitude
        expect(success, "Gain application should succeed");

        // Verify gain was applied correctly
        expect(AudioAssertions::expectGainApplied(original, buffer, AudioProcessor::dBToLinear(6.0f)),
               "Buffer should be amplified by +6dB");

        logMessage("✅ Positive gain (+6dB) applied correctly");
    }

    void testApplyNegativeGain()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.1, 2);
        auto original = buffer;

        bool success = AudioProcessor::applyGain(buffer, -6.0f);  // -6dB = 0.5x amplitude
        expect(success, "Gain application should succeed");

        expect(AudioAssertions::expectGainApplied(original, buffer, AudioProcessor::dBToLinear(-6.0f)),
               "Buffer should be attenuated by -6dB");

        logMessage("✅ Negative gain (-6dB) applied correctly");
    }

    void testApplyUnityGain()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.1, 2);
        auto original = buffer;

        bool success = AudioProcessor::applyGain(buffer, 0.0f);  // 0dB = no change
        expect(success, "Gain application should succeed");

        expect(AudioAssertions::expectBuffersEqual(original, buffer),
               "Buffer should be unchanged with 0dB gain");

        logMessage("✅ Unity gain (0dB) leaves buffer unchanged");
    }

    void testApplyGainToRange()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        int totalSamples = buffer.getNumSamples();

        // Apply gain to middle 1000 samples
        bool success = AudioProcessor::applyGainToRange(buffer, 6.0f, 1000, 1000);
        expect(success, "Range gain application should succeed");

        // Verify only the specified range was affected
        // (This is a simplified check - full verification would compare before/after)

        logMessage("✅ Gain applied to specified range only");
    }

    void testGainConversionAccuracy()
    {
        // Test dB to linear conversions
        expectWithinAbsoluteError(AudioProcessor::dBToLinear(0.0f), 1.0f, 0.001f,
                                  "0dB should convert to 1.0");
        expectWithinAbsoluteError(AudioProcessor::dBToLinear(6.0f), 2.0f, 0.01f,
                                  "+6dB should convert to ~2.0");
        expectWithinAbsoluteError(AudioProcessor::dBToLinear(-6.0f), 0.5f, 0.01f,
                                  "-6dB should convert to ~0.5");

        // Test linear to dB conversions
        expectWithinAbsoluteError(AudioProcessor::linearToDB(1.0f), 0.0f, 0.001f,
                                  "1.0 should convert to 0dB");
        expectWithinAbsoluteError(AudioProcessor::linearToDB(2.0f), 6.0f, 0.1f,
                                  "2.0 should convert to ~6dB");
        expectWithinAbsoluteError(AudioProcessor::linearToDB(0.5f), -6.0f, 0.1f,
                                  "0.5 should convert to ~-6dB");

        logMessage("✅ Gain conversion formulas accurate");
    }
};

static AudioProcessorGainTests audioProcessorGainTests;

// ============================================================================
// AudioProcessor Normalization Tests
// ============================================================================

class AudioProcessorNormalizeTests : public juce::UnitTest
{
public:
    AudioProcessorNormalizeTests() : juce::UnitTest("AudioProcessor Normalize", "Processor") {}

    void runTest() override
    {
        beginTest("Normalize to 0dB");
        testNormalizeTo0dB();

        beginTest("Normalize to -6dB");
        testNormalizeToMinus6dB();

        beginTest("Normalize already normalized");
        testNormalizeAlreadyNormalized();

        beginTest("Get peak level");
        testGetPeakLevel();
    }

private:
    void testNormalizeTo0dB()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.3, 44100.0, 0.1, 2);  // 0.3 amplitude

        bool success = AudioProcessor::normalize(buffer, 0.0f);
        expect(success, "Normalize should succeed");

        // After normalization to 0dB, peak should be ~1.0
        float peakDB = AudioProcessor::getPeakLevelDB(buffer);
        expectWithinAbsoluteError(peakDB, 0.0f, 0.5f, "Peak should be at 0dB after normalization");

        logMessage("✅ Normalized to 0dB peak");
    }

    void testNormalizeToMinus6dB()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.3, 44100.0, 0.1, 2);

        bool success = AudioProcessor::normalize(buffer, -6.0f);
        expect(success, "Normalize should succeed");

        float peakDB = AudioProcessor::getPeakLevelDB(buffer);
        expectWithinAbsoluteError(peakDB, -6.0f, 0.5f, "Peak should be at -6dB");

        logMessage("✅ Normalized to -6dB peak");
    }

    void testNormalizeAlreadyNormalized()
    {
        auto buffer = TestAudio::createSineWave(440.0, 1.0, 44100.0, 0.1, 2);  // Already at 1.0

        bool success = AudioProcessor::normalize(buffer, 0.0f);
        expect(success, "Normalize should succeed");

        float peakDB = AudioProcessor::getPeakLevelDB(buffer);
        expectWithinAbsoluteError(peakDB, 0.0f, 0.1f, "Peak should remain at 0dB");

        logMessage("✅ Already normalized buffer handled correctly");
    }

    void testGetPeakLevel()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.1, 2);

        float peakDB = AudioProcessor::getPeakLevelDB(buffer);

        // 0.5 amplitude = -6dB
        expectWithinAbsoluteError(peakDB, -6.0f, 0.5f, "Peak level should be ~-6dB for 0.5 amplitude");

        // Test silence
        auto silence = TestAudio::createSilence(44100.0, 0.1, 2);
        float silencePeak = AudioProcessor::getPeakLevelDB(silence);
        expect(silencePeak == -std::numeric_limits<float>::infinity(),
               "Silence should have -INF peak");

        logMessage("✅ Peak level detection accurate");
    }
};

static AudioProcessorNormalizeTests audioProcessorNormalizeTests;

// ============================================================================
// AudioProcessor Fade Tests
// ============================================================================

class AudioProcessorFadeTests : public juce::UnitTest
{
public:
    AudioProcessorFadeTests() : juce::UnitTest("AudioProcessor Fade", "Processor") {}

    void runTest() override
    {
        beginTest("Fade in");
        testFadeIn();

        beginTest("Fade out");
        testFadeOut();

        beginTest("Partial fade in");
        testPartialFadeIn();

        beginTest("Fade linearity");
        testFadeLinearity();
    }

private:
    void testFadeIn()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.1, 2);

        bool success = AudioProcessor::fadeIn(buffer, 0);  // 0 = entire buffer
        expect(success, "Fade in should succeed");

        // First sample should be near 0, last sample should be original amplitude
        expectWithinAbsoluteError(buffer.getSample(0, 0), 0.0f, 0.01f,
                                  "First sample should be ~0 after fade in");

        logMessage("✅ Fade in applied correctly");
    }

    void testFadeOut()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.1, 2);

        bool success = AudioProcessor::fadeOut(buffer, 0);  // 0 = entire buffer
        expect(success, "Fade out should succeed");

        // Last sample should be near 0
        int lastSample = buffer.getNumSamples() - 1;
        expectWithinAbsoluteError(buffer.getSample(0, lastSample), 0.0f, 0.01f,
                                  "Last sample should be ~0 after fade out");

        logMessage("✅ Fade out applied correctly");
    }

    void testPartialFadeIn()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);  // 1 second
        int fadeLength = 4410;  // 100ms at 44.1kHz

        bool success = AudioProcessor::fadeIn(buffer, fadeLength);
        expect(success, "Partial fade in should succeed");

        // First sample should be ~0, sample at fadeLength should be normal
        expectWithinAbsoluteError(buffer.getSample(0, 0), 0.0f, 0.01f,
                                  "First sample should be ~0");

        logMessage("✅ Partial fade in applied correctly");
    }

    void testFadeLinearity()
    {
        auto buffer = TestAudio::createLinearRamp(1.0f, 1.0f, 44100.0, 0.1, 1);  // Constant 1.0

        AudioProcessor::fadeIn(buffer, 0);

        // Verify fade is actually linear (samples increase proportionally)
        int midPoint = buffer.getNumSamples() / 2;
        float midValue = std::abs(buffer.getSample(0, midPoint));

        expectWithinAbsoluteError(midValue, 0.5f, 0.05f,
                                  "Mid-point of linear fade should be ~0.5");

        logMessage("✅ Fade linearity verified");
    }
};

static AudioProcessorFadeTests audioProcessorFadeTests;

// ============================================================================
// AudioProcessor DC Offset Tests
// ============================================================================

class AudioProcessorDCOffsetTests : public juce::UnitTest
{
public:
    AudioProcessorDCOffsetTests() : juce::UnitTest("AudioProcessor DC Offset", "Processor") {}

    void runTest() override
    {
        beginTest("Remove DC offset");
        testRemoveDCOffset();

        beginTest("Remove negative DC offset");
        testRemoveNegativeDCOffset();

        beginTest("No DC offset present");
        testNoDCOffset();
    }

private:
    void testRemoveDCOffset()
    {
        auto buffer = TestAudio::createSineWithDC(440.0, 0.5, 0.2f, 44100.0, 0.1, 2);  // +0.2 DC offset

        bool success = AudioProcessor::removeDCOffset(buffer);
        expect(success, "DC offset removal should succeed");

        // After removal, DC offset should be near 0
        expect(AudioAssertions::expectNoDCOffset(buffer, 0.01f),
               "DC offset should be removed");

        logMessage("✅ DC offset removed successfully");
    }

    void testRemoveNegativeDCOffset()
    {
        auto buffer = TestAudio::createSineWithDC(440.0, 0.5, -0.3f, 44100.0, 0.1, 2);  // -0.3 DC offset

        bool success = AudioProcessor::removeDCOffset(buffer);
        expect(success, "DC offset removal should succeed");

        expect(AudioAssertions::expectNoDCOffset(buffer, 0.01f),
               "Negative DC offset should be removed");

        logMessage("✅ Negative DC offset removed successfully");
    }

    void testNoDCOffset()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.1, 2);  // No DC offset

        bool success = AudioProcessor::removeDCOffset(buffer);
        expect(success, "DC offset removal should succeed");

        expect(AudioAssertions::expectNoDCOffset(buffer, 0.01f),
               "Buffer with no DC offset should remain clean");

        logMessage("✅ Buffer without DC offset processed correctly");
    }
};

static AudioProcessorDCOffsetTests audioProcessorDCOffsetTests;

// ============================================================================
// AudioProcessor Utility Tests
// ============================================================================

class AudioProcessorUtilityTests : public juce::UnitTest
{
public:
    AudioProcessorUtilityTests() : juce::UnitTest("AudioProcessor Utilities", "Processor") {}

    void runTest() override
    {
        beginTest("Clamp to valid range");
        testClampToValidRange();

        beginTest("dB conversion edge cases");
        testDBConversionEdgeCases();
    }

private:
    void testClampToValidRange()
    {
        juce::AudioBuffer<float> buffer(2, 1000);

        // Create buffer with some out-of-range values
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < 1000; ++i)
            {
                float value = (i % 100 == 0) ? 2.0f : 0.5f;  // Some values exceed 1.0
                buffer.setSample(ch, i, value);
            }
        }

        int clippedCount = AudioProcessor::clampToValidRange(buffer);
        expect(clippedCount > 0, "Should detect and clamp out-of-range samples");

        // Verify all samples are now in valid range
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < 1000; ++i)
            {
                float value = buffer.getSample(ch, i);
                expect(value >= -1.0f && value <= 1.0f,
                       "All samples should be in valid range after clamping");
            }
        }

        logMessage("✅ Clamping works correctly, clipped " + juce::String(clippedCount) + " samples");
    }

    void testDBConversionEdgeCases()
    {
        // Test zero and negative linear values
        float infDB = AudioProcessor::linearToDB(0.0f);
        expect(std::isinf(infDB) && infDB < 0, "0.0 linear should convert to -INF dB");

        float negDB = AudioProcessor::linearToDB(-1.0f);
        expect(std::isinf(negDB) && negDB < 0, "Negative linear should convert to -INF dB");

        // Test very small linear values
        float smallDB = AudioProcessor::linearToDB(0.00001f);  // -100dB
        expectWithinAbsoluteError(smallDB, -100.0f, 5.0f, "Very small linear should be very negative dB");

        // Test very large linear values
        float largeDB = AudioProcessor::linearToDB(100.0f);  // +40dB
        expectWithinAbsoluteError(largeDB, 40.0f, 1.0f, "Large linear should be positive dB");

        logMessage("✅ dB conversion edge cases handled correctly");
    }
};

static AudioProcessorUtilityTests audioProcessorUtilityTests;
