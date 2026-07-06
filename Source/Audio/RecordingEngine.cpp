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

    // M18: allocate the (large) recording buffer here, on record start,
    // rather than on every device (re)start. M17: reset drop counter.
    {
        juce::ScopedLock lock(m_bufferLock);
        m_droppedSampleCount.store(0);
        resolveRecordChannels();  // set m_numChannels before sizing the buffer
        if (! ensureRecordingBufferAllocated())
        {
            juce::Logger::writeToLog("RecordingEngine::startRecording: "
                                     "failed to allocate recording buffer");
            return false;
        }
    }

    // Start recording
    m_recordingState = RecordingState::RECORDING;

    // Notify listeners (on the message thread — direct broadcast).
    notifyListenersAsync();

    return true;
}

void RecordingEngine::resolveRecordChannels()
{
    // Caller holds m_bufferLock. Clamp the user request to the sane range
    // and to what the current device actually provides (if known).
    int resolved = juce::jlimit(1, MAX_CHANNELS, m_requestedChannels.load());
    const int deviceInputs = m_deviceInputChannels.load();
    if (deviceInputs > 0)
        resolved = juce::jmin(resolved, deviceInputs);
    m_numChannels.store(juce::jmax(1, resolved));
}

bool RecordingEngine::ensureRecordingBufferAllocated()
{
    // Caller holds m_bufferLock. Allocate ~1 hour at the current rate.
    const double sr = m_sampleRate.load();
    const int    ch = juce::jmax(1, m_numChannels.load());
    const int maxSamples = static_cast<int>(sr * kMaxRecordSeconds);  // 1 hour
    if (maxSamples <= 0)
        return false;

    if (m_recordedBuffer.getNumChannels() != ch
        || m_recordedBuffer.getNumSamples() != maxSamples)
    {
        m_recordedBuffer.setSize(ch, maxSamples);
    }
    m_recordedBuffer.clear();
    m_recordedSampleCount = 0;
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

    // Notify listeners. stopRecording() can be invoked from the device
    // teardown thread (audioDeviceStopped), so marshal to the message
    // thread rather than calling sendChangeMessage() here (C9).
    notifyListenersAsync();

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

void RecordingEngine::setRequestedChannelCount(int channelCount)
{
    // Applied on the next startRecording(); do not disturb an active take.
    if (m_recordingState.load() != RecordingState::RECORDING)
        m_requestedChannels.store(juce::jlimit(1, MAX_CHANNELS, channelCount));
}

int RecordingEngine::getRequestedChannelCount() const
{
    return juce::jlimit(1, MAX_CHANNELS, m_requestedChannels.load());
}

double RecordingEngine::getMaxRecordSeconds() const
{
    return kMaxRecordSeconds;
}

double RecordingEngine::getRemainingRecordSeconds() const
{
    const double sr = m_sampleRate.load();
    if (sr <= 0.0)
        return 0.0;

    // Capacity is fixed for the duration of a take (only resized under lock
    // at record start), so reading getNumSamples() lock-free here is safe.
    const int capacity = m_recordedBuffer.getNumSamples();
    if (capacity <= 0)
        return kMaxRecordSeconds;  // not yet allocated -> full cap available

    const int used = m_recordedSampleCount.load();
    return juce::jmax(0, capacity - used) / sr;
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
    m_droppedSampleCount.store(0);  // M17

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

    // Store device parameters only. M18: do NOT allocate the 1-hour
    // recording buffer here — this fires on every device / sample-rate /
    // buffer-size change, even while idle, and at pro rates the
    // allocation is gigabytes under the lock. The buffer is allocated
    // lazily in startRecording() instead.
    m_sampleRate = device->getCurrentSampleRate();

    // Record how many input channels the device exposes so the recorded
    // channel count can be resolved (against the user's mono/stereo choice)
    // on record start. Do NOT set m_numChannels here — that is the RESOLVED
    // recorded count and is decided in resolveRecordChannels() so the user's
    // mono/stereo selection is not clobbered by a device reconfigure.
    m_deviceInputChannels = juce::jmin(device->getActiveInputChannels().countNumberOfSetBits(), MAX_CHANNELS);
}

void RecordingEngine::audioDeviceStopped()
{
    // Audio device stopped - stop recording if active. This runs on the
    // device-teardown thread; stopRecording() marshals its change
    // notification to the message thread (C9), so it's safe to call here.
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
                                                       const juce::AudioIODeviceCallbackContext& /*context*/)
{
    // H-H1 FIX: set FTZ/DAZ for the whole callback, matching the playback engine.
    // Input metering / RMS math over near-silent input can otherwise generate
    // denormals that stall the record thread. First statement so it covers all
    // paths below.
    const juce::ScopedNoDenormals noDenormals;

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
        // Better to drop samples than block the audio thread.
        // M17: track the gap so it isn't invisible. Only counts when we
        // are actually recording (lock contention while idle is benign).
        // Atomic increment only — no logging/allocation on the audio
        // thread; the UI reads getDroppedSampleCount() on its timer.
        if (m_recordingState.load() == RecordingState::RECORDING)
            m_droppedSampleCount.fetch_add(numSamples);
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
        // Buffer full - stop recording gracefully. We're on the realtime
        // audio thread, so we MUST NOT sendChangeMessage()/callAsync here
        // (C9 / §6.4). Instead set the atomic m_bufferFull flag; the
        // RecordingDialog's timer polls isBufferFull() and tears down the
        // recording on the message thread, so the UI does learn that
        // recording stopped.
        m_recordingState.store(RecordingState::IDLE);
        m_bufferFull.store(true);  // Set flag for UI to poll (thread-safe, no allocation)
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

void RecordingEngine::notifyListenersAsync()
{
    // C9: ChangeBroadcaster::sendChangeMessage() is only safe on the
    // message thread. Broadcast directly when we're already there;
    // otherwise bounce onto it, guarded by a WeakReference so a
    // teardown-time destruction can't fire into freed memory.
    if (juce::MessageManager::existsAndIsCurrentThread())
    {
        sendChangeMessage();
        return;
    }

    juce::WeakReference<RecordingEngine> weakThis(this);
    juce::MessageManager::callAsync([weakThis]()
    {
        if (auto* self = weakThis.get())
            self->sendChangeMessage();
    });
}
