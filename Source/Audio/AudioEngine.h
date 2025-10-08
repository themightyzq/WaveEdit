/*
  ==============================================================================

    AudioEngine.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

/**
 * Playback state enumeration for the audio engine.
 */
enum class PlaybackState
{
    STOPPED,
    PLAYING,
    PAUSED
};

/**
 * Core audio engine for WaveEdit.
 *
 * Handles audio playback, transport control, and device management.
 * This class is thread-safe and designed for real-time audio processing.
 *
 * Key features:
 * - Playback control (play, pause, stop)
 * - Transport source management
 * - Audio device configuration
 * - State machine for playback states
 */
class AudioEngine : public juce::ChangeListener,
                    public juce::AudioIODeviceCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    //==============================================================================
    // Device Management

    /**
     * Initializes the audio device manager with default settings.
     * Should be called during application startup.
     *
     * @return true if initialization succeeded, false otherwise
     */
    bool initializeAudioDevice();

    /**
     * Gets the audio device manager for configuration.
     *
     * @return Reference to the audio device manager
     */
    juce::AudioDeviceManager& getDeviceManager();

    /**
     * Gets the audio format manager for file format support.
     *
     * @return Reference to the audio format manager
     */
    juce::AudioFormatManager& getFormatManager();

    //==============================================================================
    // File Loading

    /**
     * Loads an audio file for playback.
     * Supports WAV files (16-bit, 24-bit, 32-bit float).
     * Sample rates: 44.1kHz, 48kHz, 88.2kHz, 96kHz, 192kHz.
     *
     * @param file The audio file to load
     * @return true if load succeeded, false otherwise
     */
    bool loadAudioFile(const juce::File& file);

    /**
     * Loads audio data from a buffer for playback (used for edited audio).
     * This method switches the engine to buffer playback mode.
     *
     * @param buffer The audio buffer containing samples to play
     * @param sampleRate Sample rate of the audio data
     * @param numChannels Number of audio channels
     * @return true if load succeeded, false otherwise
     */
    bool loadFromBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate, int numChannels);

    /**
     * Closes the currently loaded audio file and releases resources.
     */
    void closeAudioFile();

    /**
     * Checks if an audio file is currently loaded.
     *
     * @return true if a file is loaded, false otherwise
     */
    bool isFileLoaded() const;

    /**
     * Checks if currently playing from buffer (edited audio) vs original file.
     *
     * @return true if playing from buffer, false if playing from file
     */
    bool isPlayingFromBuffer() const;

    /**
     * Gets the currently loaded file.
     *
     * @return The current file (may be invalid if no file is loaded)
     */
    juce::File getCurrentFile() const;

    //==============================================================================
    // Playback Control

    /**
     * Starts playback from the current position.
     */
    void play();

    /**
     * Pauses playback at the current position.
     */
    void pause();

    /**
     * Stops playback and returns to the beginning.
     */
    void stop();

    /**
     * Gets the current playback state.
     *
     * @return Current state (STOPPED, PLAYING, or PAUSED)
     */
    PlaybackState getPlaybackState() const;

    /**
     * Checks if audio is currently playing.
     *
     * @return true if playing, false otherwise
     */
    bool isPlaying() const;

    //==============================================================================
    // Transport Position

    /**
     * Gets the current playback position in seconds.
     *
     * @return Current position in seconds
     */
    double getCurrentPosition() const;

    /**
     * Sets the playback position in seconds.
     *
     * @param positionInSeconds The new position in seconds
     */
    void setPosition(double positionInSeconds);

    /**
     * Gets the total length of the loaded audio in seconds.
     *
     * @return Total length in seconds (0.0 if no file loaded)
     */
    double getTotalLength() const;

    //==============================================================================
    // Audio Properties

    /**
     * Gets the sample rate of the loaded audio file.
     *
     * @return Sample rate in Hz (0.0 if no file loaded)
     */
    double getSampleRate() const;

    /**
     * Gets the number of channels in the loaded audio file.
     *
     * @return Number of channels (0 if no file loaded)
     */
    int getNumChannels() const;

    /**
     * Gets the bit depth of the loaded audio file.
     *
     * @return Bit depth (0 if no file loaded or not applicable)
     */
    int getBitDepth() const;

    //==============================================================================
    // ChangeListener Implementation

    /**
     * Called when the transport source state changes.
     */
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

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
     * Main audio callback - processes audio on the real-time thread.
     */
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

private:
    //==============================================================================
    // Private Helper Classes

    /**
     * Memory-based audio source that plays from an AudioBuffer.
     * This is used for playback of edited audio.
     */
    class MemoryAudioSource : public juce::PositionableAudioSource
    {
    public:
        MemoryAudioSource();
        ~MemoryAudioSource() override;

        void setBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate);
        void clear();

        // PositionableAudioSource implementation
        void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
        void releaseResources() override;
        void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
        void setNextReadPosition(juce::int64 newPosition) override;
        juce::int64 getNextReadPosition() const override;
        juce::int64 getTotalLength() const override;
        bool isLooping() const override;
        void setLooping(bool shouldLoop) override;

    private:
        juce::AudioBuffer<float> m_buffer;
        double m_sampleRate;
        std::atomic<juce::int64> m_readPosition;
        bool m_isLooping;
        juce::CriticalSection m_lock;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MemoryAudioSource)
    };

    //==============================================================================
    // Private Members

    juce::AudioDeviceManager m_deviceManager;
    juce::AudioFormatManager m_formatManager;
    juce::AudioTransportSource m_transportSource;

    std::unique_ptr<juce::AudioFormatReaderSource> m_readerSource;
    std::unique_ptr<MemoryAudioSource> m_bufferSource;
    std::unique_ptr<juce::TimeSliceThread> m_backgroundThread;

    std::atomic<PlaybackState> m_playbackState;
    std::atomic<bool> m_isPlayingFromBuffer;
    juce::File m_currentFile;

    std::atomic<double> m_sampleRate;
    std::atomic<int> m_numChannels;
    std::atomic<int> m_bitDepth;

    //==============================================================================
    // Private Methods

    /**
     * Updates the playback state and notifies listeners if changed.
     */
    void updatePlaybackState(PlaybackState newState);

    /**
     * Validates that the audio file format is supported.
     *
     * @param reader The audio format reader to validate
     * @return true if format is supported, false otherwise
     */
    bool validateAudioFormat(juce::AudioFormatReader* reader);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
