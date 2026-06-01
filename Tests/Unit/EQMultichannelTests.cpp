/*
  ==============================================================================

    EQMultichannelTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Regression tests for the QA Pass-2 EQ defects:
      - H3: ParametricEQ must process ALL channels (not just 2).
      - H4: DynamicParametricEQ must process ALL channels (not just 2).
      - §6.6 / M10: DynamicParametricEQ::getFrequencyResponseAt must read JUCE's
        5-element coefficient storage correctly (a non-flat band -> non-zero dB).

    These fail before the Pass-2 fixes (channels 2..3 left untouched / response
    flat) and pass after.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/ParametricEQ.h"
#include "DSP/DynamicParametricEQ.h"

namespace
{
// Fill every channel of a buffer with a mid-frequency sine so an EQ band that
// targets that frequency will measurably change the signal energy.
void fillSine(juce::AudioBuffer<float>& buffer, double sampleRate, double freq, float amplitude)
{
    const int n = buffer.getNumSamples();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer(ch);
        for (int i = 0; i < n; ++i)
            d[i] = amplitude * static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * freq
                                                           * static_cast<double>(i) / sampleRate));
    }
}

float channelRms(const juce::AudioBuffer<float>& buffer, int ch)
{
    const int n = buffer.getNumSamples();
    double sum = 0.0;
    const float* d = buffer.getReadPointer(ch);
    for (int i = 0; i < n; ++i)
        sum += static_cast<double>(d[i]) * d[i];
    return static_cast<float>(std::sqrt(sum / juce::jmax(1, n)));
}
} // namespace

//==============================================================================
class ParametricEQMultichannelTests : public juce::UnitTest
{
public:
    ParametricEQMultichannelTests()
        : juce::UnitTest("ParametricEQ Multichannel", "DSP") {}

    void runTest() override
    {
        beginTest("H3: 4-channel buffer has channels 2 and 3 processed");

        constexpr double sr = 48000.0;
        constexpr int numSamples = 2048;
        constexpr int numChannels = 4;

        ParametricEQ eq;
        eq.prepare(sr, numSamples);

        // Strong mid-band cut at 1 kHz; the signal is a 1 kHz sine, so any
        // processed channel loses substantial energy.
        ParametricEQ::Parameters params = ParametricEQ::Parameters::createNeutral();
        params.mid.frequency = 1000.0f;
        params.mid.gain = -24.0f;
        params.mid.q = 1.0f;

        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        fillSine(buffer, sr, 1000.0, 0.5f);

        float rmsBefore = channelRms(buffer, 0);  // identical across channels

        eq.applyEQ(buffer, params);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float rmsAfter = channelRms(buffer, ch);
            expect(rmsAfter < rmsBefore * 0.5f,
                   "Channel " + juce::String(ch)
                       + " must be attenuated by the EQ (was unprocessed before H3 fix)");
        }
    }
};

//==============================================================================
class DynamicParametricEQMultichannelTests : public juce::UnitTest
{
public:
    DynamicParametricEQMultichannelTests()
        : juce::UnitTest("DynamicParametricEQ Multichannel", "DSP") {}

    void runTest() override
    {
        beginTest("H4: 4-channel buffer has channels 2 and 3 processed");

        constexpr double sr = 48000.0;
        constexpr int numSamples = 2048;
        constexpr int numChannels = 4;

        DynamicParametricEQ eq;
        eq.prepare(sr, numSamples);

        DynamicParametricEQ::Parameters params;
        DynamicParametricEQ::BandParameters band;
        band.frequency = 1000.0f;
        band.gain = -24.0f;
        band.q = 1.0f;
        band.filterType = DynamicParametricEQ::FilterType::Bell;
        band.enabled = true;
        params.bands.push_back(band);
        eq.setParameters(params);

        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        fillSine(buffer, sr, 1000.0, 0.5f);
        float rmsBefore = channelRms(buffer, 0);

        eq.applyEQ(buffer);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float rmsAfter = channelRms(buffer, ch);
            expect(rmsAfter < rmsBefore * 0.6f,
                   "Channel " + juce::String(ch)
                       + " must be attenuated by the EQ (was unprocessed before H4 fix)");
        }

        beginTest("Mono and stereo still process correctly");
        for (int chCount : { 1, 2 })
        {
            juce::AudioBuffer<float> b(chCount, numSamples);
            fillSine(b, sr, 1000.0, 0.5f);
            float before = channelRms(b, 0);
            eq.applyEQ(b);
            expect(channelRms(b, chCount - 1) < before * 0.6f,
                   "Last channel of a " + juce::String(chCount) + "-ch buffer must be processed");
        }
    }
};

//==============================================================================
class DynamicParametricEQResponseTests : public juce::UnitTest
{
public:
    DynamicParametricEQResponseTests()
        : juce::UnitTest("DynamicParametricEQ FrequencyResponse", "DSP") {}

    void runTest() override
    {
        beginTest("§6.6/M10: 5-element coeff read yields non-flat response for a boosted band");

        constexpr double sr = 48000.0;
        DynamicParametricEQ eq;
        eq.prepare(sr, 1024);

        DynamicParametricEQ::Parameters params;
        DynamicParametricEQ::BandParameters band;
        band.frequency = 1000.0f;
        band.gain = 12.0f;  // +12 dB bell at 1 kHz
        band.q = 1.0f;
        band.filterType = DynamicParametricEQ::FilterType::Bell;
        band.enabled = true;
        params.bands.push_back(band);
        eq.setParameters(params);
        eq.updateCoefficientsForVisualization();

        float atCenter = eq.getFrequencyResponseAt(1000.0);
        float atLow = eq.getFrequencyResponseAt(50.0);

        // A +12 dB bell must show a clear boost at its centre vs the skirt.
        // Before the coeff-storage fix the response read garbage / stayed flat.
        expect(atCenter > 6.0f,
               "Response at centre should be a strong boost, got " + juce::String(atCenter) + " dB");
        expect(atCenter - atLow > 4.0f,
               "Centre should be well above the low skirt (centre=" + juce::String(atCenter)
                   + " low=" + juce::String(atLow) + ")");
        expect(std::isfinite(atCenter) && std::isfinite(atLow),
               "Response values must be finite");
    }
};

//==============================================================================
static ParametricEQMultichannelTests parametricEQMultichannelTests;
static DynamicParametricEQMultichannelTests dynamicParametricEQMultichannelTests;
static DynamicParametricEQResponseTests dynamicParametricEQResponseTests;
