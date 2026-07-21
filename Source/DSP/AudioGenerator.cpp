/*
  ==============================================================================

    AudioGenerator.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "AudioGenerator.h"
#include <cmath>

void AudioGenerator::prepare(double sampleRate)
{
    m_sampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
    reset();
}

void AudioGenerator::reset()
{
    m_phase = 0.0;
    for (auto& s : m_pinkState)
        s = PinkState{};
}

void AudioGenerator::setNoiseSeed(juce::int64 baseSeed)
{
    m_hasSeed = true;
    m_noiseSeed = baseSeed;
}

void AudioGenerator::generateTone(juce::AudioBuffer<float>& buffer, Waveform waveform,
                                  double frequencyHz, float amplitude)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0)
        return;

    const double increment = frequencyHz / m_sampleRate;   // cycles per sample
    const float amp = juce::jlimit(0.0f, 1.0f, amplitude);

    // Render the mono waveform once, then copy to every channel (a tone is
    // identical across channels).
    double phase = m_phase;
    float* out = buffer.getWritePointer(0);

    for (int i = 0; i < numSamples; ++i)
    {
        float value = 0.0f;
        const float t = (float) phase;   // [0, 1)

        switch (waveform)
        {
            case Waveform::Sine:
                value = std::sin(juce::MathConstants<float>::twoPi * t);
                break;
            case Waveform::Square:
                value = (t < 0.5f) ? 1.0f : -1.0f;
                break;
            case Waveform::Saw:
                value = 2.0f * t - 1.0f;                       // ramp -1 -> +1
                break;
            case Waveform::Triangle:
                value = 4.0f * std::abs(t - std::floor(t + 0.5f)) - 1.0f;
                break;
        }

        out[i] = value * amp;

        phase += increment;
        if (phase >= 1.0)
            phase -= std::floor(phase);
    }

    m_phase = phase - std::floor(phase);

    for (int ch = 1; ch < numChannels; ++ch)
        buffer.copyFrom(ch, 0, buffer, 0, 0, numSamples);
}

void AudioGenerator::generateNoise(juce::AudioBuffer<float>& buffer, NoiseType noiseType,
                                   float amplitude)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0)
        return;

    const float amp = juce::jlimit(0.0f, 1.0f, amplitude);

    if ((int) m_pinkState.size() < numChannels)
        m_pinkState.resize((size_t) numChannels);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        // Independent stream per channel so stereo/multichannel noise is
        // decorrelated. Deterministic when a seed was set (tests).
        juce::Random rng = m_hasSeed ? juce::Random(m_noiseSeed + ch)
                                     : juce::Random(juce::Random::getSystemRandom().nextInt64());

        float* out = buffer.getWritePointer(ch);
        auto& ps = m_pinkState[(size_t) ch].b;

        for (int i = 0; i < numSamples; ++i)
        {
            const float white = rng.nextFloat() * 2.0f - 1.0f;   // [-1, 1)

            if (noiseType == NoiseType::White)
            {
                out[i] = white * amp;
            }
            else
            {
                // Paul Kellet's economy pink-noise filter.
                ps[0] = 0.99886f * ps[0] + white * 0.0555179f;
                ps[1] = 0.99332f * ps[1] + white * 0.0750759f;
                ps[2] = 0.96900f * ps[2] + white * 0.1538520f;
                ps[3] = 0.86650f * ps[3] + white * 0.3104856f;
                ps[4] = 0.55000f * ps[4] + white * 0.5329522f;
                ps[5] = -0.7616f * ps[5] - white * 0.0168980f;
                float pink = ps[0] + ps[1] + ps[2] + ps[3] + ps[4] + ps[5] + ps[6]
                             + white * 0.5362f;
                ps[6] = white * 0.115926f;

                // The filter sums to roughly +/-9; scale to unity-ish then clamp
                // so a requested amplitude is never exceeded.
                pink *= 0.11f;
                out[i] = juce::jlimit(-amp, amp, pink * amp);
            }
        }
    }
}
