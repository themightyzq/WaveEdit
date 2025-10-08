/*
  ==============================================================================

    AudioEngineEnhanced.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include "AudioEngine.h"
#include "AudioBufferManager.h"

/**
 * Enhanced audio engine that supports playback from AudioBufferManager.
 *
 * This extends the base AudioEngine to support editing operations by playing
 * from an in-memory AudioBufferManager instead of directly from a file.
 */
class AudioEngineEnhanced : public AudioEngine
{
public:
    AudioEngineEnhanced();
    ~AudioEngineEnhanced() override;

    /**
     * Sets the AudioBufferManager to use for playback.
     * After this is called, the engine will play from the buffer instead of the file.
     *
     * @param bufferManager Pointer to the buffer manager (not owned)
     */
    void setAudioBufferManager(AudioBufferManager* bufferManager);

    /**
     * Loads an audio file into both the transport and the buffer manager.
     *
     * @param file The audio file to load
     * @param loadIntoBuffer If true, also loads into AudioBufferManager
     * @return true if successful
     */
    bool loadAudioFileEnhanced(const juce::File& file, bool loadIntoBuffer = true);

    /**
     * Switches playback source to the AudioBufferManager.
     * Call this after editing operations to play the edited audio.
     */
    void useBufferForPlayback();

    /**
     * Switches playback source back to the original file.
     */
    void useFileForPlayback();

    /**
     * Checks if currently playing from buffer.
     */
    bool isUsingBuffer() const { return m_usingBuffer; }

private:
    AudioBufferManager* m_bufferManager;
    std::unique_ptr<juce::MemoryAudioSource> m_memorySource;
    bool m_usingBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngineEnhanced)
};