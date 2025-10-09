/*
  ==============================================================================

    AudioEngine.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "AudioEngine.h"

//==============================================================================
// MemoryAudioSource Implementation

AudioEngine::MemoryAudioSource::MemoryAudioSource()
    : m_sampleRate(44100.0),
      m_readPosition(0),
      m_isLooping(false)
{
}

AudioEngine::MemoryAudioSource::~MemoryAudioSource()
{
}

void AudioEngine::MemoryAudioSource::setBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    // IMPORTANT: Must be called from message thread only
    // This method allocates memory and should never be called from audio thread
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    juce::ScopedLock sl(m_lock);

    // Make a deep copy of the buffer (safe because we're on message thread)
    m_buffer.makeCopyOf(buffer);
    m_sampleRate = sampleRate;
    m_readPosition.store(0);

    juce::Logger::writeToLog("MemoryAudioSource: Set buffer with " +
                             juce::String(m_buffer.getNumSamples()) + " samples, " +
                             juce::String(m_buffer.getNumChannels()) + " channels");
}

void AudioEngine::MemoryAudioSource::clear()
{
    juce::ScopedLock sl(m_lock);
    m_buffer.setSize(0, 0);
    m_readPosition.store(0);
}

void AudioEngine::MemoryAudioSource::prepareToPlay(int /*samplesPerBlockExpected*/, double /*sampleRate*/)
{
    // Nothing special needed for preparation
}

void AudioEngine::MemoryAudioSource::releaseResources()
{
    // Nothing to release
}

void AudioEngine::MemoryAudioSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    juce::ScopedLock sl(m_lock);

    bufferToFill.clearActiveBufferRegion();

    if (m_buffer.getNumSamples() == 0)
        return;

    juce::int64 startSample = m_readPosition.load();
    int numSamplesToRead = bufferToFill.numSamples;
    int numSamplesAvailable = static_cast<int>(m_buffer.getNumSamples() - startSample);

    if (numSamplesAvailable <= 0)
    {
        // Reached end of buffer
        if (m_isLooping)
        {
            // Reset position and recalculate - avoid recursion
            m_readPosition.store(0);
            startSample = 0;
            numSamplesAvailable = m_buffer.getNumSamples();
        }
        else
        {
            return;
        }
    }

    int numSamples = juce::jmin(numSamplesToRead, numSamplesAvailable);

    // Additional safety: Ensure we don't read past buffer end
    if (startSample + numSamples > m_buffer.getNumSamples())
    {
        numSamples = static_cast<int>(m_buffer.getNumSamples() - startSample);
        if (numSamples <= 0)
            return;
    }

    // Copy audio data from buffer to output
    for (int ch = 0; ch < juce::jmin(m_buffer.getNumChannels(), bufferToFill.buffer->getNumChannels()); ++ch)
    {
        // Verify bounds before copying
        jassert(static_cast<int>(startSample) + numSamples <= m_buffer.getNumSamples());
        jassert(bufferToFill.startSample + numSamples <= bufferToFill.buffer->getNumSamples());

        bufferToFill.buffer->copyFrom(ch, bufferToFill.startSample,
                                      m_buffer, ch, static_cast<int>(startSample), numSamples);
    }

    m_readPosition.store(startSample + numSamples);
}

void AudioEngine::MemoryAudioSource::setNextReadPosition(juce::int64 newPosition)
{
    m_readPosition.store(juce::jlimit<juce::int64>(0, m_buffer.getNumSamples(), newPosition));
}

juce::int64 AudioEngine::MemoryAudioSource::getNextReadPosition() const
{
    return m_readPosition.load();
}

juce::int64 AudioEngine::MemoryAudioSource::getTotalLength() const
{
    return m_buffer.getNumSamples();
}

bool AudioEngine::MemoryAudioSource::isLooping() const
{
    return m_isLooping;
}

void AudioEngine::MemoryAudioSource::setLooping(bool shouldLoop)
{
    m_isLooping = shouldLoop;
}

//==============================================================================
// AudioEngine Implementation

AudioEngine::AudioEngine()
    : m_playbackState(PlaybackState::STOPPED),
      m_isPlayingFromBuffer(false),
      m_isLooping(false),
      m_sampleRate(0.0),
      m_numChannels(0),
      m_bitDepth(0)
{
    // Register basic audio formats (WAV for Phase 1)
    m_formatManager.registerBasicFormats();

    // Listen for transport source changes
    m_transportSource.addChangeListener(this);

    // Initialize background thread for file loading
    m_backgroundThread = std::make_unique<juce::TimeSliceThread>("Audio Loading Thread");
    m_backgroundThread->startThread();

    // Create buffer source (initially empty)
    m_bufferSource = std::make_unique<MemoryAudioSource>();
}

