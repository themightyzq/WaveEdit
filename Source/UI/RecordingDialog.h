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
     * Shows the recording dialog as a (non-modal) async window.
     *
     * @param parentComponent Parent component to center the dialog over
     * @param deviceManager Reference to the application's AudioDeviceManager
     * @param listener Listener to notify when recording completes
     * @param recordingStateCallback Optional; called with true when capture
     *        actually starts and false when it stops/closes, so a persistent
     *        transport can show a live recording indicator.
     */
    static void showDialog(juce::Component* parentComponent,
                          juce::AudioDeviceManager& deviceManager,
                          Listener* listener,
                          std::function<void(bool)> recordingStateCallback = {});

private:
    //==============================================================================
    // Private Members

    juce::AudioDeviceManager& m_deviceManager;
    std::unique_ptr<RecordingEngine> m_recordingEngine;

    juce::ListenerList<Listener> m_listeners;

    // Fired with the live capture state (true = recording) so an external
    // transport can mirror it. Set via showDialog().
    std::function<void(bool)> m_recordingStateCallback;

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

    // Level meter component: dBFS-scaled bar with a latching clip indicator.
    // Chrome (background/border) is sourced from the active theme at paint
    // time; the green/yellow/red bar is a VU domain visualisation (§6.11
    // carve-out) sourced from named constants in the .cpp.
    class LevelMeter : public juce::Component
    {
    public:
        LevelMeter();

        void paint(juce::Graphics& g) override;

        /** Feeds a new LINEAR peak (0..1+). Latches the clip flag at >= 0 dBFS. */
        void setLevel(float linearPeak);

        /** Clears the latched clip indicator (called on click and record start). */
        void resetClip();

        /** Click anywhere on the meter clears a latched clip. */
        void mouseDown(const juce::MouseEvent& e) override;

    private:
        float m_level = 0.0f;   // linear peak, clamped for drawing
        bool  m_clipped = false;
    };

    LevelMeter m_leftLevelMeter;
    LevelMeter m_rightLevelMeter;

    // Timing
    double m_recordingStartTime = 0.0;

    // Latch so the buffer-full warning is shown at most once per session
    bool m_bufferFullHandled = false;

    // True while at least one input device is available. When false the
    // record button and all three config combos stay disabled.
    bool m_hasInputDevice = true;

    // Last successfully-applied combo selections, so a failed device/rate
    // switch can be rolled back without firing another change.
    int m_currentInputDeviceId = 0;
    int m_currentSampleRateId = 0;

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
     * Applies a new AudioDeviceSetup to the shared device manager, cycling
     * the monitoring callback across the reconfigure so the level feed is
     * cleanly torn down and restarted. Returns true on success; on failure
     * the error string is non-empty and the caller restores its combo.
     *
     * @param newSetup   Desired device setup
     * @param error      Receives the device-manager error (empty on success)
     * @return true if the setup applied cleanly
     */
    bool applyDeviceSetup(const juce::AudioDeviceManager::AudioDeviceSetup& newSetup,
                          juce::String& error);

    /** Handles a user change of the input-device combo. */
    void handleInputDeviceChange();

    /** Handles a user change of the sample-rate combo. */
    void handleSampleRateChange();

    /** Pushes the current channel-combo selection into the recording engine. */
    void applyChannelSelection();

    /** Shows a themed one-line error in the status label. */
    void showStatusError(const juce::String& message);

    /**
     * Formats a time duration as MM:SS.mmm
     *
     * @param seconds Time in seconds
     * @return Formatted string
     */
    juce::String formatTime(double seconds) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingDialog)
};
