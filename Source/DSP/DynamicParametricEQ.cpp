/*
  ==============================================================================

    DynamicParametricEQ.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "DynamicParametricEQ.h"
#include <cmath>
#include <iostream>

//==============================================================================
DynamicParametricEQ::DynamicParametricEQ()
{
}

DynamicParametricEQ::~DynamicParametricEQ()
{
}

//==============================================================================
void DynamicParametricEQ::prepare(double sampleRate, int maxBlockSize)
{
    m_sampleRate = sampleRate;
    m_maxBlockSize = maxBlockSize;

    // Prepare existing band states
    const juce::ScopedLock sl(m_parameterLock);
    for (auto& band : m_bandStates)
    {
        for (auto& filter : band.filters)
            filter.reset();
        band.needsUpdate = true;
    }

    // Force coefficient update
    m_parametersChanged.store(true);
}

void DynamicParametricEQ::reset()
{
    for (auto& band : m_bandStates)
    {
        for (auto& filter : band.filters)
            filter.reset();
    }
}

//==============================================================================
void DynamicParametricEQ::setParameters(const Parameters& newParams)
{
    const juce::ScopedLock sl(m_parameterLock);

    // Check if parameters actually changed
    if (m_parameters == newParams)
        return;

    m_parameters = newParams;

    // Resize band states if needed (may allocate - message thread only)
    while (m_bandStates.size() < newParams.bands.size())
    {
        BandState newBand;
        m_bandStates.push_back(std::move(newBand));
    }

    // Update band parameters
    for (size_t i = 0; i < newParams.bands.size(); ++i)
    {
        if (m_bandStates[i].params != newParams.bands[i])
        {
            m_bandStates[i].params = newParams.bands[i];
            m_bandStates[i].needsUpdate = true;
        }
    }

    // Update output gain
    m_outputGainLinear = juce::Decibels::decibelsToGain(newParams.outputGain);

    m_parametersChanged.store(true);
}

DynamicParametricEQ::Parameters DynamicParametricEQ::getParameters() const
{
    const juce::ScopedLock sl(m_parameterLock);
    return m_parameters;
}

//==============================================================================
void DynamicParametricEQ::applyEQ(juce::AudioBuffer<float>& buffer)
{
    if (!isPrepared())
        return;

    auto block = juce::dsp::AudioBlock<float>(buffer);
    applyEQ(block);
}

void DynamicParametricEQ::applyEQ(juce::dsp::AudioBlock<float>& block)
{
    if (!isPrepared())
        return;

    // Update coefficients if needed
    if (m_parametersChanged.exchange(false))
        updateCoefficients();

    const auto numChannels = static_cast<int>(block.getNumChannels());
    const auto numSamples = block.getNumSamples();

    // Process each enabled band
    for (size_t bandIdx = 0; bandIdx < m_parameters.bands.size(); ++bandIdx)
    {
        if (bandIdx >= m_bandStates.size())
            break;

        auto& band = m_bandStates[bandIdx];
        if (!band.params.enabled || band.coefficients == nullptr)
            continue;

        // Process each channel
        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            auto* channelData = block.getChannelPointer(static_cast<size_t>(ch));
            for (size_t i = 0; i < numSamples; ++i)
            {
                channelData[i] = band.filters[ch].processSample(channelData[i]);
            }
        }
    }

    // Apply output gain
    if (std::abs(m_outputGainLinear - 1.0f) > 0.0001f)
    {
        block.multiplyBy(m_outputGainLinear);
    }
}

//==============================================================================
void DynamicParametricEQ::updateCoefficients()
{
    // Copy parameters for coefficient calculation
    Parameters params;
    {
        const juce::ScopedLock sl(m_parameterLock);
        params = m_parameters;
        m_outputGainLinear = juce::Decibels::decibelsToGain(params.outputGain);
    }

    // Update coefficients for each band that needs it
    for (size_t i = 0; i < params.bands.size() && i < m_bandStates.size(); ++i)
    {
        auto& band = m_bandStates[i];
        if (band.needsUpdate)
        {
            updateBandCoefficients(band);
            band.needsUpdate = false;
        }
    }

    m_cachedParams = params;
}

void DynamicParametricEQ::updateBandCoefficients(BandState& band)
{
    band.coefficients = createCoefficients(band.params);

    if (band.coefficients != nullptr)
    {
        for (auto& filter : band.filters)
        {
            filter.coefficients = band.coefficients;
        }
    }
}

DynamicParametricEQ::IIRCoefficients::Ptr DynamicParametricEQ::createCoefficients(
    const BandParameters& params) const
{
    if (m_sampleRate <= 0)
        return nullptr;

    // Clamp frequency to safe range (avoid Nyquist issues)
    const float nyquistLimit = static_cast<float>(m_sampleRate * 0.49);
    const float freq = juce::jlimit(MIN_FREQUENCY, nyquistLimit, params.frequency);
    const float q = juce::jlimit(MIN_Q, MAX_Q, params.q);
    const float gainDb = juce::jlimit(MIN_GAIN, MAX_GAIN, params.gain);

    switch (params.filterType)
    {
        case FilterType::Bell:
            return IIRCoefficients::makePeakFilter(m_sampleRate, freq, q,
                juce::Decibels::decibelsToGain(gainDb));

        case FilterType::LowShelf:
            return IIRCoefficients::makeLowShelf(m_sampleRate, freq, q,
                juce::Decibels::decibelsToGain(gainDb));

        case FilterType::HighShelf:
            return IIRCoefficients::makeHighShelf(m_sampleRate, freq, q,
                juce::Decibels::decibelsToGain(gainDb));

        case FilterType::LowCut:
            return IIRCoefficients::makeHighPass(m_sampleRate, freq, q);

        case FilterType::HighCut:
            return IIRCoefficients::makeLowPass(m_sampleRate, freq, q);

        case FilterType::Notch:
            return IIRCoefficients::makeNotch(m_sampleRate, freq, q);

        case FilterType::Bandpass:
            return IIRCoefficients::makeBandPass(m_sampleRate, freq, q);

        default:
            return nullptr;
    }
}

//==============================================================================
void DynamicParametricEQ::getFrequencyResponse(float* magnitudes, int numPoints,
                                                double startFreq, double endFreq,
                                                bool useLogScale) const
{
    if (magnitudes == nullptr || numPoints <= 0)
        return;

    // Initialize to 0 dB (flat response)
    for (int i = 0; i < numPoints; ++i)
        magnitudes[i] = 0.0f;

    // Hold lock for entire calculation to ensure consistent state between
    // m_bandStates and m_parameters. This is acceptable since:
    // 1. getFrequencyResponse() is only called from UI thread for visualization
    // 2. The calculation is relatively fast
    // 3. Audio thread only briefly holds lock when checking for parameter changes
    const juce::ScopedLock sl(m_parameterLock);

    float outputGainDb = m_parameters.outputGain;

    // Debug: Always log to stderr (DBG is stripped in Release)
    std::cerr << "[EQ-RESP] getFrequencyResponse called: bandStates=" << m_bandStates.size()
              << " sampleRate=" << m_sampleRate
              << " numPoints=" << numPoints << std::endl;

    int bandCount = 0;

    // Calculate frequency for each point
    for (int i = 0; i < numPoints; ++i)
    {
        double freq;
        if (useLogScale)
        {
            // Logarithmic spacing
            double t = static_cast<double>(i) / static_cast<double>(numPoints - 1);
            freq = startFreq * std::pow(endFreq / startFreq, t);
        }
        else
        {
            // Linear spacing
            freq = startFreq + (endFreq - startFreq) * i / (numPoints - 1);
        }

        // Calculate combined response at this frequency
        std::complex<double> response(1.0, 0.0);

        for (size_t bandIdx = 0; bandIdx < m_bandStates.size(); ++bandIdx)
        {
            const auto& band = m_bandStates[bandIdx];
            if (!band.params.enabled || band.coefficients == nullptr)
                continue;

            if (i == 0) bandCount++;  // Count on first iteration

            auto bandResponse = getFilterResponse(band, freq);
            response *= bandResponse;
        }

        // Convert to dB
        double magnitude = std::abs(response);
        magnitudes[i] = static_cast<float>(20.0 * std::log10(std::max(magnitude, 1e-10)));
    }

    // Debug: Always log magnitude values
    if (numPoints > 0)
    {
        float minMag = magnitudes[0], maxMag = magnitudes[0];
        for (int i = 1; i < numPoints; ++i)
        {
            if (magnitudes[i] < minMag) minMag = magnitudes[i];
            if (magnitudes[i] > maxMag) maxMag = magnitudes[i];
        }
        std::cerr << "[EQ-RESP] activeBands=" << bandCount
                  << " magnitudes: min=" << minMag << "dB max=" << maxMag << "dB" << std::endl;
    }

    // Add output gain
    for (int i = 0; i < numPoints; ++i)
        magnitudes[i] += outputGainDb;
}

float DynamicParametricEQ::getFrequencyResponseAt(double frequency) const
{
    // Hold lock for entire calculation to ensure consistent state
    const juce::ScopedLock sl(m_parameterLock);

    // Calculate combined response at this frequency
    std::complex<double> response(1.0, 0.0);

    for (size_t bandIdx = 0; bandIdx < m_bandStates.size(); ++bandIdx)
    {
        const auto& band = m_bandStates[bandIdx];
        if (!band.params.enabled || band.coefficients == nullptr)
            continue;

        auto bandResponse = getFilterResponse(band, frequency);
        response *= bandResponse;
    }

    // Convert to dB and add output gain
    double magnitude = std::abs(response);
    return static_cast<float>(20.0 * std::log10(std::max(magnitude, 1e-10))) + m_parameters.outputGain;
}

std::complex<double> DynamicParametricEQ::getFilterResponse(const BandState& band,
                                                             double frequency) const
{
    if (band.coefficients == nullptr || m_sampleRate <= 0)
        return std::complex<double>(1.0, 0.0);

    // Get filter coefficients
    const auto& coeffs = band.coefficients->coefficients;
    // JUCE stores biquad coefficients as 5 elements (a0 is skipped during normalization):
    // [b0/a0, b1/a0, b2/a0, a1/a0, a2/a0]
    // See juce_IIRFilter_Impl.h assignImpl() which skips i == a0Index
    if (coeffs.size() < 5)
        return std::complex<double>(1.0, 0.0);

    // IIR biquad coefficients - already normalized by a0
    const double b0 = coeffs[0];
    const double b1 = coeffs[1];
    const double b2 = coeffs[2];
    // a0 is normalized to 1 (not stored)
    const double a1 = coeffs[3];
    const double a2 = coeffs[4];

    // Calculate frequency response using z-transform
    // H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
    // At frequency f: z = e^(j*2*pi*f/fs)

    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / m_sampleRate;
    std::complex<double> z = std::exp(std::complex<double>(0.0, omega));
    std::complex<double> z_inv = 1.0 / z;
    std::complex<double> z_inv2 = z_inv * z_inv;

    std::complex<double> numerator = b0 + b1 * z_inv + b2 * z_inv2;
    std::complex<double> denominator = 1.0 + a1 * z_inv + a2 * z_inv2;

    return numerator / denominator;
}

//==============================================================================
int DynamicParametricEQ::addBand(float frequency, FilterType filterType)
{
    const juce::ScopedLock sl(m_parameterLock);

    if (static_cast<int>(m_parameters.bands.size()) >= MAX_BANDS)
        return -1;

    BandParameters newBand;
    newBand.frequency = frequency;
    newBand.filterType = filterType;
    newBand.gain = 0.0f;
    newBand.q = DEFAULT_Q;
    newBand.enabled = true;

    m_parameters.bands.push_back(newBand);

    // Add corresponding band state
    BandState newState;
    newState.params = newBand;
    newState.needsUpdate = true;
    m_bandStates.push_back(std::move(newState));

    m_parametersChanged.store(true);

    return static_cast<int>(m_parameters.bands.size()) - 1;
}

bool DynamicParametricEQ::removeBand(int index)
{
    const juce::ScopedLock sl(m_parameterLock);

    if (index < 0 || index >= static_cast<int>(m_parameters.bands.size()))
        return false;

    m_parameters.bands.erase(m_parameters.bands.begin() + index);

    if (index < static_cast<int>(m_bandStates.size()))
        m_bandStates.erase(m_bandStates.begin() + index);

    m_parametersChanged.store(true);
    return true;
}

bool DynamicParametricEQ::setBandParameters(int index, const BandParameters& params)
{
    const juce::ScopedLock sl(m_parameterLock);

    if (index < 0 || index >= static_cast<int>(m_parameters.bands.size()))
        return false;

    if (m_parameters.bands[index] != params)
    {
        m_parameters.bands[index] = params;

        if (index < static_cast<int>(m_bandStates.size()))
        {
            m_bandStates[index].params = params;
            m_bandStates[index].needsUpdate = true;
        }

        m_parametersChanged.store(true);
    }

    return true;
}

DynamicParametricEQ::BandParameters DynamicParametricEQ::getBandParameters(int index) const
{
    const juce::ScopedLock sl(m_parameterLock);

    if (index < 0 || index >= static_cast<int>(m_parameters.bands.size()))
        return BandParameters();

    return m_parameters.bands[index];
}

int DynamicParametricEQ::getNumBands() const
{
    const juce::ScopedLock sl(m_parameterLock);
    return static_cast<int>(m_parameters.bands.size());
}

bool DynamicParametricEQ::setBandEnabled(int index, bool enabled)
{
    const juce::ScopedLock sl(m_parameterLock);

    if (index < 0 || index >= static_cast<int>(m_parameters.bands.size()))
        return false;

    if (m_parameters.bands[index].enabled != enabled)
    {
        m_parameters.bands[index].enabled = enabled;

        if (index < static_cast<int>(m_bandStates.size()))
            m_bandStates[index].params.enabled = enabled;

        m_parametersChanged.store(true);
    }

    return true;
}

void DynamicParametricEQ::clearAllBands()
{
    const juce::ScopedLock sl(m_parameterLock);

    m_parameters.bands.clear();
    m_bandStates.clear();
    m_parametersChanged.store(true);
}

//==============================================================================
DynamicParametricEQ::Parameters DynamicParametricEQ::createDefaultPreset()
{
    Parameters params;

    // Low shelf at 100Hz
    BandParameters lowShelf;
    lowShelf.frequency = 100.0f;
    lowShelf.gain = 0.0f;
    lowShelf.q = 0.707f;
    lowShelf.filterType = FilterType::LowShelf;
    lowShelf.enabled = true;
    params.bands.push_back(lowShelf);

    // Mid bell at 1kHz
    BandParameters midBell;
    midBell.frequency = 1000.0f;
    midBell.gain = 0.0f;
    midBell.q = 1.0f;
    midBell.filterType = FilterType::Bell;
    midBell.enabled = true;
    params.bands.push_back(midBell);

    // High shelf at 8kHz
    BandParameters highShelf;
    highShelf.frequency = 8000.0f;
    highShelf.gain = 0.0f;
    highShelf.q = 0.707f;
    highShelf.filterType = FilterType::HighShelf;
    highShelf.enabled = true;
    params.bands.push_back(highShelf);

    params.outputGain = 0.0f;

    return params;
}

DynamicParametricEQ::Parameters DynamicParametricEQ::createFlatPreset()
{
    Parameters params;
    params.outputGain = 0.0f;
    return params;
}

//==============================================================================
void DynamicParametricEQ::updateCoefficientsForVisualization()
{
    // Force coefficient update for ALL bands for visualization purposes.
    // This is necessary because during drag operations, setParameters() may
    // compare against already-cached band parameters and not set needsUpdate.
    // By forcing all bands to update, we ensure getFrequencyResponse() always
    // returns accurate values reflecting the current m_parameters.

    const juce::ScopedLock sl(m_parameterLock);

    // DEBUG: Output to stderr for Release builds
    std::cerr << "[EQ-VIZ] updateCoeffsForViz: paramBands=" << m_parameters.bands.size()
              << " bandStates=" << m_bandStates.size()
              << " sampleRate=" << m_sampleRate << std::endl;

    // CRITICAL FIX: Ensure m_bandStates is properly sized BEFORE the loop
    // setParameters() should have done this, but we double-check here
    while (m_bandStates.size() < m_parameters.bands.size())
    {
        BandState newBand;
        m_bandStates.push_back(std::move(newBand));
    }

    // Force all bands to recalculate coefficients
    for (size_t i = 0; i < m_parameters.bands.size() && i < m_bandStates.size(); ++i)
    {
        // Sync band state params with current parameters
        m_bandStates[i].params = m_parameters.bands[i];
        // Force coefficient recalculation
        updateBandCoefficients(m_bandStates[i]);

        // DEBUG: Output to stderr for Release builds
        std::cerr << "[EQ-VIZ]   Band " << i
                  << ": freq=" << m_bandStates[i].params.frequency
                  << " gain=" << m_bandStates[i].params.gain
                  << " enabled=" << (m_bandStates[i].params.enabled ? 1 : 0)
                  << " hasCoeffs=" << (m_bandStates[i].coefficients != nullptr ? 1 : 0);

        // Print actual coefficient values to debug flat response
        if (m_bandStates[i].coefficients != nullptr)
        {
            const auto& coeffs = m_bandStates[i].coefficients->coefficients;
            if (coeffs.size() >= 6)
            {
                std::cerr << " [b0=" << coeffs[0] << " b1=" << coeffs[1] << " b2=" << coeffs[2]
                          << " a0=" << coeffs[3] << " a1=" << coeffs[4] << " a2=" << coeffs[5] << "]";
            }
            else
            {
                std::cerr << " [coeffs.size=" << coeffs.size() << " - INVALID!]";
            }
        }
        std::cerr << std::endl;
    }

    // Update output gain
    m_outputGainLinear = juce::Decibels::decibelsToGain(m_parameters.outputGain);

    // Clear the changed flag since we've updated everything
    m_parametersChanged.store(false);
}