AudioEngine::~AudioEngine()
{
    // Stop background thread
    if (m_backgroundThread)
    {
        m_backgroundThread->stopThread(1000);
    }

    // Clean up
    m_transportSource.removeChangeListener(this);
    m_transportSource.setSource(nullptr);
    m_readerSource.reset();
}

//==============================================================================
// Device Management

bool AudioEngine::initializeAudioDevice()
{
    // Set up audio device with default settings
    // This will use the system's default audio output device
    juce::String audioError = m_deviceManager.initialise(
        0,      // Number of input channels (0 for playback only in Phase 1)
        2,      // Number of output channels (stereo)
        nullptr, // No saved state to restore
        true,    // Select default device if possible
        {},      // Preferred default device name
        nullptr  // Preferred setup options
    );

    if (audioError.isNotEmpty())
    {
        // Initialization failed
        juce::Logger::writeToLog("Audio device initialization failed: " + audioError);
        return false;
    }

    // Add this audio engine as the audio callback
    m_deviceManager.addAudioCallback(this);

    juce::Logger::writeToLog("Audio device initialized successfully");
    return true;
}

juce::AudioDeviceManager& AudioEngine::getDeviceManager()
{
    return m_deviceManager;
}

juce::AudioFormatManager& AudioEngine::getFormatManager()
{
    return m_formatManager;
}

//==============================================================================
// File Loading

bool AudioEngine::loadAudioFile(const juce::File& file)
{
    // Validate file exists
    if (!file.existsAsFile())
    {
        juce::Logger::writeToLog("File does not exist: " + file.getFullPathName());
        return false;
    }

    // Stop playback before loading new file
    stop();

    // Release current file resources
    m_transportSource.setSource(nullptr);
    m_readerSource.reset();

    // Create a reader for the audio file
    auto* reader = m_formatManager.createReaderFor(file);

    if (reader == nullptr)
    {
        juce::Logger::writeToLog("Failed to create reader for file: " + file.getFullPathName());
        return false;
    }

    // Validate audio format
    if (!validateAudioFormat(reader))
    {
        delete reader;
        return false;
    }

    // Store file properties (thread-safe atomic stores)
    m_sampleRate.store(reader->sampleRate);
    m_numChannels.store(static_cast<int>(reader->numChannels));
    m_bitDepth.store(static_cast<int>(reader->bitsPerSample));

    // Create reader source with the audio data
    // The reader source will take ownership of the reader
    m_readerSource.reset(new juce::AudioFormatReaderSource(reader, true));

    // Connect the reader source to the transport source
    m_transportSource.setSource(
        m_readerSource.get(),
        0,                      // Buffer size to use (0 = default)
        m_backgroundThread.get(), // Background thread for non-blocking loading
        m_sampleRate,           // Sample rate of the reader
        m_numChannels           // Number of channels
    );

    // Store the current file
    m_currentFile = file;

    // Switch to file playback mode
    m_isPlayingFromBuffer.store(false);

    juce::Logger::writeToLog("Successfully loaded file: " + file.getFullPathName());
    juce::Logger::writeToLog("Sample rate: " + juce::String(m_sampleRate) + " Hz");
    juce::Logger::writeToLog("Channels: " + juce::String(m_numChannels));
    juce::Logger::writeToLog("Bit depth: " + juce::String(m_bitDepth) + " bits");

    return true;
}

