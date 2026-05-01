/*
  ==============================================================================

    TimePitchEngineTests.cpp
    WaveEdit - Professional Audio Editor

    Unit tests for the SoundTouch-backed TimePitchEngine. These exercise
    only the algorithm-level contract (length scaling for tempo, pitch
    detection on a known sine, identity/no-op behaviour). The actual
    quality of the SoundTouch processing is the upstream library's
    responsibility.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "DSP/TimePitchEngine.h"

namespace
{
    /** Generate a sine wave at @p frequency with @p durationSeconds at @p sampleRate. */
    juce::AudioBuffer<float> makeSine(double frequency, double durationSeconds,
                                       double sampleRate, int numChannels = 1)
    {
        const int n = static_cast<int>(std::round(durationSeconds * sampleRate));
        juce::AudioBuffer<float> buf(numChannels, n);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* w = buf.getWritePointer(ch);
            for (int i = 0; i < n; ++i)
                w[i] = static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi
                                                    * frequency * i / sampleRate));
        }
        return buf;
    }

    /** Estimate fundamental frequency by zero-crossing counting. Cheap and
        good enough for "did the pitch shift roughly N semitones" assertions. */
    double estimatePitchHz(const juce::AudioBuffer<float>& buf, double sampleRate)
    {
        if (buf.getNumChannels() == 0 || buf.getNumSamples() < 2)
            return 0.0;

        const auto* data = buf.getReadPointer(0);
        const int n = buf.getNumSamples();

        // Skip the first 10% to avoid SoundTouch's startup transient.
        const int start = n / 10;
        if (n - start < 2) return 0.0;

        int crossings = 0;
        for (int i = start + 1; i < n; ++i)
            if ((data[i] >= 0.0f) != (data[i - 1] >= 0.0f))
                ++crossings;

        // Two zero-crossings per cycle.
        const double effectiveDuration = static_cast<double>(n - start) / sampleRate;
        return crossings / 2.0 / effectiveDuration;
    }
}

class TimePitchEngineTests : public juce::UnitTest
{
public:
    TimePitchEngineTests() : juce::UnitTest("TimePitchEngine", "DSP") {}

    void runTest() override
    {
        constexpr double kSampleRate = 44100.0;

        beginTest("isIdentity recognises no-op recipes");
        {
            TimePitchEngine::Recipe r;
            expect(TimePitchEngine::isIdentity(r),
                   "default recipe is identity");

            r.tempoPercent = 1.0e-9;
            expect(TimePitchEngine::isIdentity(r),
                   "tiny float values still count as identity");

            r.tempoPercent = 0.5;
            expect(! TimePitchEngine::isIdentity(r),
                   "non-trivial tempo defeats identity");
        }

        beginTest("Identity recipe round-trips the buffer");
        {
            auto in = makeSine(440.0, 0.5, kSampleRate);
            TimePitchEngine::Recipe r;  // identity
            const auto out = TimePitchEngine::apply(in, kSampleRate, r);
            expectEquals(out.getNumChannels(), in.getNumChannels());
            expectEquals(out.getNumSamples(),  in.getNumSamples());

            // Per-sample equality (identity should be byte-identical).
            const auto* a = in.getReadPointer(0);
            const auto* b = out.getReadPointer(0);
            for (int i = 0; i < in.getNumSamples(); ++i)
                expect(std::abs(a[i] - b[i]) < 1.0e-7f,
                       "sample " + juce::String(i) + " mismatch");
        }

        beginTest("Tempo +100% halves the output length (~2x speed)");
        {
            auto in = makeSine(440.0, 1.0, kSampleRate);
            TimePitchEngine::Recipe r;
            r.tempoPercent = 100.0;  // 2x speed
            const auto out = TimePitchEngine::apply(in, kSampleRate, r);

            const double inSec  = in.getNumSamples()  / kSampleRate;
            const double outSec = out.getNumSamples() / kSampleRate;
            const double ratio  = outSec / inSec;
            // SoundTouch's tempo is approximate; allow ±10% slop for
            // startup/shutdown latency on a 1s buffer.
            expect(ratio > 0.45 && ratio < 0.55,
                   "expected ~0.5x length, got " + juce::String(ratio, 3));
        }

        beginTest("Tempo -50% doubles the output length (~0.5x speed)");
        {
            auto in = makeSine(440.0, 1.0, kSampleRate);
            TimePitchEngine::Recipe r;
            r.tempoPercent = -50.0;  // half speed
            const auto out = TimePitchEngine::apply(in, kSampleRate, r);

            const double inSec  = in.getNumSamples()  / kSampleRate;
            const double outSec = out.getNumSamples() / kSampleRate;
            const double ratio  = outSec / inSec;
            expect(ratio > 1.85 && ratio < 2.15,
                   "expected ~2x length, got " + juce::String(ratio, 3));
        }

        beginTest("Pitch +12 semitones doubles the perceived frequency");
        {
            auto in = makeSine(440.0, 1.0, kSampleRate);
            TimePitchEngine::Recipe r;
            r.pitchSemitones = 12.0;  // 1 octave up = freq * 2
            const auto out = TimePitchEngine::apply(in, kSampleRate, r);

            // Length should NOT change for pitch-only shift.
            const double lengthRatio = static_cast<double>(out.getNumSamples())
                                         / in.getNumSamples();
            expect(lengthRatio > 0.95 && lengthRatio < 1.05,
                   "pitch-only shift left length intact (ratio "
                   + juce::String(lengthRatio, 3) + ")");

            const double estimated = estimatePitchHz(out, kSampleRate);
            // Expect ~880 Hz; allow ±25% for SoundTouch's smearing on
            // a short buffer + zero-crossing estimator slop.
            expect(estimated > 660.0 && estimated < 1100.0,
                   "expected ~880 Hz, got " + juce::String(estimated, 1));
        }

        beginTest("Pitch -12 semitones halves the perceived frequency");
        {
            auto in = makeSine(880.0, 1.0, kSampleRate);
            TimePitchEngine::Recipe r;
            r.pitchSemitones = -12.0;
            const auto out = TimePitchEngine::apply(in, kSampleRate, r);
            const double estimated = estimatePitchHz(out, kSampleRate);
            expect(estimated > 330.0 && estimated < 550.0,
                   "expected ~440 Hz, got " + juce::String(estimated, 1));
        }

        beginTest("Stereo input is preserved");
        {
            auto in = makeSine(440.0, 0.5, kSampleRate, /*channels=*/2);
            TimePitchEngine::Recipe r;
            r.tempoPercent = 25.0;
            const auto out = TimePitchEngine::apply(in, kSampleRate, r);
            expectEquals(out.getNumChannels(), 2);
            expect(out.getNumSamples() > 0, "stereo output non-empty");
        }

        beginTest("Empty / invalid input returns empty buffer");
        {
            juce::AudioBuffer<float> empty;
            TimePitchEngine::Recipe r;
            r.tempoPercent = 50.0;
            const auto out = TimePitchEngine::apply(empty, kSampleRate, r);
            expect(out.getNumChannels() == 0 || out.getNumSamples() == 0,
                   "empty input → empty output");
        }
    }
};

static TimePitchEngineTests timePitchEngineTests;
