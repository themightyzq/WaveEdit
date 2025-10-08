/*
  ==============================================================================

    AudioFileManager.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "AudioFileManager.h"

//==============================================================================
AudioFileManager::AudioFileManager()
{
    // Register basic audio formats (WAV for Phase 1)
    m_formatManager.registerBasicFormats();
}

AudioFileManager::~AudioFileManager()
{
}

//==============================================================================
// File Information

bool AudioFileManager::getFileInfo(const juce::File& file, AudioFileInfo& outInfo)
{
    clearError();

    // Check file exists
    if (!file.existsAsFile())
    {
        setError("File does not exist: " + file.getFullPathName());
        return false;
    }

    // Create a reader for the file
    std::unique_ptr<juce::AudioFormatReader> reader(m_formatManager.createReaderFor(file));

    if (reader == nullptr)
    {
        setError("Could not create reader for file: " + file.getFullPathName());
        return false;
    }

    // Fill in the file info
    outInfo.filename = file.getFileName();
    outInfo.fullPath = file.getFullPathName();
    outInfo.sampleRate = reader->sampleRate;
    outInfo.numChannels = static_cast<int>(reader->numChannels);
    outInfo.bitDepth = static_cast<int>(reader->bitsPerSample);
    outInfo.lengthInSamples = reader->lengthInSamples;
    outInfo.lengthInSeconds = reader->lengthInSamples / reader->sampleRate;
    outInfo.formatName = reader->getFormatName();

    return true;
}

bool AudioFileManager::isValidAudioFile(const juce::File& file)
{
    clearError();

    if (!file.existsAsFile())
    {
        return false;
    }

    // Try to create a reader for the file
    std::unique_ptr<juce::AudioFormatReader> reader(m_formatManager.createReaderFor(file));

    if (reader == nullptr)
    {
        return false;
    }

    // Check that it's a WAV file (Phase 1 requirement)
    if (reader->getFormatName() != "WAV file")
    {
        setError("Only WAV files are supported in Phase 1");
        return false;
    }

    // Check sample rate is supported
    double sampleRate = reader->sampleRate;
    const double supportedRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    bool sampleRateSupported = false;

    for (double rate : supportedRates)
    {
        if (std::abs(sampleRate - rate) < 0.1)
        {
            sampleRateSupported = true;
            break;
        }
    }

    if (!sampleRateSupported)
    {
        setError("Unsupported sample rate: " + juce::String(sampleRate) + " Hz");
        return false;
    }

    // Check channel count (1-8 channels)
    int numChannels = static_cast<int>(reader->numChannels);
    if (numChannels < 1 || numChannels > 8)
    {
        setError("Unsupported channel count: " + juce::String(numChannels) + " (1-8 channels supported)");
        return false;
    }

    // Check bit depth
    int bitDepth = static_cast<int>(reader->bitsPerSample);
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32)
    {
        setError("Unsupported bit depth: " + juce::String(bitDepth) + " bits (only 16/24/32-bit supported)");
        return false;
    }

    return true;
}

juce::String AudioFileManager::getSupportedExtensions() const
{
    // Phase 1: WAV only
    return "*.wav";
}

//==============================================================================
// File Loading

bool AudioFileManager::loadIntoBuffer(const juce::File& file, juce::AudioBuffer<float>& outBuffer)
{
    clearError();

    // Validate the file first
    if (!isValidAudioFile(file))
    {
        // Error message already set by isValidAudioFile
        return false;
    }

    // Create a reader for the file
    std::unique_ptr<juce::AudioFormatReader> reader(m_formatManager.createReaderFor(file));

    if (reader == nullptr)
    {
        setError("Failed to create reader for file: " + file.getFullPathName());
        return false;
    }

    // Set up the output buffer
    outBuffer.setSize(static_cast<int>(reader->numChannels),
                      static_cast<int>(reader->lengthInSamples));

    // Read the audio data into the buffer
    bool readSuccess = reader->read(&outBuffer,
                                     0,                           // Start sample in destination
                                     static_cast<int>(reader->lengthInSamples), // Number of samples
                                     0,                           // Start sample in source
                                     true,                        // Use left channel
                                     true);                       // Use right channel

    if (!readSuccess)
    {
        setError("Failed to read audio data from file: " + file.getFullPathName());
        return false;
    }

    juce::Logger::writeToLog("Loaded audio buffer: " + juce::String(reader->numChannels) +
                             " channels, " + juce::String(reader->lengthInSamples) + " samples");

    return true;
}

juce::AudioFormatReader* AudioFileManager::createReaderFor(const juce::File& file)
{
    clearError();

    if (!file.existsAsFile())
    {
        setError("File does not exist: " + file.getFullPathName());
        return nullptr;
    }

    auto* reader = m_formatManager.createReaderFor(file);

    if (reader == nullptr)
    {
        setError("Failed to create reader for file: " + file.getFullPathName());
    }

    return reader;
}

//==============================================================================
// File Saving

bool AudioFileManager::saveAsWav(const juce::File& file,
                                  const juce::AudioBuffer<float>& buffer,
                                  double sampleRate,
                                  int bitDepth)
{
    clearError();

    // Validate parameters
    if (!validateBufferForSaving(buffer, sampleRate, bitDepth))
    {
        // Error message already set by validateBufferForSaving
        return false;
    }

    // Get the WAV format
    auto* wavFormat = getFormatForFile(file);

    if (wavFormat == nullptr)
    {
        setError("Could not find WAV format handler");
        return false;
    }

    // Create output stream
    std::unique_ptr<juce::FileOutputStream> outputStream(file.createOutputStream());

    if (outputStream == nullptr)
    {
        setError("Could not create output stream for file: " + file.getFullPathName());
        return false;
    }

    // Create writer with specified bit depth
    std::unique_ptr<juce::AudioFormatWriter> writer;

    writer.reset(wavFormat->createWriterFor(outputStream.get(),
                                             sampleRate,
                                             static_cast<unsigned int>(buffer.getNumChannels()),
                                             bitDepth,
                                             {},    // Metadata (empty for now)
                                             0));   // Quality option (not used for WAV)

    if (writer == nullptr)
    {
        setError("Could not create writer for file: " + file.getFullPathName());
        return false;
    }

    // Release ownership of the output stream (writer takes ownership)
    outputStream.release();

    // Write the buffer to file
    bool writeSuccess = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

    // Flush and close the writer
    writer.reset();

    if (!writeSuccess)
    {
        setError("Failed to write audio data to file: " + file.getFullPathName());
        return false;
    }

    juce::Logger::writeToLog("Saved WAV file: " + file.getFullPathName());
    juce::Logger::writeToLog("Sample rate: " + juce::String(sampleRate) + " Hz, Bit depth: " +
                             juce::String(bitDepth) + " bits");

    return true;
}

bool AudioFileManager::overwriteFile(const juce::File& file,
                                      const juce::AudioBuffer<float>& buffer,
                                      double sampleRate,
                                      int bitDepth)
{
    clearError();

    // Check that the file exists
    if (!file.existsAsFile())
    {
        setError("Cannot overwrite file that does not exist: " + file.getFullPathName());
        return false;
    }

    // Save using the standard save method (which will overwrite)
    return saveAsWav(file, buffer, sampleRate, bitDepth);
}

//==============================================================================
// Error Handling

juce::String AudioFileManager::getLastError() const
{
    return m_lastError;
}

void AudioFileManager::clearError()
{
    m_lastError.clear();
}

//==============================================================================
// Private Methods

void AudioFileManager::setError(const juce::String& errorMessage)
{
    m_lastError = errorMessage;
    juce::Logger::writeToLog("AudioFileManager error: " + errorMessage);
}

bool AudioFileManager::validateBufferForSaving(const juce::AudioBuffer<float>& buffer,
                                                double sampleRate,
                                                int bitDepth)
{
    // Check buffer is not empty
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
    {
        setError("Cannot save empty audio buffer");
        return false;
    }

    // Check sample rate
    const double supportedRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    bool sampleRateSupported = false;

    for (double rate : supportedRates)
    {
        if (std::abs(sampleRate - rate) < 0.1)
        {
            sampleRateSupported = true;
            break;
        }
    }

    if (!sampleRateSupported)
    {
        setError("Unsupported sample rate for saving: " + juce::String(sampleRate) + " Hz");
        return false;
    }

    // Check bit depth
    if (bitDepth != 16 && bitDepth != 24 && bitDepth != 32)
    {
        setError("Unsupported bit depth for saving: " + juce::String(bitDepth) + " bits");
        return false;
    }

    // Check channel count
    int numChannels = buffer.getNumChannels();
    if (numChannels < 1 || numChannels > 8)
    {
        setError("Unsupported channel count for saving: " + juce::String(numChannels) +
                " (1-8 channels supported)");
        return false;
    }

    return true;
}

juce::AudioFormat* AudioFileManager::getFormatForFile(const juce::File& file)
{
    // Get the file extension
    juce::String extension = file.getFileExtension().toLowerCase();

    // Find the appropriate format
    for (int i = 0; i < m_formatManager.getNumKnownFormats(); ++i)
    {
        auto* format = m_formatManager.getKnownFormat(i);

        if (format->getFileExtensions().contains(extension))
        {
            return format;
        }
    }

    return nullptr;
}
