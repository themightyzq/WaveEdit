/*
  ==============================================================================

    AudioBufferInputSource.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

/**
 * An InputSource that wraps an AudioBuffer<float> for use with AudioThumbnail.
 *
 * This class creates a temporary in-memory WAV file from the audio buffer,
 * allowing AudioThumbnail to use setSource() instead of manual addBlock() calls.
 *
 * Usage:
 *   auto source = new AudioBufferInputSource(myBuffer, 44100.0, 2);
 *   thumbnail.setSource(source);
 *
 * Note: The AudioThumbnail takes ownership of the source and will delete it.
 */
class AudioBufferInputSource : public juce::InputSource
{
public:
    /**
     * Creates an InputSource from an audio buffer.
     *
     * @param buffer The audio buffer to wrap (will be copied)
     * @param sampleRate The sample rate of the audio data
     * @param numChannels Number of audio channels
     */
    AudioBufferInputSource(const juce::AudioBuffer<float>& buffer,
                          double sampleRate,
                          int numChannels);

    ~AudioBufferInputSource() override;

    /**
     * Creates an InputStream for reading the audio data.
     * The AudioThumbnail will call this to read the WAV data.
     *
     * @return A new MemoryInputStream containing WAV-formatted audio data
     */
    juce::InputStream* createInputStream() override;

    /**
     * Creates an InputStream for a specific byte range.
     * Required by InputSource interface but not used by AudioThumbnail.
     *
     * @return A new MemoryInputStream for the specified range
     */
    juce::InputStream* createInputStreamFor(const juce::String& relatedItemPath) override;

    /**
     * Returns a hash code representing this source.
     * Used by AudioThumbnail for cache management.
     */
    juce::int64 hashCode() const override;

private:
    /**
     * Creates a temporary WAV file in memory from the audio buffer.
     * This includes proper WAV headers followed by the audio samples.
     */
    void createWavData();

    /**
     * Writes a WAV file header to the memory block.
     *
     * @param output The MemoryOutputStream to write to
     * @param numSamples Total number of samples in the buffer
     */
    void writeWavHeader(juce::MemoryOutputStream& output, int numSamples);

    // Store the original buffer data
    juce::AudioBuffer<float> m_buffer;
    double m_sampleRate;
    int m_numChannels;

    // WAV file data (header + samples)
    juce::MemoryBlock m_wavData;

    // Hash code for cache management
    juce::int64 m_hashCode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioBufferInputSource)
};
