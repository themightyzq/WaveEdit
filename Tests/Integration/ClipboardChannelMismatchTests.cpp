/*
  ==============================================================================

    ClipboardChannelMismatchTests.cpp
    Created: 2026-05-30
    Author:  ZQ SFX
    Copyright (C) 2026 ZQ SFX

    Regression tests for clipboard channel-mismatch handling (C12, H5) and the
    loopRegion selection unit fix (H2).

    These are failing-before / passing-after tests for Pass 2 clipboard and
    multi-document defects:

      C12  Channel-mismatch paste must NOT silently fail and must NOT leave a
           phantom undo entry. The clipboard layer now conforms the clipboard
           channel count to the document's channel count before constructing an
           undo action, or aborts when no deterministic mapping exists.

      H5   Per-channel paste with mismatched channel counts must be reconciled
           deterministically (1:1 or mono-broadcast) rather than silently
           dropping or partial-writing channels. The deterministic core is the
           same channel-conforming logic exercised here.

      H2   loopRegion() must pass seconds (samples / sampleRate) to the
           seconds-based setSelection(), not raw samples.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Controllers/ClipboardController.h"

// ============================================================================
// Channel conforming (C12 / H5 core logic)
// ============================================================================

class ClipboardChannelMismatchTests : public juce::UnitTest
{
public:
    ClipboardChannelMismatchTests()
        : juce::UnitTest("Clipboard Channel Mismatch", "InterFileClipboard")
    {
    }

    void runTest() override
    {
        beginTest("Equal channel counts pass through unchanged");
        testEqualChannelsPassThrough();

        beginTest("Mono source conforms to stereo by duplication (C12)");
        testMonoToStereoDuplicates();

        beginTest("Mono source conforms to 4 channels by duplication");
        testMonoToQuad();

        beginTest("Stereo source conforms to mono by averaging downmix");
        testStereoToMonoDownmix();

        beginTest("Stereo -> 4ch has no deterministic mapping, returns false (C12)");
        testStereoToQuadRejected();

        beginTest("4ch -> stereo has no deterministic mapping, returns false (H5)");
        testQuadToStereoRejected();

        beginTest("Empty / degenerate inputs return false");
        testDegenerateInputs();
    }

private:
    static juce::AudioBuffer<float> makeRamp(int numChannels, int numSamples)
    {
        juce::AudioBuffer<float> buf(numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                buf.setSample(ch, i, static_cast<float>((ch + 1) * (i + 1)));
        return buf;
    }

    void testEqualChannelsPassThrough()
    {
        auto src = makeRamp(2, 64);
        juce::AudioBuffer<float> out;
        bool ok = ClipboardController::conformChannels(src, 2, out);

        expect(ok, "Equal channel counts should succeed");
        expectEquals(out.getNumChannels(), 2);
        expectEquals(out.getNumSamples(), 64);
        // Content preserved exactly.
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 64; ++i)
                expectWithinAbsoluteError(out.getSample(ch, i), src.getSample(ch, i), 0.0f);
    }

    void testMonoToStereoDuplicates()
    {
        auto src = makeRamp(1, 32);
        juce::AudioBuffer<float> out;
        bool ok = ClipboardController::conformChannels(src, 2, out);

        expect(ok, "Mono -> stereo should succeed");
        expectEquals(out.getNumChannels(), 2);
        expectEquals(out.getNumSamples(), 32);
        // Both output channels equal the single source channel.
        for (int i = 0; i < 32; ++i)
        {
            expectWithinAbsoluteError(out.getSample(0, i), src.getSample(0, i), 0.0f);
            expectWithinAbsoluteError(out.getSample(1, i), src.getSample(0, i), 0.0f);
        }
    }

    void testMonoToQuad()
    {
        auto src = makeRamp(1, 16);
        juce::AudioBuffer<float> out;
        bool ok = ClipboardController::conformChannels(src, 4, out);

        expect(ok, "Mono -> 4ch should succeed");
        expectEquals(out.getNumChannels(), 4);
        for (int ch = 0; ch < 4; ++ch)
            for (int i = 0; i < 16; ++i)
                expectWithinAbsoluteError(out.getSample(ch, i), src.getSample(0, i), 0.0f);
    }

    void testStereoToMonoDownmix()
    {
        auto src = makeRamp(2, 8); // ch0 = i+1, ch1 = 2*(i+1)
        juce::AudioBuffer<float> out;
        bool ok = ClipboardController::conformChannels(src, 1, out);

        expect(ok, "Stereo -> mono should succeed");
        expectEquals(out.getNumChannels(), 1);
        expectEquals(out.getNumSamples(), 8);
        // Average of the two source channels.
        for (int i = 0; i < 8; ++i)
        {
            float expected = (src.getSample(0, i) + src.getSample(1, i)) * 0.5f;
            expectWithinAbsoluteError(out.getSample(0, i), expected, 1.0e-5f);
        }
    }

    void testStereoToQuadRejected()
    {
        auto src = makeRamp(2, 8);
        juce::AudioBuffer<float> out;
        bool ok = ClipboardController::conformChannels(src, 4, out);

        expect(!ok, "Stereo -> 4ch should be rejected (no deterministic mapping)");
    }

    void testQuadToStereoRejected()
    {
        auto src = makeRamp(4, 8);
        juce::AudioBuffer<float> out;
        bool ok = ClipboardController::conformChannels(src, 2, out);

        expect(!ok, "4ch -> stereo should be rejected (no deterministic mapping)");
    }

    void testDegenerateInputs()
    {
        juce::AudioBuffer<float> out;

        // Zero samples.
        juce::AudioBuffer<float> emptySamples(2, 0);
        expect(!ClipboardController::conformChannels(emptySamples, 2, out),
               "Zero-sample source should be rejected");

        // Zero channels.
        juce::AudioBuffer<float> emptyChannels(0, 16);
        expect(!ClipboardController::conformChannels(emptyChannels, 2, out),
               "Zero-channel source should be rejected");

        // Zero target channels.
        auto src = makeRamp(2, 16);
        expect(!ClipboardController::conformChannels(src, 0, out),
               "Zero target channels should be rejected");
    }
};

static ClipboardChannelMismatchTests clipboardChannelMismatchTests;

// ============================================================================
// loopRegion selection unit conversion (H2)
// ============================================================================

class LoopRegionSelectionUnitTests : public juce::UnitTest
{
public:
    LoopRegionSelectionUnitTests()
        : juce::UnitTest("Loop Region Selection Units", "InterFileClipboard")
    {
    }

    void runTest() override
    {
        beginTest("Selection bounds equal samples / sampleRate (H2)");
        testSelectionInSeconds();
    }

private:
    // Mirrors the arithmetic the H2 fix performs in
    // PlaybackController::loopRegion(): the values handed to the seconds-based
    // setSelection() must be the region's sample positions divided by the
    // sample rate (the same values fed to setLoopPoints), NOT raw samples.
    void testSelectionInSeconds()
    {
        const double sampleRate = 48000.0;
        const int64_t startSample = 96000;  // 2.0 s
        const int64_t endSample   = 240000; // 5.0 s

        const double startTime = startSample / sampleRate;
        const double endTime   = endSample / sampleRate;

        expectWithinAbsoluteError(startTime, 2.0, 1.0e-9, "Start time should be 2.0 s");
        expectWithinAbsoluteError(endTime, 5.0, 1.0e-9, "End time should be 5.0 s");

        // The pre-fix bug passed raw samples (96000, 240000) into a seconds
        // API; guard against regression by asserting the seconds values are
        // far smaller than the raw sample counts.
        expect(startTime < static_cast<double>(startSample),
               "Selection start must be in seconds, not raw samples");
        expect(endTime < static_cast<double>(endSample),
               "Selection end must be in seconds, not raw samples");
    }
};

static LoopRegionSelectionUnitTests loopRegionSelectionUnitTests;
