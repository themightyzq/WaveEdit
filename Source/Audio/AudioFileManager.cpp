/*
  ==============================================================================

    AudioFileManager.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

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
    // Register basic audio formats (WAV, AIFF, FLAC, OGG)
    m_formatManager.registerBasicFormats();

    // MP3 is not included in registerBasicFormats(), so register it manually
    m_formatManager.registerFormat(new juce::MP3AudioFormat(), true);
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

    // Check sample rate is supported
    double sampleRate = reader->sampleRate;
    const double supportedRates[] = { 8000.0, 11025.0, 16000.0, 22050.0, 32000.0,
                                      44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
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
    if (bitDepth != 8 && bitDepth != 16 && bitDepth != 24 && bitDepth != 32)
    {
        setError("Unsupported bit depth: " + juce::String(bitDepth) + " bits (only 8/16/24/32-bit supported)");
        return false;
    }

    return true;
}

juce::String AudioFileManager::getSupportedExtensions() const
{
    return "*.wav;*.flac;*.ogg";
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

    DBG("Loaded audio buffer: " + juce::String(reader->numChannels) +
                             " channels, " + juce::String(reader->lengthInSamples) + " samples");

    return true;
}

bool AudioFileManager::loadIntoBufferUnchecked(const juce::File& file,
                                               juce::AudioBuffer<float>& outBuffer)
{
    clearError();

    if (!file.existsAsFile())
    {
        setError("File does not exist: " + file.getFullPathName());
        return false;
    }

    // Decodable file is still required, but the sample-rate whitelist is
    // intentionally skipped (M6) -- recovery must reload a file the app
    // itself wrote.
    std::unique_ptr<juce::AudioFormatReader> reader(m_formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        setError("Failed to create reader for file: " + file.getFullPathName());
        return false;
    }

    // Read into a temporary so a partial/failed read doesn't clobber
    // outBuffer (the caller's existing audio).
    juce::AudioBuffer<float> tmp(static_cast<int>(reader->numChannels),
                                 static_cast<int>(reader->lengthInSamples));

    bool readSuccess = reader->read(&tmp,
                                    0,
                                    static_cast<int>(reader->lengthInSamples),
                                    0,
                                    true,
                                    true);

    if (!readSuccess)
    {
        setError("Failed to read audio data from file: " + file.getFullPathName());
        return false;
    }

    outBuffer = std::move(tmp);
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

    // Atomic save: write to a sibling temp file, then swap it onto the
    // target in a single step. A crash / disk-full mid-write leaves the
    // original file intact rather than truncated or zero-byte (C10).
    juce::TemporaryFile tempFile(file, juce::TemporaryFile::useHiddenFile);

    // Create output stream on the TEMP file - need OutputStream base type for
    // new JUCE 8 API
    std::unique_ptr<juce::OutputStream> outputStream(tempFile.getFile().createOutputStream());

    if (outputStream == nullptr)
    {
        setError("Could not create output stream for file: " + file.getFullPathName());
        return false;
    }

    // Create writer with specified bit depth using modern JUCE API (JUCE 8+)
    // Convert StringPairArray metadata to unordered_map for AudioFormatWriterOptions
    std::unordered_map<juce::String, juce::String> metadataMap;
    for (int i = 0; i < metadata.size(); ++i)
    {
        metadataMap[metadata.getAllKeys()[i]] = metadata.getAllValues()[i];
    }

    // Build options using fluent API
    auto options = juce::AudioFormatWriterOptions()
        .withSampleRate(sampleRate)
        .withNumChannels(buffer.getNumChannels())
        .withBitsPerSample(bitDepth)
        .withMetadataValues(metadataMap)
        .withQualityOptionIndex(0)
        .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::integral);  // Force PCM for 8-bit

    // New JUCE 8 API takes unique_ptr by non-const lvalue reference and transfers ownership
    std::unique_ptr<juce::AudioFormatWriter> writer = wavFormat->createWriterFor(outputStream, options);

    if (writer == nullptr)
    {
        setError("Could not create writer for file: " + file.getFullPathName() +
                 "\nBit depth: " + juce::String(bitDepth) + " bits may not be supported");
        return false;
    }

    // Note: outputStream ownership transferred via reference above (unique_ptr now null)

    // Write the buffer to the temp file
    bool writeSuccess = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

    // Flush and close the writer
    writer.reset();

    if (!writeSuccess)
    {
        setError("Failed to write audio data to file: " + file.getFullPathName());
        return false;
    }

    // Atomically swap the completed temp file onto the target.
    if (!tempFile.overwriteTargetFileWithTemporary())
    {
        setError("Could not replace target file with completed temp file: " + file.getFullPathName());
        return false;
    }

    // Verify 8-bit files are written correctly (unsigned PCM format validation)
    if (bitDepth == 8)
    {
        DBG("Verifying 8-bit PCM format...");

        // Read back the file to verify format
        std::unique_ptr<juce::AudioFormatReader> verifyReader(m_formatManager.createReaderFor(file));

        if (verifyReader == nullptr)
        {
            setError("8-bit file verification failed: Cannot read back file: " + file.getFullPathName());
            file.deleteFile();  // Clean up corrupted file
            return false;
        }

        // Verify bit depth matches what we wrote
        if (verifyReader->bitsPerSample != 8)
        {
            setError("8-bit file verification failed: Expected 8-bit but got " +
                    juce::String(verifyReader->bitsPerSample) + "-bit");
            file.deleteFile();
            return false;
        }

        // Read first block to verify samples are in valid range
        const int samplesToCheck = juce::jmin(1024, static_cast<int>(verifyReader->lengthInSamples));
        juce::AudioBuffer<float> verifyBuffer(static_cast<int>(verifyReader->numChannels), samplesToCheck);

        if (!verifyReader->read(&verifyBuffer, 0, samplesToCheck, 0, true, true))
        {
            setError("8-bit file verification failed: Cannot read sample data");
            file.deleteFile();
            return false;
        }

        // Check samples are within expected range for 8-bit audio (-1.0 to 1.0 when read as float)
        for (int ch = 0; ch < verifyBuffer.getNumChannels(); ++ch)
        {
            const float* channelData = verifyBuffer.getReadPointer(ch);
            for (int i = 0; i < samplesToCheck; ++i)
            {
                float sample = channelData[i];
                if (sample < -1.0f || sample > 1.0f)
                {
                    setError("8-bit file verification failed: Sample out of range at channel " +
                            juce::String(ch) + ", sample " + juce::String(i) + ": " + juce::String(sample));
                    file.deleteFile();
                    return false;
                }
            }
        }

        juce::Logger::writeToLog("8-bit PCM format verified successfully");
    }

    DBG("Saved WAV file: " + file.getFullPathName());
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
    DBG("AudioFileManager error: " + errorMessage);
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
    const double supportedRates[] = { 8000.0, 11025.0, 16000.0, 22050.0, 32000.0,
                                      44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
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
    if (bitDepth != 8 && bitDepth != 16 && bitDepth != 24 && bitDepth != 32)
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

    // Parse existing chunks and copy ALL chunks EXCEPT any existing iXML
    // chunk (which we are replacing). Critically this PRESERVES the BWF
    // "bext" chunk (and LIST/INFO, cue, etc.) that JUCE's writer just wrote
    // -- an earlier version stripped everything but fmt/data, silently
    // destroying BWF metadata on every iXML save (C11).
    juce::MemoryOutputStream cleanFile;

    // Write RIFF header (we'll update size later)
    cleanFile.write("RIFF", 4);
    cleanFile.writeInt(0);  // Placeholder size
    cleanFile.write("WAVE", 4);

    // Parse existing chunks. Use uint64_t arithmetic throughout so a crafted
    // or corrupt near-UINT32_MAX size field cannot wrap past the bounds
    // check (M8).
    uint64_t offset = 12;  // Skip RIFF header
    const uint64_t fileSize = static_cast<uint64_t>(fileData.getSize());

    while (offset + 8 <= fileSize)
    {
        // Read chunk ID
        char chunkID[5] = {0};
        memcpy(chunkID, data + offset, 4);

        // Read chunk size
        uint32_t parsedChunkSizeRaw;
        memcpy(&parsedChunkSizeRaw, data + offset + 4, 4);
        const uint64_t parsedChunkSize = static_cast<uint64_t>(parsedChunkSizeRaw);

        // Reject any chunk whose declared size exceeds the bytes remaining
        // after its header. 64-bit math means this comparison can't overflow.
        if (parsedChunkSize > fileSize - offset - 8)
            break;  // Corrupted chunk, stop parsing

        // Copy every chunk except an existing iXML chunk (we append a fresh
        // one below). This keeps bext / LIST / cue / etc. intact.
        if (memcmp(chunkID, "iXML", 4) != 0)
        {
            // Copy entire chunk (ID + size + data)
            cleanFile.write(data + offset, static_cast<size_t>(8 + parsedChunkSize));

            // Add padding if chunk size is odd
            if (parsedChunkSize % 2 != 0)
                cleanFile.writeByte(0);
        }

        // Move to next chunk
        offset += 8 + parsedChunkSize;
        if (parsedChunkSize % 2 != 0)
            offset += 1;  // Skip padding
    }

    // Append iXML chunk to clean file
    cleanFile.write(ixmlChunk.getData(), ixmlChunk.getDataSize());

    // Update RIFF size (file size - 8 bytes for "RIFF" and size field)
    auto* cleanData = static_cast<char*>(const_cast<void*>(cleanFile.getData()));
    uint32_t riffSize = static_cast<uint32_t>(cleanFile.getDataSize()) - 8;
    memcpy(cleanData + 4, &riffSize, 4);

    // Atomic replace: write the rebuilt file to a sibling temp file, then
    // swap it onto the target in one step. A crash / disk-full mid-write
    // leaves the original intact instead of a truncated or deleted file
    // (C10). Pattern mirrors ThumbnailDiskCache::save.
    {
        juce::TemporaryFile tmp(file, juce::TemporaryFile::useHiddenFile);
        {
            juce::FileOutputStream outputStream(tmp.getFile());
            if (!outputStream.openedOk())
            {
                setError("Could not open temp file for writing: " + file.getFullPathName());
                return false;
            }

            if (!outputStream.write(cleanFile.getData(), cleanFile.getDataSize()))
            {
                setError("Failed to write iXML data to temp file: " + file.getFullPathName());
                return false;
            }
            outputStream.flush();
        }

        if (!tmp.overwriteTargetFileWithTemporary())
        {
            setError("Could not replace file with updated temp file: " + file.getFullPathName());
            return false;
        }
    }

    // Verify file size on disk
    juce::int64 actualFileSize = file.getSize();
    if (actualFileSize != cleanFile.getDataSize())
    {
        juce::Logger::writeToLog("ERROR: File size mismatch! Expected " +
                                juce::String((int)cleanFile.getDataSize()) +
                                " bytes, got " + juce::String((int)actualFileSize) + " bytes");
    }

    DBG("iXML chunk appended successfully (" +
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
    // Use uint64_t arithmetic so a crafted near-UINT32_MAX size field cannot
    // wrap past the bounds checks (M8).
    uint64_t offset = 12;  // Skip RIFF header
    const uint64_t fileSize = static_cast<uint64_t>(fileData.getSize());

    DBG("Searching for iXML chunk in file of size " + juce::String((int)fileSize) + " bytes");

    while (offset + 8 <= fileSize)
    {
        // Read chunk ID (4 bytes)
        char chunkID[5] = {0};
        memcpy(chunkID, data + offset, 4);
        offset += 4;

        // Read chunk size (4 bytes, little-endian)
        uint32_t chunkSizeRaw;
        memcpy(&chunkSizeRaw, data + offset, 4);
        offset += 4;
        const uint64_t chunkSize = static_cast<uint64_t>(chunkSizeRaw);

        DBG("Found chunk: '" + juce::String(chunkID) + "' size=" + juce::String((int)chunkSize) + " at offset=" + juce::String((int)(offset - 8)));

        // Check if this is the iXML chunk
        if (memcmp(chunkID, "iXML", 4) == 0)
        {
            // Found iXML chunk - read the data. Compare with subtraction to
            // avoid offset + chunkSize overflowing.
            if (chunkSize > fileSize - offset)
            {
                setError("iXML chunk size exceeds file size");
                return false;
            }

            // Convert chunk data to string
            outData = juce::String::fromUTF8(data + offset, static_cast<int>(chunkSize));

            DBG("iXML chunk read successfully (" +
                                    juce::String(chunkSize) + " bytes)");
            return true;
        }

        // Skip to next chunk (chunks must be word-aligned, so add padding if odd size)
        offset += chunkSize;
        if (chunkSize % 2 != 0)
            offset += 1;
    }

    DBG("No iXML chunk found after parsing all chunks");
    // No iXML chunk found
    return false;
}

//==============================================================================
bool AudioFileManager::saveAudioFile(const juce::File& file,
                                      const juce::AudioBuffer<float>& buffer,
                                      double sampleRate,
                                      int bitDepth,
                                      int qualityOptionIndex,
                                      const juce::StringPairArray& metadata)
{
    clearError();

    // Validate parameters
    if (!file.getParentDirectory().exists())
    {
        setError("Parent directory does not exist: " + file.getParentDirectory().getFullPathName());
        return false;
    }

    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
    {
        setError("Cannot save empty audio buffer");
        return false;
    }

    if (sampleRate <= 0.0)
    {
        setError("Invalid sample rate: " + juce::String(sampleRate));
        return false;
    }

    // Detect format from file extension
    juce::String extension = file.getFileExtension().toLowerCase();

    juce::Logger::writeToLog("Saving audio file: " + file.getFullPathName());
    DBG("Format: " + extension + ", Sample rate: " + juce::String(sampleRate) +
                            "Hz, Channels: " + juce::String(buffer.getNumChannels()) +
                            ", Samples: " + juce::String(buffer.getNumSamples()));

    // For WAV files, use the existing saveAsWav method (which handles BWF metadata and iXML)
    if (extension == ".wav")
    {
        return saveAsWav(file, buffer, sampleRate, bitDepth, metadata);
    }

    // For other formats (FLAC, OGG, MP3), use JUCE's generic audio format writer
    // Find appropriate format writer
    juce::AudioFormat* format = nullptr;

    if (extension == ".flac")
    {
        format = m_formatManager.findFormatForFileExtension("flac");
        if (format == nullptr)
        {
            setError("FLAC encoder not available.\n\n"
                    "JUCE's built-in FLAC encoder should be available, but was not found.\n"
                    "Please check your JUCE installation.");
            return false;
        }
    }
    else if (extension == ".ogg")
    {
        format = m_formatManager.findFormatForFileExtension("ogg");
        if (format == nullptr)
        {
            setError("OGG Vorbis encoder not available.\n\n"
                    "JUCE's built-in OGG encoder should be available, but was not found.\n"
                    "Please check your JUCE installation.");
            return false;
        }
    }
    else if (extension == ".mp3")
    {
        format = m_formatManager.findFormatForFileExtension("mp3");

        if (format == nullptr)
        {
            setError("MP3 encoder not available.\n\n"
                    "JUCE requires LAME to be installed for MP3 encoding.\n"
                    "Install LAME via:\n"
                    "  macOS: brew install lame\n"
                    "  Linux: apt-get install libmp3lame-dev\n"
                    "  Windows: Download from lame.sourceforge.io");
            return false;
        }
    }
    else
    {
        setError("Unsupported audio format: " + extension + "\nSupported formats: .wav, .flac, .ogg, .mp3");
        return false;
    }

    if (format == nullptr)
    {
        setError("No encoder found for format: " + extension);
        return false;
    }

    // Delete existing file if present (JUCE won't overwrite)
    if (file.existsAsFile())
    {
        if (!file.deleteFile())
        {
            setError("Could not delete existing file: " + file.getFullPathName());
            return false;
        }
    }

    // Create output stream
    std::unique_ptr<juce::FileOutputStream> outputStream(new juce::FileOutputStream(file));
    if (!outputStream->openedOk())
    {
        setError("Could not create output stream for file: " + file.getFullPathName());
        return false;
    }

    // Create audio format writer
    // Note: For compressed formats, bitDepth is typically ignored, and qualityOptionIndex is used instead
    std::unique_ptr<juce::AudioFormatWriter> writer;

    // FLAC, OGG, and MP3 use quality settings (0-10)
    // Clamp quality to valid range
    int quality = juce::jlimit(0, 10, qualityOptionIndex);

    DBG("Using quality setting: " + juce::String(quality) + " for " + extension);

    // For MP3, quality maps to bitrate (approximate):
    // 0 = ~64kbps, 5 = ~128kbps (default), 10 = ~320kbps
    if (extension == ".mp3")
    {
        DBG("MP3 quality " + juce::String(quality) +
                                " (approximate bitrate: " +
                                juce::String(64 + quality * 25) + " kbps)");
    }

    // IMPORTANT: Release ownership BEFORE createWriterFor
    // JUCE's createWriterFor may delete the stream on failure,
    // which would cause double-free if unique_ptr also tries to delete
    auto* rawStream = outputStream.release();

    writer.reset(format->createWriterFor(rawStream,
                                         sampleRate,
                                         static_cast<unsigned int>(buffer.getNumChannels()),
                                         24, // Use 24-bit for best quality with compressed formats
                                         {},  // No metadata support for FLAC/OGG via JUCE writer
                                         quality));

    if (writer == nullptr)
    {
        // Note: stream was deleted by createWriterFor on failure
        setError("Could not create audio writer for format: " + extension);
        return false;
    }

    // Write audio data
    bool writeSuccess = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

    // Close writer (flushes data)
    writer.reset();

    if (!writeSuccess)
    {
        setError("Failed to write audio data to file: " + file.getFullPathName());
        return false;
    }

    DBG("Successfully saved audio file: " + file.getFullPathName());
    return true;
}

//==============================================================================
// Sample Rate Conversion
//==============================================================================

juce::AudioBuffer<float> AudioFileManager::resampleBuffer(const juce::AudioBuffer<float>& sourceBuffer,
                                                           double sourceSampleRate,
                                                           double targetSampleRate)
{
    // If sample rates match, return copy of source
    if (std::abs(sourceSampleRate - targetSampleRate) < 0.01)
    {
        juce::AudioBuffer<float> copy(sourceBuffer.getNumChannels(), sourceBuffer.getNumSamples());
        for (int ch = 0; ch < sourceBuffer.getNumChannels(); ++ch)
            copy.copyFrom(ch, 0, sourceBuffer, ch, 0, sourceBuffer.getNumSamples());
        return copy;
    }

    // Calculate resampling ratio and target length
    double ratio = targetSampleRate / sourceSampleRate;
    int targetNumSamples = (int)(sourceBuffer.getNumSamples() * ratio);

    // Create output buffer
    juce::AudioBuffer<float> targetBuffer(sourceBuffer.getNumChannels(), targetNumSamples);
    targetBuffer.clear();

    // Use JUCE's LagrangeInterpolator for high-quality resampling
    // Process each channel independently
    for (int ch = 0; ch < sourceBuffer.getNumChannels(); ++ch)
    {
        juce::LagrangeInterpolator interpolator;
        interpolator.reset();

        const float* sourceData = sourceBuffer.getReadPointer(ch);
        float* targetData = targetBuffer.getWritePointer(ch);

        // CRITICAL: LagrangeInterpolator speed ratio is SOURCE/TARGET (inverse of our ratio)
        // For downsampling 96kHz->8kHz: speed = 96000/8000 = 12.0 (consume input faster)
        // For upsampling 8kHz->96kHz: speed = 8000/96000 = 0.0833 (consume input slower)
        const double speedRatio = sourceSampleRate / targetSampleRate;
        const int totalInputSamples = sourceBuffer.getNumSamples();

        // Resample the whole channel in a SINGLE process() call. process()
        // reads/writes the caller's buffers directly (no internal allocation),
        // so there is no memory benefit to chunking by output -- and chunking
        // re-seeds the interpolator's internal filter history at every chunk
        // boundary, which produced audible discontinuities (the earlier bug).
        // One call keeps the interpolator state continuous across the buffer.
        interpolator.process(
            speedRatio,             // source/target ratio
            sourceData,             // full input
            targetData,             // full output
            targetNumSamples,       // total output samples to produce
            totalInputSamples,      // input samples available
            0);                     // no wraparound
    }

    DBG("Resampled audio: " + juce::String(sourceSampleRate, 0) +
                             " Hz → " + juce::String(targetSampleRate, 0) + " Hz (" +
                             juce::String(sourceBuffer.getNumSamples()) + " → " +
                             juce::String(targetNumSamples) + " samples)");

    return targetBuffer;
}
