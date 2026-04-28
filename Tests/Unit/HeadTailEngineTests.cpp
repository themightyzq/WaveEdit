/*
  ==============================================================================

    HeadTailEngineTests.cpp
    Created: 2025
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Unit tests for HeadTailEngine DSP operations.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "DSP/HeadTailEngine.h"
#include "DSP/HeadTailRecipe.h"
#include "TestAudioFiles.h"

class HeadTailEngineTests : public juce::UnitTest
{
public:
    HeadTailEngineTests() : juce::UnitTest("HeadTailEngine", "DSP") {}

    void runTest() override
    {
        beginTest("Detection - silence boundaries");
        testDetection();

        beginTest("Detection - with padding");
        testDetectionWithPadding();

        beginTest("Fixed head trim");
        testFixedHeadTrim();

        beginTest("Fixed tail trim");
        testFixedTailTrim();

        beginTest("Silence padding");
        testSilencePadding();

        beginTest("Fade application");
        testFadeApplication();

        beginTest("Combined recipe");
        testCombinedRecipe();

        beginTest("Short file trim");
        testShortFileTrim();

        beginTest("All silence buffer");
        testAllSilence();

        beginTest("Empty buffer");
        testEmptyBuffer();
    }

private:
    static constexpr double kSampleRate = 44100.0;

    //--------------------------------------------------------------------------
    // Helper: create buffer with silence + sine + silence
    //--------------------------------------------------------------------------
    juce::AudioBuffer<float> createSilenceSineSilence(
        double silenceBeforeMs, double sineDurationMs, double silenceAfterMs,
        float amplitude = 0.5f)
    {
        int silenceBefore = static_cast<int>(silenceBeforeMs * kSampleRate / 1000.0);
        int sineLen = static_cast<int>(sineDurationMs * kSampleRate / 1000.0);
        int silenceAfter = static_cast<int>(silenceAfterMs * kSampleRate / 1000.0);
        int total = silenceBefore + sineLen + silenceAfter;

        juce::AudioBuffer<float> buffer(1, total);
        buffer.clear();

        // Fill sine portion
        double freq = 440.0;
        double angleInc = juce::MathConstants<double>::twoPi * freq / kSampleRate;
        double angle = 0.0;
        for (int s = 0; s < sineLen; ++s)
        {
            buffer.setSample(0, silenceBefore + s,
                             amplitude * static_cast<float>(std::sin(angle)));
            angle += angleInc;
        }

        return buffer;
    }

    //--------------------------------------------------------------------------
    void testDetection()
    {
        // 500ms silence + 1s sine + 500ms silence
        auto buffer = createSilenceSineSilence(500.0, 1000.0, 500.0);

        HeadTailRecipe recipe;
        recipe.detectEnabled = true;
        recipe.thresholdDB = -40.0f;
        recipe.holdTimeMs = 1.0f;  // Short hold for precise detection

        juce::AudioBuffer<float> output;
        auto report = HeadTailEngine::process(buffer, kSampleRate, recipe, output);

        expect(report.success, "Processing should succeed");
        expect(report.detectionFound, "Detection should find content");

        // Start should be near 22050 (500ms * 44100)
        int64_t expectedStart = static_cast<int64_t>(500.0 * kSampleRate / 1000.0);
        expect(std::abs(report.detectedStartSample - expectedStart) < 500,
               "Detected start should be near 22050 (got "
               + juce::String(report.detectedStartSample) + ")");

        // End should be near 66150 (1500ms * 44100)
        int64_t expectedEnd = static_cast<int64_t>(1500.0 * kSampleRate / 1000.0);
        expect(std::abs(report.detectedEndSample - expectedEnd) < 500,
               "Detected end should be near 66150 (got "
               + juce::String(report.detectedEndSample) + ")");

        logMessage("Detection boundaries verified");
    }

    //--------------------------------------------------------------------------
    void testDetectionWithPadding()
    {
        auto buffer = createSilenceSineSilence(500.0, 1000.0, 500.0);

        HeadTailRecipe recipe;
        recipe.detectEnabled = true;
        recipe.thresholdDB = -40.0f;
        recipe.holdTimeMs = 1.0f;
        recipe.leadingPadMs = 50.0f;
        recipe.trailingPadMs = 50.0f;

        juce::AudioBuffer<float> output;
        auto report = HeadTailEngine::process(buffer, kSampleRate, recipe, output);

        expect(report.success, "Processing should succeed");

        // With 50ms padding on each side, the output should be longer than
        // just the detected content
        int64_t expectedMinLength = static_cast<int64_t>(1000.0 * kSampleRate / 1000.0);

        expect(output.getNumSamples() >= static_cast<int>(expectedMinLength),
               "Output should be at least as long as the sine content");
        expect(output.getNumSamples() > static_cast<int>(expectedMinLength),
               "Output should include padding beyond detected content");

        logMessage("Detection with padding verified");
    }

    //--------------------------------------------------------------------------
    void testFixedHeadTrim()
    {
        // 2s sine wave
        auto buffer = TestAudio::createSineWave(440.0, 0.5, kSampleRate, 2.0, 1);
        int originalLength = buffer.getNumSamples();

        HeadTailRecipe recipe;
        recipe.headTrimEnabled = true;
        recipe.headTrimMs = 100.0f;

        juce::AudioBuffer<float> output;
        auto report = HeadTailEngine::process(buffer, kSampleRate, recipe, output);

        expect(report.success, "Processing should succeed");

        int64_t trimSamples = static_cast<int64_t>(100.0 * kSampleRate / 1000.0);
        int expectedLength = originalLength - static_cast<int>(trimSamples);

        expect(output.getNumSamples() == expectedLength,
               "Output should be 100ms shorter (expected "
               + juce::String(expectedLength) + ", got "
               + juce::String(output.getNumSamples()) + ")");

        logMessage("Fixed head trim verified");
    }

    //--------------------------------------------------------------------------
    void testFixedTailTrim()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.5, kSampleRate, 2.0, 1);
        int originalLength = buffer.getNumSamples();

        HeadTailRecipe recipe;
        recipe.tailTrimEnabled = true;
        recipe.tailTrimMs = 100.0f;

        juce::AudioBuffer<float> output;
        auto report = HeadTailEngine::process(buffer, kSampleRate, recipe, output);

        expect(report.success, "Processing should succeed");

        int64_t trimSamples = static_cast<int64_t>(100.0 * kSampleRate / 1000.0);
        int expectedLength = originalLength - static_cast<int>(trimSamples);

        expect(output.getNumSamples() == expectedLength,
               "Output should be 100ms shorter (expected "
               + juce::String(expectedLength) + ", got "
               + juce::String(output.getNumSamples()) + ")");

        logMessage("Fixed tail trim verified");
    }

    //--------------------------------------------------------------------------
    void testSilencePadding()
    {
        // 1s sine wave
        auto buffer = TestAudio::createSineWave(440.0, 0.5, kSampleRate, 1.0, 1);

        HeadTailRecipe recipe;
        recipe.prependSilenceEnabled = true;
        recipe.prependSilenceMs = 50.0f;
        recipe.appendSilenceEnabled = true;
        recipe.appendSilenceMs = 100.0f;

        juce::AudioBuffer<float> output;
        auto report = HeadTailEngine::process(buffer, kSampleRate, recipe, output);

        expect(report.success, "Processing should succeed");

        int64_t prependSamples = static_cast<int64_t>(50.0 * kSampleRate / 1000.0);
        int64_t appendSamples = static_cast<int64_t>(100.0 * kSampleRate / 1000.0);
        int expectedLength = buffer.getNumSamples()
                           + static_cast<int>(prependSamples)
                           + static_cast<int>(appendSamples);

        expect(output.getNumSamples() == expectedLength,
               "Output length should include padding (expected "
               + juce::String(expectedLength) + ", got "
               + juce::String(output.getNumSamples()) + ")");

        // Verify prepended silence
        bool prependIsSilent = true;
        for (int s = 0; s < static_cast<int>(prependSamples); ++s)
        {
            if (std::abs(output.getSample(0, s)) > 0.0001f)
            {
                prependIsSilent = false;
                break;
            }
        }
        expect(prependIsSilent, "Prepended samples should be silent");

        expect(report.silencePrependedSamples == prependSamples,
               "Report should record prepended samples");
        expect(report.silenceAppendedSamples == appendSamples,
               "Report should record appended samples");

        logMessage("Silence padding verified");
    }

    //--------------------------------------------------------------------------
    void testFadeApplication()
    {
        // 1s sine at 0.5 amplitude
        auto buffer = TestAudio::createSineWave(440.0, 0.5, kSampleRate, 1.0, 1);

        HeadTailRecipe recipe;
        recipe.fadeInEnabled = true;
        recipe.fadeInMs = 100.0f;

        juce::AudioBuffer<float> output;
        auto report = HeadTailEngine::process(buffer, kSampleRate, recipe, output);

        expect(report.success, "Processing should succeed");

        // First sample should be near 0
        float firstSample = std::abs(output.getSample(0, 0));
        expect(firstSample < 0.01f,
               "First sample should be near 0 after fade in (got "
               + juce::String(firstSample) + ")");

        // Sample at ~100ms should be close to original amplitude
        int fadeEndSample = static_cast<int>(100.0 * kSampleRate / 1000.0);
        // Check a sample near the end of the fade region
        // The sine value at that position will vary, but the gain envelope should be ~1.0
        float sampleAtFadeEnd = std::abs(output.getSample(0, fadeEndSample - 1));
        float originalAtFadeEnd = std::abs(buffer.getSample(0, fadeEndSample - 1));
        // The fade gain at the last sample should be very close to 1.0
        expect(sampleAtFadeEnd > originalAtFadeEnd * 0.9f,
               "Sample near fade end should be close to original amplitude");

        expect(report.fadeInAppliedMs > 99.0f && report.fadeInAppliedMs < 101.0f,
               "Report should record fade in duration");

        logMessage("Fade application verified");
    }

    //--------------------------------------------------------------------------
    void testCombinedRecipe()
    {
        // 500ms silence + 1s sine + 500ms silence
        auto buffer = createSilenceSineSilence(500.0, 1000.0, 500.0);

        HeadTailRecipe recipe;
        recipe.detectEnabled = true;
        recipe.thresholdDB = -40.0f;
        recipe.holdTimeMs = 1.0f;
        recipe.leadingPadMs = 10.0f;
        recipe.trailingPadMs = 10.0f;
        recipe.prependSilenceEnabled = true;
        recipe.prependSilenceMs = 20.0f;
        recipe.appendSilenceEnabled = true;
        recipe.appendSilenceMs = 20.0f;
        recipe.fadeInEnabled = true;
        recipe.fadeInMs = 10.0f;
        recipe.fadeOutEnabled = true;
        recipe.fadeOutMs = 10.0f;

        juce::AudioBuffer<float> output;
        auto report = HeadTailEngine::process(buffer, kSampleRate, recipe, output);

        expect(report.success, "Combined processing should succeed");
        expect(report.detectionFound, "Detection should find content");
        expect(output.getNumSamples() > 0, "Output should not be empty");

        // Output should be roughly: 20ms silence + 10ms pad + 1s sine + 10ms pad + 20ms silence
        // ~1060ms total, significantly shorter than original 2000ms
        expect(output.getNumSamples() < buffer.getNumSamples(),
               "Output should be shorter than input after trimming silence");
        expect(report.finalDurationMs < report.originalDurationMs,
               "Final duration should be less than original");

        logMessage("Combined recipe verified: "
                   + juce::String(report.originalDurationMs, 1) + "ms -> "
                   + juce::String(report.finalDurationMs, 1) + "ms");
    }

    //--------------------------------------------------------------------------
    void testShortFileTrim()
    {
        // 50ms file, try to trim 100ms from head
        auto buffer = TestAudio::createSineWave(440.0, 0.5, kSampleRate, 0.05, 1);

        HeadTailRecipe recipe;
        recipe.headTrimEnabled = true;
        recipe.headTrimMs = 100.0f;

        juce::AudioBuffer<float> output;
        auto report = HeadTailEngine::process(buffer, kSampleRate, recipe, output);

        // Should handle gracefully -- not crash, produce at least 1 sample
        expect(report.success, "Should handle over-trimming gracefully");
        expect(output.getNumSamples() >= 1, "Output should have at least 1 sample");

        logMessage("Short file trim handled gracefully");
    }

    //--------------------------------------------------------------------------
    void testAllSilence()
    {
        auto buffer = TestAudio::createSilence(kSampleRate, 1.0, 1);

        HeadTailRecipe recipe;
        recipe.detectEnabled = true;
        recipe.thresholdDB = -40.0f;
        recipe.notFoundBehavior = HeadTailRecipe::NotFoundBehavior::Skip;

        juce::AudioBuffer<float> output;
        auto report = HeadTailEngine::process(buffer, kSampleRate, recipe, output);

        expect(report.success, "Processing all-silence should succeed");
        expect(!report.detectionFound, "Detection should report not found");

        // With Skip behavior, the full buffer should be kept (no trimming)
        expect(output.getNumSamples() == buffer.getNumSamples(),
               "Skip behavior should keep full buffer");

        logMessage("All-silence buffer handled correctly");
    }

    //--------------------------------------------------------------------------
    void testEmptyBuffer()
    {
        juce::AudioBuffer<float> buffer;

        HeadTailRecipe recipe;

        juce::AudioBuffer<float> output;
        auto report = HeadTailEngine::process(buffer, kSampleRate, recipe, output);

        expect(!report.success, "Empty buffer should fail");
        expect(report.errorMessage.isNotEmpty(), "Should have error message");

        logMessage("Empty buffer handled correctly");
    }
};

static HeadTailEngineTests headTailEngineTests;
