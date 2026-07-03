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
      m_currentParams(Parameters::createNeutral())
{
}

//==============================================================================
// Public Methods

void ParametricEQ::prepare(double sampleRate, int maxSamplesPerBlock)
{
    // Message thread only: allocates. Take the lock so a concurrent audio
    // callback (device restart edge) never processes half-prepared filters.
    const juce::ScopedLock sl(m_lock);

    m_sampleRate = sampleRate;
    m_maxSamplesPerBlock = maxSamplesPerBlock;

    // C2 FIX: prepare once for the app-wide channel ceiling so applyEQ() never
    // has to (re)prepare -- ProcessorDuplicator processes however many channels
    // the buffer actually has, up to the prepared count.
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(maxSamplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(MAX_CHANNELS);

    m_lowShelf.prepare(spec);
    m_midPeak.prepare(spec);
    m_highShelf.prepare(spec);

    // Seed the shared states with real (flat) biquads so their coefficient
    // arrays have their final size; adoptPendingCoefficients() can then copy
    // element-wise without ever resizing on the audio thread.
    if (auto flat = Coefficients::makePeakFilter(sampleRate, 1000.0, 0.707f, 1.0f))
    {
        *m_lowShelf.state  = *flat;
        *m_midPeak.state   = *flat;
        *m_highShelf.state = *flat;
    }
    m_hasCoefficients = false;   // Flat until setParameters() publishes real ones
    m_pendingReady = false;

    // Re-stage coefficients for the (possibly new) sample rate.
    // juce::CriticalSection is recursive, so re-taking m_lock inside
    // buildPendingCoefficients() while prepare() holds it is safe.
    if (m_haveParams)
        buildPendingCoefficients(m_currentParams);
}

void ParametricEQ::setParameters(const Parameters& params)
{
    // Message thread only (allocates).
    jassert(m_sampleRate > 0.0);
    if (m_sampleRate <= 0.0)
        return;

    if (m_haveParams &&
        juce::exactlyEqual(params.low.frequency, m_currentParams.low.frequency) &&
        juce::exactlyEqual(params.low.gain, m_currentParams.low.gain) &&
        juce::exactlyEqual(params.low.q, m_currentParams.low.q) &&
        juce::exactlyEqual(params.mid.frequency, m_currentParams.mid.frequency) &&
        juce::exactlyEqual(params.mid.gain, m_currentParams.mid.gain) &&
        juce::exactlyEqual(params.mid.q, m_currentParams.mid.q) &&
        juce::exactlyEqual(params.high.frequency, m_currentParams.high.frequency) &&
        juce::exactlyEqual(params.high.gain, m_currentParams.high.gain) &&
        juce::exactlyEqual(params.high.q, m_currentParams.high.q))
    {
        return;   // No change
    }

    m_currentParams = params;
    m_haveParams = true;

    buildPendingCoefficients(params);
}

void ParametricEQ::applyEQ(juce::AudioBuffer<float>& buffer)
{
    if (m_sampleRate <= 0.0)
    {
        jassertfalse;  // prepare() must be called first
        return;
    }

    // Real-time thread: never block. If setParameters()/prepare() holds the
    // lock, skip the EQ for this block (one dry block is inaudible; blocking
    // or racing the coefficient write is not acceptable).
    const juce::ScopedTryLock stl(m_lock);
    if (! stl.isLocked())
        return;

    if (m_pendingReady)
    {
        adoptPendingCoefficients();
        m_pendingReady = false;
        m_hasCoefficients = true;
    }

    if (! m_hasCoefficients)
        return;   // Neutral until first real parameters arrive

    // Prepared for MAX_CHANNELS; anything beyond that would read past the
    // duplicator's per-channel states, so pass it through untouched.
    if (buffer.getNumChannels() > MAX_CHANNELS)
        return;

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

void ParametricEQ::buildPendingCoefficients(const Parameters& params)
{
    // Message thread: allocation happens HERE, never on the audio thread.
    // Calculate Nyquist frequency with a 0.49 safety margin to avoid filter
    // instability at the edge.
    const float nyquistLimit = static_cast<float>(m_sampleRate * 0.49);

    auto low = Coefficients::makeLowShelf(
        m_sampleRate,
        juce::jlimit(20.0f, nyquistLimit, params.low.frequency),
        params.low.q,
        dbToLinearGain(params.low.gain));

    auto mid = Coefficients::makePeakFilter(
        m_sampleRate,
        juce::jlimit(20.0f, nyquistLimit, params.mid.frequency),
        params.mid.q,
        dbToLinearGain(params.mid.gain));

    auto high = Coefficients::makeHighShelf(
        m_sampleRate,
        juce::jlimit(20.0f, nyquistLimit, params.high.frequency),
        params.high.q,
        dbToLinearGain(params.high.gain));

    if (low == nullptr || mid == nullptr || high == nullptr)
        return;   // Degenerate parameters; keep last-good coefficients

    // Publish under the lock. The Ptr assignments (and the release of any
    // previously-pending, never-adopted set) happen on this thread only.
    const juce::ScopedLock sl(m_lock);
    m_pendingLow  = low;
    m_pendingMid  = mid;
    m_pendingHigh = high;
    m_pendingReady = true;
}

void ParametricEQ::adoptPendingCoefficients()
{
    // Audio thread, m_lock held. Copy raw coefficient VALUES into the live
    // shared states -- element-wise, no allocation, no refcount traffic.
    auto adopt = [](const Coefficients::Ptr& src, Coefficients& dst)
    {
        if (src == nullptr)
            return;
        auto& from = src->coefficients;
        auto& to   = dst.coefficients;
        if (from.size() != to.size())
        {
            jassertfalse;   // Biquad sizes should always match (seeded in prepare)
            return;
        }
        for (int i = 0; i < from.size(); ++i)
            to.setUnchecked(i, from[i]);
    };

    adopt(m_pendingLow,  *m_lowShelf.state);
    adopt(m_pendingMid,  *m_midPeak.state);
    adopt(m_pendingHigh, *m_highShelf.state);
}

float ParametricEQ::dbToLinearGain(float gainDB)
{
    return std::pow(10.0f, gainDB / 20.0f);
}
