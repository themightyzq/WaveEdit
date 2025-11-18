/*
  ==============================================================================

    SpectrumAnalyzer.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "SpectrumAnalyzer.h"
#include "../Audio/AudioEngine.h"
#include <cmath>

//==============================================================================
// Display Configuration Constants

namespace
{
    /**
     * FFT Display Scaling Factor
     *
     * Compensates for FFT energy spreading across frequency bins.
     * Without this, typical audio signals would appear too quiet in the display.
     *
     * This value was determined empirically by testing with:
     * - Full-scale sine waves at various frequencies
     * - Pink noise (-12dBFS average)
     * - Typical music and speech content
     *
     * A value of 4.0 provides good visual range for most audio content,
     * mapping typical signals to the -40dB to 0dB display range.
     */
    constexpr float FFT_DISPLAY_SCALE = 4.0f;

    /**
     * Exponential Smoothing Factor (0.0 - 1.0)
     *
     * Controls the responsiveness vs. smoothness tradeoff:
     * - 0.0 = No smoothing (instant response, jittery)
     * - 1.0 = Maximum smoothing (smooth but very slow)
     * - 0.75 = Good balance for real-time visualization
     *
     * Higher values create smoother animations but slower response to transients.
     * This value provides a professional "slick" appearance while maintaining
     * adequate responsiveness to changes in audio content.
     */
    constexpr float SMOOTHING_FACTOR = 0.75f;
}

SpectrumAnalyzer::SpectrumAnalyzer()
    : m_currentFFTSize(FFTSize::SIZE_2048)
    , m_currentWindow(WindowFunction::HANN)
    , m_fftOrder(11) // 2^11 = 2048
    , m_fifoIndex(0)
    , m_nextFFTBlockReady(false)
    , m_audioEngine(nullptr)
    , m_minFrequency(20.0f)
    , m_maxFrequency(20000.0f)
    , m_minDB(-80.0f)
    , m_maxDB(0.0f)
{
    m_sampleRate.store(44100.0);

    // Initialize buffers
    m_fftData.fill(0.0f);
    m_scopeData.fill(0.0f);
    m_peakHold.fill(m_minDB);

    for (int i = 0; i < MAX_FFT_SIZE; ++i)
    {
        m_peakHoldTime[i] = 0;
    }

    // Create FFT and window
    updateFFTConfiguration();

    // Start timer for UI updates (30fps)
    startTimer(1000 / UPDATE_RATE_HZ);
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{
    stopTimer();
}

//==============================================================================
// FFT Configuration

void SpectrumAnalyzer::setFFTSize(FFTSize size)
{
    if (m_currentFFTSize != size)
    {
        m_currentFFTSize = size;
        updateFFTConfiguration();
        reset();
    }
}

void SpectrumAnalyzer::setWindowFunction(WindowFunction window)
{
    if (m_currentWindow != window)
    {
        m_currentWindow = window;
        updateFFTConfiguration();
    }
}

void SpectrumAnalyzer::updateFFTConfiguration()
{
    // Calculate FFT order from size
    int fftSize = static_cast<int>(m_currentFFTSize);
    m_fftOrder = static_cast<int>(std::log2(fftSize));

    // Create new FFT object
    m_fft = std::make_unique<juce::dsp::FFT>(m_fftOrder);

    // Create windowing function
    auto windowType = juce::dsp::WindowingFunction<float>::hann;

    switch (m_currentWindow)
    {
        case WindowFunction::HANN:
            windowType = juce::dsp::WindowingFunction<float>::hann;
            break;
        case WindowFunction::HAMMING:
            windowType = juce::dsp::WindowingFunction<float>::hamming;
            break;
        case WindowFunction::BLACKMAN:
            windowType = juce::dsp::WindowingFunction<float>::blackman;
            break;
        case WindowFunction::RECTANGULAR:
            windowType = juce::dsp::WindowingFunction<float>::rectangular;
            break;
    }

    m_window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, windowType);
}

//==============================================================================
// Audio Data Input

void SpectrumAnalyzer::pushAudioData(const float* buffer, int numSamples)
{
    if (m_nextFFTBlockReady.load())
    {
        return; // Skip if previous FFT block hasn't been consumed yet
    }

    int fftSize = static_cast<int>(m_currentFFTSize);

    // Thread-safe write to FIFO
    const juce::SpinLock::ScopedLockType lock(m_fftLock);

    for (int i = 0; i < numSamples; ++i)
    {
        m_fftData[m_fifoIndex] = buffer[i];
        m_fifoIndex++;

        if (m_fifoIndex == fftSize)
        {
            // We have enough samples for an FFT
            m_fifoIndex = 0;
            m_nextFFTBlockReady.store(true);
            break;
        }
    }
}

