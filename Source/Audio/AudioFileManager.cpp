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
    outInfo.metadata = reader->metadataValues;  // Read BWF metadata

    return true;
}

bool AudioFileManager::isValidAudioFile(const juce::File& file)
{
    clearError();

    if (!file.existsAsFile())
    {
        setError("File does not exist: " + file.getFullPathName());
        return false;
    }

    // Try to create a reader for the file
    std::unique_ptr<juce::AudioFormatReader> reader(m_formatManager.createReaderFor(file));

    if (reader == nullptr)
    {
        setError("Could not create reader for file: " + file.getFullPathName());
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
                                  int bitDepth,
                                  const juce::StringPairArray& metadata)
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
                                             metadata,  // BWF metadata passed through
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
                                      int bitDepth,
                                      const juce::StringPairArray& metadata)
{
    clearError();

    // Check that the file exists
    if (!file.existsAsFile())
    {
        setError("Cannot overwrite file that does not exist: " + file.getFullPathName());
        return false;
    }

    // Save using the standard save method (which will overwrite)
    return saveAsWav(file, buffer, sampleRate, bitDepth, metadata);
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

bool AudioFileManager::appendiXMLChunk(const juce::File& file, const juce::String& ixmlData)
{
    clearError();

    if (!file.existsAsFile())
    {
        setError("File does not exist: " + file.getFullPathName());
        return false;
    }

    if (ixmlData.isEmpty())
    {
        // No iXML data to write - this is okay
        return true;
    }

    // Open file for reading and writing
    juce::FileInputStream inputStream(file);
    if (!inputStream.openedOk())
    {
        setError("Could not open file for reading: " + file.getFullPathName());
        return false;
    }

    // Read entire file into memory
    juce::MemoryBlock fileData;
    inputStream.readIntoMemoryBlock(fileData);
    inputStream.setPosition(0);

    // Verify RIFF/WAVE header
    if (fileData.getSize() < 12)
    {
        setError("File too small to be a valid WAV file");
        return false;
    }

    auto* data = static_cast<const char*>(fileData.getData());
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
    {
        setError("File is not a valid WAV/RIFF file");
        return false;
    }

    // Create iXML chunk data
    juce::MemoryOutputStream ixmlChunk;

    // Write chunk ID
    ixmlChunk.write("iXML", 4);

    // Convert XML string to UTF-8 bytes
    juce::MemoryBlock xmlBytes;
    xmlBytes.append(ixmlData.toUTF8(), ixmlData.getNumBytesAsUTF8());

    // Chunk size (must be even, pad with 0 if odd)
    uint32_t chunkSize = static_cast<uint32_t>(xmlBytes.getSize());
    bool needsPadding = (chunkSize % 2) != 0;

    // Write chunk size (little-endian - JUCE's writeInt is little-endian by default)
    ixmlChunk.writeInt(static_cast<int>(chunkSize));

    // Write XML data
    ixmlChunk.write(xmlBytes.getData(), xmlBytes.getSize());

    // Add padding byte if needed
    if (needsPadding)
    {
        ixmlChunk.writeByte(0);
    }

    // Parse existing chunks and copy only ESSENTIAL chunks (fmt and data)
    // This prevents file corruption from nested RIFF chunks or duplicate iXML chunks
    juce::MemoryOutputStream cleanFile;

    // DEBUG: Log input file size
    juce::Logger::writeToLog("DEBUG: Input file size = " + juce::String((int)fileData.getSize()) + " bytes");

    // Write RIFF header (we'll update size later)
    cleanFile.write("RIFF", 4);
    cleanFile.writeInt(0);  // Placeholder size
    cleanFile.write("WAVE", 4);

    // Parse existing chunks and copy only fmt and data chunks
    size_t offset = 12;  // Skip RIFF header
    size_t fileSize = fileData.getSize();

    while (offset + 8 <= fileSize)
    {
        // Read chunk ID
        char chunkID[5] = {0};
        memcpy(chunkID, data + offset, 4);

        // Read chunk size
        uint32_t parsedChunkSize;
        memcpy(&parsedChunkSize, data + offset + 4, 4);

        // Check if chunk data is within file bounds
        if (offset + 8 + parsedChunkSize > fileSize)
            break;  // Corrupted chunk, stop parsing

        // Only copy fmt and data chunks (skip iXML, LIST, JUNK, RIFF, and other chunks)
        if (memcmp(chunkID, "fmt ", 4) == 0 || memcmp(chunkID, "data", 4) == 0)
        {
            // DEBUG: Log chunk being copied
            juce::Logger::writeToLog("DEBUG: Copying chunk '" + juce::String(chunkID, 4) +
                                    "' size=" + juce::String((int)parsedChunkSize) +
                                    " at offset=" + juce::String((int)offset));

            // Copy entire chunk (ID + size + data)
            cleanFile.write(data + offset, 8 + parsedChunkSize);

            // Add padding if chunk size is odd
            if (parsedChunkSize % 2 != 0)
                cleanFile.writeByte(0);
        }
        else
        {
            // DEBUG: Log chunk being skipped
            juce::Logger::writeToLog("DEBUG: Skipping chunk '" + juce::String(chunkID, 4) +
                                    "' size=" + juce::String((int)parsedChunkSize));
        }

        // Move to next chunk
        offset += 8 + parsedChunkSize;
        if (parsedChunkSize % 2 != 0)
            offset += 1;  // Skip padding
    }

    // DEBUG: Log size before appending iXML
    juce::Logger::writeToLog("DEBUG: Clean file size before iXML = " + juce::String((int)cleanFile.getDataSize()) + " bytes");

    // Append iXML chunk to clean file
    cleanFile.write(ixmlChunk.getData(), ixmlChunk.getDataSize());

    // DEBUG: Log size after appending iXML
    juce::Logger::writeToLog("DEBUG: Clean file size after iXML = " + juce::String((int)cleanFile.getDataSize()) + " bytes");

    // Update RIFF size (file size - 8 bytes for "RIFF" and size field)
    auto* cleanData = static_cast<char*>(const_cast<void*>(cleanFile.getData()));
    uint32_t riffSize = static_cast<uint32_t>(cleanFile.getDataSize()) - 8;
    memcpy(cleanData + 4, &riffSize, 4);

    // DEBUG: Log RIFF size being written
    juce::Logger::writeToLog("DEBUG: RIFF size field = " + juce::String((int)riffSize) + " bytes");

    // DEBUG: Log what we're about to write
    juce::Logger::writeToLog("DEBUG: Writing " + juce::String((int)cleanFile.getDataSize()) +
                            " bytes to " + file.getFullPathName());

    // CRITICAL: We must delete the file first to avoid appending!
    // FileOutputStream doesn't truncate by default
    if (!file.deleteFile())
    {
        setError("Could not delete file for overwriting: " + file.getFullPathName());
        return false;
    }

    // Write updated file
    juce::FileOutputStream outputStream(file);
    if (!outputStream.openedOk())
    {
        setError("Could not open file for writing: " + file.getFullPathName());
        return false;
    }

    outputStream.write(cleanFile.getData(), cleanFile.getDataSize());
    outputStream.flush();

    // DEBUG: Verify file size on disk
    juce::int64 actualFileSize = file.getSize();
    juce::Logger::writeToLog("DEBUG: Actual file size on disk = " + juce::String((int)actualFileSize) + " bytes");

    if (actualFileSize != cleanFile.getDataSize())
    {
        juce::Logger::writeToLog("ERROR: File size mismatch! Expected " +
                                juce::String((int)cleanFile.getDataSize()) +
                                " bytes, got " + juce::String((int)actualFileSize) + " bytes");
    }

    juce::Logger::writeToLog("iXML chunk appended successfully (" +
                            juce::String(chunkSize) + " bytes, total file size: " +
                            juce::String((int)cleanFile.getDataSize()) + " bytes)");

    return true;
}

bool AudioFileManager::readiXMLChunk(const juce::File& file, juce::String& outData)
{
    clearError();

    if (!file.existsAsFile())
    {
        setError("File does not exist: " + file.getFullPathName());
        return false;
    }

    // Open file for reading
    juce::FileInputStream inputStream(file);
    if (!inputStream.openedOk())
    {
        setError("Could not open file for reading: " + file.getFullPathName());
        return false;
    }

    // Read entire file into memory
    juce::MemoryBlock fileData;
    inputStream.readIntoMemoryBlock(fileData);

    // Verify RIFF/WAVE header
    if (fileData.getSize() < 12)
    {
        setError("File too small to be a valid WAV file");
        return false;
    }

    auto* data = static_cast<const char*>(fileData.getData());
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
    {
        setError("File is not a valid WAV/RIFF file");
        return false;
    }

    // Parse chunks to find iXML chunk
    // RIFF structure: "RIFF" + size (4 bytes) + "WAVE" + chunks
    size_t offset = 12;  // Skip RIFF header
    size_t fileSize = fileData.getSize();

    juce::Logger::writeToLog("Searching for iXML chunk in file of size " + juce::String((int)fileSize) + " bytes");

    while (offset + 8 <= fileSize)
    {
        // Read chunk ID (4 bytes)
        char chunkID[5] = {0};
        memcpy(chunkID, data + offset, 4);
        offset += 4;

        // Read chunk size (4 bytes, little-endian)
        uint32_t chunkSize;
        memcpy(&chunkSize, data + offset, 4);
        offset += 4;

        juce::Logger::writeToLog("Found chunk: '" + juce::String(chunkID) + "' size=" + juce::String((int)chunkSize) + " at offset=" + juce::String((int)(offset - 8)));

        // Check if this is the iXML chunk
        if (memcmp(chunkID, "iXML", 4) == 0)
        {
            // Found iXML chunk - read the data
            if (offset + chunkSize > fileSize)
            {
                setError("iXML chunk size exceeds file size");
                return false;
            }

            // Convert chunk data to string
            outData = juce::String::fromUTF8(data + offset, static_cast<int>(chunkSize));

            juce::Logger::writeToLog("iXML chunk read successfully (" +
                                    juce::String(chunkSize) + " bytes)");
            return true;
        }

        // Skip to next chunk (chunks must be word-aligned, so add padding if odd size)
        offset += chunkSize;
        if (chunkSize % 2 != 0)
            offset += 1;
    }

    juce::Logger::writeToLog("No iXML chunk found after parsing all chunks");
    // No iXML chunk found
    return false;
}
