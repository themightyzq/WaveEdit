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
#include "ChannelLayout.h"
#include <cmath>
#include <iostream>

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
    std::cerr << "[BUFFER] replaceRange: start=" << startSample
              << ", toReplace=" << numSamplesToReplace
              << ", newAudioSamples=" << newAudio.getNumSamples()
              << ", newAudioCh=" << newAudio.getNumChannels()
              << ", currentBufferCh=" << m_buffer.getNumChannels()
              << ", currentBufferSamples=" << m_buffer.getNumSamples() << std::endl;
    std::cerr.flush();

    // First delete the range
    if (!deleteRange(startSample, numSamplesToReplace))
    {
        std::cerr << "[BUFFER] replaceRange: deleteRange FAILED" << std::endl;
        std::cerr.flush();
        return false;
    }

    std::cerr << "[BUFFER] replaceRange: deleteRange succeeded, buffer now has "
              << m_buffer.getNumSamples() << " samples" << std::endl;
    std::cerr.flush();

    // Then insert the new audio at the same position
    bool result = insertAudio(startSample, newAudio);
    std::cerr << "[BUFFER] replaceRange: insertAudio returned " << (result ? "true" : "false")
              << ", buffer now has " << m_buffer.getNumSamples() << " samples" << std::endl;
    std::cerr.flush();
    return result;
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

bool AudioBufferManager::silenceRangeForChannels(int64_t startSample, int64_t numSamples, int channelMask)
{
    juce::ScopedLock sl(m_lock);

    // If channelMask is -1, silence all channels
    if (channelMask == -1)
    {
        return silenceRange(startSample, numSamples);
    }

    // Validate range
    if (startSample < 0 || numSamples <= 0 ||
        startSample + numSamples > m_buffer.getNumSamples())
    {
        juce::Logger::writeToLog("AudioBufferManager: Invalid range in silenceRangeForChannels");
        return false;
    }

    // Silence only the specified channels
    int numChannelsSilenced = 0;
    for (int ch = 0; ch < m_buffer.getNumChannels(); ++ch)
    {
        if ((channelMask & (1 << ch)) != 0)
        {
            m_buffer.clear(ch, static_cast<int>(startSample), static_cast<int>(numSamples));
            numChannelsSilenced++;
        }
    }

    juce::Logger::writeToLog("AudioBufferManager: Silenced " + juce::String(numSamples) +
                             " samples on " + juce::String(numChannelsSilenced) + " channels");

    return true;
}

juce::AudioBuffer<float> AudioBufferManager::getAudioRangeForChannels(int64_t startSample, int64_t numSamples, int channelMask) const
{
    juce::ScopedLock sl(m_lock);

    // If channelMask is -1, return all channels (same as getAudioRange)
    if (channelMask == -1)
    {
        return getAudioRange(startSample, numSamples);
    }

    // Count the number of channels to copy
    int outputChannelCount = 0;
    for (int ch = 0; ch < m_buffer.getNumChannels(); ++ch)
    {
        if ((channelMask & (1 << ch)) != 0)
            outputChannelCount++;
    }

    if (outputChannelCount == 0)
    {
        return juce::AudioBuffer<float>();
    }

    // Validate range
    int64_t actualStart = juce::jmax((int64_t)0, startSample);
    int64_t actualEnd = juce::jmin((int64_t)m_buffer.getNumSamples(), startSample + numSamples);
    int64_t actualSamples = actualEnd - actualStart;

    if (actualSamples <= 0)
    {
        return juce::AudioBuffer<float>();
    }

    // Create output buffer with only the requested channels
    juce::AudioBuffer<float> result(outputChannelCount, static_cast<int>(actualSamples));

    int outputCh = 0;
    for (int ch = 0; ch < m_buffer.getNumChannels(); ++ch)
    {
        if ((channelMask & (1 << ch)) != 0)
        {
            result.copyFrom(outputCh, 0, m_buffer, ch, static_cast<int>(actualStart), static_cast<int>(actualSamples));
            outputCh++;
        }
    }

    return result;
}

