/*
  ==============================================================================

    RecordingDialog.h
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
#include <juce_audio_devices/juce_audio_devices.h>
#include "../Audio/RecordingEngine.h"

/**
 * Recording dialog for WaveEdit.
 *
 * Provides GUI for:
 * - Input device selection
 * - Recording controls (Record/Pause/Stop)
 * - Real-time level meters for input monitoring
 * - Elapsed time display
 * - Sample rate/channel configuration
 *
 * Creates a new document with recorded audio when recording stops.
 */
class RecordingDialog : public juce::Component,
                        public juce::Button::Listener,
                        public juce::ComboBox::Listener,
                        public juce::ChangeListener,
                        public juce::Timer
{
public:
    /**
     * Listener interface for recording completion.
     */
    class Listener
    {
    public:
        virtual ~Listener() = default;

        /**
         * Called when recording completes successfully.
         *
         * @param audioBuffer The recorded audio buffer
         * @param sampleRate Sample rate of the recording
         * @param numChannels Number of channels in the recording
         */
        virtual void recordingCompleted(const juce::AudioBuffer<float>& audioBuffer,
                                       double sampleRate,
                                       int numChannels) = 0;
    };

    /**
     * Constructor.
     *
     * @param deviceManager Reference to the application's AudioDeviceManager
     */
    explicit RecordingDialog(juce::AudioDeviceManager& deviceManager);
    ~RecordingDialog() override;

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // Button::Listener overrides

    void buttonClicked(juce::Button* button) override;

    //==============================================================================
    // ComboBox::Listener overrides

    void comboBoxChanged(juce::ComboBox* comboBox) override;

    //==============================================================================
    // ChangeListener overrides

    /**
     * Called when recording engine state changes.
     */
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    //==============================================================================
    // Timer overrides

    /**
     * Called periodically to update UI (levels, elapsed time).
     */
    void timerCallback() override;

    //==============================================================================
    // Listener Management

    /**
     * Adds a listener to be notified when recording completes.
     *
     * @param listener The listener to add
     */
    void addListener(Listener* listener);

    /**
     * Removes a listener.
     *
     * @param listener The listener to remove
     */
    void removeListener(Listener* listener);

    //==============================================================================
    // Static Helper

    /**
     * Shows the recording dialog as a modal window.
     *
     * @param parentComponent Parent component to center the dialog over
     * @param deviceManager Reference to the application's AudioDeviceManager
     * @param listener Listener to notify when recording completes
     */
    static void showDialog(juce::Component* parentComponent,
                          juce::AudioDeviceManager& deviceManager,
                          Listener* listener);

private:
    //==============================================================================
    // Private Members

    juce::AudioDeviceManager& m_deviceManager;
    std::unique_ptr<RecordingEngine> m_recordingEngine;

    juce::ListenerList<Listener> m_listeners;

    // Input device selection
    juce::Label m_inputDeviceLabel;
    juce::ComboBox m_inputDeviceSelector;

    // Sample rate selection
    juce::Label m_sampleRateLabel;
    juce::ComboBox m_sampleRateSelector;

    // Channel configuration
    juce::Label m_channelConfigLabel;
    juce::ComboBox m_channelConfigSelector;

    // Recording controls
    juce::TextButton m_recordButton;
    juce::TextButton m_stopButton;
    juce::TextButton m_cancelButton;

    // Status display
    juce::Label m_statusLabel;
    juce::Label m_elapsedTimeLabel;

    // Level meters (simple text-based for MVP)
    juce::Label m_leftLevelLabel;
    juce::Label m_rightLevelLabel;

    // Level meter components (visual bars)
    class LevelMeter : public juce::Component
    {
    public:
        LevelMeter();
        void paint(juce::Graphics& g) override;
        void setLevel(float level);

    private:
        float m_level = 0.0f;
    };

    LevelMeter m_leftLevelMeter;
    LevelMeter m_rightLevelMeter;

    // Timing
    double m_recordingStartTime = 0.0;

    //==============================================================================
    // Private Methods

    /**
     * Populates the input device combo box.
     */
    void populateInputDevices();

    /**
     * Populates the sample rate combo box.
     */
    void populateSampleRates();

    /**
     * Populates the channel configuration combo box.
     */
    void populateChannelConfigurations();

    /**
     * Starts recording with current settings.
     */
    void startRecording();

    /**
     * Stops recording and notifies listeners.
     */
    void stopRecording();

    /**
     * Updates the UI state based on recording state.
     */
    void updateUIState();

    /**
     * Updates the elapsed time display.
     */
    void updateElapsedTime();

    /**
     * Updates the level meter displays.
     */
    void updateLevelMeters();

    /**
     * Formats a time duration as MM:SS.mmm
     *
     * @param seconds Time in seconds
     * @return Formatted string
     */
    juce::String formatTime(double seconds) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingDialog)
};
