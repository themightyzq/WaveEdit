/*
  ==============================================================================

    AudioEngineEnhanced.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "AudioEngineEnhanced.h"

AudioEngineEnhanced::AudioEngineEnhanced()
    : m_bufferManager(nullptr),
      m_usingBuffer(false)
{
}

AudioEngineEnhanced::~AudioEngineEnhanced()
{
    m_memorySource.reset();
}

void AudioEngineEnhanced::setAudioBufferManager(AudioBufferManager* bufferManager)
{
    m_bufferManager = bufferManager;
}

bool AudioEngineEnhanced::loadAudioFileEnhanced(const juce::File& file, bool loadIntoBuffer)
{
    // First load using the base class method
    if (!loadAudioFile(file))
    {
        return false;
    }

    // Also load into the buffer manager if requested
    if (loadIntoBuffer && m_bufferManager != nullptr)
    {
        if (!m_bufferManager->loadFromFile(file, getFormatManager()))
        {
            juce::Logger::writeToLog("Failed to load file into AudioBufferManager");
            return false;
        }
    }

    m_usingBuffer = false;
    return true;
}

void AudioEngineEnhanced::useBufferForPlayback()
{
    if (m_bufferManager == nullptr || !m_bufferManager->hasAudioData())
    {
        juce::Logger::writeToLog("Cannot use buffer for playback: No buffer data");
        return;
    }

    // Stop current playback
    stop();

    // Create a memory source from the buffer
    const auto& buffer = m_bufferManager->getBuffer();
    m_memorySource = std::make_unique<juce::MemoryAudioSource>(
        buffer,
        false,  // Don't release the buffer when source is deleted
        false   // Don't loop
    );

    // Set the transport source to use the memory source
    m_transportSource.setSource(
        m_memorySource.get(),
        0,                      // Buffer size (0 = default)
        m_backgroundThread.get(),
        m_bufferManager->getSampleRate(),
        m_bufferManager->getNumChannels()
    );

    m_usingBuffer = true;

    // Update stored properties
    m_sampleRate.store(m_bufferManager->getSampleRate());
    m_numChannels.store(m_bufferManager->getNumChannels());
    m_bitDepth.store(m_bufferManager->getBitDepth());

    juce::Logger::writeToLog("Switched to buffer playback");
}

void AudioEngineEnhanced::useFileForPlayback()
{
    if (!isFileLoaded())
    {
        juce::Logger::writeToLog("Cannot use file for playback: No file loaded");
        return;
    }

    // Stop current playback
    stop();

    // Switch back to file source
    m_transportSource.setSource(
        m_readerSource.get(),
        0,
        m_backgroundThread.get(),
        m_sampleRate,
        m_numChannels
    );

    m_memorySource.reset();
    m_usingBuffer = false;

    juce::Logger::writeToLog("Switched to file playback");
}