bool AudioBufferManager::replaceChannelsInRange(int64_t startSample, const juce::AudioBuffer<float>& sourceAudio, int channelMask)
{
    juce::ScopedLock sl(m_lock);

    // Validate range
    if (startSample < 0 || sourceAudio.getNumSamples() <= 0 ||
        startSample + sourceAudio.getNumSamples() > m_buffer.getNumSamples())
    {
        juce::Logger::writeToLog("AudioBufferManager: Invalid range in replaceChannelsInRange");
        return false;
    }

    int numSamples = sourceAudio.getNumSamples();

    // If channelMask is -1, replace all channels
    if (channelMask == -1)
    {
        int channelsToReplace = juce::jmin(m_buffer.getNumChannels(), sourceAudio.getNumChannels());
        for (int ch = 0; ch < channelsToReplace; ++ch)
        {
            m_buffer.copyFrom(ch, static_cast<int>(startSample), sourceAudio, ch, 0, numSamples);
        }
        return true;
    }

    // Count focused channels to determine source channel mapping
    int sourceChIndex = 0;
    for (int ch = 0; ch < m_buffer.getNumChannels(); ++ch)
    {
        if ((channelMask & (1 << ch)) != 0)
        {
            // Map source channel to destination channel
            int srcCh = sourceChIndex % sourceAudio.getNumChannels();
            m_buffer.copyFrom(ch, static_cast<int>(startSample), sourceAudio, srcCh, 0, numSamples);
            sourceChIndex++;
        }
    }

    juce::Logger::writeToLog("AudioBufferManager: Replaced " + juce::String(numSamples) +
                             " samples on " + juce::String(sourceChIndex) + " channels");

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

//==============================================================================
// Channel conversion

bool AudioBufferManager::convertToStereo()
{
    juce::ScopedLock sl(m_lock);

    int currentChannels = m_buffer.getNumChannels();

    // No-op if already stereo
    if (currentChannels == 2)
    {
        juce::Logger::writeToLog("AudioBufferManager: Buffer is already stereo, skipping conversion");
        return false;
    }

    if (m_buffer.getNumSamples() == 0)
    {
        juce::Logger::writeToLog("AudioBufferManager: Empty buffer, skipping stereo conversion");
        return false;
    }

    // Use ChannelConverter for generalized N-channel to stereo conversion
    juce::AudioBuffer<float> stereoBuffer = waveedit::ChannelConverter::convert(
        m_buffer, 2, waveedit::ChannelLayoutType::Stereo);

    // Replace buffer
    m_buffer = std::move(stereoBuffer);

    juce::Logger::writeToLog("AudioBufferManager: Converted " + juce::String(currentChannels) +
                             " channels to stereo (" + juce::String(m_buffer.getNumSamples()) + " samples)");

    return true;
}

bool AudioBufferManager::convertToMono()
{
    juce::ScopedLock sl(m_lock);

    int currentChannels = m_buffer.getNumChannels();

    // No-op if already mono
    if (currentChannels == 1)
    {
        juce::Logger::writeToLog("AudioBufferManager: Buffer is already mono, skipping conversion");
        return false;
    }

    if (m_buffer.getNumSamples() == 0)
    {
        juce::Logger::writeToLog("AudioBufferManager: Empty buffer, skipping mono conversion");
        return false;
    }

    // Use ChannelConverter for generalized N-channel to mono mixdown
    juce::AudioBuffer<float> monoBuffer = waveedit::ChannelConverter::convert(
        m_buffer, 1, waveedit::ChannelLayoutType::Mono);

    // Replace buffer
    m_buffer = std::move(monoBuffer);

    juce::Logger::writeToLog("AudioBufferManager: Converted " + juce::String(currentChannels) +
                             " channels to mono (" + juce::String(m_buffer.getNumSamples()) + " samples)");

    return true;
}

bool AudioBufferManager::convertToChannelCount(int targetChannels)
{
    juce::ScopedLock sl(m_lock);

    // Validate target channel count
    if (targetChannels < 1 || targetChannels > 8)
    {
        juce::Logger::writeToLog("AudioBufferManager: Invalid target channel count: " +
                                 juce::String(targetChannels) + " (must be 1-8)");
        return false;
    }

    int currentChannels = m_buffer.getNumChannels();

    // No-op if already at target count
    if (currentChannels == targetChannels)
    {
        juce::Logger::writeToLog("AudioBufferManager: Buffer already has " +
                                 juce::String(targetChannels) + " channels, skipping conversion");
        return false;
    }

    if (m_buffer.getNumSamples() == 0)
    {
        juce::Logger::writeToLog("AudioBufferManager: Empty buffer, skipping channel conversion");
        return false;
    }

    // Use ChannelConverter for generalized conversion
    juce::AudioBuffer<float> convertedBuffer = waveedit::ChannelConverter::convert(
        m_buffer, targetChannels);

    // Replace buffer
    m_buffer = std::move(convertedBuffer);

    juce::Logger::writeToLog("AudioBufferManager: Converted " + juce::String(currentChannels) +
                             " channels to " + juce::String(targetChannels) +
                             " channels (" + juce::String(m_buffer.getNumSamples()) + " samples)");

    return true;
}

void AudioBufferManager::setBuffer(const juce::AudioBuffer<float>& newBuffer, double sampleRate)
{
    juce::ScopedLock sl(m_lock);

    m_buffer.setSize(newBuffer.getNumChannels(), newBuffer.getNumSamples());
    for (int ch = 0; ch < newBuffer.getNumChannels(); ++ch)
    {
        m_buffer.copyFrom(ch, 0, newBuffer, ch, 0, newBuffer.getNumSamples());
    }
    m_sampleRate = sampleRate;

    juce::Logger::writeToLog("AudioBufferManager: setBuffer with " +
                             juce::String(m_buffer.getNumChannels()) + " channels, " +
                             juce::String(m_buffer.getNumSamples()) + " samples");
}
