/*
  ==============================================================================

    AudioEngine.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "AudioEngine.h"
#include "../Automation/AutomationManager.h"
#include "../UI/SpectrumAnalyzer.h"
#include "../UI/GraphicalEQEditor.h"

// Static member definition
std::atomic<AudioEngine*> AudioEngine::s_previewingEngine{nullptr};

//==============================================================================
// MemoryAudioSource Implementation

AudioEngine::MemoryAudioSource::MemoryAudioSource()
    : m_buffer(std::make_shared<const juce::AudioBuffer<float>>()),
      m_sampleRate(44100.0),
      m_readPosition(0),
      m_isLooping(false)
{
}

AudioEngine::MemoryAudioSource::~MemoryAudioSource()
{
}

void AudioEngine::MemoryAudioSource::setBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate, bool preservePosition)
{
    // IMPORTANT: Must be called from message thread only
    // This method allocates memory and should never be called from audio thread
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Capture current position BEFORE the deep copy.
    juce::int64 savedPosition = preservePosition ? m_readPosition.load() : 0;

    // H20 FIX: do the expensive deep copy OFF-lock. The audio thread keeps
    // playing the previous buffer snapshot during this copy and never blocks.
    auto newBuffer = std::make_shared<juce::AudioBuffer<float>>();
    newBuffer->makeCopyOf(buffer);
    const juce::int64 newLength = newBuffer->getNumSamples();

    juce::int64 newPosition = preservePosition ? juce::jmin(savedPosition, newLength) : 0;

    // Swap in the new buffer under a brief lock (pointer swap only). Use
    // swap, not move-assign: assignment would release the OLD buffer inside
    // the lock, and a multi-hundred-MB deallocation would stall any audio
    // callback blocked on m_lock. After the swap, newBuffer holds the old
    // buffer and frees it below, outside the locked scope. (M1)
    BufferPtr incoming = std::move(newBuffer);
    {
        juce::ScopedLock sl(m_lock);
        std::swap(m_buffer, incoming);
        m_sampleRate = sampleRate;
    }
    // 'incoming' now owns the OLD buffer and releases it here, off-lock.

    // Publish the new length and position for lock-free readers (L1).
    m_bufferLength.store(newLength);
    m_readPosition.store(newPosition);
}

void AudioEngine::MemoryAudioSource::clear()
{
    auto emptyBuffer = std::make_shared<juce::AudioBuffer<float>>();
    BufferPtr previous;
    {
        juce::ScopedLock sl(m_lock);
        previous = std::move(m_buffer);
        m_buffer = std::move(emptyBuffer);
    }
    // 'previous' releases the old buffer here, outside the lock (M1).
    m_bufferLength.store(0);
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
    // H20 FIX: copy the current buffer snapshot under a brief lock (refcount bump
    // only), then release the lock and work on the snapshot. setBuffer()'s deep
    // copy happens off-lock, so the audio thread never blocks on it. Holding our
    // own shared_ptr keeps the buffer alive even if setBuffer() swaps mid-block.
    BufferPtr bufferSnapshot;
    {
        juce::ScopedLock sl(m_lock);
        bufferSnapshot = m_buffer;
    }

    bufferToFill.clearActiveBufferRegion();

    const juce::AudioBuffer<float>& src = *bufferSnapshot;
    if (src.getNumSamples() == 0)
    {
        return;
    }

    juce::int64 startSample = m_readPosition.load();
    int numSamplesToRead = bufferToFill.numSamples;
    int numSamplesAvailable = static_cast<int>(src.getNumSamples() - startSample);

    if (numSamplesAvailable <= 0)
    {
        // Reached end of buffer
        if (m_isLooping)
        {
            // Reset position and recalculate - avoid recursion
            m_readPosition.store(0);
            startSample = 0;
            numSamplesAvailable = src.getNumSamples();
        }
        else
        {
            return;
        }
    }

    int numSamples = juce::jmin(numSamplesToRead, numSamplesAvailable);

    // Additional safety: Ensure we don't read past buffer end
    if (startSample + numSamples > src.getNumSamples())
    {
        numSamples = static_cast<int>(src.getNumSamples() - startSample);
        if (numSamples <= 0)
            return;
    }

    // Copy audio data from buffer to output
    // IMPORTANT: Handle mono-to-stereo conversion professionally
    // Mono files should play centered (equal on both channels), not just left channel
    int sourceChannels = src.getNumChannels();
    int outputChannels = bufferToFill.buffer->getNumChannels();

    if (sourceChannels == 1 && outputChannels == 2)
    {
        // Mono to stereo: duplicate mono channel to both L and R for center-panned playback
        // This matches professional audio editor behavior (Sound Forge, Pro Tools, etc.)
        bufferToFill.buffer->copyFrom(0, bufferToFill.startSample,
                                      src, 0, static_cast<int>(startSample), numSamples);
        bufferToFill.buffer->copyFrom(1, bufferToFill.startSample,
                                      src, 0, static_cast<int>(startSample), numSamples);
    }
    else
    {
        // Standard channel mapping: match channels one-to-one up to minimum of source/output
        for (int ch = 0; ch < juce::jmin(sourceChannels, outputChannels); ++ch)
        {
            bufferToFill.buffer->copyFrom(ch, bufferToFill.startSample,
                                          src, ch, static_cast<int>(startSample), numSamples);
        }
    }

    m_readPosition.store(startSample + numSamples);
}

void AudioEngine::MemoryAudioSource::setNextReadPosition(juce::int64 newPosition)
{
    // L1 FIX: read length from the atomic mirror instead of touching m_buffer
    // without the lock. Consistent with the H20 atomic swap.
    juce::int64 clampedPosition = juce::jlimit<juce::int64>(0, m_bufferLength.load(), newPosition);
    m_readPosition.store(clampedPosition);
}

juce::int64 AudioEngine::MemoryAudioSource::getNextReadPosition() const
{
    return m_readPosition.load();
}

juce::int64 AudioEngine::MemoryAudioSource::getTotalLength() const
{
    return m_bufferLength.load();
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
      m_loopStartTime(-1.0),
      m_loopEndTime(-1.0),
      m_sampleRate(0.0),
      m_numChannels(0),
      m_bitDepth(0),
      m_levelMonitoringEnabled(false),
      m_spectrumAnalyzer(nullptr),
      m_previewMode(PreviewMode::DISABLED)
{
    // Register basic audio formats (WAV, FLAC, OGG, MP3)
    m_formatManager.registerBasicFormats();

    // Listen for transport source changes
    m_transportSource.addChangeListener(this);

    // Initialize background thread for file loading
    m_backgroundThread = std::make_unique<juce::TimeSliceThread>("Audio Loading Thread");
    m_backgroundThread->startThread();

    // Create buffer sources (initially empty)
    m_bufferSource = std::make_unique<MemoryAudioSource>();
    m_previewBufferSource = std::make_unique<MemoryAudioSource>();

    // Initialize parametric EQ processor
    m_parametricEQ = std::make_unique<ParametricEQ>();

    // Initialize dynamic parametric EQ processor (20-band, multiple filter types)
    m_dynamicEQ = std::make_unique<DynamicParametricEQ>();

    // Initialize level monitoring state
    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
    {
        m_peakLevels[ch].store(0.0f);
        m_rmsLevels[ch].store(0.0f);
        m_channelSolo[ch].store(false);
        m_channelMute[ch].store(false);
    }
}

AudioEngine::~AudioEngine()
{
    // CRITICAL: Clear global preview pointer if it points to us
    // This prevents dangling pointer issues when an AudioEngine is destroyed
    // while in preview mode
    AudioEngine* current = s_previewingEngine.load();
    if (current == this)
    {
        s_previewingEngine.store(nullptr);
    }

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
        0,      // Number of input channels (0 for playback only - recording in future release)
        2,      // Number of output channels (stereo)
        nullptr, // No saved state to restore
        true,    // Select default device if possible
        {},      // Preferred default device name
        nullptr  // Preferred setup options
    );

    if (audioError.isNotEmpty())
    {
        // Initialization failed
        return false;
    }

    // Add this audio engine as the audio callback
    m_deviceManager.addAudioCallback(this);

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
        return false;
    }

    // Validate channel count matches buffer
    if (numChannels != buffer.getNumChannels())
    {
        return false;
    }

    // Validate sample rate is in reasonable range
    if (sampleRate <= 0.0 || sampleRate < 8000.0 || sampleRate > 192000.0)
    {
        return false;
    }

    // H7 FIX: a full O(N) NaN/Inf scan here froze the message thread for seconds
    // on long files (a 1-hr 96k stereo file is ~690M samples), violating the §9
    // load-time budget. reloadBufferPreservingPlayback() never did this scan, so
    // the check was also asymmetric. We now sample a bounded subset (head, tail,
    // and an even stride across the buffer) so the cost is O(1) regardless of
    // length while still catching whole-buffer corruption. Per-sample DSP paths
    // are responsible for not introducing NaN/Inf, so a probabilistic check is
    // an adequate safety net here.
    {
        constexpr int maxProbesPerChannel = 4096;
        const int numSamples = buffer.getNumSamples();
        const int stride = juce::jmax(1, numSamples / maxProbesPerChannel);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer(ch);

            // Always check the very first and last sample, then a strided subset.
            if (numSamples > 0 && (!std::isfinite(data[0]) || !std::isfinite(data[numSamples - 1])))
                return false;

            for (int i = 0; i < numSamples; i += stride)
            {
                if (!std::isfinite(data[i]))
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
        return false;
    }

    // Set the buffer in our memory source
    m_bufferSource->setBuffer(buffer, sampleRate);

    // Connect the buffer source to the transport source
    // Verify the buffer source pointer is valid before using it
    if (m_bufferSource.get() == nullptr)
    {
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

    return true;
}

bool AudioEngine::reloadBufferPreservingPlayback(const juce::AudioBuffer<float>& buffer, double sampleRate, int numChannels)
{
    // IMPORTANT: This method must only be called from the message thread
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Validate buffer
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        return false;
    }

    // Validate channel count and sample rate
    if (numChannels != buffer.getNumChannels())
    {
        return false;
    }

    if (sampleRate <= 0.0 || sampleRate < 8000.0 || sampleRate > 192000.0)
    {
        return false;
    }

    // Capture current playback state. We always capture position so paused
    // edits keep the cursor where the user left it — only the play/no-play
    // restoration is conditional. Matches the CLAUDE.md §6.5 protocol.
    const bool   wasPlaying      = isPlaying();
    const double currentPosition = getCurrentPosition();

    // CRITICAL: Disconnect transport before updating buffer
    // This flushes AudioTransportSource's internal buffers so it will read fresh audio
    // after reconnecting. Without this, the transport continues playing cached audio
    // from the old buffer even after we update it.
    m_transportSource.setSource(nullptr);

    // Update buffer in memory source (this is thread-safe with the lock in setBuffer)
    // Don't pass preservePosition - we'll manage position via transport reconnect
    if (m_bufferSource)
    {
        m_bufferSource->setBuffer(buffer, sampleRate, false);
    }
    else
    {
        return false;
    }

    // Reconnect transport to buffer source with updated audio
    // This forces fresh audio to be read from the new buffer
    m_transportSource.setSource(
        m_bufferSource.get(),
        0,                      // Buffer size to use (0 = default)
        nullptr,                // No background thread needed for memory buffer
        sampleRate,             // Sample rate
        numChannels             // Number of channels
    );

    // Update stored properties
    m_sampleRate.store(sampleRate);
    m_numChannels.store(numChannels);
    m_isPlayingFromBuffer.store(true);

    // Restore position (clamped to the new buffer length in case the edit
    // shortened it). Always do this so paused edits don't snap to 0.
    const double newLength       = buffer.getNumSamples() / sampleRate;
    const double clampedPosition = juce::jmin(currentPosition, newLength);
    m_transportSource.setPosition(clampedPosition);

    // Resume playback only if we were playing.
    if (wasPlaying)
        m_transportSource.start();

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

    // Clear preview selection offset
    m_previewSelectionStartSamples.store(0);
}

bool AudioEngine::isFileLoaded() const
{
    // File is "loaded" if we have a reader source, or a buffer source, or a preview buffer
    return m_readerSource != nullptr
           || m_isPlayingFromBuffer.load()
           || m_previewMode.load() != PreviewMode::DISABLED;
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
        return;
    }

    // Don't reset position here - caller should use setPosition() before play()
    // if they want to start from a specific position

    // Enable level monitoring for meters
    setLevelMonitoringEnabled(true);

    m_transportSource.start();
    updatePlaybackState(PlaybackState::PLAYING);
}

void AudioEngine::pause()
{
    if (!isFileLoaded())
    {
        return;
    }

    m_transportSource.stop();
    updatePlaybackState(PlaybackState::PAUSED);
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

    // Disable level monitoring and reset meters
    setLevelMonitoringEnabled(false);

    // CRITICAL: Clear loop points and looping state to prevent stale state
    // from affecting next playback session. This ensures clean state after stop.
    clearLoopPoints();
    setLooping(false);
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
}

bool AudioEngine::isLooping() const
{
    return m_isLooping.load();
}

void AudioEngine::setLoopPoints(double loopStart, double loopEnd)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    m_loopStartTime.store(loopStart);
    m_loopEndTime.store(loopEnd);
}

void AudioEngine::clearLoopPoints()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    m_loopStartTime.store(-1.0);
    m_loopEndTime.store(-1.0);
}

//==============================================================================
// Transport Position

double AudioEngine::getCurrentPosition() const
{
    if (!isFileLoaded())
    {
        return 0.0;
    }

    double position = m_transportSource.getCurrentPosition();

    // Add preview offset ONLY for OFFLINE_BUFFER mode
    // OFFLINE_BUFFER: Playing extracted buffer starting at 0, need to add offset for FILE position
    // REALTIME_DSP: Playing actual file, position is already in FILE coordinates
    if (m_previewMode.load() == PreviewMode::OFFLINE_BUFFER)
    {
        position += getPreviewSelectionOffsetSeconds();
    }

    return position;
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
// Level Monitoring

void AudioEngine::setLevelMonitoringEnabled(bool enabled)
{
    m_levelMonitoringEnabled.store(enabled);

    // Reset levels when disabling
    if (!enabled)
    {
        for (int ch = 0; ch < MAX_CHANNELS; ++ch)
        {
            m_peakLevels[ch].store(0.0f);
            m_rmsLevels[ch].store(0.0f);
        }
    }
}

float AudioEngine::getPeakLevel(int channel) const
{
    if (channel >= 0 && channel < MAX_CHANNELS)
    {
        return m_peakLevels[channel].load();
    }
    return 0.0f;
}

float AudioEngine::getRMSLevel(int channel) const
{
    if (channel >= 0 && channel < MAX_CHANNELS)
    {
        return m_rmsLevels[channel].load();
    }
    return 0.0f;
}

//==============================================================================
// Channel Solo/Mute

void AudioEngine::setChannelSolo(int channel, bool solo)
{
    if (channel >= 0 && channel < MAX_CHANNELS)
    {
        m_channelSolo[channel].store(solo);
        DBG("AudioEngine: Channel " + juce::String(channel) +
                                 (solo ? " solo ON" : " solo OFF"));
    }
}

bool AudioEngine::isChannelSolo(int channel) const
{
    if (channel >= 0 && channel < MAX_CHANNELS)
    {
        return m_channelSolo[channel].load();
    }
    return false;
}

void AudioEngine::setChannelMute(int channel, bool mute)
{
    if (channel >= 0 && channel < MAX_CHANNELS)
    {
        m_channelMute[channel].store(mute);
        DBG("AudioEngine: Channel " + juce::String(channel) +
                                 (mute ? " muted" : " unmuted"));
    }
}

bool AudioEngine::isChannelMute(int channel) const
{
    if (channel >= 0 && channel < MAX_CHANNELS)
    {
        return m_channelMute[channel].load();
    }
    return false;
}

bool AudioEngine::hasAnySolo() const
{
    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
    {
        if (m_channelSolo[ch].load())
            return true;
    }
    return false;
}

void AudioEngine::clearAllSoloMute()
{
    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
    {
        m_channelSolo[ch].store(false);
        m_channelMute[ch].store(false);
    }
    DBG("AudioEngine: All solo/mute cleared");
}

void AudioEngine::setSpectrumAnalyzer(SpectrumAnalyzer* spectrumAnalyzer)
{
    // C8 FIX: take m_analyzerLock (blocking, message thread) so we cannot clear
    // or swap the pointer while the audio callback is mid-dereference. The
    // callback try-locks and skips on contention, so this never blocks audio.
    const juce::ScopedLock sl(m_analyzerLock);
    m_spectrumAnalyzer.store(spectrumAnalyzer);
}

void AudioEngine::setGraphicalEQEditor(GraphicalEQEditor* eqEditor)
{
    // C8 FIX: see setSpectrumAnalyzer.
    const juce::ScopedLock sl(m_analyzerLock);
    m_graphicalEQEditor.store(eqEditor);
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

            // NOTE: We do NOT auto-disable preview mode here anymore!
            // This was causing the preview to get disabled during loadPreviewBuffer()
            // when releaseResources() triggers a stream finished notification.
            // Preview mode should only be disabled explicitly by dialogs when they close.
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
        // Capture the device output rate (may differ from the file rate). The
        // position-driven fade preview needs it to map output samples to file time.
        m_deviceSampleRate.store(device->getCurrentSampleRate());

        m_transportSource.prepareToPlay(device->getCurrentBufferSizeSamples(),
                                         device->getCurrentSampleRate());

        // Prepare the ParametricEQ for real-time processing
        if (m_parametricEQ)
        {
            m_parametricEQ->prepare(device->getCurrentSampleRate(),
                                    device->getCurrentBufferSizeSamples());
        }

        // Prepare the DynamicParametricEQ for real-time processing (20-band)
        if (m_dynamicEQ)
        {
            m_dynamicEQ->prepare(device->getCurrentSampleRate(),
                                 device->getCurrentBufferSizeSamples());
        }

        // Prepare the VST3/AU plugin chain for real-time processing
        m_pluginChain.prepareToPlay(device->getCurrentSampleRate(),
                                     device->getCurrentBufferSizeSamples());
    }
}

void AudioEngine::audioDeviceStopped()
{
    // Release audio resources
    m_transportSource.releaseResources();

    // Release plugin chain resources
    m_pluginChain.releaseResources();
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

    // CRITICAL FIX: Auto-mute if another AudioEngine is in preview mode
    // This prevents audio mixing when multiple documents are open
    AudioEngine* previewingEngine = s_previewingEngine.load();
    bool shouldMute = (previewingEngine != nullptr && previewingEngine != this)
                      || m_isMuted.load();

    if (shouldMute)
    {
        // Buffer is already cleared - return silence
        // Either another engine is previewing, or we're manually muted
        return;
    }

    // Get audio from the transport source
    juce::AudioSourceChannelInfo channelInfo(buffer);

    m_transportSource.getNextAudioBlock(channelInfo);

    //==============================================================================
    // LOOP POINT HANDLING: Sample-accurate loop point checking
    // If loop points are set and we've passed the loop end, either loop back or stop
    // CRITICAL: Only process if BOTH loop points are valid to prevent stale state bugs
    double loopStart = m_loopStartTime.load();
    double loopEnd = m_loopEndTime.load();

    // Safety check: Only process loop points if both are valid and properly ordered
    if (loopStart >= 0.0 && loopEnd >= 0.0 && loopEnd > loopStart)
    {
        // CRITICAL FIX: Calculate position based on preview mode
        // Loop points are always in FILE coordinates, but transport position varies:
        // - OFFLINE_BUFFER: transport plays from 0, need to add offset for FILE position
        // - REALTIME_DSP/DISABLED: transport position IS the FILE position
        double currentPos;
        PreviewMode mode = m_previewMode.load();

        if (mode == PreviewMode::OFFLINE_BUFFER)
        {
            // Transport plays extracted buffer from 0, convert to FILE coordinates
            currentPos = m_transportSource.getCurrentPosition() + getPreviewSelectionOffsetSeconds();
        }
        else
        {
            // Transport plays actual file, position is already in FILE coordinates
            currentPos = m_transportSource.getCurrentPosition();
        }

        if (currentPos >= loopEnd)
        {
            if (m_isLooping.load())
            {
                // Continuous looping mode (preview with loop enabled)
                // Set transport position based on mode
                if (mode == PreviewMode::OFFLINE_BUFFER)
                {
                    // For extracted buffer, loop back to start of buffer (0)
                    m_transportSource.setPosition(0.0);
                }
                else
                {
                    // For file playback, loop back to FILE position
                    m_transportSource.setPosition(loopStart);
                }
            }
            else
            {
                // One-shot playback mode (selection playback)
                // Stop at end of selection, then auto-clear loop points
                m_transportSource.stop();
                updatePlaybackState(PlaybackState::STOPPED);

                // Auto-clear loop points after one-shot playback completes
                // This prevents stale loop points from affecting next playback
                m_loopStartTime.store(-1.0);
                m_loopEndTime.store(-1.0);
            }
        }
    }

    // CRITICAL: Handle mono-to-stereo conversion for file playback
    // If the source is mono (1 channel) but output is stereo (2 channels),
    // duplicate the mono channel to both L and R for center-panned playback.
    // This matches professional audio editor behavior (Sound Forge, Pro Tools, etc.)
    int sourceChannels = m_numChannels.load();
    if (sourceChannels == 1 && numOutputChannels == 2)
    {
        // Duplicate mono (channel 0) to right channel (channel 1)
        // The transport source already filled channel 0, now copy it to channel 1
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }

    //==============================================================================
    // PLUGIN PARAMETER AUTOMATION: Apply automation curves before plugin processing
    // Lock-free: reads from each AutomationCurve's atomic point list.
    if (m_automationManager != nullptr && m_pluginChainEnabled.load())
    {
        double currentPos = m_transportSource.getCurrentPosition();
        m_automationManager->applyAutomation(m_pluginChain, currentPos);
    }

    //==============================================================================
    // VST3/AU PLUGIN CHAIN: Real-time effect processing (Soundminer-style monitoring)
    // This runs ALWAYS during playback when enabled, not just in preview mode.
    // Enables professional real-time plugin monitoring - users can hear effects
    // while the chain panel is open and plugins are enabled (checked).
    // Plugin chain uses SpinLock for thread-safe audio processing.
    if (m_pluginChainEnabled.load() && !m_pluginChain.isEmpty())
    {
        m_pluginChain.processBlock(buffer, m_emptyMidiBuffer);
    }

    //==============================================================================
    // PREVIEW SYSTEM: Apply real-time DSP processing (Phase 1)
    // This processes audio AFTER transport but BEFORE monitoring/visualization
    // Enables instant preview of effects without modifying main buffer
    // NOTE: These processors are PREVIEW-SPECIFIC (for dialog previews like Gain, Fade, EQ)
    // Plugin chain processing above runs always during normal playback.

    // Only apply preview DSP if preview mode is enabled AND bypass is not active
    // When bypassed, audio plays through without any preview processing (for A/B comparison)
    if (m_previewMode.load() == PreviewMode::REALTIME_DSP && !m_previewBypassed.load())
    {
        // Process chain - order matters for best results:
        // 1. DC Offset removal (clean up signal first)
        // 2. Gain/Normalize (amplitude adjustment)
        // 3. EQ (frequency shaping)
        // 4. Fade (final envelope)
        // 5. Preview plugin instance (for OfflinePluginDialog)

        // 1. Apply DC Offset removal if enabled
        double sampleRate = m_sampleRate.load();
        if (sampleRate > 0)
        {
            m_dcOffsetProcessor.process(buffer, sampleRate);
        }

        // 2. Apply gain processor (for Gain dialog)
        m_gainProcessor.process(buffer);

        // 3. Apply normalize processor (for Normalize dialog)
        m_normalizeProcessor.process(buffer);

        // 5. Apply parametric EQ if enabled. C1/C2 FIX: parameters and
        // coefficients are pushed from the message thread via
        // ParametricEQ::setParameters(); applyEQ() only try-locks, adopts the
        // staged coefficient values without allocating, and processes.
        if (m_parametricEQEnabled.get() && m_parametricEQ)
        {
            m_parametricEQ->applyEQ(buffer);
        }

        // 5c. Apply dynamic parametric EQ if enabled (20-band, multiple filter
        // types). Same C1 contract: every mutator builds coefficients on the
        // message thread under the EQ's own lock; applyEQ() only processes.
        if (m_dynamicEQEnabled.get() && m_dynamicEQ)
        {
            m_dynamicEQ->applyEQ(buffer);
        }

        // 6. Apply fade processor last (affects overall envelope).
        // Position-driven: feed the current transport position within the fade
        // span (file samples) + the file-samples-per-output-sample ratio, so the
        // fade is rate-independent and re-aligns on each loop wrap. (sampleRate
        // here is the FILE rate, m_sampleRate.)
        if (sampleRate > 0)
        {
            const double deviceRate = m_deviceSampleRate.load();
            const double posSec = m_transportSource.getCurrentPosition() - m_previewRegionStartSec.load();
            const double posFileSamples = juce::jmax(0.0, posSec) * sampleRate;
            const double fileSamplesPerOutputSample = (deviceRate > 0.0) ? (sampleRate / deviceRate) : 1.0;
            m_fadeProcessor.process(buffer, posFileSamples, fileSamplesPerOutputSample);
        }

        // 7. Apply preview plugin instance (for OfflinePluginDialog real-time preview)
        // This allows plugins like FabFilter Pro-Q 4 to receive audio and display
        // their visualizations (spectrum, waveform, etc.) during preview
        juce::AudioPluginInstance* previewPlugin = m_previewPluginInstance.load();
        if (previewPlugin != nullptr)
        {
            previewPlugin->processBlock(buffer, m_emptyMidiBuffer);
        }
    }

    //==============================================================================
    // CHANNEL SOLO/MUTE: Apply per-channel monitoring controls
    // This runs after all DSP processing so users hear the final result with solo/mute applied
    bool anySolo = hasAnySolo();
    for (int ch = 0; ch < juce::jmin(numOutputChannels, MAX_CHANNELS); ++ch)
    {
        bool shouldMuteChannel = false;

        // Check if channel should be silent
        if (m_channelMute[ch].load())
        {
            // Explicit mute always silences the channel
            shouldMuteChannel = true;
        }
        else if (anySolo && !m_channelSolo[ch].load())
        {
            // If any channel is solo'd, non-solo'd channels are silenced
            shouldMuteChannel = true;
        }

        if (shouldMuteChannel)
        {
            buffer.clear(ch, 0, numSamples);
        }
    }

    //==============================================================================
    // Calculate and store audio levels for meters (if monitoring is enabled AND playing)
    if (m_levelMonitoringEnabled.load() && m_transportSource.isPlaying())
    {
        for (int ch = 0; ch < juce::jmin(numOutputChannels, MAX_CHANNELS); ++ch)
        {
            float peak = 0.0f;
            float rmsSum = 0.0f;
            const float* channelData = buffer.getReadPointer(ch);

            // Calculate peak and RMS for this buffer
            for (int i = 0; i < numSamples; ++i)
            {
                float sample = std::abs(channelData[i]);
                peak = juce::jmax(peak, sample);
                rmsSum += sample * sample;
            }

            // Store peak level (absolute max in buffer)
            m_peakLevels[ch].store(peak);

            // Store RMS level (root mean square)
            float rms = (numSamples > 0) ? std::sqrt(rmsSum / numSamples) : 0.0f;
            m_rmsLevels[ch].store(rms);
        }
    }
    else if (m_levelMonitoringEnabled.load())
    {
        // When not playing, reset levels to zero
        for (int ch = 0; ch < MAX_CHANNELS; ++ch)
        {
            m_peakLevels[ch].store(0.0f);
            m_rmsLevels[ch].store(0.0f);
        }
    }

    // Feed the spectrum analyzer / graphical EQ editor with audio data.
    // C8 FIX: try-lock m_analyzerLock so these raw, non-owned component pointers
    // cannot be destroyed by the message-thread setter while we dereference them.
    // On contention we simply skip visualisation feeding for this block (never
    // block the audio thread). The pointers are atomic, so we load once.
    if (buffer.getNumChannels() > 0)
    {
        const juce::ScopedTryLock stl(m_analyzerLock);
        if (stl.isLocked())
        {
            // Feed spectrum analyzer (if connected AND either normal playback or preview)
            SpectrumAnalyzer* analyzer = m_spectrumAnalyzer.load();
            bool isNormalPlayback = m_transportSource.isPlaying();
            bool isPreviewPlayback = (m_previewMode.load() != PreviewMode::DISABLED);

            if (analyzer != nullptr && (isNormalPlayback || isPreviewPlayback))
            {
                // Mix down to mono for spectrum analysis (use left channel)
                const float* channelData = buffer.getReadPointer(0);
                analyzer->pushAudioData(channelData, numSamples);
            }

            // Feed graphical EQ editor (if connected AND in EQ preview mode)
            GraphicalEQEditor* eqEditor = m_graphicalEQEditor.load();
            if (eqEditor != nullptr && m_dynamicEQEnabled.get())
            {
                const float* channelData = buffer.getReadPointer(0);
                eqEditor->pushAudioData(channelData, numSamples);
            }
        }
    }
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
    const double supportedRates[] = { 8000.0, 11025.0, 16000.0, 22050.0, 32000.0,
                                      44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
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
        return false;
    }

    // Check channel count (mono, stereo, or multichannel up to 8 channels)
    int numChannels = static_cast<int>(reader->numChannels);
    if (numChannels < 1 || numChannels > 8)
    {
        return false;
    }

    // Check bit depth (8, 16, 24, or 32-bit)
    int bitDepth = static_cast<int>(reader->bitsPerSample);
    if (bitDepth != 8 && bitDepth != 16 && bitDepth != 24 && bitDepth != 32)
    {
        return false;
    }

    // Format is valid
    return true;
}

//==============================================================================
// Preview System Implementation
// All preview-system methods (setPreviewMode, loadPreviewBuffer, the
// per-processor setters, bypass/reset/disable helpers, selection offset)
// live in AudioEngine_Preview.cpp under CLAUDE.md §7.5.
