/*
  ==============================================================================

    Meters.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

/**
 * Professional audio level meters component with peak, RMS, and clipping detection.
 *
 * Features:
 * - Real-time peak level display (ballistic decay for smooth visuals)
 * - RMS (average) level indication
 * - Clipping detection (red indicator for levels >±1.0)
 * - Thread-safe level monitoring (audio thread → UI thread communication)
 * - Professional visual design matching industry standards
 *
 * Design Philosophy:
 * - Inspired by professional tools (Sound Forge, Pro Tools, Reaper)
 * - Clean, minimal design with clear visual feedback
 * - Vertical meters (standard orientation for audio software)
 * - Color coding: Green (safe) → Yellow (approaching limit) → Red (clipping)
 */
class Meters : public juce::Component,
               public juce::Timer
{
public:
    Meters();
    ~Meters() override;

    //==============================================================================
    // Level Setting (called from audio thread via atomic operations)

    /**
     * Sets the peak level for a specific channel (thread-safe).
     * Called from audio thread, displays on UI thread via timer.
     *
     * @param channel Channel index (0 = left, 1 = right)
     * @param level Peak level in range [0.0, 1.0+] (can exceed 1.0 for clipping)
     */
    void setPeakLevel(int channel, float level);

    /**
     * Sets the RMS (average) level for a specific channel (thread-safe).
     *
     * @param channel Channel index (0 = left, 1 = right)
     * @param level RMS level in range [0.0, 1.0+]
     */
    void setRMSLevel(int channel, float level);

    /**
     * Resets all meters to zero and clears clipping indicators.
     */
    void reset();

    //==============================================================================
    // Component Overrides

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    //==============================================================================
    // Meter State (thread-safe atomic values)

    static constexpr int MAX_CHANNELS = 2;  // Stereo support for MVP

    std::atomic<float> m_peakLevels[MAX_CHANNELS];      // Current peak levels [0.0, 1.0+]
    std::atomic<float> m_rmsLevels[MAX_CHANNELS];       // Current RMS levels [0.0, 1.0+]
    std::atomic<bool> m_clipping[MAX_CHANNELS];         // Clipping detected flag

    float m_peakHold[MAX_CHANNELS];                     // Peak hold values for visual persistence
    float m_smoothedPeak[MAX_CHANNELS];                 // Smoothed peak for ballistic decay
    float m_smoothedRMS[MAX_CHANNELS];                  // Smoothed RMS for visual stability

    int m_peakHoldTime[MAX_CHANNELS];                   // Peak hold timer (in timer callbacks)
    int m_clippingTime[MAX_CHANNELS];                   // Clipping indicator hold timer

    //==============================================================================
    // Visual Constants

    static constexpr float CLIPPING_THRESHOLD = 1.0f;   // Level at which clipping is detected
    static constexpr float WARNING_THRESHOLD = 0.8f;    // Level at which meter turns yellow
    static constexpr float SAFE_THRESHOLD = 0.6f;       // Level at which meter is green

    static constexpr float PEAK_DECAY_RATE = 0.95f;     // Ballistic decay rate for peak smoothing
    static constexpr float RMS_SMOOTHING = 0.85f;       // Smoothing factor for RMS display

    static constexpr int PEAK_HOLD_TIME_MS = 2000;      // How long to hold peak indicator (ms)
    static constexpr int CLIPPING_HOLD_TIME_MS = 3000;  // How long to show red clipping indicator (ms)
    static constexpr int UPDATE_RATE_HZ = 30;           // UI update rate (30fps for smooth meters)

    //==============================================================================
    // Helper Methods

    /**
     * Converts a linear level [0.0, 1.0+] to decibels for display.
     *
     * @param level Linear level value
     * @return Level in dB, clamped to reasonable range (-60dB to +6dB)
     */
    float levelToDecibels(float level) const;

    /**
     * Gets the meter color based on current level.
     *
     * @param level Current level in range [0.0, 1.0+]
     * @param isClipping True if clipping has been detected
     * @return Color for meter bar (green, yellow, or red)
     */
    juce::Colour getMeterColor(float level, bool isClipping) const;

    /**
     * Draws a single vertical meter bar.
     *
     * @param g Graphics context
     * @param bounds Area to draw meter in
     * @param channel Channel index (0 = left, 1 = right)
     * @param channelName Display name ("L" or "R")
     */
    void drawMeterBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                      int channel, const juce::String& channelName);

    /**
     * Draws the dB scale markings on the meter.
     *
     * @param g Graphics context
     * @param bounds Area for scale markings
     */
    void drawScale(juce::Graphics& g, juce::Rectangle<float> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Meters)
};
