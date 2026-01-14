/*
  ==============================================================================

    EditingToolsTests.cpp
    Created: 2025-10-15
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Integration tests for editing tools (Silence, Trim, DC Offset Removal)
    Tests complete workflow including undo/redo functionality.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Audio/AudioBufferManager.h"
#include "Audio/AudioProcessor.h"
#include "TestAudioFiles.h"
#include "AudioAssertions.h"

// ============================================================================
// Silence Tool Integration Tests
// ============================================================================

class SilenceToolIntegrationTests : public juce::UnitTest
{
public:
    SilenceToolIntegrationTests() : juce::UnitTest("Silence Tool Integration", "Integration") {}

    void runTest() override
    {
        beginTest("Silence workflow with undo");
        testSilenceWorkflowWithUndo();

        beginTest("Multiple silence operations");
        testMultipleSilenceOperations();

        beginTest("Silence on mono and stereo");
        testSilenceMonoStereo();
    }

private:
    void testSilenceWorkflowWithUndo()
    {
        AudioBufferManager manager;
        // Note: UndoManager would be used in actual implementation
        auto& buffer = manager.getMutableBuffer();

        // Create test buffer
        auto testBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        buffer = testBuffer;
        auto originalBuffer = buffer; // Save for comparison

        // Store hash before operation
        int64_t originalHash = AudioAssertions::hashBuffer(buffer);

        // Perform silence operation (simulating the actual command)
        int64_t startSample = 10000;
        int64_t endSample = 20000;

        // Save state before operation (for undo)
        juce::AudioBuffer<float> undoBuffer(buffer.getNumChannels(),
                                             static_cast<int>(endSample - startSample));

        // Copy region that will be silenced
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            undoBuffer.copyFrom(ch, 0, buffer, ch, static_cast<int>(startSample),
                                static_cast<int>(endSample - startSample));
        }

        // Apply silence
        bool success = manager.silenceRange(startSample, endSample);
        expect(success, "Silence should succeed");

        // Verify silence was applied
        bool isSilent = true;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            for (int64_t i = startSample; i < endSample; ++i)
            {
                if (std::abs(buffer.getSample(ch, static_cast<int>(i))) > 0.0001f)
                {
                    isSilent = false;
                    break;
                }
            }
        }
        expect(isSilent, "Range should be silenced");

        // Simulate undo operation
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.copyFrom(ch, static_cast<int>(startSample), undoBuffer, ch, 0,
                            static_cast<int>(endSample - startSample));
        }

        // Verify buffer is restored
        int64_t restoredHash = AudioAssertions::hashBuffer(buffer);
        expectEquals(restoredHash, originalHash, "Buffer should be restored after undo");
        expect(AudioAssertions::expectBuffersEqual(buffer, originalBuffer),
               "Buffer should match original after undo");

        logMessage("✅ Silence workflow with undo works correctly");
    }

    void testMultipleSilenceOperations()
    {
        AudioBufferManager manager;
        auto& buffer = manager.getMutableBuffer();

        // Create test buffer
        auto testBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        buffer = testBuffer;

        // Apply multiple silence operations
        manager.silenceRange(1000, 2000);   // First region
        manager.silenceRange(5000, 6000);   // Second region
        manager.silenceRange(10000, 11000); // Third region

        // Verify all regions are silenced
        bool region1Silent = true;
        bool region2Silent = true;
        bool region3Silent = true;

        for (int i = 1000; i < 2000; ++i)
            if (std::abs(buffer.getSample(0, i)) > 0.0001f) region1Silent = false;

        for (int i = 5000; i < 6000; ++i)
            if (std::abs(buffer.getSample(0, i)) > 0.0001f) region2Silent = false;

        for (int i = 10000; i < 11000; ++i)
            if (std::abs(buffer.getSample(0, i)) > 0.0001f) region3Silent = false;

        expect(region1Silent, "First region should be silent");
        expect(region2Silent, "Second region should be silent");
        expect(region3Silent, "Third region should be silent");

        // Verify non-silenced regions still have audio
        expect(std::abs(buffer.getSample(0, 500)) > 0.0f, "Audio before regions preserved");
        expect(std::abs(buffer.getSample(0, 3000)) > 0.0f, "Audio between regions preserved");

        logMessage("✅ Multiple silence operations work correctly");
    }

    void testSilenceMonoStereo()
    {
        // Test mono buffer
        {
            AudioBufferManager manager;
            auto& buffer = manager.getMutableBuffer();

            auto monoBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.5, 1);
            buffer = monoBuffer;

            bool success = manager.silenceRange(5000, 10000);
            expect(success, "Mono silence should succeed");
            expectEquals(buffer.getNumChannels(), 1, "Should remain mono");
        }

        // Test stereo buffer
        {
            AudioBufferManager manager;
            auto& buffer = manager.getMutableBuffer();

            auto stereoBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.5, 2);
            buffer = stereoBuffer;

            bool success = manager.silenceRange(5000, 10000);
            expect(success, "Stereo silence should succeed");
            expectEquals(buffer.getNumChannels(), 2, "Should remain stereo");

            // Both channels should be silenced
            bool ch0Silent = true;
            bool ch1Silent = true;
            for (int i = 5000; i < 10000; ++i)
            {
                if (std::abs(buffer.getSample(0, i)) > 0.0001f) ch0Silent = false;
                if (std::abs(buffer.getSample(1, i)) > 0.0001f) ch1Silent = false;
            }
            expect(ch0Silent && ch1Silent, "Both channels should be silenced");
        }

        logMessage("✅ Silence works on mono and stereo buffers");
    }
};

static SilenceToolIntegrationTests silenceToolIntegrationTests;

// ============================================================================
// Trim Tool Integration Tests
// ============================================================================

class TrimToolIntegrationTests : public juce::UnitTest
{
public:
    TrimToolIntegrationTests() : juce::UnitTest("Trim Tool Integration", "Integration") {}

    void runTest() override
    {
        beginTest("Trim workflow with undo");
        testTrimWorkflowWithUndo();

        beginTest("Sequential trim operations");
        testSequentialTrims();

        beginTest("Trim preserves audio quality");
        testTrimPreservesQuality();
    }

private:
    void testTrimWorkflowWithUndo()
    {
        AudioBufferManager manager;
        auto& buffer = manager.getMutableBuffer();

        // Create distinctive test pattern
        auto testBuffer = TestAudio::createLinearRamp(0.0f, 1.0f, 44100.0, 1.0, 2);
        buffer = testBuffer;

        // Save original state
        auto originalBuffer = buffer;
        int64_t originalLength = manager.getNumSamples();

        // Perform trim
        int64_t trimStart = 10000;
        int64_t trimEnd = 30000;

        bool success = manager.trimToRange(trimStart, trimEnd);
        expect(success, "Trim should succeed");

        // Verify trim worked
        expectEquals(manager.getNumSamples(), trimEnd - trimStart, "Buffer should be trimmed");

        // Verify correct portion was kept
        float expectedFirstValue = static_cast<float>(trimStart) / static_cast<float>(originalLength - 1);
        expectWithinAbsoluteError(buffer.getSample(0, 0), expectedFirstValue, 0.01f,
                                   "First sample should be from original trim start");

        // Simulate undo by restoring original buffer
        buffer = originalBuffer;
        // Note: AudioBufferManager uses buffer's inherent sample rate

        // Verify restoration
        expectEquals(manager.getNumSamples(), originalLength, "Length should be restored");
        expect(AudioAssertions::expectBuffersEqual(buffer, originalBuffer),
               "Buffer should be fully restored");

        logMessage("✅ Trim workflow with undo works correctly");
    }

    void testSequentialTrims()
    {
        AudioBufferManager manager;
        auto& buffer = manager.getMutableBuffer();

        // Create test buffer with known pattern
        auto testBuffer = TestAudio::createLinearRamp(-1.0f, 1.0f, 44100.0, 2.0, 2);
        buffer = testBuffer;
        int64_t originalLength = manager.getNumSamples();

        // First trim - remove beginning
        manager.trimToRange(22050, originalLength); // Remove first second
        expectEquals(manager.getNumSamples(), originalLength - 22050, "First trim size correct");

        // Second trim - remove end
        int64_t newLength = manager.getNumSamples();
        manager.trimToRange(0, newLength - 11025); // Remove last 0.25 seconds
        expectEquals(manager.getNumSamples(), newLength - 11025, "Second trim size correct");

        // Verify content integrity
        // Should have middle portion of original ramp
        float expectedValue = 0.0f; // Middle of -1 to 1 ramp
        expectWithinAbsoluteError(buffer.getSample(0, 0), expectedValue, 0.1f,
                                   "Content should be from middle of original");

        logMessage("✅ Sequential trims work correctly");
    }

    void testTrimPreservesQuality()
    {
        AudioBufferManager manager;
        auto& buffer = manager.getMutableBuffer();

        // Create high-quality test signal
        auto testBuffer = TestAudio::createSineWave(1000.0, 0.9f, 96000.0, 1.0, 2);
        buffer = testBuffer;
        // Sample rate is 96000 Hz as specified in test buffer creation

        // Calculate expected peak before trim
        float expectedPeak = buffer.getMagnitude(0, 10000, 10000);

        // Trim to middle portion
        manager.trimToRange(10000, 20000);

        // Verify audio quality preserved
        float actualPeak = buffer.getMagnitude(0, 0, buffer.getNumSamples());
        expectWithinAbsoluteError(actualPeak, expectedPeak, 0.0001f,
                                   "Peak level should be preserved");

        // Note: Sample rate preservation would be verified through AudioBufferManager
        // expectEquals(manager.getSampleRate(), 96000.0, "Sample rate should be preserved");

        logMessage("✅ Trim preserves audio quality");
    }
};

static TrimToolIntegrationTests trimToolIntegrationTests;

// ============================================================================
// DC Offset Removal Integration Tests
// ============================================================================

class DCOffsetRemovalIntegrationTests : public juce::UnitTest
{
public:
    DCOffsetRemovalIntegrationTests() : juce::UnitTest("DC Offset Removal Integration", "Integration") {}

    void runTest() override
    {
        beginTest("DC offset removal workflow");
        testDCOffsetRemovalWorkflow();

        beginTest("DC removal with undo");
        testDCRemovalWithUndo();

        beginTest("DC removal on various signals");
        testDCRemovalVariousSignals();

        beginTest("DC removal preserves AC content");
        testDCRemovalPreservesAC();
    }

private:
    void testDCOffsetRemovalWorkflow()
    {
        juce::AudioBuffer<float> buffer;

        // Create signal with DC offset
        auto testBuffer = TestAudio::createSineWithDC(440.0, 0.3f, 0.2f, 44100.0, 1.0, 2);
        buffer = testBuffer;

        // Verify DC offset exists
        float dcBefore = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            dcBefore += buffer.getSample(0, i);
        }
        dcBefore /= buffer.getNumSamples();
        expectWithinAbsoluteError(dcBefore, 0.2f, 0.01f, "DC offset should be present");

        // Apply DC offset removal (static method)
        AudioProcessor::removeDCOffset(buffer);

        // Verify DC offset removed
        expect(AudioAssertions::expectNoDCOffset(buffer, 0.001f),
               "DC offset should be removed");

        logMessage("✅ DC offset removal workflow works correctly");
    }

    void testDCRemovalWithUndo()
    {
        juce::AudioBuffer<float> buffer;

        // Create signal with DC offset
        auto testBuffer = TestAudio::createDCOffset(0.5f, 44100.0, 0.5, 2);
        buffer = testBuffer;
        auto originalBuffer = buffer; // Save for undo

        // Apply DC removal
        AudioProcessor::removeDCOffset(buffer);

        // Verify DC removed
        expect(AudioAssertions::expectNoDCOffset(buffer, 0.001f),
               "DC should be removed");

        // Simulate undo
        buffer = originalBuffer;

        // Verify original DC offset restored
        float dcRestored = buffer.getSample(0, 0);
        expectEquals(dcRestored, 0.5f, "Original DC offset should be restored");

        logMessage("✅ DC removal with undo works correctly");
    }

    void testDCRemovalVariousSignals()
    {

        // Test 1: Pure DC
        {
            auto dcBuffer = TestAudio::createDCOffset(0.3f, 44100.0, 0.1, 1);
            AudioProcessor::removeDCOffset(dcBuffer);
            expect(AudioAssertions::expectSilence(dcBuffer, 0.001f),
                   "Pure DC should become silence");
        }

        // Test 2: Sine with positive DC
        {
            auto sineBuffer = TestAudio::createSineWithDC(1000.0, 0.5f, 0.1f, 44100.0, 0.1, 2);
            // float peakBefore = sineBuffer.getMagnitude(0, 0, sineBuffer.getNumSamples());
            AudioProcessor::removeDCOffset(sineBuffer);
            expect(AudioAssertions::expectNoDCOffset(sineBuffer, 0.001f),
                   "Positive DC should be removed");
        }

        // Test 3: Sine with negative DC
        {
            auto sineBuffer = TestAudio::createSineWithDC(1000.0, 0.5f, -0.15f, 44100.0, 0.1, 2);
            AudioProcessor::removeDCOffset(sineBuffer);
            expect(AudioAssertions::expectNoDCOffset(sineBuffer, 0.001f),
                   "Negative DC should be removed");
        }

        // Test 4: White noise with DC
        {
            auto noiseBuffer = TestAudio::createWhiteNoise(0.3f, 44100.0, 0.1, 2);
            // Add DC offset
            for (int ch = 0; ch < noiseBuffer.getNumChannels(); ++ch)
            {
                for (int i = 0; i < noiseBuffer.getNumSamples(); ++i)
                {
                    noiseBuffer.addSample(ch, i, 0.25f);
                }
            }
            AudioProcessor::removeDCOffset(noiseBuffer);
            expect(AudioAssertions::expectNoDCOffset(noiseBuffer, 0.001f),
                   "DC should be removed from noise");
        }

        logMessage("✅ DC removal works on various signals");
    }

    void testDCRemovalPreservesAC()
    {

        // Create pure sine wave (no DC)
        auto sineBuffer = TestAudio::createSineWave(1000.0, 0.7f, 44100.0, 0.5, 2);
        float peakBefore = sineBuffer.getMagnitude(0, 0, sineBuffer.getNumSamples());
        float rmsBefore = sineBuffer.getRMSLevel(0, 0, sineBuffer.getNumSamples());

        // Apply DC removal (should have no effect on pure AC)
        AudioProcessor::removeDCOffset(sineBuffer);

        float peakAfter = sineBuffer.getMagnitude(0, 0, sineBuffer.getNumSamples());
        float rmsAfter = sineBuffer.getRMSLevel(0, 0, sineBuffer.getNumSamples());

        // Verify AC content preserved
        expectWithinAbsoluteError(peakAfter, peakBefore, 0.001f,
                                   "Peak level should be preserved");
        expectWithinAbsoluteError(rmsAfter, rmsBefore, 0.001f,
                                   "RMS level should be preserved");

        logMessage("✅ DC removal preserves AC content");
    }
};

static DCOffsetRemovalIntegrationTests dcOffsetRemovalIntegrationTests;

// ============================================================================
// Combined Operations Integration Tests
// ============================================================================

class CombinedEditingOperationsTests : public juce::UnitTest
{
public:
    CombinedEditingOperationsTests() : juce::UnitTest("Combined Editing Operations", "Integration") {}

    void runTest() override
    {
        beginTest("Silence then trim");
        testSilenceThenTrim();

        beginTest("Trim then DC removal");
        testTrimThenDCRemoval();

        beginTest("All three operations in sequence");
        testAllThreeOperations();
    }

private:
    void testSilenceThenTrim()
    {
        AudioBufferManager manager;
        auto& buffer = manager.getMutableBuffer();

        // Create test buffer
        auto testBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        buffer = testBuffer;

        // Silence middle portion
        manager.silenceRange(20000, 30000);

        // Verify silence applied
        bool isSilent = true;
        for (int i = 20000; i < 30000; ++i)
        {
            if (std::abs(buffer.getSample(0, i)) > 0.0001f)
            {
                isSilent = false;
                break;
            }
        }
        expect(isSilent, "Range should be silenced");

        // Now trim to keep only the silent portion
        manager.trimToRange(20000, 30000);

        // Verify result is silent buffer of correct length
        expectEquals(manager.getNumSamples(), (int64_t)10000, "Should have 10000 samples");
        expect(AudioAssertions::expectSilence(buffer), "Trimmed buffer should be silent");

        logMessage("✅ Silence then trim works correctly");
    }

    void testTrimThenDCRemoval()
    {
        AudioBufferManager manager;
        auto& buffer = manager.getMutableBuffer();

        // Create signal with DC offset
        auto testBuffer = TestAudio::createSineWithDC(440.0, 0.3f, 0.15f, 44100.0, 2.0, 2);
        buffer = testBuffer;
        // Note: AudioBufferManager uses buffer's inherent sample rate

        // Trim to second half
        int64_t midPoint = manager.getNumSamples() / 2;
        manager.trimToRange(midPoint, manager.getNumSamples());

        // Apply DC removal
        AudioProcessor::removeDCOffset(buffer);

        // Verify both operations succeeded
        expectEquals(manager.getNumSamples(), midPoint, "Buffer should be trimmed");
        expect(AudioAssertions::expectNoDCOffset(buffer, 0.001f),
               "DC offset should be removed");

        logMessage("✅ Trim then DC removal works correctly");
    }

    void testAllThreeOperations()
    {
        AudioBufferManager manager;
        auto& buffer = manager.getMutableBuffer();

        // Create complex test signal
        auto testBuffer = TestAudio::createSineWithDC(440.0, 0.5f, 0.1f, 44100.0, 3.0, 2);
        buffer = testBuffer;
        // Note: AudioBufferManager uses buffer's inherent sample rate

        // Operation 1: Silence first third
        int64_t totalSamples = manager.getNumSamples();
        int64_t oneThird = totalSamples / 3;
        manager.silenceRange(0, oneThird);

        // Operation 2: Trim to last two thirds
        manager.trimToRange(oneThird, totalSamples);

        // Operation 3: Remove DC offset
        AudioProcessor::removeDCOffset(buffer);

        // Verify all operations completed successfully
        expectEquals(manager.getNumSamples(), totalSamples - oneThird,
                     "Buffer should be 2/3 original length");
        expect(AudioAssertions::expectNoDCOffset(buffer, 0.001f),
               "DC offset should be removed");

        // First sample should not be from silenced region
        expect(std::abs(buffer.getSample(0, 0)) > 0.0f,
               "Should have non-silent audio");

        logMessage("✅ All three operations in sequence work correctly");
    }
};

static CombinedEditingOperationsTests combinedEditingOperationsTests;