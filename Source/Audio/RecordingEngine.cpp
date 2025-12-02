/*
  ==============================================================================

    RecordingEngine.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "RecordingEngine.h"

//==============================================================================
// Constructor / Destructor

RecordingEngine::RecordingEngine()
    : m_recordingState(RecordingState::IDLE),
      m_sampleRate(44100.0),
      m_numChannels(2),
      m_bufferFull(false),
      m_recordedSampleCount(0)
{
    // Initialize level meters to zero
    for (int i = 0; i < MAX_CHANNELS; ++i)
    {
        m_inputPeakLevels[i] = 0.0f;
        m_inputRMSLevels[i] = 0.0f;
    }

    // Preallocate temporary recording buffer (avoid allocations on audio thread)
    m_tempRecordingBuffer.setSize(MAX_CHANNELS, TEMP_BUFFER_SIZE);
}

RecordingEngine::~RecordingEngine()
{
    stopRecording();
}

//==============================================================================
// Recording Control

bool RecordingEngine::startRecording()
{
    if (m_recordingState != RecordingState::IDLE)
    {
        jassertfalse;  // Already recording
        return false;
    }

    // Clear any previous recording
    clearRecording();

    // Start recording
    m_recordingState = RecordingState::RECORDING;

    // Notify listeners
    sendChangeMessage();

    return true;
}

bool RecordingEngine::stopRecording()
{
    if (m_recordingState == RecordingState::IDLE)
    {
        return false;  // Not recording
    }

    // Stop recording
    m_recordingState = RecordingState::IDLE;

    // Trim the recorded buffer to actual sample count
    {
        juce::ScopedLock lock(m_bufferLock);

        int actualSamples = m_recordedSampleCount.load();
        if (actualSamples > 0 && actualSamples < m_recordedBuffer.getNumSamples())
        {
            juce::AudioBuffer<float> trimmedBuffer(m_numChannels, actualSamples);
            for (int ch = 0; ch < m_numChannels; ++ch)
            {
                trimmedBuffer.copyFrom(ch, 0, m_recordedBuffer, ch, 0, actualSamples);
            }
            m_recordedBuffer = std::move(trimmedBuffer);
        }
    }

    // Notify listeners
    sendChangeMessage();

    return true;
}

RecordingState RecordingEngine::getRecordingState() const
{
    return m_recordingState.load();
}

bool RecordingEngine::isRecording() const
{
    return m_recordingState == RecordingState::RECORDING;
}

bool RecordingEngine::isBufferFull() const
{
    return m_bufferFull.load();
}

//==============================================================================
// Recorded Audio Access

const juce::AudioBuffer<float>& RecordingEngine::getRecordedAudio() const
{
    return m_recordedBuffer;
}

double RecordingEngine::getRecordedSampleRate() const
{
    return m_sampleRate.load();
}

int RecordingEngine::getRecordedNumChannels() const
{
    return m_numChannels.load();
}

double RecordingEngine::getRecordingDuration() const
{
    double sr = m_sampleRate.load();
    if (sr <= 0.0)
        return 0.0;

    return static_cast<double>(m_recordedSampleCount.load()) / sr;
}

void RecordingEngine::clearRecording()
{
    juce::ScopedLock lock(m_bufferLock);

    m_recordedBuffer.clear();
    m_recordedSampleCount = 0;
    m_bufferFull.store(false);

    // Reset level meters
    for (int i = 0; i < MAX_CHANNELS; ++i)
    {
        m_inputPeakLevels[i] = 0.0f;
        m_inputRMSLevels[i] = 0.0f;
    }
}

//==============================================================================
// Level Monitoring

float RecordingEngine::getInputPeakLevel(int channel) const
{
    if (channel < 0 || channel >= MAX_CHANNELS)
        return 0.0f;

    return m_inputPeakLevels[channel].load();
}

float RecordingEngine::getInputRMSLevel(int channel) const
{
    if (channel < 0 || channel >= MAX_CHANNELS)
        return 0.0f;

    return m_inputRMSLevels[channel].load();
}

//==============================================================================
// AudioIODeviceCallback Implementation

void RecordingEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    // Store device parameters
    m_sampleRate = device->getCurrentSampleRate();
    m_numChannels = juce::jmin(device->getActiveInputChannels().countNumberOfSetBits(), MAX_CHANNELS);

    // Allocate recording buffer (1 hour at current sample rate)
    int maxSamples = static_cast<int>(m_sampleRate * 3600.0);  // 1 hour

    juce::ScopedLock lock(m_bufferLock);
    m_recordedBuffer.setSize(m_numChannels, maxSamples);
    m_recordedBuffer.clear();
    m_recordedSampleCount = 0;
}

void RecordingEngine::audioDeviceStopped()
{
    // Audio device stopped - stop recording if active
    if (m_recordingState != RecordingState::IDLE)
    {
        stopRecording();
    }
}

void RecordingEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                       int numInputChannels,
                                                       float* const* outputChannelData,
                                                       int numOutputChannels,
                                                       int numSamples,
                                                       const juce::AudioIODeviceCallbackContext& context)
{
    // Clear output buffers (recording doesn't need playback)
    for (int i = 0; i < numOutputChannels; ++i)
    {
        if (outputChannelData[i] != nullptr)
        {
            juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);
        }
    }

    // Ensure we have valid input data
    if (inputChannelData == nullptr || numInputChannels == 0 || numSamples == 0)
        return;

    // ALWAYS update input level meters for preview/monitoring
    // This allows the UI to show levels even before recording starts
    updateInputLevels(inputChannelData, numInputChannels, numSamples);

    // Only append to buffer when actually recording
    if (m_recordingState == RecordingState::RECORDING)
    {
        appendToRecordingBuffer(inputChannelData, numInputChannels, numSamples);
    }
}

//==============================================================================
// Private Methods

void RecordingEngine::updateInputLevels(const float* const* audioData, int numChannels, int numSamples)
{
    int channelsToMonitor = juce::jmin(numChannels, MAX_CHANNELS);

    for (int ch = 0; ch < channelsToMonitor; ++ch)
    {
        if (audioData[ch] == nullptr)
            continue;

        // Calculate peak level
        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float absValue = std::abs(audioData[ch][i]);
            if (absValue > peak)
                peak = absValue;
        }

        // Calculate RMS level
        float sumSquares = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float value = audioData[ch][i];
            sumSquares += value * value;
        }
        float rms = std::sqrt(sumSquares / static_cast<float>(numSamples));

        // Update atomic values (with simple decay for peak hold)
        float currentPeak = m_inputPeakLevels[ch].load();
        m_inputPeakLevels[ch] = juce::jmax(peak, currentPeak * 0.95f);  // Simple decay

        m_inputRMSLevels[ch] = rms;
    }
}

void RecordingEngine::appendToRecordingBuffer(const float* const* audioData, int numChannels, int numSamples)
{
    // CRITICAL: Null check to prevent crashes when no device available
    if (audioData == nullptr || numSamples <= 0 || numChannels <= 0)
    {
        return;
    }

    // CRITICAL: Use tryLock to avoid blocking on audio thread
    if (!m_bufferLock.tryEnter())
    {
        // Skip this buffer if we can't get the lock immediately
        // Better to drop samples than block the audio thread
        return;
    }

    // Manual unlock using RAII pattern
    struct ScopedUnlocker
    {
        juce::CriticalSection& lock;
        ~ScopedUnlocker() { lock.exit(); }
    } unlocker{m_bufferLock};

    int currentSampleCount = m_recordedSampleCount.load();
    int availableSpace = m_recordedBuffer.getNumSamples() - currentSampleCount;

    // Check if we have space in the buffer
    if (availableSpace < numSamples)
    {
        // Buffer full - stop recording gracefully
        m_recordingState.store(RecordingState::IDLE);
        m_bufferFull.store(true);  // Set flag for UI to poll (thread-safe, no allocation)
        jassertfalse;  // Debug notification
        return;
    }

    // Copy samples to recording buffer
    int channelsToCopy = juce::jmin(numChannels, m_numChannels.load());
    for (int ch = 0; ch < channelsToCopy; ++ch)
    {
        if (audioData[ch] != nullptr)
        {
            m_recordedBuffer.copyFrom(ch, currentSampleCount, audioData[ch], numSamples);
        }
    }

    // Update sample count
    m_recordedSampleCount = currentSampleCount + numSamples;
}
