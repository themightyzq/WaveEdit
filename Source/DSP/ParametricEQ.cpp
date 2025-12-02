/*
  ==============================================================================

    ParametricEQ.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "ParametricEQ.h"
#include <cmath>

//==============================================================================
// Parameter Defaults

ParametricEQ::Parameters ParametricEQ::Parameters::createNeutral()
{
    Parameters params;

    // Low shelf (100 Hz, 0 dB, Q=0.707)
    params.low.frequency = 100.0f;
    params.low.gain = 0.0f;
    params.low.q = 0.707f;

    // Mid peak (1000 Hz, 0 dB, Q=0.707)
    params.mid.frequency = 1000.0f;
    params.mid.gain = 0.0f;
    params.mid.q = 0.707f;

    // High shelf (10000 Hz, 0 dB, Q=0.707)
    params.high.frequency = 10000.0f;
    params.high.gain = 0.0f;
    params.high.q = 0.707f;

    return params;
}

//==============================================================================
// Constructor / Destructor

ParametricEQ::ParametricEQ()
    : m_sampleRate(0.0),
      m_maxSamplesPerBlock(0),
      m_lastNumChannels(0),
      m_currentParams(Parameters::createNeutral()),
      m_coefficientsNeedUpdate(true)
{
}

//==============================================================================
// Public Methods

void ParametricEQ::prepare(double sampleRate, int maxSamplesPerBlock)
{
    m_sampleRate = sampleRate;
    m_maxSamplesPerBlock = maxSamplesPerBlock;

    // Prepare DSP processors
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(maxSamplesPerBlock);
    spec.numChannels = 2;  // Stereo (default)

    m_lowShelf.prepare(spec);
    m_midPeak.prepare(spec);
    m_highShelf.prepare(spec);

    m_lastNumChannels = 2;  // Track initial channel count

    // Force coefficient update on next applyEQ call
    m_coefficientsNeedUpdate = true;
}

void ParametricEQ::applyEQ(juce::AudioBuffer<float>& buffer, const Parameters& params)
{

    if (m_sampleRate <= 0.0)
    {
        jassertfalse;  // prepare() must be called first
        return;
    }

    // Re-prepare filters ONLY if channel count has changed
    // CRITICAL: Calling prepare() unconditionally resets filter state!
    // ProcessorDuplicator needs to match the buffer channel count exactly
    int numChannels = buffer.getNumChannels();
    if (numChannels != m_lastNumChannels)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = m_sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(buffer.getNumSamples());
        spec.numChannels = static_cast<juce::uint32>(numChannels);

        m_lowShelf.prepare(spec);
        m_midPeak.prepare(spec);
        m_highShelf.prepare(spec);

        m_lastNumChannels = numChannels;
        m_coefficientsNeedUpdate = true;  // Force coefficient update after prepare
    }

    // Check if parameters have changed
    if (params.low.frequency != m_currentParams.low.frequency ||
        params.low.gain != m_currentParams.low.gain ||
        params.low.q != m_currentParams.low.q ||
        params.mid.frequency != m_currentParams.mid.frequency ||
        params.mid.gain != m_currentParams.mid.gain ||
        params.mid.q != m_currentParams.mid.q ||
        params.high.frequency != m_currentParams.high.frequency ||
        params.high.gain != m_currentParams.high.gain ||
        params.high.q != m_currentParams.high.q)
    {
        m_currentParams = params;
        m_coefficientsNeedUpdate = true;
    }

    // Update filter coefficients if needed
    if (m_coefficientsNeedUpdate)
    {
        updateCoefficients(params);
        m_coefficientsNeedUpdate = false;
    }

    // Create DSP block from entire buffer
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    // Apply filters in series: Low -> Mid -> High
    m_lowShelf.process(context);
    m_midPeak.process(context);
    m_highShelf.process(context);
}

//==============================================================================
// Private Methods

void ParametricEQ::updateCoefficients(const Parameters& params)
{
    // Calculate Nyquist frequency (sample rate / 2)
    // Use 0.49 * sampleRate as safety margin to avoid filter instability
    float nyquistLimit = static_cast<float>(m_sampleRate * 0.49);

    // Low shelf filter
    {
        // Clamp frequency to valid range (20 Hz to Nyquist)
        float clampedFreq = juce::jlimit(20.0f, nyquistLimit, params.low.frequency);
        float gainFactor = dbToLinearGain(params.low.gain);

        auto coeffs = Coefficients::makeLowShelf(
            m_sampleRate,
            clampedFreq,
            params.low.q,
            gainFactor
        );

        if (coeffs != nullptr)
        {
            *m_lowShelf.state = *coeffs;  // Copy coefficients
        }
    }

    // Mid peaking filter
    {
        // Clamp frequency to valid range (20 Hz to Nyquist)
        float clampedFreq = juce::jlimit(20.0f, nyquistLimit, params.mid.frequency);
        float gainFactor = dbToLinearGain(params.mid.gain);

        auto coeffs = Coefficients::makePeakFilter(
            m_sampleRate,
            clampedFreq,
            params.mid.q,
            gainFactor
        );

        if (coeffs != nullptr)
        {
            *m_midPeak.state = *coeffs;  // Copy coefficients
        }
    }

    // High shelf filter
    {
        // Clamp frequency to valid range (20 Hz to Nyquist)
        float clampedFreq = juce::jlimit(20.0f, nyquistLimit, params.high.frequency);
        float gainFactor = dbToLinearGain(params.high.gain);

        auto coeffs = Coefficients::makeHighShelf(
            m_sampleRate,
            clampedFreq,
            params.high.q,
            gainFactor
        );

        if (coeffs != nullptr)
        {
            *m_highShelf.state = *coeffs;  // Copy coefficients
        }
    }
}

float ParametricEQ::dbToLinearGain(float gainDB)
{
    return std::pow(10.0f, gainDB / 20.0f);
}
