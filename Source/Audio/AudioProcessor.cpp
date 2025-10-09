/*
  ==============================================================================

    AudioProcessor.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "AudioProcessor.h"

//==============================================================================
// Gain and Level Operations

bool AudioProcessor::applyGain(juce::AudioBuffer<float>& buffer, float gainDB)
{
    // Validate buffer
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        juce::Logger::writeToLog("AudioProcessor::applyGain - Empty buffer");
        return false;
    }

    // Validate gain range (-60dB to +20dB is reasonable)
    if (gainDB < -60.0f || gainDB > 20.0f)
    {
        juce::Logger::writeToLog(juce::String::formatted(
            "AudioProcessor::applyGain - Gain out of range: %.2f dB (valid: -60 to +20)",
            gainDB));
        return false;
    }

    // Convert dB to linear gain
    float linearGain = dBToLinear(gainDB);

    // Apply gain to all channels
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        buffer.applyGain(ch, 0, buffer.getNumSamples(), linearGain);
    }

    juce::Logger::writeToLog(juce::String::formatted(
        "AudioProcessor::applyGain - Applied %.2f dB gain (%.2fx linear) to %d channels",
        gainDB, linearGain, buffer.getNumChannels()));

    return true;
}

bool AudioProcessor::normalize(juce::AudioBuffer<float>& buffer, float targetDB)
{
    // Validate buffer
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        juce::Logger::writeToLog("AudioProcessor::normalize - Empty buffer");
        return false;
    }

    // Validate target level
    if (targetDB > 0.0f || targetDB < -60.0f)
    {
        juce::Logger::writeToLog(juce::String::formatted(
            "AudioProcessor::normalize - Target level out of range: %.2f dB (valid: -60 to 0)",
            targetDB));
        return false;
    }

    // Find peak across all channels
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float channelPeak = buffer.getMagnitude(ch, 0, buffer.getNumSamples());
        peak = std::max(peak, channelPeak);
    }

    // Check if buffer is silent
    if (peak < 1e-6f) // -120dB threshold
    {
        juce::Logger::writeToLog("AudioProcessor::normalize - Buffer is silent (peak < -120dB)");
        return false;
    }

    // Calculate required gain
    float targetLinear = dBToLinear(targetDB);
    float requiredGain = targetLinear / peak;
    float requiredGainDB = linearToDB(requiredGain);

    // Apply gain to all channels
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        buffer.applyGain(ch, 0, buffer.getNumSamples(), requiredGain);
    }

    juce::Logger::writeToLog(juce::String::formatted(
        "AudioProcessor::normalize - Peak: %.2f (%.2f dB), Applied: %.2f dB gain, Target: %.2f dB",
        peak, linearToDB(peak), requiredGainDB, targetDB));

    return true;
}

float AudioProcessor::getPeakLevelDB(const juce::AudioBuffer<float>& buffer)
{
    // Validate buffer
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        return -std::numeric_limits<float>::infinity();
    }

    // Find peak across all channels
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float channelPeak = buffer.getMagnitude(ch, 0, buffer.getNumSamples());
        peak = std::max(peak, channelPeak);
    }

    // Convert to dB
    return linearToDB(peak);
}

//==============================================================================
// Fade Operations

bool AudioProcessor::fadeIn(juce::AudioBuffer<float>& buffer, int numSamples)
{
    // Validate buffer
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        juce::Logger::writeToLog("AudioProcessor::fadeIn - Empty buffer");
        return false;
    }

    // Use entire buffer if numSamples not specified or out of range
    int fadeSamples = numSamples;
    if (fadeSamples <= 0 || fadeSamples > buffer.getNumSamples())
    {
        fadeSamples = buffer.getNumSamples();
    }

    // Apply linear fade in to all channels
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);

        for (int i = 0; i < fadeSamples; ++i)
        {
            float gain = static_cast<float>(i) / static_cast<float>(fadeSamples);
            channelData[i] *= gain;
        }
    }

    juce::Logger::writeToLog(juce::String::formatted(
        "AudioProcessor::fadeIn - Applied %d sample fade to %d channels",
        fadeSamples, buffer.getNumChannels()));

    return true;
}

bool AudioProcessor::fadeOut(juce::AudioBuffer<float>& buffer, int numSamples)
{
    // Validate buffer
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        juce::Logger::writeToLog("AudioProcessor::fadeOut - Empty buffer");
        return false;
    }

    // Use entire buffer if numSamples not specified or out of range
    int fadeSamples = numSamples;
    if (fadeSamples <= 0 || fadeSamples > buffer.getNumSamples())
    {
        fadeSamples = buffer.getNumSamples();
    }

    // Calculate start position (fade from end backwards)
    int startSample = buffer.getNumSamples() - fadeSamples;

    // Apply linear fade out to all channels
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);

        for (int i = 0; i < fadeSamples; ++i)
        {
            float gain = 1.0f - (static_cast<float>(i) / static_cast<float>(fadeSamples));
            channelData[startSample + i] *= gain;
        }
    }

    juce::Logger::writeToLog(juce::String::formatted(
        "AudioProcessor::fadeOut - Applied %d sample fade to %d channels",
        fadeSamples, buffer.getNumChannels()));

    return true;
}

//==============================================================================
// DC Offset Operations

bool AudioProcessor::removeDCOffset(juce::AudioBuffer<float>& buffer)
{
    // Validate buffer
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        juce::Logger::writeToLog("AudioProcessor::removeDCOffset - Empty buffer");
        return false;
    }

    // Process each channel independently
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);
        int numSamples = buffer.getNumSamples();

        // Calculate DC offset (average value)
        double sum = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            sum += channelData[i];
        }
        float dcOffset = static_cast<float>(sum / numSamples);

        // Subtract DC offset from all samples
        for (int i = 0; i < numSamples; ++i)
        {
            channelData[i] -= dcOffset;
        }

        juce::Logger::writeToLog(juce::String::formatted(
            "AudioProcessor::removeDCOffset - Channel %d: removed %.6f DC offset",
            ch, dcOffset));
    }

    return true;
}

//==============================================================================
// Utility Functions

int AudioProcessor::clampToValidRange(juce::AudioBuffer<float>& buffer)
{
    // Validate buffer
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        return 0;
    }

    int clippedSamples = 0;

    // Clamp each channel
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (channelData[i] > 1.0f)
            {
                channelData[i] = 1.0f;
                ++clippedSamples;
            }
            else if (channelData[i] < -1.0f)
            {
                channelData[i] = -1.0f;
                ++clippedSamples;
            }
        }
    }

    if (clippedSamples > 0)
    {
        juce::Logger::writeToLog(juce::String::formatted(
            "AudioProcessor::clampToValidRange - Clamped %d samples (%.2f%%)",
            clippedSamples,
            100.0f * clippedSamples / (buffer.getNumSamples() * buffer.getNumChannels())));
    }

    return clippedSamples;
}
