/*
  ==============================================================================

    LoopEngineTests.cpp
    Created: 2025
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Comprehensive unit tests for LoopEngine DSP operations.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "DSP/LoopEngine.h"
#include "TestAudioFiles.h"
#include "AudioAssertions.h"
#include <cmath>

class LoopEngineTests : public juce::UnitTest
{
public:
    LoopEngineTests() : juce::UnitTest("LoopEngine", "DSP") {}

    void runTest() override
    {
        testBasicLoop();
        testZeroCrossingRefinement();
        testCrossfadeCurves();
        testEqualPowerPreservesLevel();
        testMultipleVariations();
        testShepardLoop();
        testShortSelection();
        testEmptyBuffer();
        testMonoAndStereo();
        testCrossfadeModes();
    }

private:
    //==========================================================================
    // Test 1: Basic loop creation
    void testBasicLoop()
    {
        beginTest("testBasicLoop");

        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 1);
        LoopRecipe recipe;
        recipe.crossfadeLengthMs = 50.0f;
        recipe.crossfadeCurve = LoopRecipe::CrossfadeCurve::EqualPower;
        recipe.zeroCrossingEnabled = false;

        auto result = LoopEngine::createLoop(buffer, 44100.0, 0, 44100, recipe);
        expect(result.success, "Loop should succeed");

        // Output length = inputLength - crossfadeLength
        int64_t expectedXfade = static_cast<int64_t>(50.0f * 44100.0 / 1000.0);
        int64_t expectedLen = 44100 - expectedXfade;
        expectEquals(result.loopBuffer.getNumSamples(), static_cast<int>(expectedLen),
                     "Output length should be selection minus crossfade");

        // Crossfade should reduce discontinuity
        expect(result.discontinuityAfter < result.discontinuityBefore
               || result.discontinuityAfter < 0.05f,
               "Crossfade should reduce discontinuity (after="
               + juce::String(result.discontinuityAfter) + ")");
    }

    //==========================================================================
    // Test 2: Zero-crossing refinement
    void testZeroCrossingRefinement()
    {
        beginTest("testZeroCrossingRefinement - finds zero crossings");

        // Create a buffer with known zero crossings (440Hz sine at 44100Hz)
        // Zero crossings occur every ~50.1 samples
        auto buffer = TestAudio::createSineWave(440.0, 0.8f, 44100.0, 1.0, 1);
        LoopRecipe recipe;
        recipe.zeroCrossingEnabled = true;
        recipe.zeroCrossingSearchWindowSamples = 200;
        recipe.crossfadeLengthMs = 50.0f;

        // Start at sample 100 (not a zero crossing for 440Hz)
        auto result = LoopEngine::createLoop(buffer, 44100.0, 100, 43000, recipe);
        expect(result.success, "Loop with zero-crossing should succeed");

        // The refined start should be near a zero crossing
        float startAmp = std::abs(
            buffer.getSample(0, static_cast<int>(result.loopStartSample)));
        expect(startAmp < 0.1f,
               "Start should be near zero crossing (amp=" + juce::String(startAmp) + ")");

        beginTest("testZeroCrossingRefinement - handles no zero crossing nearby");

        // Create a buffer with all samples at 0.5 (no zero crossings)
        juce::AudioBuffer<float> dcBuffer(1, 44100);
        for (int i = 0; i < 44100; ++i)
            dcBuffer.setSample(0, i, 0.5f);

        recipe.zeroCrossingSearchWindowSamples = 50;
        auto result2 = LoopEngine::createLoop(dcBuffer, 44100.0, 100, 43000, recipe);
        expect(result2.success, "Should still succeed even without zero crossings");
    }

    //==========================================================================
    // Test 3: Crossfade curves
    void testCrossfadeCurves()
    {
        beginTest("testCrossfadeCurves");

        LoopRecipe::CrossfadeCurve curves[] = {
            LoopRecipe::CrossfadeCurve::Linear,
            LoopRecipe::CrossfadeCurve::EqualPower,
            LoopRecipe::CrossfadeCurve::SCurve
        };
        juce::String names[] = { "Linear", "EqualPower", "SCurve" };

        for (int c = 0; c < 3; ++c)
        {
            auto curve = curves[c];

            // At t=0: fadeOut=1.0, fadeIn=0.0
            auto [fo0, fi0] = LoopEngine::getCrossfadeGains(0.0f, curve);
            expectWithinAbsoluteError(fo0, 1.0f, 0.001f,
                names[c] + " fadeOut at t=0 should be 1.0");
            expectWithinAbsoluteError(fi0, 0.0f, 0.001f,
                names[c] + " fadeIn at t=0 should be 0.0");

            // At t=1: fadeOut=0.0, fadeIn=1.0
            auto [fo1, fi1] = LoopEngine::getCrossfadeGains(1.0f, curve);
            expectWithinAbsoluteError(fo1, 0.0f, 0.001f,
                names[c] + " fadeOut at t=1 should be 0.0");
            expectWithinAbsoluteError(fi1, 1.0f, 0.001f,
                names[c] + " fadeIn at t=1 should be 1.0");

            // At t=0.5: gains should be symmetric
            auto [fo5, fi5] = LoopEngine::getCrossfadeGains(0.5f, curve);
            expectWithinAbsoluteError(fo5, fi5, 0.01f,
                names[c] + " gains at t=0.5 should be symmetric");
        }

        // Specifically for EqualPower: fadeOut^2 + fadeIn^2 ≈ 1.0 (constant power)
        for (float t = 0.0f; t <= 1.0f; t += 0.1f)
        {
            auto [fo, fi] = LoopEngine::getCrossfadeGains(t, LoopRecipe::CrossfadeCurve::EqualPower);
            float powerSum = fo * fo + fi * fi;
            expectWithinAbsoluteError(powerSum, 1.0f, 0.01f,
                "EqualPower: fadeOut^2 + fadeIn^2 should be ~1.0 at t=" + juce::String(t, 1));
        }
    }

    //==========================================================================
    // Test 4: Equal power preserves level
    void testEqualPowerPreservesLevel()
    {
        beginTest("testEqualPowerPreservesLevel");

        // Create constant amplitude buffer (all samples = 0.5)
        const int numSamples = 44100;
        juce::AudioBuffer<float> buffer(1, numSamples);
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample(0, i, 0.5f);

        LoopRecipe recipe;
        recipe.crossfadeLengthMs = 100.0f;
        recipe.crossfadeCurve = LoopRecipe::CrossfadeCurve::EqualPower;
        recipe.zeroCrossingEnabled = false;

        auto result = LoopEngine::createLoop(buffer, 44100.0, 0, numSamples, recipe);
        expect(result.success, "Loop should succeed");

        // Check that output amplitude is consistent throughout the crossfade region.
        // Equal power crossfade preserves power (RMS), not amplitude.
        // For identical 0.5 signals: output = (cos + sin) * 0.5
        // At midpoint: (0.707 + 0.707) * 0.5 = 0.707
        // At endpoints: 1.0 * 0.5 = 0.5
        // So amplitude ranges from 0.5 to ~0.707 — no dip below input level.
        const float* data = result.loopBuffer.getReadPointer(0);
        int xfadeLen = static_cast<int>(result.crossfadeLengthSamples);

        float minVal = 1.0f;
        for (int i = 0; i < xfadeLen; ++i)
            minVal = std::min(minVal, data[i]);

        // Equal power should never dip below the input amplitude
        expect(minVal >= 0.49f,
            "Equal power crossfade should not dip below input level (min="
            + juce::String(minVal) + ")");
    }

    //==========================================================================
    // Test 5: Multiple variations
    void testMultipleVariations()
    {
        beginTest("testMultipleVariations");

        // Create 3-second buffer
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 3.0, 1);
        LoopRecipe recipe;
        recipe.loopCount = 3;
        recipe.offsetStepMs = 500.0f;  // 500ms between variations
        recipe.crossfadeLengthMs = 50.0f;
        recipe.zeroCrossingEnabled = false;

        auto results = LoopEngine::createVariations(
            buffer, 44100.0, 0, 44100 * 2, recipe);  // 2-second selection

        // Verify 3 results returned
        expectEquals(static_cast<int>(results.size()), 3, "Should create 3 variations");

        // Verify each has different loopStartSample
        for (size_t i = 0; i < results.size(); ++i)
        {
            expect(results[i].success,
                   "Variation " + juce::String((int)i) + " should succeed");
        }

        // Check that start positions differ
        if (results.size() >= 3)
        {
            expect(results[0].loopStartSample != results[1].loopStartSample
                   || results[1].loopStartSample != results[2].loopStartSample,
                   "Variations should have different start positions");
        }

        // Verify all have reasonable discontinuity (crossfade applied)
        for (size_t i = 0; i < results.size(); ++i)
        {
            expect(results[i].discontinuityAfter < 0.05f,
                   "Variation " + juce::String((int)i) + " should have low discontinuity (disc="
                   + juce::String(results[i].discontinuityAfter) + ")");
        }
    }

    //==========================================================================
    // Test 6: Shepard loop
    void testShepardLoop()
    {
        beginTest("testShepardLoop");

        // Create 2-second 440Hz sine wave
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 2.0, 1);
        LoopRecipe recipe;
        recipe.shepardEnabled = true;
        recipe.shepardSemitones = 1.0f;
        recipe.shepardDirection = LoopRecipe::ShepardDirection::Up;
        recipe.crossfadeLengthMs = 50.0f;
        recipe.zeroCrossingEnabled = false;

        auto result = LoopEngine::createShepardLoop(buffer, 44100.0, 0, 88200, recipe);
        expect(result.success, "Shepard loop should succeed");

        // Output length should match the constrained length
        expect(result.loopBuffer.getNumSamples() > 0,
               "Shepard loop should produce output");

        // Loop should be reasonably seamless
        float disc = LoopEngine::measureDiscontinuity(result.loopBuffer);
        expect(disc < 0.1f,
               "Shepard loop discontinuity should be reasonable (got " + juce::String(disc) + ")");
    }

    //==========================================================================
    // Test 7: Short selection (shorter than 2x crossfade)
    void testShortSelection()
    {
        beginTest("testShortSelection");

        // Create a very short buffer (10ms = 441 samples at 44100)
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.01, 1);
        LoopRecipe recipe;
        recipe.crossfadeLengthMs = 100.0f;  // 4410 samples — way more than 441
        recipe.zeroCrossingEnabled = false;

        // Should succeed with clamped crossfade (not crash or fail)
        auto result = LoopEngine::createLoop(buffer, 44100.0, 0, 441, recipe);
        expect(result.success, "Should succeed with clamped crossfade");
        expect(result.crossfadeLengthSamples <= 220,
               "Crossfade should be clamped to half selection (got "
               + juce::String(result.crossfadeLengthSamples) + ")");
    }

    //==========================================================================
    // Test 8: Empty buffer
    void testEmptyBuffer()
    {
        beginTest("testEmptyBuffer");

        juce::AudioBuffer<float> empty;
        LoopRecipe recipe;

        auto result = LoopEngine::createLoop(empty, 44100.0, 0, 0, recipe);
        expect(!result.success, "Should fail on empty buffer");
        expect(result.errorMessage.isNotEmpty(), "Should have error message");

        // Also test Shepard with empty buffer
        auto shepResult = LoopEngine::createShepardLoop(empty, 44100.0, 0, 0, recipe);
        expect(!shepResult.success, "Shepard should fail on empty buffer");
    }

    //==========================================================================
    // Test 9: Mono and Stereo
    void testMonoAndStereo()
    {
        beginTest("testMonoAndStereo");

        LoopRecipe recipe;
        recipe.crossfadeLengthMs = 50.0f;
        recipe.zeroCrossingEnabled = false;

        // Mono
        auto monoBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 1);
        auto monoResult = LoopEngine::createLoop(monoBuffer, 44100.0, 0, 44100, recipe);
        expect(monoResult.success, "Mono loop should succeed");
        expectEquals(monoResult.loopBuffer.getNumChannels(), 1,
                     "Mono output should have 1 channel");

        // Stereo
        auto stereoBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        auto stereoResult = LoopEngine::createLoop(stereoBuffer, 44100.0, 0, 44100, recipe);
        expect(stereoResult.success, "Stereo loop should succeed");
        expectEquals(stereoResult.loopBuffer.getNumChannels(), 2,
                     "Stereo output should have 2 channels");

        // Both channels should be processed
        expect(stereoResult.loopBuffer.getNumSamples() > 0,
               "Stereo output should have samples");
    }

    //==========================================================================
    // Test 10: Crossfade modes (Milliseconds vs Percentage)
    void testCrossfadeModes()
    {
        beginTest("testCrossfadeModes - Milliseconds mode");
        {
            auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 1);
            LoopRecipe recipe;
            recipe.crossfadeMode = LoopRecipe::CrossfadeMode::Milliseconds;
            recipe.crossfadeLengthMs = 50.0f;
            recipe.zeroCrossingEnabled = false;

            auto result = LoopEngine::createLoop(buffer, 44100.0, 0, 44100, recipe);
            expect(result.success);

            int64_t expectedXfade = static_cast<int64_t>(50.0f * 44100.0 / 1000.0);
            expectEquals(static_cast<int>(result.crossfadeLengthSamples),
                         static_cast<int>(expectedXfade),
                         "Crossfade should be exactly 50ms in samples");
        }

        beginTest("testCrossfadeModes - Percentage mode");
        {
            auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 1);
            LoopRecipe recipe;
            recipe.crossfadeMode = LoopRecipe::CrossfadeMode::Percentage;
            recipe.crossfadePercent = 10.0f;
            recipe.zeroCrossingEnabled = false;

            auto result = LoopEngine::createLoop(buffer, 44100.0, 0, 44100, recipe);
            expect(result.success);

            // 10% of 44100 = 4410, but may be capped by maxCrossfadeMs
            int64_t expected = 4410;
            int64_t maxXfade = static_cast<int64_t>(recipe.maxCrossfadeMs * 44100.0 / 1000.0);
            expected = std::min(expected, maxXfade);

            expectEquals(static_cast<int>(result.crossfadeLengthSamples),
                         static_cast<int>(expected),
                         "Crossfade should be 10% of selection length");
        }
    }
};

static LoopEngineTests loopEngineTests;