bool AudioEngine::loadFromBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate, int numChannels)
{
    // IMPORTANT: This method must only be called from the message thread
    // It performs memory allocation and modifies the transport source
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Validate buffer
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        juce::Logger::writeToLog("AudioEngine::loadFromBuffer - Cannot load from empty buffer");
        return false;
    }

    // Validate channel count matches buffer
    if (numChannels != buffer.getNumChannels())
    {
        juce::Logger::writeToLog("AudioEngine::loadFromBuffer - Channel count mismatch: expected " +
                                 juce::String(numChannels) + ", got " + juce::String(buffer.getNumChannels()));
        return false;
    }

    // Validate sample rate is in reasonable range
    if (sampleRate <= 0.0 || sampleRate < 8000.0 || sampleRate > 192000.0)
    {
        juce::Logger::writeToLog("AudioEngine::loadFromBuffer - Invalid sample rate: " +
                                 juce::String(sampleRate) + " Hz (must be 8kHz-192kHz)");
        return false;
    }

    // Validate audio data doesn't contain NaN or Inf
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* data = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (!std::isfinite(data[i]))
            {
                juce::Logger::writeToLog("AudioEngine::loadFromBuffer - Buffer contains invalid samples (NaN/Inf)");
                return false;
            }
        }
    }

    // Stop playback before switching sources
    stop();

    // Release current sources (thread-safe as we're on message thread)
    m_transportSource.setSource(nullptr);
    m_readerSource.reset();

    // Store audio properties (thread-safe atomic stores)
    m_sampleRate.store(sampleRate);
    m_numChannels.store(numChannels);

    // Set bit depth to 32-bit float for buffer playback
    // (AudioBuffer<float> is always 32-bit float internally)
    // Keep the original bit depth from file for save operations
    if (m_bitDepth.load() == 0)
    {
        m_bitDepth.store(32); // Default if no file was loaded before
    }
    // Note: When saving, we'll use the original file's bit depth if available

    // Ensure buffer source exists
    if (m_bufferSource == nullptr)
    {
        juce::Logger::writeToLog("AudioEngine::loadFromBuffer - Buffer source is null, cannot load");
        return false;
    }

    // Set the buffer in our memory source
    m_bufferSource->setBuffer(buffer, sampleRate);

    // Connect the buffer source to the transport source
    // Verify the buffer source pointer is valid before using it
    if (m_bufferSource.get() == nullptr)
    {
        juce::Logger::writeToLog("AudioEngine::loadFromBuffer - Failed to get valid buffer source pointer");
        return false;
    }

    m_transportSource.setSource(
        m_bufferSource.get(),
        0,                      // Buffer size to use (0 = default)
        nullptr,                // No background thread needed for memory buffer
        sampleRate,             // Sample rate
        numChannels             // Number of channels
    );

    // Switch to buffer playback mode
    m_isPlayingFromBuffer.store(true);

    juce::Logger::writeToLog("Successfully loaded buffer for playback");
    juce::Logger::writeToLog("Sample rate: " + juce::String(sampleRate) + " Hz");
    juce::Logger::writeToLog("Channels: " + juce::String(numChannels));
    juce::Logger::writeToLog("Samples: " + juce::String(buffer.getNumSamples()));
    juce::Logger::writeToLog("Duration: " + juce::String(buffer.getNumSamples() / sampleRate, 2) + " seconds");

    return true;
}

void AudioEngine::closeAudioFile()
{
    // Stop playback
    stop();

    // Release resources
    m_transportSource.setSource(nullptr);
    m_readerSource.reset();
    m_bufferSource->clear();

    // Clear file properties (thread-safe atomic stores)
    m_currentFile = juce::File();
    m_sampleRate.store(0.0);
    m_numChannels.store(0);
    m_bitDepth.store(0);
    m_isPlayingFromBuffer.store(false);

    juce::Logger::writeToLog("Audio file closed");
}

bool AudioEngine::isFileLoaded() const
{
    return m_readerSource != nullptr || m_isPlayingFromBuffer.load();
}

bool AudioEngine::isPlayingFromBuffer() const
{
    return m_isPlayingFromBuffer.load();
}

juce::File AudioEngine::getCurrentFile() const
{
    return m_currentFile;
}

//==============================================================================
// Playback Control

void AudioEngine::play()
{
    if (!isFileLoaded())
    {
        juce::Logger::writeToLog("Cannot play: No file loaded");
        return;
    }

    // Don't reset position here - caller should use setPosition() before play()
    // if they want to start from a specific position

    m_transportSource.start();
    updatePlaybackState(PlaybackState::PLAYING);

    juce::Logger::writeToLog("Playback started");
}

void AudioEngine::pause()
{
    if (!isFileLoaded())
    {
        return;
    }

    m_transportSource.stop();
    updatePlaybackState(PlaybackState::PAUSED);

    juce::Logger::writeToLog("Playback paused");
}

void AudioEngine::stop()
{
    if (!isFileLoaded())
    {
        return;
    }

    m_transportSource.stop();
    m_transportSource.setPosition(0.0);
    updatePlaybackState(PlaybackState::STOPPED);

    juce::Logger::writeToLog("Playback stopped");
}

PlaybackState AudioEngine::getPlaybackState() const
{
    return m_playbackState.load();
}

bool AudioEngine::isPlaying() const
{
    return m_playbackState == PlaybackState::PLAYING;
}

//==============================================================================
// Loop Control

