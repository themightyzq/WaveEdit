/*
  ==============================================================================

    RecordingEngine.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>

/**
 * Recording state enumeration.
 */
enum class RecordingState
{
    IDLE,       // Not recording
    RECORDING,  // Actively recording
    PAUSED      // Recording paused (not implemented in MVP)
};

/**
 * Audio recording engine for WaveEdit.
 *
 * Handles real-time audio input recording with:
 * - Thread-safe buffer accumulation
 * - Level monitoring for input
 * - Sample rate conversion
 * - Recording state management
 *
 * Thread Safety:
 * - Audio callback runs on real-time audio thread
 * - UI queries run on message thread
 * - All shared state protected with atomics or locks
 */
class RecordingEngine : public juce::AudioIODeviceCallback,
                        public juce::ChangeBroadcaster
{
public:
    RecordingEngine();
    ~RecordingEngine() override;

    //==============================================================================
    // Recording Control

    /**
     * Starts recording from the currently selected input device.
     * Creates a new recording buffer and begins accumulating samples.
     *
     * @return true if recording started successfully, false otherwise
     */
    bool startRecording();

    /**
     * Stops recording and finalizes the buffer.
     * After calling this, use getRecordedAudio() to retrieve the buffer.
     *
     * @return true if recording stopped successfully, false otherwise
     */
    bool stopRecording();

    /**
     * Gets the current recording state.
     *
     * @return Current state (IDLE, RECORDING, or PAUSED)
     */
    RecordingState getRecordingState() const;

    /**
     * Checks if currently recording.
     *
     * @return true if recording, false otherwise
     */
    bool isRecording() const;

    /**
     * Checks if the recording buffer is full.
     * Thread-safe, can be polled from UI thread.
     *
     * @return true if buffer is full and recording has stopped
     */
    bool isBufferFull() const;

    //==============================================================================
    // Recorded Audio Access

    /**
     * Gets the recorded audio buffer.
     * Only valid after stopRecording() has been called.
     *
     * @return Reference to the recorded audio buffer
     */
    const juce::AudioBuffer<float>& getRecordedAudio() const;

    /**
     * Gets the sample rate of the recorded audio.
     *
     * @return Sample rate in Hz
     */
    double getRecordedSampleRate() const;

    /**
     * Gets the number of channels in the recorded audio.
     *
     * @return Number of channels
     */
    int getRecordedNumChannels() const;

    /**
     * Gets the total recording duration in seconds.
     *
     * @return Duration in seconds
     */
    double getRecordingDuration() const;

    /**
     * Clears the recorded audio buffer and resets state.
     */
    void clearRecording();

    //==============================================================================
    // Level Monitoring

    /**
     * Gets the current peak input level for a specific channel.
     * Thread-safe, can be called from UI thread.
     *
     * @param channel Channel index (0 = left, 1 = right)
     * @return Peak level in range [0.0, 1.0+]
     */
    float getInputPeakLevel(int channel) const;

    /**
     * Gets the current RMS input level for a specific channel.
     * Thread-safe, can be called from UI thread.
     *
     * @param channel Channel index (0 = left, 1 = right)
     * @return RMS level in range [0.0, 1.0+]
     */
    float getInputRMSLevel(int channel) const;

    //==============================================================================
    // AudioIODeviceCallback Implementation

    /**
     * Called when audio device is about to start.
     */
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;

    /**
     * Called when audio device has stopped.
     */
    void audioDeviceStopped() override;

    /**
     * Main audio callback - processes incoming audio on the real-time thread.
     * This is where we capture audio samples during recording.
     */
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

private:
    //==============================================================================
    // Private Members

    // Recording state
    std::atomic<RecordingState> m_recordingState;
    std::atomic<double> m_sampleRate;
    std::atomic<int> m_numChannels;
    std::atomic<bool> m_bufferFull;  // Set by audio thread when buffer is full

    // Audio buffer for recorded samples
    juce::AudioBuffer<float> m_recordedBuffer;
    std::atomic<int> m_recordedSampleCount;
    juce::CriticalSection m_bufferLock;

    // Temporary buffer for audio callback (preallocated to avoid allocations on audio thread)
    juce::AudioBuffer<float> m_tempRecordingBuffer;
    static constexpr int TEMP_BUFFER_SIZE = 44100 * 2;  // 2 seconds at 44.1kHz

    // Level monitoring
    static constexpr int MAX_CHANNELS = 2;  // Stereo support
    std::atomic<float> m_inputPeakLevels[MAX_CHANNELS];
    std::atomic<float> m_inputRMSLevels[MAX_CHANNELS];

    //==============================================================================
    // Private Methods

    /**
     * Updates input level meters from audio data.
     * Called from audio callback thread.
     *
     * @param audioData Audio samples to analyze
     * @param numChannels Number of channels
     * @param numSamples Number of samples per channel
     */
    void updateInputLevels(const float* const* audioData, int numChannels, int numSamples);

    /**
     * Appends audio samples to the recording buffer.
     * Called from audio callback thread.
     *
     * @param audioData Audio samples to append
     * @param numChannels Number of channels
     * @param numSamples Number of samples per channel
     */
    void appendToRecordingBuffer(const float* const* audioData, int numChannels, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingEngine)
};
