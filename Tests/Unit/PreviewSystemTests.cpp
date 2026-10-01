/*
  ==============================================================================

    PreviewSystemTests.cpp
    Copyright (C) 2026 ZQ SFX

    Unit coverage for the shared real-time preview helpers on AudioEngine:
      - computePreviewRegion(): selection -> validated, clamped preview region.
      - fadeGainAt(): position-driven fade gain (rate-independent, loop-aligned).

    These are the pure pieces of the preview system that previously lived as
    copy-pasted logic in each dialog and had drifted (one forgot setLooping,
    one ignored the selection, the fade free-ran against a mismatched rate).
    The engine session methods (startSelectionPreview/...) drive a real audio
    device and are verified by manual/audio QA.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "Audio/AudioEngine.h"

class PreviewRegionTests : public juce::UnitTest
{
public:
    PreviewRegionTests() : juce::UnitTest("Preview Region", "DSP") {}

    void runTest() override
    {
        const double sr = 44100.0;
        const int64_t total = 441000; // 10 seconds

        beginTest("Valid selection maps to its time range");
        {
            auto r = AudioEngine::computePreviewRegion(44100, 88200, sr, total, true);
            expect(r.valid);
            expectWithinAbsoluteError(r.startSec, 1.0, 1.0e-9);
            expectWithinAbsoluteError(r.endSec, 2.0, 1.0e-9);
            expect(r.loop);
        }

        beginTest("Loop flag is carried through");
        {
            auto r = AudioEngine::computePreviewRegion(0, 44100, sr, total, false);
            expect(r.valid);
            expect(! r.loop);
        }

        beginTest("Empty selection falls back to the whole buffer");
        {
            auto r = AudioEngine::computePreviewRegion(5000, 5000, sr, total, true);
            expect(r.valid);
            expectWithinAbsoluteError(r.startSec, 0.0, 1.0e-9);
            expectWithinAbsoluteError(r.endSec, total / sr, 1.0e-9);
        }

        beginTest("Inverted selection falls back to the whole buffer");
        {
            auto r = AudioEngine::computePreviewRegion(88200, 44100, sr, total, true);
            expect(r.valid);
            expectWithinAbsoluteError(r.startSec, 0.0, 1.0e-9);
            expectWithinAbsoluteError(r.endSec, total / sr, 1.0e-9);
        }

        beginTest("Out-of-range selection is clamped to the buffer");
        {
            auto r = AudioEngine::computePreviewRegion(-1000, total + 99999, sr, total, true);
            expect(r.valid);
            expectWithinAbsoluteError(r.startSec, 0.0, 1.0e-9);
            expectWithinAbsoluteError(r.endSec, total / sr, 1.0e-9);
        }

        beginTest("Zero/negative sample rate is invalid");
        {
            expect(! AudioEngine::computePreviewRegion(0, 44100, 0.0, total, true).valid);
            expect(! AudioEngine::computePreviewRegion(0, 44100, -44100.0, total, true).valid);
        }

        beginTest("Empty buffer is invalid");
        {
            expect(! AudioEngine::computePreviewRegion(0, 44100, sr, 0, true).valid);
        }
    }
};

static PreviewRegionTests previewRegionTests;

class FadeGainTests : public juce::UnitTest
{
public:
    FadeGainTests() : juce::UnitTest("Fade Gain (position-driven)", "DSP") {}

    void runTest() override
    {
        const double dur = 1000.0; // span in samples

        beginTest("Linear fade-in: 0 at start, 1 at end, 0.5 at midpoint");
        {
            expectWithinAbsoluteError(AudioEngine::fadeGainAt(0.0, dur, true, 0), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError(AudioEngine::fadeGainAt(dur, dur, true, 0), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError(AudioEngine::fadeGainAt(dur * 0.5, dur, true, 0), 0.5f, 1.0e-6f);
        }

        beginTest("Linear fade-out is the mirror of fade-in");
        {
            expectWithinAbsoluteError(AudioEngine::fadeGainAt(0.0, dur, false, 0), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError(AudioEngine::fadeGainAt(dur, dur, false, 0), 0.0f, 1.0e-6f);
        }

        beginTest("Position is clamped: before start -> 0, past end -> 1 (fade-in)");
        {
            expectWithinAbsoluteError(AudioEngine::fadeGainAt(-500.0, dur, true, 0), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError(AudioEngine::fadeGainAt(dur * 3.0, dur, true, 0), 1.0f, 1.0e-6f);
        }

        beginTest("All curve types: 0 at start, ~1 at end, monotonic non-decreasing");
        {
            for (int curve = 0; curve <= 3; ++curve)
            {
                expectWithinAbsoluteError(AudioEngine::fadeGainAt(0.0, dur, true, curve), 0.0f, 0.05f);
                expectWithinAbsoluteError(AudioEngine::fadeGainAt(dur, dur, true, curve), 1.0f, 0.05f);

                float prev = -1.0f;
                for (int k = 0; k <= 10; ++k)
                {
                    const float g = AudioEngine::fadeGainAt(dur * (k / 10.0), dur, true, curve);
                    expect(g >= prev - 1.0e-4f, "curve " + juce::String(curve) + " must be non-decreasing");
                    prev = g;
                }
            }
        }

        beginTest("Zero/negative duration is a unity no-op");
        {
            expectWithinAbsoluteError(AudioEngine::fadeGainAt(500.0, 0.0, true, 0), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError(AudioEngine::fadeGainAt(500.0, -10.0, false, 2), 1.0f, 1.0e-6f);
        }
    }
};

static FadeGainTests fadeGainTests;
