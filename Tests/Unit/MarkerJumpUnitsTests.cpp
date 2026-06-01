/*
  ==============================================================================

    MarkerJumpUnitsTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Pass-2 regression test for C2: marker jump used the wrong units twice.

    The controller does, per fix:
      currentSample = timeToSample(getCurrentPosition())   // seconds -> samples
      index         = getNextMarkerIndex(currentSample)     // samples domain
      seekSeconds   = sampleToTime(marker->getPosition())   // samples -> seconds

    This test reproduces that arithmetic at the logic level (no AudioEngine):
      - converting the playhead seconds to samples picks the correct marker
        (the OLD code passed raw seconds, so a 1.0s playhead = "1 sample" and
        always selected the first marker / flew the playhead to ~12 hours);
      - the resulting seek target in seconds equals markerSamples / sampleRate.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "Utils/Marker.h"
#include "Utils/MarkerManager.h"

// ============================================================================
class MarkerJumpUnitsTests : public juce::UnitTest
{
public:
    MarkerJumpUnitsTests()
        : juce::UnitTest("Marker Jump Units (C2)", "MarkerSystem") {}

    void runTest() override
    {
        beginTest("Next-marker search converts playhead seconds to samples");
        testNextMarkerUsesSamples();

        beginTest("Seek target converts marker samples back to seconds");
        testSeekTargetInSeconds();

        beginTest("Previous-marker search converts playhead seconds to samples");
        testPreviousMarkerUsesSamples();
    }

private:
    // Mirror of AudioBufferManager's conversions (kept local so the test does
    // not need to spin up the engine).
    static int64_t timeToSample(double seconds, double sampleRate)
    {
        return (int64_t) std::llround(seconds * sampleRate);
    }
    static double sampleToTime(int64_t sample, double sampleRate)
    {
        return (double) sample / sampleRate;
    }

    void testNextMarkerUsesSamples()
    {
        const double sr = 48000.0;
        MarkerManager mgr;
        mgr.addMarker(Marker("a", (int64_t)(0.5 * sr)));   // 0.5 s
        mgr.addMarker(Marker("b", (int64_t)(1.5 * sr)));   // 1.5 s
        mgr.addMarker(Marker("c", (int64_t)(2.5 * sr)));   // 2.5 s

        // Playhead at 1.0 seconds. Correct conversion -> 48000 samples, which
        // is past marker "a" (24000) and before "b" (72000): next == index 1.
        const double playheadSeconds = 1.0;
        const int64_t currentSample = timeToSample(playheadSeconds, sr);
        expectEquals(currentSample, (int64_t) 48000);

        const int next = mgr.getNextMarkerIndex(currentSample);
        expectEquals(next, 1, "next marker after 1.0s is 'b'");

        // The OLD bug passed raw seconds (1) as the sample position. Show that
        // would have wrongly selected marker 'a' (the first marker > 1 sample).
        const int buggy = mgr.getNextMarkerIndex((int64_t) playheadSeconds);
        expectEquals(buggy, 0, "raw-seconds search wrongly returns the first marker");
    }

    void testSeekTargetInSeconds()
    {
        const double sr = 44100.0;
        MarkerManager mgr;
        const int64_t markerSamples = (int64_t)(2.0 * sr);  // 88200
        mgr.addMarker(Marker("x", markerSamples));

        const Marker* m = mgr.getMarker(0);
        expect(m != nullptr);

        // Correct seek target: samples / sampleRate == 2.0 seconds.
        const double seek = sampleToTime(m->getPosition(), sr);
        expectWithinAbsoluteError(seek, 2.0, 1e-9);

        // The OLD bug fed the sample COUNT (88200) straight into a
        // seconds-based setPosition -> ~24.5 hours. Demonstrate the magnitude.
        expect((double) m->getPosition() > 80000.0,
               "raw sample count as seconds is absurdly large");
    }

    void testPreviousMarkerUsesSamples()
    {
        const double sr = 44100.0;
        MarkerManager mgr;
        mgr.addMarker(Marker("a", (int64_t)(1.0 * sr)));
        mgr.addMarker(Marker("b", (int64_t)(2.0 * sr)));
        mgr.addMarker(Marker("c", (int64_t)(3.0 * sr)));

        const double playheadSeconds = 2.5;
        const int64_t currentSample = timeToSample(playheadSeconds, sr);
        const int prev = mgr.getPreviousMarkerIndex(currentSample);
        expectEquals(prev, 1, "previous marker before 2.5s is 'b'");
    }
};

static MarkerJumpUnitsTests markerJumpUnitsTests;