void SpectrumAnalyzer::reset()
{
    m_fftData.fill(0.0f);
    m_scopeData.fill(0.0f);
    m_peakHold.fill(m_minDB);
    m_fifoIndex = 0;
    m_nextFFTBlockReady.store(false);

    for (int i = 0; i < MAX_FFT_SIZE; ++i)
    {
        m_peakHoldTime[i] = 0;
    }

    repaint();
}

void SpectrumAnalyzer::setAudioEngine(AudioEngine* audioEngine)
{
    m_audioEngine = audioEngine;

    // Get sample rate from audio engine if available
    if (m_audioEngine)
    {
        double sampleRate = m_audioEngine->getSampleRate();
        m_sampleRate.store(sampleRate > 0.0 ? sampleRate : 44100.0);
    }

    reset();
}

//==============================================================================
// Timer Callback

void SpectrumAnalyzer::timerCallback()
{
    if (m_nextFFTBlockReady.load())
    {
        processFFT();
        m_nextFFTBlockReady.store(false);
        repaint();
    }

    // Update peak hold timers
    bool needsRepaint = false;
    int fftSize = static_cast<int>(m_currentFFTSize);

    for (int i = 0; i < fftSize / 2; ++i)
    {
        if (m_peakHoldTime[i] > 0)
        {
            m_peakHoldTime[i]--;
            if (m_peakHoldTime[i] == 0)
            {
                m_peakHold[i] = m_minDB;
                needsRepaint = true;
            }
        }
    }

    if (needsRepaint)
    {
        repaint();
    }
}

void SpectrumAnalyzer::processFFT()
{
    int fftSize = static_cast<int>(m_currentFFTSize);

    // Copy FFT data to local buffer (thread-safe)
    std::array<float, MAX_FFT_SIZE * 2> localFFTData;
    {
        const juce::SpinLock::ScopedLockType lock(m_fftLock);
        std::copy(m_fftData.begin(), m_fftData.begin() + fftSize, localFFTData.begin());
    }

    // Apply windowing function
    m_window->multiplyWithWindowingTable(localFFTData.data(), fftSize);

    // Perform FFT
    m_fft->performFrequencyOnlyForwardTransform(localFFTData.data());

    // Convert to dB and update scope data with smoothing
    for (int i = 0; i < fftSize / 2; ++i)
    {
        // Calculate magnitude in dB with proper normalization
        // Normalize by FFT size and apply empirically-determined display scale
        float magnitude = localFFTData[i] / (float)fftSize;
        magnitude *= FFT_DISPLAY_SCALE;

        float dB = magnitude > 0.0001f ? juce::Decibels::gainToDecibels(magnitude) : m_minDB;
        dB = juce::jlimit(m_minDB, m_maxDB, dB);

        // Apply exponential smoothing for professional animation
        m_scopeData[i] = m_scopeData[i] * SMOOTHING_FACTOR + dB * (1.0f - SMOOTHING_FACTOR);

        // Update peak hold
        if (dB > m_peakHold[i])
        {
            m_peakHold[i] = dB;
            m_peakHoldTime[i] = (PEAK_HOLD_TIME_MS * UPDATE_RATE_HZ) / 1000;
        }
    }
}

//==============================================================================
// Component Overrides

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    // Dark background
    g.fillAll(juce::Colour(0xff1a1a1a));

    auto bounds = getLocalBounds().toFloat();
    const float padding = 4.0f;
    const float axisHeight = 20.0f;
    const float axisWidth = 40.0f;

    // Reserve space for axes
    juce::Rectangle<float> spectrumBounds = bounds.reduced(padding);
    juce::Rectangle<float> frequencyAxisBounds = spectrumBounds.removeFromBottom(axisHeight);
    juce::Rectangle<float> magnitudeAxisBounds = spectrumBounds.removeFromLeft(axisWidth);

    // Draw axes
    drawFrequencyAxis(g, frequencyAxisBounds);
    drawMagnitudeAxis(g, magnitudeAxisBounds);

    // Draw spectrum
    drawSpectrum(g, spectrumBounds);
}

