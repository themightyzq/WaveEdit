/*
  ==============================================================================

    TransportControls.h
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
#include "../Audio/AudioEngine.h"

/**
 * Transport Controls component for WaveEdit.
 *
 * Provides playback control UI with the following features:
 * - Play button (Space or F12)
 * - Pause button (Enter or Ctrl+F12)
 * - Stop button
 * - Loop toggle button (Q)
 * - Current position display (time and samples)
 * - Visual playback state indicators
 * - Scrubbing support (click to jump to position)
 *
 * This component follows Sound Forge Pro's transport control design.
 */
class TransportControls : public juce::Component,
                          public juce::Timer
{
public:
    /**
     * Constructor.
     *
     * @param audioEngine Reference to the audio engine for playback control
     * @param waveformDisplay Reference to the waveform display for selection boundaries
     */
    TransportControls(AudioEngine& audioEngine, class WaveformDisplay& waveformDisplay);

    /**
     * Destructor.
     */
    ~TransportControls() override;

    //==============================================================================
    // Component Overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // Timer Callback

    /**
     * Updates the position display and button states.
     * Called at regular intervals (50ms for smooth updates).
     */
    void timerCallback() override;

    //==============================================================================
    // Loop Control

    /**
     * Gets the current loop state.
     *
     * @return true if looping is enabled, false otherwise
     */
    bool isLoopEnabled() const;

    /**
     * Sets the loop state.
     *
     * @param shouldLoop true to enable looping, false to disable
     */
    void setLoopEnabled(bool shouldLoop);

    /**
     * Toggles the loop state.
     */
    void toggleLoop();

private:
    //==============================================================================
    // Private Members

    AudioEngine& m_audioEngine;
    class WaveformDisplay& m_waveformDisplay;

    // Transport buttons (using DrawableButton for icons)
    std::unique_ptr<juce::DrawableButton> m_playButton;
    std::unique_ptr<juce::DrawableButton> m_pauseButton;
    std::unique_ptr<juce::DrawableButton> m_stopButton;
    std::unique_ptr<juce::DrawableButton> m_loopButton;

    // Position display labels
    std::unique_ptr<juce::Label> m_timeLabel;

    // Loop state
    bool m_loopEnabled;

    // State tracking for efficient updates
    PlaybackState m_lastState;
    double m_lastPosition;

    //==============================================================================
    // Private Methods

    /**
     * Creates icon drawables for transport buttons.
     */
    static std::unique_ptr<juce::Drawable> createPlayIcon();
    static std::unique_ptr<juce::Drawable> createPauseIcon();
    static std::unique_ptr<juce::Drawable> createStopIcon();
    static std::unique_ptr<juce::Drawable> createLoopIcon();

    /**
     * Updates button visual states based on current playback state.
     */
    void updateButtonStates();

    /**
     * Updates the position display with current time and sample position.
     */
    void updatePositionDisplay();

    /**
     * Formats a time value as HH:MM:SS.mmm
     *
     * @param timeInSeconds Time in seconds
     * @return Formatted time string
     */
    juce::String formatTime(double timeInSeconds) const;

    /**
     * Formats a sample position with thousands separators.
     *
     * @param sample Sample position
     * @return Formatted sample string
     */
    juce::String formatSample(int64_t sample) const;

    //==============================================================================
    // Button Callbacks

    void onPlayClicked();
    void onPauseClicked();
    void onStopClicked();
    void onLoopClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportControls)
};
