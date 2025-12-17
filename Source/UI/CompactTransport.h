/*
  ==============================================================================

    CompactTransport.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

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
 * Compact single-row transport controls designed for the customizable toolbar.
 *
 * Layout (~180px width, 36px height):
 * [REC][<<|][STOP][>||][|>>][LOOP] | 00:00.000
 *
 * Features:
 * - Record button with pulsing indicator
 * - Rewind to start (|<<)
 * - Stop button
 * - Combined Play/Pause toggle button (>||)
 * - Forward to end (>>|)
 * - Loop toggle button
 * - Compact time display (click to cycle formats)
 *
 * Key differences from TransportControls (80px):
 * - Single-row layout (36px vs 80px)
 * - Combined Play/Pause toggle
 * - Skip to start/end buttons
 * - Record button included
 * - Clickable time display to cycle formats
 */
class CompactTransport : public juce::Component,
                          public juce::Timer
{
public:
    //==============================================================================
    // Constants

    static constexpr int kPreferredHeight = 36;
    static constexpr int kPreferredWidth = 200;
    static constexpr int kButtonSize = 24;

    //==============================================================================
    // Time display formats

    enum class TimeFormat
    {
        Time,           // HH:MM:SS.mmm
        CompactTime,    // MM:SS.ms
        Samples         // Sample position
    };

    //==============================================================================

    /**
     * Default constructor for late binding of document context.
     */
    CompactTransport();

    /**
     * Destructor.
     */
    ~CompactTransport() override;

    //==============================================================================
    // Document Context

    /**
     * Set the audio engine for playback control.
     * @param audioEngine Pointer to audio engine, or nullptr when no document
     */
    void setAudioEngine(AudioEngine* audioEngine);

    /**
     * Set the waveform display for selection boundaries.
     * @param waveformDisplay Pointer to waveform display, or nullptr when no document
     */
    void setWaveformDisplay(class WaveformDisplay* waveformDisplay);

    //==============================================================================
    // Component Overrides

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

    //==============================================================================
    // Timer Callback

    void timerCallback() override;

    //==============================================================================
    // Loop Control

    bool isLoopEnabled() const;
    void setLoopEnabled(bool shouldLoop);
    void toggleLoop();

    //==============================================================================
    // Time Format Control

    TimeFormat getTimeFormat() const { return m_timeFormat; }
    void setTimeFormat(TimeFormat format);
    void cycleTimeFormat();

private:
    //==============================================================================
    // Private Members

    AudioEngine* m_audioEngine;
    class WaveformDisplay* m_waveformDisplay;

    // Transport buttons
    std::unique_ptr<juce::DrawableButton> m_recordButton;
    std::unique_ptr<juce::DrawableButton> m_rewindButton;
    std::unique_ptr<juce::DrawableButton> m_stopButton;
    std::unique_ptr<juce::DrawableButton> m_playPauseButton;
    std::unique_ptr<juce::DrawableButton> m_forwardButton;
    std::unique_ptr<juce::DrawableButton> m_loopButton;

    // Time display
    std::unique_ptr<juce::Label> m_timeLabel;

    // State
    bool m_loopEnabled;
    TimeFormat m_timeFormat;

    // State tracking for efficient updates
    PlaybackState m_lastState;
    double m_lastPosition;
    bool m_recordPulse;  // For pulsing record indicator

    //==============================================================================
    // Icon Creation

    static std::unique_ptr<juce::Drawable> createRecordIcon();
    static std::unique_ptr<juce::Drawable> createRewindIcon();
    static std::unique_ptr<juce::Drawable> createStopIcon();
    static std::unique_ptr<juce::Drawable> createPlayIcon();
    static std::unique_ptr<juce::Drawable> createPauseIcon();
    static std::unique_ptr<juce::Drawable> createForwardIcon();
    static std::unique_ptr<juce::Drawable> createLoopIcon();

    //==============================================================================
    // State Updates

    void updateButtonStates();
    void updatePositionDisplay();
    void updatePlayPauseIcon();

    //==============================================================================
    // Formatting

    juce::String formatTime(double timeInSeconds) const;
    juce::String formatCompactTime(double timeInSeconds) const;
    juce::String formatSamples(int64_t samples) const;

    //==============================================================================
    // Button Callbacks

    void onRecordClicked();
    void onRewindClicked();
    void onStopClicked();
    void onPlayPauseClicked();
    void onForwardClicked();
    void onLoopClicked();
    void onTimeLabelClicked();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompactTransport)
};
