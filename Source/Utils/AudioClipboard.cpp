/*
  ==============================================================================

    AudioClipboard.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "AudioClipboard.h"

//==============================================================================
AudioClipboard::AudioClipboard()
    : m_sampleRate(44100.0)
{
}

AudioClipboard& AudioClipboard::getInstance()
{
    static AudioClipboard instance;
    return instance;
}

void AudioClipboard::copyAudio(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    juce::ScopedLock sl(m_lock);

    // Copy buffer data
    m_buffer.makeCopyOf(buffer);
    m_sampleRate = sampleRate;

    juce::Logger::writeToLog("AudioClipboard: Copied " + juce::String(buffer.getNumSamples()) +
                             " samples, " + juce::String(buffer.getNumChannels()) + " channels, " +
                             juce::String(sampleRate) + " Hz");
}

const juce::AudioBuffer<float>& AudioClipboard::getAudio() const
{
    return m_buffer;
}

double AudioClipboard::getSampleRate() const
{
    juce::ScopedLock sl(m_lock);
    return m_sampleRate;
}

bool AudioClipboard::hasAudio() const
{
    juce::ScopedLock sl(m_lock);
    return m_buffer.getNumSamples() > 0 && m_buffer.getNumChannels() > 0;
}

void AudioClipboard::clear()
{
    juce::ScopedLock sl(m_lock);
    m_buffer.setSize(0, 0);
    m_sampleRate = 44100.0;

    juce::Logger::writeToLog("AudioClipboard: Cleared");
}

int AudioClipboard::getNumChannels() const
{
    juce::ScopedLock sl(m_lock);
    return m_buffer.getNumChannels();
}

int AudioClipboard::getNumSamples() const
{
    juce::ScopedLock sl(m_lock);
    return m_buffer.getNumSamples();
}
