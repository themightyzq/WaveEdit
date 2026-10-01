/*
  ==============================================================================

    AudioEngineMemorySource.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Definition of AudioEngine's private MemoryAudioSource, moved out of
    AudioEngine.h (CLAUDE.md 7.5 header size cap). Implemented in
    AudioEngine_MemorySource.cpp. Include only from AudioEngine*.cpp.

  ==============================================================================
*/

#pragma once

#include "AudioEngine.h"

/**
 * Memory-based audio source that plays from an AudioBuffer.
 * This is used for playback of edited audio.
 */
class AudioEngine::MemoryAudioSource : public juce::PositionableAudioSource
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
    // H20/L1/M-H1 FIX: the playback buffer is held behind a shared_ptr that
    // is swapped by the message thread. setBuffer()/clear() perform the
    // expensive deep makeCopyOf and the old-buffer free OFF-lock, holding
    // m_lock only for the pointer swap. The audio thread reads via a
    // ScopedTryLock and dereferences the RAW pointer inside the locked scope
    // -- no blocking (skips to silence on contention) and no shared_ptr
    // refcount traffic on the audio thread (§6.4).
    using BufferPtr = std::shared_ptr<const juce::AudioBuffer<float>>;
    BufferPtr m_buffer;                      // guarded by m_lock (pointer swap only)
    std::atomic<juce::int64> m_bufferLength{0};  // mirrors m_buffer length for lock-free reads
    double m_sampleRate;
    std::atomic<juce::int64> m_readPosition;
    bool m_isLooping;
    juce::CriticalSection m_lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MemoryAudioSource)
};
