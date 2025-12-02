/*
  ==============================================================================

    AudioBufferManager.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "AudioBufferManager.h"
#include <cmath>

//==============================================================================
AudioBufferManager::AudioBufferManager()
    : m_sampleRate(44100.0),
      m_bitDepth(16)
{
}

AudioBufferManager::~AudioBufferManager()
{
}

//==============================================================================
// Loading and initialization

bool AudioBufferManager::loadFromFile(const juce::File& file, juce::AudioFormatManager& formatManager)
{
    juce::ScopedLock sl(m_lock);

    // Create reader for the file
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader == nullptr)
    {
        juce::Logger::writeToLog("AudioBufferManager: Failed to create reader for " + file.getFileName());
        return false;
    }

    // Store properties
    m_sampleRate = reader->sampleRate;
    m_bitDepth = static_cast<int>(reader->bitsPerSample);

    // Allocate buffer for entire file
    int numChannels = static_cast<int>(reader->numChannels);
    int64_t numSamples = reader->lengthInSamples;

    m_buffer.setSize(numChannels, static_cast<int>(numSamples));

    // Read entire file into buffer
    reader->read(&m_buffer, 0, static_cast<int>(numSamples), 0, true, true);

    juce::Logger::writeToLog("AudioBufferManager: Loaded " + juce::String(numSamples) +
                             " samples, " + juce::String(numChannels) + " channels, " +
                             juce::String(m_sampleRate) + " Hz, " +
                             juce::String(m_bitDepth) + " bits");

    return true;
}

void AudioBufferManager::clear()
{
    juce::ScopedLock sl(m_lock);
    m_buffer.setSize(0, 0);
    m_sampleRate = 44100.0;
    m_bitDepth = 16;
}

bool AudioBufferManager::hasAudioData() const
{
    return m_buffer.getNumSamples() > 0 && m_buffer.getNumChannels() > 0;
}

//==============================================================================
// Audio properties

double AudioBufferManager::getLengthInSeconds() const
{
    if (m_sampleRate <= 0.0 || m_buffer.getNumSamples() == 0)
    {
        return 0.0;
    }

    return static_cast<double>(m_buffer.getNumSamples()) / m_sampleRate;
}

//==============================================================================
// Conversion utilities

int64_t AudioBufferManager::timeToSample(double timeInSeconds) const
{
    if (m_sampleRate <= 0.0)
    {
        return 0;
    }

    // Round to nearest sample for sample accuracy
    return static_cast<int64_t>(std::round(timeInSeconds * m_sampleRate));
}

double AudioBufferManager::sampleToTime(int64_t samplePosition) const
{
    if (m_sampleRate <= 0.0)
    {
        return 0.0;
    }

    return static_cast<double>(samplePosition) / m_sampleRate;
}

//==============================================================================
// Buffer access

juce::AudioBuffer<float> AudioBufferManager::getAudioRange(int64_t startSample, int64_t numSamples) const
{
    juce::ScopedLock sl(m_lock);

    // DEBUG: Log the extraction request
    juce::Logger::writeToLog("AudioBufferManager::getAudioRange called:");
    juce::Logger::writeToLog("  Start sample: " + juce::String(startSample));
    juce::Logger::writeToLog("  Num samples requested: " + juce::String(numSamples));
    juce::Logger::writeToLog("  Total buffer samples: " + juce::String(m_buffer.getNumSamples()));
    juce::Logger::writeToLog("  Buffer channels: " + juce::String(m_buffer.getNumChannels()));

    // Validate range
    if (startSample < 0 || numSamples <= 0 ||
        startSample + numSamples > m_buffer.getNumSamples())
    {
        juce::Logger::writeToLog("AudioBufferManager: Invalid range in getAudioRange");
        juce::Logger::writeToLog("  Range check failed: startSample=" + juce::String(startSample) +
                                " numSamples=" + juce::String(numSamples) +
                                " totalSamples=" + juce::String(m_buffer.getNumSamples()));
        return juce::AudioBuffer<float>();
    }

    // Create new buffer and copy the range
    juce::AudioBuffer<float> rangeBuff(m_buffer.getNumChannels(), static_cast<int>(numSamples));

    for (int ch = 0; ch < m_buffer.getNumChannels(); ++ch)
    {
        rangeBuff.copyFrom(ch, 0, m_buffer, ch, static_cast<int>(startSample), static_cast<int>(numSamples));
    }

    juce::Logger::writeToLog("  Successfully extracted " + juce::String(rangeBuff.getNumSamples()) +
                            " samples from position " + juce::String(startSample));

    return rangeBuff;
}

//==============================================================================
// Editing operations

bool AudioBufferManager::deleteRange(int64_t startSample, int64_t numSamples)
{
    juce::ScopedLock sl(m_lock);

    // Validate range
    if (startSample < 0 || numSamples <= 0 ||
        startSample + numSamples > m_buffer.getNumSamples())
    {
        juce::Logger::writeToLog("AudioBufferManager: Invalid range in deleteRange");
        return false;
    }

    int numChannels = m_buffer.getNumChannels();
    int oldNumSamples = m_buffer.getNumSamples();
    int newNumSamples = oldNumSamples - static_cast<int>(numSamples);

    // If deleting everything, just clear
    if (newNumSamples <= 0)
    {
        m_buffer.setSize(numChannels, 0);
        return true;
    }

    // Create new buffer without the deleted range
    juce::AudioBuffer<float> newBuffer(numChannels, newNumSamples);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        // Copy samples before the deleted range
        if (startSample > 0)
        {
            newBuffer.copyFrom(ch, 0, m_buffer, ch, 0, static_cast<int>(startSample));
        }

        // Copy samples after the deleted range
        int samplesAfterDelete = oldNumSamples - static_cast<int>(startSample + numSamples);
        if (samplesAfterDelete > 0)
        {
            newBuffer.copyFrom(ch, static_cast<int>(startSample),
                              m_buffer, ch, static_cast<int>(startSample + numSamples),
                              samplesAfterDelete);
        }
    }

    // Replace buffer
    m_buffer = newBuffer;

    juce::Logger::writeToLog("AudioBufferManager: Deleted " + juce::String(numSamples) +
                             " samples starting at " + juce::String(startSample));

    return true;
}

bool AudioBufferManager::insertAudio(int64_t insertPosition, const juce::AudioBuffer<float>& audioToInsert)
{
    juce::ScopedLock sl(m_lock);

    // Validate insert position
    if (insertPosition < 0 || insertPosition > m_buffer.getNumSamples())
    {
        juce::Logger::writeToLog("AudioBufferManager: Invalid insert position");
        return false;
    }

    // Check channel compatibility
    if (m_buffer.getNumChannels() != audioToInsert.getNumChannels())
    {
        juce::Logger::writeToLog("AudioBufferManager: Channel count mismatch in insertAudio");
        return false;
    }

    int numChannels = m_buffer.getNumChannels();
    int oldNumSamples = m_buffer.getNumSamples();
    int insertNumSamples = audioToInsert.getNumSamples();
    int newNumSamples = oldNumSamples + insertNumSamples;

    // Create new buffer with space for inserted audio
    juce::AudioBuffer<float> newBuffer(numChannels, newNumSamples);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        // Copy samples before insert position
        if (insertPosition > 0)
        {
            newBuffer.copyFrom(ch, 0, m_buffer, ch, 0, static_cast<int>(insertPosition));
        }

        // Copy inserted audio
        newBuffer.copyFrom(ch, static_cast<int>(insertPosition),
                          audioToInsert, ch, 0, insertNumSamples);

        // Copy samples after insert position
        int samplesAfterInsert = oldNumSamples - static_cast<int>(insertPosition);
        if (samplesAfterInsert > 0)
        {
            newBuffer.copyFrom(ch, static_cast<int>(insertPosition) + insertNumSamples,
                              m_buffer, ch, static_cast<int>(insertPosition),
                              samplesAfterInsert);
        }
    }

    // Replace buffer
    m_buffer = newBuffer;

    juce::Logger::writeToLog("AudioBufferManager: Inserted " + juce::String(insertNumSamples) +
                             " samples at position " + juce::String(insertPosition));

    return true;
}

bool AudioBufferManager::replaceRange(int64_t startSample, int64_t numSamplesToReplace,
                                     const juce::AudioBuffer<float>& newAudio)
{
    // First delete the range
    if (!deleteRange(startSample, numSamplesToReplace))
    {
        return false;
    }

    // Then insert the new audio at the same position
    return insertAudio(startSample, newAudio);
}

bool AudioBufferManager::silenceRange(int64_t startSample, int64_t numSamples)
{
    juce::ScopedLock sl(m_lock);

    // Validate range
    if (startSample < 0 || numSamples <= 0 ||
        startSample + numSamples > m_buffer.getNumSamples())
    {
        juce::Logger::writeToLog("AudioBufferManager: Invalid range in silenceRange");
        return false;
    }

    // Fill the range with zeros (digital silence)
    for (int ch = 0; ch < m_buffer.getNumChannels(); ++ch)
    {
        m_buffer.clear(ch, static_cast<int>(startSample), static_cast<int>(numSamples));
    }

    juce::Logger::writeToLog("AudioBufferManager: Silenced " + juce::String(numSamples) +
                             " samples starting at " + juce::String(startSample));

    return true;
}

bool AudioBufferManager::trimToRange(int64_t startSample, int64_t numSamples)
{
    juce::ScopedLock sl(m_lock);

    // Validate range
    if (startSample < 0 || numSamples <= 0 ||
        startSample + numSamples > m_buffer.getNumSamples())
    {
        juce::Logger::writeToLog("AudioBufferManager: Invalid range in trimToRange");
        return false;
    }

    // If trimming to entire buffer, nothing to do
    if (startSample == 0 && numSamples == m_buffer.getNumSamples())
    {
        return true;
    }

    // Create new buffer with only the specified range
    int numChannels = m_buffer.getNumChannels();
    juce::AudioBuffer<float> newBuffer(numChannels, static_cast<int>(numSamples));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        newBuffer.copyFrom(ch, 0, m_buffer, ch, static_cast<int>(startSample), static_cast<int>(numSamples));
    }

    // Replace buffer
    m_buffer = newBuffer;

    juce::Logger::writeToLog("AudioBufferManager: Trimmed to " + juce::String(numSamples) +
                             " samples starting at " + juce::String(startSample));

    return true;
}
