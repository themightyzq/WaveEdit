/*
  ==============================================================================

    AudioGeneratorTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Unit tests for the Generate-menu signal generator (Source/DSP/AudioGenerator).

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "DSP/AudioGenerator.h"

namespace
{
float peakOf(const juce::AudioBuffer<float>& b, int ch)
{
    return b.getMagnitude(ch, 0, b.getNumSamples());
}

float rmsOf(const juce::AudioBuffer<float>& b, int ch)
{
    return b.getRMSLevel(ch, 0, b.getNumSamples());
}

float meanOf(const juce::AudioBuffer<float>& b, int ch)
{
    double sum = 0.0;
    const float* d = b.getReadPointer(ch);
    for (int i = 0; i < b.getNumSamples(); ++i)
        sum += d[i];
    return (float) (sum / juce::jmax(1, b.getNumSamples()));
}

// Counts upward zero-crossings -> cycles, to estimate frequency.
int upwardCrossings(const juce::AudioBuffer<float>& b, int ch)
{
    const float* d = b.getReadPointer(ch);
    int count = 0;
    for (int i = 1; i < b.getNumSamples(); ++i)
        if (d[i - 1] < 0.0f && d[i] >= 0.0f)
            ++count;
    return count;
}
} // namespace

//==============================================================================
class AudioGeneratorTests : public juce::UnitTest
{
public:
    AudioGeneratorTests() : juce::UnitTest("Audio Generator", "Unit") {}

    void runTest() override
    {
        testSinePeakAndRms();
        testSineFrequency();
        testWaveformShapes();
        testToneChannelsIdentical();
        testWhiteNoise();
        testPinkNoiseTilt();
        testNoiseDecorrelated();
        testDeterministicSeed();
        testNoNaNOrClip();
    }

private:
    void testSinePeakAndRms()
    {
        beginTest("Sine peak == amplitude and RMS ~= amp/sqrt(2)");

        const double sr = 48000.0;
        AudioGenerator gen;
        gen.prepare(sr);

        juce::AudioBuffer<float> buf(1, (int) sr);   // 1 second
        const float amp = 0.5f;
        gen.generateTone(buf, AudioGenerator::Waveform::Sine, 1000.0, amp);

        expectWithinAbsoluteError(peakOf(buf, 0), amp, 0.01f, "peak matches amplitude");
        expectWithinAbsoluteError(rmsOf(buf, 0), amp / std::sqrt(2.0f), 0.01f,
                                  "sine RMS is amp/sqrt(2)");
    }

    void testSineFrequency()
    {
        beginTest("Generated frequency matches request (zero-crossing count)");

        const double sr = 44100.0;
        AudioGenerator gen;
        gen.prepare(sr);

        juce::AudioBuffer<float> buf(1, (int) sr);   // 1 second -> crossings ~= Hz
        gen.generateTone(buf, AudioGenerator::Waveform::Sine, 440.0, 0.8f);

        const int cycles = upwardCrossings(buf, 0);
        expect(std::abs(cycles - 440) <= 2, "≈440 upward crossings/sec, got "
                                             + juce::String(cycles));
    }

    void testWaveformShapes()
    {
        beginTest("Square/Saw/Triangle stay within [-amp, amp] and reach the rails");

        const double sr = 48000.0;
        const float amp = 0.7f;

        for (auto wf : { AudioGenerator::Waveform::Square,
                         AudioGenerator::Waveform::Saw,
                         AudioGenerator::Waveform::Triangle })
        {
            AudioGenerator gen;
            gen.prepare(sr);
            juce::AudioBuffer<float> buf(1, 4800);
            gen.generateTone(buf, wf, 100.0, amp);

            expect(peakOf(buf, 0) <= amp + 1.0e-4f, "does not exceed amplitude");
            expect(peakOf(buf, 0) > amp * 0.9f, "reaches near the amplitude rail");
        }
    }

    void testToneChannelsIdentical()
    {
        beginTest("Tone is identical across channels");

        AudioGenerator gen;
        gen.prepare(44100.0);
        juce::AudioBuffer<float> buf(2, 2048);
        gen.generateTone(buf, AudioGenerator::Waveform::Saw, 220.0, 0.5f);

        const float* l = buf.getReadPointer(0);
        const float* r = buf.getReadPointer(1);
        float maxDiff = 0.0f;
        for (int i = 0; i < buf.getNumSamples(); ++i)
            maxDiff = juce::jmax(maxDiff, std::abs(l[i] - r[i]));
        expectLessThan(maxDiff, 1.0e-6f, "both channels carry the same tone");
    }

    void testWhiteNoise()
    {
        beginTest("White noise: mean ~= 0, RMS in a sane range, within amplitude");

        AudioGenerator gen;
        gen.prepare(44100.0);
        gen.setNoiseSeed(1234);

        juce::AudioBuffer<float> buf(1, 44100);
        const float amp = 0.5f;
        gen.generateNoise(buf, AudioGenerator::NoiseType::White, amp);

        expectWithinAbsoluteError(meanOf(buf, 0), 0.0f, 0.02f, "zero-mean");
        expect(peakOf(buf, 0) <= amp + 1.0e-4f, "within amplitude");
        // Uniform [-amp,amp] RMS = amp/sqrt(3) ~= 0.2887*amp.
        expectWithinAbsoluteError(rmsOf(buf, 0), amp / std::sqrt(3.0f), 0.03f,
                                  "uniform-noise RMS");
    }

    void testPinkNoiseTilt()
    {
        beginTest("Pink noise has more low-band than high-band energy");

        AudioGenerator gen;
        gen.prepare(48000.0);
        gen.setNoiseSeed(99);

        juce::AudioBuffer<float> buf(1, 48000);
        gen.generateNoise(buf, AudioGenerator::NoiseType::Pink, 0.8f);

        // Crude spectral tilt: energy of a low-pass-ish first difference vs the
        // signal. Pink noise (falling spectrum) has proportionally less
        // high-frequency energy than white, so the sample-to-sample difference
        // energy is a smaller fraction of total energy.
        const float* d = buf.getReadPointer(0);
        double sigEnergy = 0.0, diffEnergy = 0.0;
        for (int i = 1; i < buf.getNumSamples(); ++i)
        {
            sigEnergy += (double) d[i] * d[i];
            const double diff = (double) d[i] - d[i - 1];
            diffEnergy += diff * diff;
        }
        const double hfRatio = diffEnergy / juce::jmax(1.0e-9, sigEnergy);
        // White noise gives hfRatio ~= 2.0; pink is markedly lower.
        expect(hfRatio < 1.0, "pink HF/total ratio is low (got "
                              + juce::String(hfRatio, 3) + ")");
    }

    void testNoiseDecorrelated()
    {
        beginTest("Per-channel noise is decorrelated");

        AudioGenerator gen;
        gen.prepare(44100.0);
        gen.setNoiseSeed(7);

        juce::AudioBuffer<float> buf(2, 44100);
        gen.generateNoise(buf, AudioGenerator::NoiseType::White, 0.9f);

        const float* l = buf.getReadPointer(0);
        const float* r = buf.getReadPointer(1);
        double dot = 0.0, el = 0.0, er = 0.0;
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            dot += (double) l[i] * r[i];
            el += (double) l[i] * l[i];
            er += (double) r[i] * r[i];
        }
        const double corr = dot / std::sqrt(juce::jmax(1.0e-9, el * er));
        expect(std::abs(corr) < 0.05, "cross-correlation near zero (got "
                                      + juce::String(corr, 4) + ")");
    }

    void testDeterministicSeed()
    {
        beginTest("Same seed -> identical noise (reproducible tests)");

        juce::AudioBuffer<float> a(1, 8192), b(1, 8192);

        AudioGenerator g1; g1.prepare(44100.0); g1.setNoiseSeed(42);
        g1.generateNoise(a, AudioGenerator::NoiseType::White, 0.5f);

        AudioGenerator g2; g2.prepare(44100.0); g2.setNoiseSeed(42);
        g2.generateNoise(b, AudioGenerator::NoiseType::White, 0.5f);

        float maxDiff = 0.0f;
        for (int i = 0; i < a.getNumSamples(); ++i)
            maxDiff = juce::jmax(maxDiff, std::abs(a.getReadPointer(0)[i] - b.getReadPointer(0)[i]));
        expectLessThan(maxDiff, 1.0e-7f, "identical output for identical seed");
    }

    void testNoNaNOrClip()
    {
        beginTest("No NaN/inf produced by tone or noise");

        AudioGenerator gen;
        gen.prepare(44100.0);
        gen.setNoiseSeed(5);

        juce::AudioBuffer<float> tone(2, 4096), noise(2, 4096);
        gen.generateTone(tone, AudioGenerator::Waveform::Triangle, 777.0, 1.0f);
        gen.generateNoise(noise, AudioGenerator::NoiseType::Pink, 1.0f);

        for (auto* buf : { &tone, &noise })
            for (int ch = 0; ch < buf->getNumChannels(); ++ch)
            {
                const float* d = buf->getReadPointer(ch);
                for (int i = 0; i < buf->getNumSamples(); ++i)
                    expect(std::isfinite(d[i]), "sample is finite");
            }
    }
};

static AudioGeneratorTests audioGeneratorTests;
