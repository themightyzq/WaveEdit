/*
  ==============================================================================

    AudioBufferManager.h
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
#include <juce_audio_formats/juce_audio_formats.h>

/**
 * Manages an editable audio buffer for sample-accurate editing operations.
 *
 * This class holds the audio data in memory and provides methods for:
 * - Sample-accurate cut, copy, paste, delete operations
 * - Converting between time and sample positions
 * - Getting audio data for specific ranges
 */
class AudioBufferManager
{
public:
    AudioBufferManager();
    ~AudioBufferManager();

    //==============================================================================
    // Loading and initialization

    /**
     * Loads audio data from a file into the editable buffer.
     *
     * @param file The audio file to load
     * @param formatManager The format manager to use for reading
     * @return true if successful, false otherwise
     */
    bool loadFromFile(const juce::File& file, juce::AudioFormatManager& formatManager);

    /**
     * Clears the buffer and resets all properties.
     */
    void clear();

    /**
     * Checks if audio data is loaded.
     *
     * @return true if buffer has audio data
     */
    bool hasAudioData() const;

    //==============================================================================
    // Audio properties

    /**
     * Gets the sample rate of the audio.
     */
    double getSampleRate() const { return m_sampleRate; }

    /**
     * Gets the number of channels.
     */
    int getNumChannels() const { return m_buffer.getNumChannels(); }

    /**
     * Gets the total number of samples.
     */
    int64_t getNumSamples() const { return m_buffer.getNumSamples(); }

    /**
     * Gets the total length in seconds.
     */
    double getLengthInSeconds() const;

    /**
     * Gets the bit depth of the original file.
     */
    int getBitDepth() const { return m_bitDepth; }

    //==============================================================================
    // Conversion utilities

    /**
     * Converts a time position to a sample position.
     *
     * @param timeInSeconds Time position in seconds
     * @return Sample position (rounded to nearest sample)
     */
    int64_t timeToSample(double timeInSeconds) const;

    /**
     * Converts a sample position to a time position.
     *
     * @param samplePosition Sample position
     * @return Time position in seconds
     */
    double sampleToTime(int64_t samplePosition) const;

    //==============================================================================
    // Buffer access

    /**
     * Gets read-only access to the audio buffer.
     */
    const juce::AudioBuffer<float>& getBuffer() const { return m_buffer; }

    /**
     * Gets a copy of audio data for a specific range.
     *
     * @param startSample Start sample (inclusive)
     * @param numSamples Number of samples to copy
     * @return New buffer containing the requested range
     */
    juce::AudioBuffer<float> getAudioRange(int64_t startSample, int64_t numSamples) const;

    //==============================================================================
    // Editing operations

    /**
     * Deletes a range of samples from the buffer.
     *
     * @param startSample Start sample (inclusive)
     * @param numSamples Number of samples to delete
     * @return true if successful
     */
    bool deleteRange(int64_t startSample, int64_t numSamples);

    /**
     * Inserts audio data at a specific position.
     *
     * @param insertPosition Sample position to insert at
     * @param audioToInsert Audio data to insert
     * @return true if successful
     */
    bool insertAudio(int64_t insertPosition, const juce::AudioBuffer<float>& audioToInsert);

    /**
     * Replaces a range with new audio data (deletes old range, inserts new data).
     *
     * @param startSample Start sample (inclusive)
     * @param numSamplesToReplace Number of samples to replace
     * @param newAudio New audio data
     * @return true if successful
     */
    bool replaceRange(int64_t startSample, int64_t numSamplesToReplace,
                     const juce::AudioBuffer<float>& newAudio);

private:
    juce::AudioBuffer<float> m_buffer;
    double m_sampleRate;
    int m_bitDepth;
    juce::CriticalSection m_lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioBufferManager)
};
