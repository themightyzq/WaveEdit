/*
  ==============================================================================

    AudioClipboard.h
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

/**
 * Singleton clipboard for audio data.
 *
 * Manages copied/cut audio data with proper sample rate and channel information.
 * Thread-safe for multi-threaded access.
 */
class AudioClipboard
{
public:
    /**
     * Gets the singleton instance.
     */
    static AudioClipboard& getInstance();

    /**
     * Copies audio data to the clipboard.
     *
     * @param buffer The audio buffer to copy
     * @param sampleRate The sample rate of the audio
     */
    void copyAudio(const juce::AudioBuffer<float>& buffer, double sampleRate);

    /**
     * Gets the audio data from the clipboard.
     *
     * @return Const reference to the clipboard buffer
     */
    const juce::AudioBuffer<float>& getAudio() const;

    /**
     * Gets the sample rate of the clipboard audio.
     *
     * @return Sample rate in Hz
     */
    double getSampleRate() const;

    /**
     * Checks if the clipboard has audio data.
     *
     * @return true if clipboard has data, false otherwise
     */
    bool hasAudio() const;

    /**
     * Clears the clipboard.
     */
    void clear();

    /**
     * Gets the number of channels in the clipboard.
     *
     * @return Number of channels
     */
    int getNumChannels() const;

    /**
     * Gets the number of samples in the clipboard.
     *
     * @return Number of samples
     */
    int getNumSamples() const;

private:
    AudioClipboard();
    ~AudioClipboard() = default;

    // Prevent copying
    AudioClipboard(const AudioClipboard&) = delete;
    AudioClipboard& operator=(const AudioClipboard&) = delete;

    juce::AudioBuffer<float> m_buffer;
    double m_sampleRate;
    juce::CriticalSection m_lock;
};
