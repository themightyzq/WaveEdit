/*
  ==============================================================================

    Meters.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "Meters.h"
#include "ThemeManager.h"
#include "UIConstants.h"
#include "../Audio/AudioEngine.h"

Meters::Meters()
    : m_numChannels(2),
      m_audioEngine(nullptr)
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

    waveedit::ThemeManager::getInstance().addChangeListener(this);

    // Start timer for UI updates (30fps for smooth meter animation)
    startTimer(1000 / UPDATE_RATE_HZ);
}

Meters::~Meters()
{
    stopTimer();
    waveedit::ThemeManager::getInstance().removeChangeListener(this);
}

void Meters::setNumChannels(int numChannels)
{
    const int clamped = juce::jlimit(1, static_cast<int>(MAX_CHANNELS), numChannels);
    if (clamped != m_numChannels)
    {
        m_numChannels = clamped;
        repaint();
    }
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

    // Track the source's channel count so mono files show one bar labelled
    // "M" rather than a phantom stereo pair (see drawMeterBar idle handling).
    if (m_audioEngine != nullptr)
        setNumChannels(m_audioEngine->getNumChannels());

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
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();

    g.fillAll(theme.background); // Recessed background surface

    auto bounds = getLocalBounds().toFloat();
    const float padding = 4.0f;
    const float scaleWidth = 30.0f;
    const float meterSpacing = 4.0f;

    // Draw scale on the left
    juce::Rectangle<float> scaleBounds(padding, padding, scaleWidth, bounds.getHeight() - padding * 2);
    drawScale(g, scaleBounds);

    // Calculate per-meter width for the active channel count.
    const int numBars = juce::jmax(1, m_numChannels);
    float totalWidth = bounds.getWidth() - padding * 2 - scaleWidth;
    float meterWidth = (totalWidth - meterSpacing * (numBars - 1)) / static_cast<float>(numBars);

    // Draw one meter bar per active channel, labeled generically.
    float x = scaleBounds.getRight() + padding;
    for (int ch = 0; ch < numBars; ++ch)
    {
        juce::Rectangle<float> meterBounds(
            x,
            padding,
            meterWidth,
            bounds.getHeight() - padding * 2
        );
        drawMeterBar(g, meterBounds, ch, channelLabel(ch));
        x += meterWidth + meterSpacing;
    }
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
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();

    if (isClipping)
    {
        return theme.error; // Clipping: bright red
    }

    // Compare in dBFS so the colour zones match how engineers read meters:
    //   green  < -18 dB, yellow -18..-6 dB, red (hot) > -6 dB.
    const float dB = juce::Decibels::gainToDecibels(level);

    if (dB >= WARNING_THRESHOLD_DB)
    {
        return theme.error; // Hot: approaching clip
    }
    else if (dB >= SAFE_THRESHOLD_DB)
    {
        return theme.warning; // Caution: moderate level
    }
    else
    {
        return theme.success; // Safe: low to moderate level
    }
}

void Meters::drawMeterBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                          int channel, const juce::String& channelName)
{
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();
    const bool active = isMeteringActive();

    // Draw bar background. When metering is inactive (no engine/file), use a
    // subtle muted tint so "no signal" reads differently from true silence.
    g.setColour(active ? theme.panel
                       : theme.textMuted.withAlpha(0.15f));
    g.fillRoundedRectangle(bounds, 2.0f);

    // Draw channel label at bottom
    g.setColour(active ? theme.text : theme.textMuted);
    g.setFont(waveedit::ui::monospaceFont());
    auto labelBounds = bounds.removeFromBottom(15.0f);
    g.drawText(channelName, labelBounds, juce::Justification::centred, false);

    // Adjust bounds for actual meter bar
    bounds = bounds.reduced(2.0f);

    // Inactive metering: leave the bar empty (background already tinted) and
    // show a small "no signal" hint so it's visibly distinct from silence.
    if (!active)
    {
        g.setColour(theme.textMuted.withAlpha(0.6f));
        g.setFont(waveedit::ui::monospaceFont().withHeight(8.0f));
        g.drawText("--", bounds, juce::Justification::centred, false);
        return;
    }

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
        g.setColour(theme.text);
        g.drawLine(bounds.getX(), holdY, bounds.getRight(), holdY, 2.0f);
    }

    // Draw clipping indicator at top
    if (isClipping)
    {
        auto clipBounds = bounds.removeFromTop(6.0f);
        g.setColour(theme.error);
        g.fillRoundedRectangle(clipBounds, 1.0f);
    }
}

void Meters::drawScale(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();
    g.setColour(theme.textMuted);
    g.setFont(waveedit::ui::monospaceFont().withHeight(9.0f));

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
        g.drawText(label, static_cast<int>(bounds.getX()), static_cast<int>(y) - 6,
                   static_cast<int>(bounds.getWidth()) - 8, 12, juce::Justification::centredRight, false);
    }
}

juce::String Meters::channelLabel(int channel) const
{
    // Mono: single bar labeled "M".
    if (m_numChannels <= 1)
        return "M";

    // Stereo (and the first two of multichannel): L / R.
    // Third channel = C (centre); beyond that, numeric (1-based).
    switch (channel)
    {
        case 0: return "L";
        case 1: return "R";
        case 2: return "C";
        default: return juce::String(channel + 1);
    }
}

void Meters::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &waveedit::ThemeManager::getInstance())
        repaint();
}