void SpectrumAnalyzer::resized()
{
    // Nothing to resize, spectrum is drawn dynamically
}

void SpectrumAnalyzer::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    // Zoom frequency range with mouse wheel
    float zoomFactor = 1.0f + (wheel.deltaY * 0.1f);

    float range = m_maxFrequency - m_minFrequency;
    float newRange = range * zoomFactor;

    // Clamp to reasonable limits
    if (newRange < 100.0f)
        newRange = 100.0f;
    if (newRange > 20000.0f)
        newRange = 20000.0f;

    float center = (m_minFrequency + m_maxFrequency) / 2.0f;
    m_minFrequency = center - newRange / 2.0f;
    m_maxFrequency = center + newRange / 2.0f;

    // Clamp to valid range
    if (m_minFrequency < 20.0f)
    {
        m_minFrequency = 20.0f;
        m_maxFrequency = m_minFrequency + newRange;
    }
    if (m_maxFrequency > 20000.0f)
    {
        m_maxFrequency = 20000.0f;
        m_minFrequency = m_maxFrequency - newRange;
    }

    repaint();
}

//==============================================================================
// Helper Methods

float SpectrumAnalyzer::frequencyToX(float frequency, juce::Rectangle<float> bounds) const
{
    // Logarithmic frequency scale
    if (frequency <= m_minFrequency)
        return bounds.getX();
    if (frequency >= m_maxFrequency)
        return bounds.getRight();

    float logMin = std::log10(m_minFrequency);
    float logMax = std::log10(m_maxFrequency);
    float logFreq = std::log10(frequency);

    float normalized = (logFreq - logMin) / (logMax - logMin);
    return bounds.getX() + normalized * bounds.getWidth();
}

float SpectrumAnalyzer::xToFrequency(float x, juce::Rectangle<float> bounds) const
{
    // Inverse of frequencyToX
    float normalized = (x - bounds.getX()) / bounds.getWidth();
    normalized = juce::jlimit(0.0f, 1.0f, normalized);

    float logMin = std::log10(m_minFrequency);
    float logMax = std::log10(m_maxFrequency);
    float logFreq = logMin + normalized * (logMax - logMin);

    return std::pow(10.0f, logFreq);
}

float SpectrumAnalyzer::dBToY(float dB, juce::Rectangle<float> bounds) const
{
    float normalized = (dB - m_minDB) / (m_maxDB - m_minDB);
    normalized = juce::jlimit(0.0f, 1.0f, normalized);

    // Inverted Y axis (0 dB at top)
    return bounds.getBottom() - normalized * bounds.getHeight();
}

juce::Colour SpectrumAnalyzer::getColorForMagnitude(float dB) const
{
    // Gradient from blue (low) → green (mid) → yellow (high) → red (very high)
    float normalized = (dB - m_minDB) / (m_maxDB - m_minDB);
    normalized = juce::jlimit(0.0f, 1.0f, normalized);

    if (normalized < 0.25f)
    {
        // Blue to cyan
        float t = normalized / 0.25f;
        return juce::Colour(0, static_cast<uint8_t>(t * 255), 255);
    }
    else if (normalized < 0.5f)
    {
        // Cyan to green
        float t = (normalized - 0.25f) / 0.25f;
        return juce::Colour(0, 255, static_cast<uint8_t>((1.0f - t) * 255));
    }
    else if (normalized < 0.75f)
    {
        // Green to yellow
        float t = (normalized - 0.5f) / 0.25f;
        return juce::Colour(static_cast<uint8_t>(t * 255), 255, 0);
    }
    else
    {
        // Yellow to red
        float t = (normalized - 0.75f) / 0.25f;
        return juce::Colour(255, static_cast<uint8_t>((1.0f - t) * 255), 0);
    }
}

