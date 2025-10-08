/*
  ==============================================================================

    AudioBufferInputSource.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "AudioBufferInputSource.h"

AudioBufferInputSource::AudioBufferInputSource(const juce::AudioBuffer<float>& buffer,
                                               double sampleRate,
                                               int numChannels)
    : m_buffer(numChannels, buffer.getNumSamples()),
      m_sampleRate(sampleRate),
      m_numChannels(numChannels)
{
    // Validate input
    jassert(buffer.getNumSamples() > 0);
    jassert(numChannels > 0 && numChannels <= buffer.getNumChannels());
    jassert(sampleRate > 0);

    // CRITICAL: Validate buffer size to prevent integer overflow in WAV generation
    // WAV format has ~4GB limit, but we use a safer 2GB limit
    const juce::int64 estimatedSize = static_cast<juce::int64>(buffer.getNumSamples()) *
                                       numChannels * sizeof(juce::int16) + 44;
    jassert(estimatedSize <= 0x7FFFFFFF); // 2GB safety limit

    if (estimatedSize > 0x7FFFFFFF)
    {
        juce::Logger::writeToLog("ERROR: Buffer too large for WAV format conversion");
        // Set to empty state - caller should check for failure
        m_buffer.setSize(0, 0);
        m_hashCode = 0;
        return;
    }

    // Copy the buffer data (only copy the channels we need)
    int channelsToCopy = juce::jmin(numChannels, buffer.getNumChannels());
    for (int ch = 0; ch < channelsToCopy; ++ch)
    {
        m_buffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
    }

    // Generate a hash code for cache management
    // Sample multiple points throughout the buffer for better uniqueness
    m_hashCode = static_cast<juce::int64>(buffer.getNumSamples()) * 31
                + static_cast<juce::int64>(sampleRate) * 17
                + static_cast<juce::int64>(numChannels) * 13;

    // Sample multiple points throughout the buffer for better hash distribution
    const int samplePoints = juce::jmin(10, buffer.getNumSamples());
    for (int i = 0; i < samplePoints; ++i)
    {
        int sampleIndex = (buffer.getNumSamples() * i) / juce::jmax(1, samplePoints);
        if (sampleIndex < buffer.getNumSamples() && buffer.getNumChannels() > 0)
        {
            float sample = buffer.getSample(0, sampleIndex);
            // Use bit representation for deterministic hashing across platforms
            juce::uint32 bits = *reinterpret_cast<juce::uint32*>(&sample);
            m_hashCode = m_hashCode * 31 + bits;
        }
    }

    // Add timestamp for uniqueness (different edits at different times get different hashes)
    m_hashCode += juce::Time::currentTimeMillis();

    // Create the WAV data
    createWavData();

    juce::Logger::writeToLog(juce::String::formatted(
        "AudioBufferInputSource created: %d samples, %.1f Hz, %d channels, WAV size: %d bytes",
        buffer.getNumSamples(), sampleRate, numChannels, static_cast<int>(m_wavData.getSize())));
}

AudioBufferInputSource::~AudioBufferInputSource()
{
}

void AudioBufferInputSource::createWavData()
{
    juce::MemoryOutputStream output(m_wavData, false);

    int numSamples = m_buffer.getNumSamples();

    // Write WAV header
    writeWavHeader(output, numSamples);

    // Write interleaved sample data (16-bit PCM)
    // WAV format stores samples as interleaved: L1, R1, L2, R2, ...
    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < m_numChannels; ++ch)
        {
            // Convert float sample [-1.0, 1.0] to 16-bit integer [-32768, 32767]
            float sample = m_buffer.getSample(ch, i);

            // Clamp to valid range
            sample = juce::jlimit(-1.0f, 1.0f, sample);

            // Convert to 16-bit
            int16_t intSample = static_cast<int16_t>(sample * 32767.0f);

            // Write little-endian
            output.writeByte(static_cast<char>(intSample & 0xFF));
            output.writeByte(static_cast<char>((intSample >> 8) & 0xFF));
        }
    }

    juce::Logger::writeToLog(juce::String::formatted(
        "WAV data created: %d samples, %d channels, %d bytes",
        numSamples, m_numChannels, static_cast<int>(m_wavData.getSize())));
}

void AudioBufferInputSource::writeWavHeader(juce::MemoryOutputStream& output, int numSamples)
{
    const int bitsPerSample = 16;
    const int bytesPerSample = bitsPerSample / 8;
    const int blockAlign = m_numChannels * bytesPerSample;
    const int byteRate = static_cast<int>(m_sampleRate) * blockAlign;

    // CRITICAL: Calculate data chunk size using int64 to prevent overflow
    const juce::int64 dataChunkSize64 = static_cast<juce::int64>(numSamples) * blockAlign;
    jassert(dataChunkSize64 <= std::numeric_limits<int32_t>::max());
    const int dataChunkSize = static_cast<int>(dataChunkSize64);

    const int fileSize = 44 + dataChunkSize - 8; // Total size minus 8 bytes (RIFF header)

    // RIFF header (12 bytes)
    output.write("RIFF", 4);                          // ChunkID
    output.writeInt(fileSize);                        // ChunkSize (little-endian)
    output.write("WAVE", 4);                          // Format

    // fmt subchunk (24 bytes)
    output.write("fmt ", 4);                          // Subchunk1ID
    output.writeInt(16);                              // Subchunk1Size (16 for PCM)
    output.writeShort(1);                             // AudioFormat (1 = PCM)
    output.writeShort(static_cast<short>(m_numChannels)); // NumChannels
    output.writeInt(static_cast<int>(m_sampleRate));  // SampleRate
    output.writeInt(byteRate);                        // ByteRate
    output.writeShort(static_cast<short>(blockAlign)); // BlockAlign
    output.writeShort(static_cast<short>(bitsPerSample)); // BitsPerSample

    // data subchunk (8 bytes header + actual data)
    output.write("data", 4);                          // Subchunk2ID
    output.writeInt(dataChunkSize);                   // Subchunk2Size

    // Audio data follows (written by caller)
}

juce::InputStream* AudioBufferInputSource::createInputStream()
{
    // Create a new MemoryInputStream from the WAV data
    // The caller (AudioThumbnail) takes ownership and will delete this
    return new juce::MemoryInputStream(m_wavData, false);
}

juce::InputStream* AudioBufferInputSource::createInputStreamFor(const juce::String& relatedItemPath)
{
    // Not used by AudioThumbnail, but required by InputSource interface
    juce::ignoreUnused(relatedItemPath);
    return createInputStream();
}

juce::int64 AudioBufferInputSource::hashCode() const
{
    // Return the pre-computed hash code
    // AudioThumbnail uses this for cache management
    return m_hashCode;
}
