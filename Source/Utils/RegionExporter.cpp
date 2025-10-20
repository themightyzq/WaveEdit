/*
  ==============================================================================

    RegionExporter.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "RegionExporter.h"

int RegionExporter::exportRegions(const juce::AudioBuffer<float>& buffer,
                                   double sampleRate,
                                   const RegionManager& regionManager,
                                   const juce::File& sourceFile,
                                   const ExportSettings& settings,
                                   ProgressCallback progressCallback)
{
    int numRegions = regionManager.getNumRegions();

    if (numRegions == 0)
    {
        juce::Logger::writeToLog("RegionExporter: No regions to export");
        return 0;
    }

    if (!settings.outputDirectory.exists() || !settings.outputDirectory.isDirectory())
    {
        juce::Logger::writeToLog("RegionExporter: Invalid output directory");
        return -1;
    }

    int successCount = 0;

    for (int i = 0; i < numRegions; ++i)
    {
        auto* region = regionManager.getRegion(i);
        if (region == nullptr)
        {
            continue;
        }

        // Progress callback
        if (progressCallback != nullptr)
        {
            bool shouldContinue = progressCallback(i, numRegions, region->getName());
            if (!shouldContinue)
            {
                juce::Logger::writeToLog("RegionExporter: Export cancelled by user");
                break;
            }
        }

        // Generate filename
        juce::String filename = generateFilename(sourceFile, *region, i,
                                                  settings.includeRegionName,
                                                  settings.includeIndex);

        juce::File outputFile = settings.outputDirectory.getChildFile(filename);

        // Export region
        juce::String errorMessage;
        bool success = exportSingleRegion(buffer, sampleRate, *region,
                                          outputFile, settings.bitDepth,
                                          errorMessage);

        if (success)
        {
            ++successCount;
            juce::Logger::writeToLog("RegionExporter: Exported region " +
                                     juce::String(i + 1) + "/" + juce::String(numRegions) +
                                     " - " + outputFile.getFileName());
        }
        else
        {
            juce::Logger::writeToLog("RegionExporter: Failed to export region " +
                                     juce::String(i + 1) + ": " + errorMessage);
        }
    }

    return successCount;
}

juce::String RegionExporter::generateFilename(const juce::File& sourceFile,
                                               const Region& region,
                                               int regionIndex,
                                               bool includeRegionName,
                                               bool includeIndex)
{
    // Base filename (without extension)
    juce::String baseName = sourceFile.getFileNameWithoutExtension();

    juce::String filename = baseName;

    if (includeRegionName)
    {
        // Sanitize region name for filename (replace invalid characters)
        juce::String regionName = region.getName();
        regionName = regionName.replaceCharacters("/\\:*?\"<>|", "_________");

        if (regionName.isNotEmpty())
        {
            filename += "_" + regionName;
        }
    }

    if (includeIndex)
    {
        // Add region index (1-based for user-friendliness)
        filename += "_" + juce::String(regionIndex + 1);
    }

    filename += ".wav";

    return filename;
}

bool RegionExporter::exportSingleRegion(const juce::AudioBuffer<float>& buffer,
                                         double sampleRate,
                                         const Region& region,
                                         const juce::File& outputFile,
                                         int bitDepth,
                                         juce::String& errorMessage)
{
    // Validate region bounds
    int64_t startSample = region.getStartSample();
    int64_t endSample = region.getEndSample();
    int64_t totalSamples = buffer.getNumSamples();

    if (startSample < 0 || startSample >= totalSamples)
    {
        errorMessage = "Region start position is out of bounds";
        return false;
    }

    if (endSample < startSample || endSample > totalSamples)
    {
        errorMessage = "Region end position is out of bounds";
        return false;
    }

    // Calculate region length
    auto regionLength = static_cast<int>(endSample - startSample);
    if (regionLength <= 0)
    {
        errorMessage = "Region has invalid length";
        return false;
    }

    // Create audio format writer
    auto writer = createWavWriter(outputFile, sampleRate, buffer.getNumChannels(), bitDepth);
    if (writer == nullptr)
    {
        errorMessage = "Failed to create audio writer for output file";
        return false;
    }

    // Extract region data into temporary buffer
    juce::AudioBuffer<float> regionBuffer(buffer.getNumChannels(), regionLength);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        regionBuffer.copyFrom(ch, 0, buffer, ch,
                              static_cast<int>(startSample), regionLength);
    }

    // Write to file
    bool writeSuccess = writer->writeFromAudioSampleBuffer(regionBuffer, 0, regionLength);

    // Flush and close writer
    writer.reset();

    if (!writeSuccess)
    {
        errorMessage = "Failed to write audio data to file";
        outputFile.deleteFile(); // Clean up partial file
        return false;
    }

    return true;
}

std::unique_ptr<juce::AudioFormatWriter> RegionExporter::createWavWriter(
    const juce::File& outputFile,
    double sampleRate,
    int numChannels,
    int bitDepth)
{
    // Create WAV format
    juce::WavAudioFormat wavFormat;

    // Delete existing file if present
    if (outputFile.existsAsFile())
    {
        outputFile.deleteFile();
    }

    // Create output stream
    auto outputStream = outputFile.createOutputStream();
    if (outputStream == nullptr)
    {
        juce::Logger::writeToLog("RegionExporter: Failed to create output stream for " +
                                 outputFile.getFullPathName());
        return nullptr;
    }

    // Determine bits per sample
    int bitsPerSample = bitDepth;
    if (bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32)
    {
        juce::Logger::writeToLog("RegionExporter: Invalid bit depth " +
                                 juce::String(bitDepth) + ", using 24-bit");
        bitsPerSample = 24;
    }

    // Create writer with options
    // Note: JUCE 7+ deprecates the old createWriterFor() in favor of using AudioFormatWriterOptions
    // For now we suppress the warning until we can properly test the new API
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    juce::StringPairArray metadata;
    auto writer = wavFormat.createWriterFor(outputStream.get(),
                                            sampleRate,
                                            static_cast<unsigned int>(numChannels),
                                            bitsPerSample,
                                            metadata,
                                            0); // quality hint (unused for WAV)

    #pragma GCC diagnostic pop

    if (writer != nullptr)
    {
        outputStream.release(); // Writer now owns the stream
    }

    return std::unique_ptr<juce::AudioFormatWriter>(writer);
}
