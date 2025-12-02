/*
  ==============================================================================

    AudioEngine.h
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
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

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
 * Preview mode enumeration for real-time DSP effects.
 *
 * This enables a universal preview system for all audio processing operations
 * (EQ, Gain, Normalize, Fade, etc.) without modifying the main audio buffer.
 *
 * Architecture:
 * - DISABLED: Normal playback from main or preview buffer (no DSP)
 * - REALTIME_DSP: Real-time effects via ProcessorChain (EQ, Gain, Fade)
 * - OFFLINE_BUFFER: Pre-rendered preview buffer (Normalize, Time Stretch)
 */
enum class PreviewMode
{
    DISABLED,        // No preview, play main buffer
    REALTIME_DSP,    // Preview via ProcessorChain (instant, no latency)
    OFFLINE_BUFFER   // Preview via pre-rendered buffer (for heavy effects)
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
     * Supports WAV files (8-bit, 16-bit, 24-bit, 32-bit float).
     * Sample rates: 8kHz, 11.025kHz, 16kHz, 22.05kHz, 32kHz, 44.1kHz, 48kHz, 88.2kHz, 96kHz, 176.4kHz, 192kHz.
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
     * Reloads audio buffer while preserving playback state.
     * If currently playing, playback continues from the same position without interruption.
     * Used for real-time edits like gain adjustments during playback.
     *
     * @param buffer The updated audio buffer
     * @param sampleRate Sample rate of the audio data
     * @param numChannels Number of audio channels
     * @return true if reload succeeded, false otherwise
     */
    bool reloadBufferPreservingPlayback(const juce::AudioBuffer<float>& buffer, double sampleRate, int numChannels);

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
    // Loop Control

    /**
     * Enables or disables looping for playback.
     * When enabled, playback will restart from the beginning when it reaches the end.
     *
     * @param shouldLoop true to enable looping, false to disable
     */
    void setLooping(bool shouldLoop);

    /**
     * Checks if looping is currently enabled.
     *
     * @return true if looping is enabled, false otherwise
     */
    bool isLooping() const;

    /**
     * Sets loop points for selection-based looping.
     * When looping is enabled and loop points are set, playback will loop
     * between loopStart and loopEnd instead of looping the entire file.
     *
     * @param loopStart Start position in seconds (use -1 to disable loop points)
     * @param loopEnd End position in seconds
     */
    void setLoopPoints(double loopStart, double loopEnd);

    /**
     * Clears loop points, returning to full-file looping behavior.
     */
    void clearLoopPoints();

    //==============================================================================
    // Level Monitoring

    /**
     * Enables or disables level monitoring for meters.
     * When enabled, audio levels are calculated in the audio callback.
     *
     * @param enabled true to enable monitoring, false to disable
     */
    void setLevelMonitoringEnabled(bool enabled);

    /**
     * Gets the current peak level for a specific channel.
     * Thread-safe, can be called from UI thread.
     *
     * @param channel Channel index (0 = left, 1 = right)
     * @return Peak level in range [0.0, 1.0+]
     */
    float getPeakLevel(int channel) const;

    /**
     * Gets the current RMS level for a specific channel.
     * Thread-safe, can be called from UI thread.
     *
     * @param channel Channel index (0 = left, 1 = right)
     * @return RMS level in range [0.0, 1.0+]
     */
    float getRMSLevel(int channel) const;

    //==============================================================================
    // Spectrum Analyzer Support

    /**
     * Sets the spectrum analyzer to receive audio data during playback.
     * Pass nullptr to disconnect spectrum analyzer.
     *
     * @param spectrumAnalyzer Pointer to spectrum analyzer component, or nullptr
     */
    void setSpectrumAnalyzer(class SpectrumAnalyzer* spectrumAnalyzer);

    //==============================================================================
    // Preview System (Universal DSP Preview)

    /**
     * Sets the preview mode for real-time audio processing.
     *
     * DISABLED: Normal playback (no DSP)
     * REALTIME_DSP: Apply ProcessorChain effects in real-time (EQ, Gain, Fade)
     * OFFLINE_BUFFER: Play pre-rendered preview buffer (Normalize, Time Stretch)
     *
     * IMPORTANT: Only one AudioEngine can be in preview mode at a time across
     * all open documents. When an engine enters preview mode, all other engines
     * automatically mute their output to prevent audio mixing. This ensures users
     * hear only the preview audio, not a mix with other documents.
     *
     * Thread Safety: This method must be called from the message thread only.
     * The preview state is communicated to audio callbacks via atomic operations.
     *
     * @param mode The preview mode to enable
     */
    void setPreviewMode(PreviewMode mode);

