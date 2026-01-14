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
#include "../DSP/ParametricEQ.h"
#include "../DSP/DynamicParametricEQ.h"
#include "../Plugins/PluginChain.h"

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
    // Channel Solo/Mute

    /**
     * Solos a specific channel for monitoring.
     * When any channel is solo'd, only solo'd channels are audible.
     * Thread-safe.
     *
     * @param channel Channel index (0-7)
     * @param solo true to solo, false to unsolo
     */
    void setChannelSolo(int channel, bool solo);

    /**
     * Gets the solo state for a specific channel.
     *
     * @param channel Channel index (0-7)
     * @return true if channel is solo'd
     */
    bool isChannelSolo(int channel) const;

    /**
     * Mutes a specific channel.
     * Muted channels are always silent regardless of solo state.
     * Thread-safe.
     *
     * @param channel Channel index (0-7)
     * @param mute true to mute, false to unmute
     */
    void setChannelMute(int channel, bool mute);

    /**
     * Gets the mute state for a specific channel.
     *
     * @param channel Channel index (0-7)
     * @return true if channel is muted
     */
    bool isChannelMute(int channel) const;

    /**
     * Checks if any channel is currently solo'd.
     * Used to determine solo-mode behavior (only play solo'd channels).
     *
     * @return true if at least one channel is solo'd
     */
    bool hasAnySolo() const;

    /**
     * Clears all solo and mute states (reset to default).
     */
    void clearAllSoloMute();

    //==============================================================================
    // Spectrum Analyzer Support

    /**
     * Sets the spectrum analyzer to receive audio data during playback.
     * Pass nullptr to disconnect spectrum analyzer.
     *
     * @param spectrumAnalyzer Pointer to spectrum analyzer component, or nullptr
     */
    void setSpectrumAnalyzer(class SpectrumAnalyzer* spectrumAnalyzer);

    /**
     * Sets the graphical EQ editor to receive audio data during preview.
     * Pass nullptr to disconnect graphical EQ editor.
     * Used for real-time spectrum visualization in the Graphical EQ dialog.
     *
     * @param eqEditor Pointer to graphical EQ editor component, or nullptr
     */
    void setGraphicalEQEditor(class GraphicalEQEditor* eqEditor);

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
     * Enables normalize preview with real-time gain adjustment.
     * Thread-safe: Can be called from message thread.
     *
     * @param gainDB Required gain in decibels to reach target level
     * @param enabled true to enable normalize processing, false to bypass
     */
    void setNormalizePreview(float gainDB, bool enabled);

    /**
     * Enables fade in/out preview with real-time processing.
     * Thread-safe: Can be called from message thread.
     *
     * @param fadeIn true for fade in, false for fade out
     * @param curveType 0=Linear, 1=Logarithmic, 2=Exponential, 3=S-Curve
     * @param durationMs Fade duration in milliseconds
     * @param enabled true to enable fade processing, false to bypass
     */
    void setFadePreview(bool fadeIn, int curveType, float durationMs, bool enabled);

    /**
     * Enables DC offset removal preview with real-time processing.
     * Thread-safe: Can be called from message thread.
     *
     * @param enabled true to enable DC offset removal, false to bypass
     */
    void setDCOffsetPreview(bool enabled);

    /**
     * Set parametric EQ parameters for real-time preview.
     * Thread-safe: Can be called from message thread.
     *
     * @param params EQ parameters to apply
     * @param enabled true to enable EQ processing, false to bypass
     */
    void setParametricEQPreview(const ParametricEQ::Parameters& params, bool enabled);

    /**
     * Get current parametric EQ enabled state.
     *
     * @return true if EQ is enabled, false otherwise
     */
    bool isParametricEQEnabled() const { return m_parametricEQEnabled.get(); }

    /**
     * Set dynamic parametric EQ parameters for real-time preview.
     * Thread-safe: Can be called from message thread.
     *
     * The dynamic EQ supports up to 20 bands with multiple filter types:
     * Bell, Low Shelf, High Shelf, Low Cut, High Cut, Notch, Bandpass.
     *
     * @param params EQ parameters to apply
     * @param enabled true to enable EQ processing, false to bypass
     */
    void setDynamicEQPreview(const DynamicParametricEQ::Parameters& params, bool enabled);

    /**
     * Get current dynamic EQ enabled state.
     *
     * @return true if dynamic EQ is enabled, false otherwise
     */
    bool isDynamicEQEnabled() const { return m_dynamicEQEnabled.get(); }

    /**
     * Get read-only access to the dynamic EQ for frequency response display.
     * Used by UI to render EQ curves.
     *
     * @return Pointer to the dynamic EQ processor (nullptr if not initialized)
     */
    const DynamicParametricEQ* getDynamicEQ() const { return m_dynamicEQ.get(); }

    //==============================================================================
    // Preview Processor Unified API (Phase 2)

    /**
     * Query structure for active processor states in preview chain.
     * Provides unified access to check which processors are currently enabled.
     */
    struct PreviewProcessorInfo
    {
        bool gainActive;
        bool normalizeActive;
        bool fadeActive;
        bool dcOffsetActive;
        bool eqActive;
    };

    /**
     * Resets all preview processors to initial state.
     * Call when entering preview mode to clear filter history.
     * Thread-safe: Can be called from message thread.
     */
    void resetAllPreviewProcessors();

    /**
     * Disables all preview processors.
     * Use when closing a preview dialog to ensure clean state.
     * Thread-safe: Can be called from message thread.
     */
    void disableAllPreviewProcessors();

    /**
     * Gets current active state of all preview processors.
     * Thread-safe: Uses atomic reads.
     *
     * @return Structure with enabled state of each processor
     */
    PreviewProcessorInfo getPreviewProcessorInfo() const;

    //==============================================================================
    // Preview Bypass Support

    /**
     * Sets the preview bypass state.
     * When bypassed, all preview DSP processing is skipped for A/B comparison.
     * Thread-safe: Can be called from message thread.
     *
     * @param bypassed true to bypass preview processing, false to enable
     */
    void setPreviewBypassed(bool bypassed);

    /**
     * Gets the current preview bypass state.
     * Thread-safe: Uses atomic read.
     *
     * @return true if preview processing is bypassed
     */
    bool isPreviewBypassed() const;

    //==============================================================================
    // Plugin Chain Support

    /**
     * Gets the plugin chain for this audio engine.
     * The plugin chain can be used to add VST3/AU effect plugins to the signal path.
     *
     * @return Reference to the plugin chain
     */
    PluginChain& getPluginChain() { return m_pluginChain; }
    const PluginChain& getPluginChain() const { return m_pluginChain; }

    /**
     * Enables or disables plugin chain processing during playback.
     * Thread-safe, can be called from UI thread.
     *
     * @param enabled true to enable plugin processing, false to bypass
     */
    void setPluginChainEnabled(bool enabled) { m_pluginChainEnabled.store(enabled); }

    /**
     * Gets the plugin chain enabled state.
     *
     * @return true if plugin chain processing is enabled
     */
    bool isPluginChainEnabled() const { return m_pluginChainEnabled.load(); }

    /**
     * Sets a single preview plugin instance for real-time offline plugin preview.
     * This is used by OfflinePluginDialog to enable real-time preview with
     * plugin visualization (e.g., FabFilter Pro-Q 4 spectrum display).
     *
     * The plugin instance is NOT owned by AudioEngine - caller maintains ownership.
     * Thread-safe, can be called from UI thread.
     *
     * @param instance Pointer to the plugin instance, or nullptr to clear
     */
    void setPreviewPluginInstance(juce::AudioPluginInstance* instance);

    /**
     * Gets the current preview plugin instance.
     *
     * @return Pointer to the preview plugin instance, or nullptr if none set
     */
    juce::AudioPluginInstance* getPreviewPluginInstance() const { return m_previewPluginInstance.load(); }

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
    static constexpr int MAX_CHANNELS = 8;  // Up to 7.1 surround
    std::atomic<float> m_peakLevels[MAX_CHANNELS];
    std::atomic<float> m_rmsLevels[MAX_CHANNELS];

    // Channel solo/mute state for monitoring
    std::atomic<bool> m_channelSolo[MAX_CHANNELS];
    std::atomic<bool> m_channelMute[MAX_CHANNELS];

    // Spectrum analyzer (not owned, just a pointer for data feeding)
    class SpectrumAnalyzer* m_spectrumAnalyzer;

    // Graphical EQ editor (not owned, just a pointer for spectrum data feeding during preview)
    class GraphicalEQEditor* m_graphicalEQEditor = nullptr;

    //==============================================================================
    // Preview System Members

    // Preview mode state
    std::atomic<PreviewMode> m_previewMode;

    // Preview bypass state - when true, all preview DSP processing is bypassed
    // Allows A/B comparison between processed and unprocessed audio
    std::atomic<bool> m_previewBypassed{false};

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

    // Normalize processor for real-time preview (applies fixed gain)
    struct NormalizeProcessor
    {
        std::atomic<float> gainDB{0.0f};
        std::atomic<bool> enabled{false};

        void process(juce::AudioBuffer<float>& buffer)
        {
            if (!enabled.load()) return;
            float gain = juce::Decibels::decibelsToGain(gainDB.load());
            buffer.applyGain(gain);
        }
    };
    NormalizeProcessor m_normalizeProcessor;

    // Fade processor for real-time fade in/out preview
    struct FadeProcessor
    {
        enum class FadeType { FADE_IN, FADE_OUT };
        enum class CurveType { LINEAR, LOGARITHMIC, EXPONENTIAL, S_CURVE };

        std::atomic<FadeType> fadeType{FadeType::FADE_IN};
        std::atomic<CurveType> curveType{CurveType::LINEAR};
        std::atomic<float> fadeDurationSamples{44100.0f};  // 1 second at 44.1kHz
        std::atomic<bool> enabled{false};
        std::atomic<int64_t> samplesProcessed{0};

        void reset() { samplesProcessed.store(0); }

        void process(juce::AudioBuffer<float>& buffer, double /*sampleRate*/)
        {
            if (!enabled.load()) return;

            const int numSamples = buffer.getNumSamples();
            const int numChannels = buffer.getNumChannels();
            const float duration = fadeDurationSamples.load();
            const FadeType type = fadeType.load();
            const CurveType curve = curveType.load();
            int64_t currentSample = samplesProcessed.load();

            for (int i = 0; i < numSamples; ++i)
            {
                // P2 FIX: Wrap sample index for looping support
                // When looping preview, fade should restart at 0 for each iteration
                int64_t sampleIndex = currentSample + i;
                if (duration > 0.0f && sampleIndex >= static_cast<int64_t>(duration))
                    sampleIndex = sampleIndex % static_cast<int64_t>(duration);

                float progress = juce::jmin(1.0f, static_cast<float>(sampleIndex) / duration);

                // Apply curve shaping
                float gain = progress;
                switch (curve)
                {
                    case CurveType::LOGARITHMIC:
                        gain = std::log10(1.0f + progress * 9.0f);  // log10(1 to 10) = 0 to 1
                        break;
                    case CurveType::EXPONENTIAL:
                        gain = (std::exp(progress * 2.0f) - 1.0f) / (std::exp(2.0f) - 1.0f);
                        break;
                    case CurveType::S_CURVE:
                        gain = 0.5f * (1.0f + std::tanh(6.0f * (progress - 0.5f)));
                        break;
                    case CurveType::LINEAR:
                    default:
                        break;
                }

                // Invert for fade out
                if (type == FadeType::FADE_OUT)
                    gain = 1.0f - gain;

                // Apply gain to all channels
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    buffer.getWritePointer(ch)[i] *= gain;
                }
            }

            // P2 FIX: Wrap sample counter for looping support
            int64_t newSampleCount = currentSample + numSamples;
            if (duration > 0.0f)
                newSampleCount = newSampleCount % static_cast<int64_t>(duration);
            samplesProcessed.store(newSampleCount);
        }
    };
    FadeProcessor m_fadeProcessor;

    // DC Offset processor for real-time DC removal
    struct DCOffsetProcessor
    {
        std::atomic<bool> enabled{false};
        std::atomic<float> highpassFreq{5.0f};  // 5Hz high-pass filter

        // Simple DC blocking filter (high-pass at 5Hz)
        struct DCBlocker
        {
            float x1 = 0.0f;  // Previous input
            float y1 = 0.0f;  // Previous output
            float alpha = 0.995f;  // Filter coefficient

            void updateCoefficient(double sampleRate, float cutoffHz)
            {
                // Calculate alpha for first-order high-pass filter
                float rc = 1.0f / (2.0f * juce::MathConstants<float>::pi * cutoffHz);
                float dt = 1.0f / static_cast<float>(sampleRate);
                alpha = rc / (rc + dt);
            }

            float processSample(float input)
            {
                float output = alpha * (y1 + input - x1);
                x1 = input;
                y1 = output;
                return output;
            }

            void reset()
            {
                x1 = 0.0f;
                y1 = 0.0f;
            }
        };

        std::array<DCBlocker, 8> dcBlockers;  // Support up to 8 channels

        void process(juce::AudioBuffer<float>& buffer, double sampleRate)
        {
            if (!enabled.load()) return;

            const int numChannels = buffer.getNumChannels();
            const int numSamples = buffer.getNumSamples();
            const float freq = highpassFreq.load();

            // Update filter coefficients if needed
            for (int ch = 0; ch < numChannels && ch < 8; ++ch)
            {
                dcBlockers[static_cast<size_t>(ch)].updateCoefficient(sampleRate, freq);

                // Process samples
                float* channelData = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    channelData[i] = dcBlockers[static_cast<size_t>(ch)].processSample(channelData[i]);
                }
            }
        }

        void reset()
        {
            for (auto& blocker : dcBlockers)
                blocker.reset();
        }
    };
    DCOffsetProcessor m_dcOffsetProcessor;

    // Parametric EQ processor for real-time preview (3-band fixed)
    std::unique_ptr<ParametricEQ> m_parametricEQ;
    juce::Atomic<bool> m_parametricEQEnabled{false};
    ParametricEQ::Parameters m_parametricEQParams;  // Accessed only from audio thread after atomic flag
    juce::Atomic<bool> m_parametricEQParamsChanged{false};
    ParametricEQ::Parameters m_pendingParametricEQParams;  // Written from message thread

    // Dynamic Parametric EQ processor for real-time preview (20-band, multiple filter types)
    std::unique_ptr<DynamicParametricEQ> m_dynamicEQ;
    juce::Atomic<bool> m_dynamicEQEnabled{false};
    DynamicParametricEQ::Parameters m_dynamicEQParams;  // Accessed only from audio thread after atomic flag
    juce::Atomic<bool> m_dynamicEQParamsChanged{false};
    DynamicParametricEQ::Parameters m_pendingDynamicEQParams;  // Written from message thread

    // VST3/AU Plugin Chain for real-time effects processing
    PluginChain m_pluginChain;
    std::atomic<bool> m_pluginChainEnabled{false};
    juce::MidiBuffer m_emptyMidiBuffer;  // Empty MIDI buffer for processBlock (effects-only)

    // Mute flag to prevent audio output during other document's preview
    std::atomic<bool> m_isMuted{false};

    // Preview plugin instance for OfflinePluginDialog real-time preview
    // NOT owned by AudioEngine - caller maintains ownership
    std::atomic<juce::AudioPluginInstance*> m_previewPluginInstance{nullptr};

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
