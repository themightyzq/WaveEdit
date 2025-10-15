/*
  ==============================================================================

    Meters.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "Meters.h"

Meters::Meters()
    : m_audioEngine(nullptr)
{
    // Initialize atomic values and state arrays
    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
    {
        m_peakLevels[ch].store(0.0f);
        m_rmsLevels[ch].store(0.0f);
        m_clipping[ch].store(false);

        m_peakHold[ch] = 0.0f;
        m_smoothedPeak[ch] = 0.0f;
        m_smoothedRMS[ch] = 0.0f;
        m_peakHoldTime[ch] = 0;
        m_clippingTime[ch] = 0;
    }

    // Start timer for UI updates (30fps for smooth meter animation)
    startTimer(1000 / UPDATE_RATE_HZ);
}

Meters::~Meters()
{
    stopTimer();
}

//==============================================================================
// Level Setting

void Meters::setPeakLevel(int channel, float level)
{
    if (channel >= 0 && channel < MAX_CHANNELS)
    {
        m_peakLevels[channel].store(level);

        // Detect clipping (level exceeds ±1.0)
        if (level >= CLIPPING_THRESHOLD)
        {
            m_clipping[channel].store(true);
        }
    }
}

void Meters::setRMSLevel(int channel, float level)
{
    if (channel >= 0 && channel < MAX_CHANNELS)
    {
        m_rmsLevels[channel].store(level);
    }
}

void Meters::reset()
{
    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
    {
        m_peakLevels[ch].store(0.0f);
        m_rmsLevels[ch].store(0.0f);
        m_clipping[ch].store(false);

        m_peakHold[ch] = 0.0f;
        m_smoothedPeak[ch] = 0.0f;
        m_smoothedRMS[ch] = 0.0f;
        m_peakHoldTime[ch] = 0;
        m_clippingTime[ch] = 0;
    }

    repaint();
}

void Meters::setAudioEngine(AudioEngine* audioEngine)
{
    m_audioEngine = audioEngine;

    // Reset meters when changing audio source
    reset();
}

//==============================================================================
// Timer Callback

void Meters::timerCallback()
{
    bool needsRepaint = false;

    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
    {
        // Read atomic values (thread-safe from audio thread)
        float currentPeak = m_peakLevels[ch].load();
        float currentRMS = m_rmsLevels[ch].load();

        // Apply ballistic decay to peak for smooth visual response
        if (currentPeak > m_smoothedPeak[ch])
        {
            // Fast attack: instantly jump to new peak
            m_smoothedPeak[ch] = currentPeak;
            needsRepaint = true;
        }
        else
        {
            // Slow decay: gradually fall
            m_smoothedPeak[ch] *= PEAK_DECAY_RATE;
            if (m_smoothedPeak[ch] < 0.001f)
            {
                m_smoothedPeak[ch] = 0.0f;
            }
            needsRepaint = true;
        }

        // Smooth RMS for visual stability
        m_smoothedRMS[ch] = m_smoothedRMS[ch] * RMS_SMOOTHING + currentRMS * (1.0f - RMS_SMOOTHING);

        // Peak hold logic
        if (currentPeak > m_peakHold[ch])
        {
            m_peakHold[ch] = currentPeak;
            m_peakHoldTime[ch] = (PEAK_HOLD_TIME_MS * UPDATE_RATE_HZ) / 1000; // Convert ms to timer ticks
            needsRepaint = true;
        }
        else if (m_peakHoldTime[ch] > 0)
        {
            m_peakHoldTime[ch]--;
            if (m_peakHoldTime[ch] == 0)
            {
                m_peakHold[ch] = 0.0f;
                needsRepaint = true;
            }
        }

        // Clipping indicator hold logic
        if (m_clipping[ch].load())
        {
            m_clippingTime[ch] = (CLIPPING_HOLD_TIME_MS * UPDATE_RATE_HZ) / 1000;
            m_clipping[ch].store(false); // Reset flag
            needsRepaint = true;
        }
        else if (m_clippingTime[ch] > 0)
        {
            m_clippingTime[ch]--;
            if (m_clippingTime[ch] == 0)
            {
                needsRepaint = true;
            }
        }
    }

    if (needsRepaint)
    {
        repaint();
    }
}

//==============================================================================
// Component Overrides

void Meters::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a)); // Dark background

    auto bounds = getLocalBounds().toFloat();
    const float padding = 4.0f;
    const float scaleWidth = 30.0f;
    const float meterSpacing = 4.0f;

    // Calculate meter layout
    float totalWidth = bounds.getWidth() - padding * 2 - scaleWidth;
    float meterWidth = (totalWidth - meterSpacing) / 2.0f; // Two meters (L + R)

    // Draw scale on the left
    juce::Rectangle<float> scaleBounds(padding, padding, scaleWidth, bounds.getHeight() - padding * 2);
    drawScale(g, scaleBounds);

    // Draw left channel meter
    juce::Rectangle<float> leftMeterBounds(
        scaleBounds.getRight() + padding,
        padding,
        meterWidth,
        bounds.getHeight() - padding * 2
    );
    drawMeterBar(g, leftMeterBounds, 0, "L");

    // Draw right channel meter
    juce::Rectangle<float> rightMeterBounds(
        leftMeterBounds.getRight() + meterSpacing,
        padding,
        meterWidth,
        bounds.getHeight() - padding * 2
    );
    drawMeterBar(g, rightMeterBounds, 1, "R");
}

void Meters::resized()
{
    // Nothing to resize, meters are drawn dynamically
}

//==============================================================================
// Helper Methods

float Meters::levelToDecibels(float level) const
{
    if (level <= 0.0f)
    {
        return -60.0f; // Silence
    }

    float dB = 20.0f * std::log10(level);
    return juce::jlimit(-60.0f, 6.0f, dB); // Clamp to reasonable range
}

juce::Colour Meters::getMeterColor(float level, bool isClipping) const
{
    if (isClipping)
    {
        return juce::Colours::red; // Clipping: bright red
    }
    else if (level >= WARNING_THRESHOLD)
    {
        return juce::Colours::yellow; // Warning: approaching limit
    }
    else if (level >= SAFE_THRESHOLD)
    {
        return juce::Colours::orange; // Caution: moderate level
    }
    else
    {
        return juce::Colours::green; // Safe: low to moderate level
    }
}

void Meters::drawMeterBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                          int channel, const juce::String& channelName)
{
    // Draw background
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(bounds, 2.0f);

    // Draw channel label at bottom
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font("Monospace", 10.0f, juce::Font::bold));
    auto labelBounds = bounds.removeFromBottom(15.0f);
    g.drawText(channelName, labelBounds, juce::Justification::centred, false);

    // Adjust bounds for actual meter bar
    bounds = bounds.reduced(2.0f);

    // Draw meter segments (colored bars based on level)
    float meterHeight = bounds.getHeight();
    float peakLevel = m_smoothedPeak[channel];
    float rmsLevel = m_smoothedRMS[channel];
    bool isClipping = m_clippingTime[channel] > 0;

    // Draw RMS level (darker, behind peak)
    if (rmsLevel > 0.0f)
    {
        float rmsHeight = rmsLevel * meterHeight;
        juce::Rectangle<float> rmsBounds(
            bounds.getX(),
            bounds.getBottom() - rmsHeight,
            bounds.getWidth(),
            rmsHeight
        );

        g.setColour(getMeterColor(rmsLevel, false).darker(0.5f));
        g.fillRoundedRectangle(rmsBounds, 1.0f);
    }

    // Draw peak level (brighter, on top of RMS)
    if (peakLevel > 0.0f)
    {
        float peakHeight = peakLevel * meterHeight;
        juce::Rectangle<float> peakBounds(
            bounds.getX(),
            bounds.getBottom() - peakHeight,
            bounds.getWidth(),
            peakHeight
        );

        g.setColour(getMeterColor(peakLevel, isClipping));
        g.fillRoundedRectangle(peakBounds, 1.0f);
    }

    // Draw peak hold indicator (thin line)
    if (m_peakHold[channel] > 0.0f)
    {
        float holdY = bounds.getBottom() - (m_peakHold[channel] * meterHeight);
        g.setColour(juce::Colours::white);
        g.drawLine(bounds.getX(), holdY, bounds.getRight(), holdY, 2.0f);
    }

    // Draw clipping indicator at top
    if (isClipping)
    {
        auto clipBounds = bounds.removeFromTop(6.0f);
        g.setColour(juce::Colours::red);
        g.fillRoundedRectangle(clipBounds, 1.0f);
    }
}

void Meters::drawScale(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(juce::Colours::grey);
    g.setFont(juce::Font("Monospace", 9.0f, juce::Font::plain));

    // Draw dB scale markings at key points
    const float dBMarkings[] = { 0.0f, -3.0f, -6.0f, -12.0f, -24.0f, -48.0f };
    const int numMarkings = sizeof(dBMarkings) / sizeof(dBMarkings[0]);

    float meterHeight = bounds.getHeight();

    for (int i = 0; i < numMarkings; ++i)
    {
        float dB = dBMarkings[i];
        float linearLevel = juce::Decibels::decibelsToGain(dB);

        // Calculate Y position (inverted, 0dB at top)
        float y = bounds.getY() + (1.0f - linearLevel) * meterHeight;

        // Draw tick mark
        g.drawLine(bounds.getRight() - 5.0f, y, bounds.getRight(), y, 1.0f);

        // Draw label
        juce::String label = (dB == 0.0f) ? "0" : juce::String(static_cast<int>(dB));
        g.drawText(label, bounds.getX(), y - 6.0f, bounds.getWidth() - 8.0f, 12.0f,
                   juce::Justification::centredRight, false);
    }
}
