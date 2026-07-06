/*
  ==============================================================================

    AudioEngine_MemorySource.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    MemoryAudioSource: the lock-light in-memory AudioSource that feeds the
    transport from an AudioBuffer. Extracted from AudioEngine.cpp to stay
    under the Sec 7.5 file-size cap.

  ==============================================================================
*/

#include "AudioEngine.h"

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
    bufferToFill.clearActiveBufferRegion();

    // M-H1 FIX: try-lock and read the buffer through a RAW pointer inside the
    // locked scope -- no blocking, and no shared_ptr refcount traffic on the
    // audio thread. setBuffer()/clear() do their expensive deep copy and old-
    // buffer free OFF-lock and only swap the pointer under m_lock, so holding
    // m_lock across this bounded per-block copy is safe (the message thread
    // waits at most one block for the swap). On the rare contended block
    // (a writer mid pointer-swap) we emit the already-cleared silence and
    // return -- the audio thread NEVER blocks (§6.4). The previous code held a
    // blocking ScopedLock and copied the shared_ptr (a refcount bump/decrement).
    const juce::ScopedTryLock stl(m_lock);
    if (! stl.isLocked())
        return;

    if (m_buffer == nullptr)
        return;

    const juce::AudioBuffer<float>& src = *m_buffer;
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