void AudioEngine::setLooping(bool shouldLoop)
{
    m_isLooping.store(shouldLoop);

    // Apply loop state to buffer source if currently playing from buffer
    if (m_isPlayingFromBuffer.load() && m_bufferSource)
    {
        m_bufferSource->setLooping(shouldLoop);
    }

    // Note: AudioFormatReaderSource doesn't directly support looping,
    // so we'll handle file-based looping by restarting playback when it ends

    juce::Logger::writeToLog(shouldLoop ? "Loop enabled" : "Loop disabled");
}

bool AudioEngine::isLooping() const
{
    return m_isLooping.load();
}

//==============================================================================
// Transport Position

double AudioEngine::getCurrentPosition() const
{
    if (!isFileLoaded())
    {
        return 0.0;
    }

    return m_transportSource.getCurrentPosition();
}

void AudioEngine::setPosition(double positionInSeconds)
{
    if (!isFileLoaded())
    {
        return;
    }

    // Clamp position to valid range
    double length = getTotalLength();
    positionInSeconds = juce::jlimit(0.0, length, positionInSeconds);

    m_transportSource.setPosition(positionInSeconds);

    juce::Logger::writeToLog("Position set to: " + juce::String(positionInSeconds) + " seconds");
}

double AudioEngine::getTotalLength() const
{
    if (!isFileLoaded())
    {
        return 0.0;
    }

    return m_transportSource.getLengthInSeconds();
}

//==============================================================================
// Audio Properties

double AudioEngine::getSampleRate() const
{
    return m_sampleRate.load();
}

int AudioEngine::getNumChannels() const
{
    return m_numChannels.load();
}

int AudioEngine::getBitDepth() const
{
    return m_bitDepth.load();
}

//==============================================================================
// ChangeListener Implementation

void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &m_transportSource)
    {
        // Check if playback has stopped naturally (reached end of file)
        if (m_transportSource.hasStreamFinished())
        {
            updatePlaybackState(PlaybackState::STOPPED);
            juce::Logger::writeToLog("Playback finished (end of file reached)");
        }
    }
}

//==============================================================================
// AudioIODeviceCallback Implementation

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    // Prepare the transport source for playback
    if (device != nullptr)
    {
        m_transportSource.prepareToPlay(device->getCurrentBufferSizeSamples(),
                                         device->getCurrentSampleRate());
    }
}

void AudioEngine::audioDeviceStopped()
{
    // Release audio resources
    m_transportSource.releaseResources();
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* /*inputChannelData*/,
                                                    int /*numInputChannels*/,
                                                    float* const* outputChannelData,
                                                    int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext& /*context*/)
{
    // Create an audio buffer for the output
    juce::AudioBuffer<float> buffer(const_cast<float**>(outputChannelData), numOutputChannels, numSamples);

    // Clear the buffer first
    buffer.clear();

    // Get audio from the transport source
    juce::AudioSourceChannelInfo channelInfo(buffer);
    m_transportSource.getNextAudioBlock(channelInfo);
}

//==============================================================================
// Private Methods

void AudioEngine::updatePlaybackState(PlaybackState newState)
{
    auto currentState = m_playbackState.load();
    if (currentState != newState)
    {
        m_playbackState.store(newState);
        // In future phases, this could notify listeners of state changes
    }
}

bool AudioEngine::validateAudioFormat(juce::AudioFormatReader* reader)
{
    if (reader == nullptr)
    {
        return false;
    }

    // Check sample rate is supported
    double sampleRate = reader->sampleRate;
    const double supportedRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    bool sampleRateSupported = false;

    for (double rate : supportedRates)
    {
        if (std::abs(sampleRate - rate) < 0.1) // Allow small floating point differences
        {
            sampleRateSupported = true;
            break;
        }
    }

    if (!sampleRateSupported)
    {
        juce::Logger::writeToLog("Unsupported sample rate: " + juce::String(sampleRate) + " Hz");
        juce::Logger::writeToLog("Supported rates: 44.1kHz, 48kHz, 88.2kHz, 96kHz, 192kHz");
        return false;
    }

    // Check channel count (mono, stereo, or multichannel up to 8 channels)
    int numChannels = static_cast<int>(reader->numChannels);
    if (numChannels < 1 || numChannels > 8)
    {
        juce::Logger::writeToLog("Unsupported channel count: " + juce::String(numChannels));
        juce::Logger::writeToLog("Supported: 1-8 channels");
        return false;
    }

    // Check bit depth (16, 24, or 32-bit for Phase 1)
    int bitDepth = static_cast<int>(reader->bitsPerSample);
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32)
    {
        juce::Logger::writeToLog("Unsupported bit depth: " + juce::String(bitDepth) + " bits");
        juce::Logger::writeToLog("Supported: 16-bit, 24-bit, or 32-bit");
        return false;
    }

    // Format is valid
    return true;
}