void SpectrumAnalyzer::drawFrequencyAxis(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(juce::Colours::grey);
    g.setFont(juce::Font("Monospace", 9.0f, juce::Font::plain));

    // Draw frequency markings at key points (20, 50, 100, 200, 500, 1k, 2k, 5k, 10k, 20k)
    const float frequencies[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
                                  1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    const char* labels[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
    const int numMarkings = sizeof(frequencies) / sizeof(frequencies[0]);

    auto spectrumBounds = bounds.withTrimmedBottom(bounds.getHeight());
    spectrumBounds.setY(bounds.getY() - bounds.getHeight() - 4.0f);

    for (int i = 0; i < numMarkings; ++i)
    {
        float freq = frequencies[i];
        if (freq >= m_minFrequency && freq <= m_maxFrequency)
        {
            float x = frequencyToX(freq, spectrumBounds);

            // Draw tick mark
            g.drawLine(x, bounds.getY(), x, bounds.getY() + 5.0f, 1.0f);

            // Draw label
            g.drawText(labels[i], x - 15.0f, bounds.getY() + 5.0f, 30.0f, 15.0f,
                       juce::Justification::centredTop, false);
        }
    }

    // Draw "Hz" label
    g.drawText("Hz", bounds.getRight() - 25.0f, bounds.getY() + 5.0f, 25.0f, 15.0f,
               juce::Justification::centredRight, false);
}

void SpectrumAnalyzer::drawMagnitudeAxis(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(juce::Colours::grey);
    g.setFont(juce::Font("Monospace", 9.0f, juce::Font::plain));

    // Draw dB scale markings at key points
    const float dBMarkings[] = { 0.0f, -10.0f, -20.0f, -30.0f, -40.0f, -50.0f, -60.0f, -70.0f };
    const int numMarkings = sizeof(dBMarkings) / sizeof(dBMarkings[0]);

    auto spectrumBounds = bounds.withTrimmedLeft(bounds.getWidth());
    spectrumBounds.setX(bounds.getRight() + 4.0f);

    for (int i = 0; i < numMarkings; ++i)
    {
        float dB = dBMarkings[i];
        if (dB >= m_minDB && dB <= m_maxDB)
        {
            float y = dBToY(dB, spectrumBounds);

            // Draw tick mark
            g.drawLine(bounds.getRight(), y, bounds.getRight() + 5.0f, y, 1.0f);

            // Draw label
            juce::String label = juce::String(static_cast<int>(dB));
            g.drawText(label, bounds.getX(), y - 6.0f, bounds.getWidth() - 8.0f, 12.0f,
                       juce::Justification::centredRight, false);
        }
    }

    // Draw "dB" label
    g.drawText("dB", bounds.getX(), bounds.getY(), bounds.getWidth() - 8.0f, 15.0f,
               juce::Justification::centredRight, false);
}

void SpectrumAnalyzer::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    int fftSize = static_cast<int>(m_currentFFTSize);
    double sampleRate = m_sampleRate.load();
    float binWidth = static_cast<float>(sampleRate / fftSize);

    // Draw spectrum as filled path
    juce::Path spectrumPath;
    bool pathStarted = false;

    for (int i = 1; i < fftSize / 2; ++i)
    {
        float frequency = i * binWidth;

        if (frequency < m_minFrequency)
            continue;
        if (frequency > m_maxFrequency)
            break;

        float x = frequencyToX(frequency, bounds);
        float dB = m_scopeData[i];
        float y = dBToY(dB, bounds);

        if (!pathStarted)
        {
            spectrumPath.startNewSubPath(x, bounds.getBottom());
            spectrumPath.lineTo(x, y);
            pathStarted = true;
        }
        else
        {
            spectrumPath.lineTo(x, y);
        }
    }

    if (pathStarted)
    {
        // Close path to bottom right
        spectrumPath.lineTo(bounds.getRight(), bounds.getBottom());
        spectrumPath.closeSubPath();

        // Fill with gradient
        juce::ColourGradient gradient(
            juce::Colour(0x8000ffff), bounds.getX(), bounds.getBottom(),
            juce::Colour(0x80ff0000), bounds.getX(), bounds.getY(),
            false
        );
        g.setGradientFill(gradient);
        g.fillPath(spectrumPath);

        // Draw outline
        g.setColour(juce::Colours::cyan.withAlpha(0.8f));
        g.strokePath(spectrumPath, juce::PathStrokeType(1.0f));
    }

    // Draw peak hold
    g.setColour(juce::Colours::yellow.withAlpha(0.5f));
    for (int i = 1; i < fftSize / 2; ++i)
    {
        float frequency = i * binWidth;

        if (frequency < m_minFrequency)
            continue;
        if (frequency > m_maxFrequency)
            break;

        if (m_peakHoldTime[i] > 0)
        {
            float x = frequencyToX(frequency, bounds);
            float dB = m_peakHold[i];
            float y = dBToY(dB, bounds);

            g.drawHorizontalLine(static_cast<int>(y), x - 1.0f, x + 1.0f);
        }
    }
}