    /**
     * Gets the current preview mode.
     *
     * @return Current preview mode
     */
    PreviewMode getPreviewMode() const;

    /**
     * Loads a pre-rendered preview buffer for offline effects.
     * Used for heavy DSP operations that cannot run in real-time.
     *
     * @param previewBuffer The pre-processed audio buffer
     * @param sampleRate Sample rate of the preview audio
     * @param numChannels Number of channels in preview audio
     * @return true if preview buffer loaded successfully
     */
    bool loadPreviewBuffer(const juce::AudioBuffer<float>& previewBuffer, double sampleRate, int numChannels);

    /**
     * Enables or disables a specific real-time gain processor.
     * Thread-safe, can be called from UI thread.
     *
     * @param gainDB Gain adjustment in decibels (-60 to +40)
     * @param enabled true to enable gain processing, false to bypass
     */
    void setGainPreview(float gainDB, bool enabled);

    /**
     * Mutes/unmutes this audio engine's output.
     * Used to prevent audio mixing when another document is previewing.
     * Thread-safe, can be called from UI thread.
     *
     * @param muted true to mute output, false to unmute
     */
    void setMuted(bool muted) { m_isMuted.store(muted); }

    /**
     * Gets the current mute state.
     *
     * @return true if muted, false if unmuted
     */
    bool isMuted() const { return m_isMuted.load(); }

    /**
     * Sets the preview selection offset for accurate cursor positioning during preview mode.
     * When previewing a selection, the audio plays from position 0 but should display
     * as if playing from the selection start position.
     *
     * Thread-safe: Uses atomic operations.
     *
     * @param selectionStartSamples The selection start position in samples from the main buffer
     */
    void setPreviewSelectionOffset(int64_t selectionStartSamples);

    /**
     * Gets the preview selection offset in seconds.
     * Used to adjust cursor position during preview mode playback.
     *
     * Thread-safe: Uses atomic operations.
     *
     * @return Offset in seconds (0.0 if no offset set)
     */
    double getPreviewSelectionOffsetSeconds() const;

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

        void setBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate, bool preservePosition = false);
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
    std::atomic<bool> m_isLooping;
    std::atomic<double> m_loopStartTime;  // Loop start in seconds (-1 = disabled)
    std::atomic<double> m_loopEndTime;    // Loop end in seconds
    juce::File m_currentFile;

    std::atomic<double> m_sampleRate;
    std::atomic<int> m_numChannels;
    std::atomic<int> m_bitDepth;

    // Level monitoring state
    std::atomic<bool> m_levelMonitoringEnabled;
    static constexpr int MAX_CHANNELS = 2;  // Stereo support for MVP
    std::atomic<float> m_peakLevels[MAX_CHANNELS];
    std::atomic<float> m_rmsLevels[MAX_CHANNELS];

    // Spectrum analyzer (not owned, just a pointer for data feeding)
    class SpectrumAnalyzer* m_spectrumAnalyzer;

    //==============================================================================
    // Preview System Members

    // Preview mode state
    std::atomic<PreviewMode> m_previewMode;

    // Preview buffer for offline effects (Normalize, Time Stretch, etc.)
    std::unique_ptr<MemoryAudioSource> m_previewBufferSource;

    // Real-time DSP chain for instant preview (EQ, Gain, Fade)
    // Simple gain processor for Phase 1 (will expand to ProcessorChain in Phase 2)
    struct GainProcessor
    {
        std::atomic<float> gainDB{0.0f};
        std::atomic<bool> enabled{false};

        void process(juce::AudioBuffer<float>& buffer)
        {
            if (enabled.load())
            {
                float gainLinear = juce::Decibels::decibelsToGain(gainDB.load());
                buffer.applyGain(gainLinear);
            }
        }
    };

    GainProcessor m_gainProcessor;

    // Mute flag to prevent audio output during other document's preview
    std::atomic<bool> m_isMuted{false};

    // Preview selection offset for accurate cursor positioning
    std::atomic<int64_t> m_previewSelectionStartSamples{0};

    // Static member to track which AudioEngine is in preview mode (if any)
    static std::atomic<AudioEngine*> s_previewingEngine;

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
