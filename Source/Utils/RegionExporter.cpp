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
#include <algorithm>
#include <climits>
#include <set>

namespace
{
    // H16: Windows reserved device names (case-insensitive, with or without an
    // extension). createOutputStream() fails for these on Windows, so a region
    // named e.g. "CON" or "com1" would silently fail to export. We escape the
    // stem by prefixing an underscore. The filename here is the FULL name
    // (already includes ".wav"); we test the stem only.
    bool isWindowsReservedStem(const juce::String& stem)
    {
        static const char* kReserved[] = {
            "CON", "PRN", "AUX", "NUL",
            "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
        };

        const juce::String upper = stem.toUpperCase();
        for (const char* name : kReserved)
            if (upper == name)
                return true;

        return false;
    }

    // Escapes a generated "<stem>.wav" filename so its stem is never a Windows
    // reserved device name.
    juce::String escapeReservedFilename(const juce::String& filename)
    {
        const juce::String stem = filename.upToLastOccurrenceOf(".", false, false);
        if (isWindowsReservedStem(stem))
            return "_" + filename;
        return filename;
    }

    // H15: returns a collision-free filename within @p used by appending a
    // numeric suffix (_1, _2, ...) before the extension when needed. The
    // chosen name is recorded so subsequent calls keep deduping.
    juce::String makeUniqueFilename(const juce::String& filename,
                                    std::set<juce::String>& used)
    {
        // Case-insensitive collision check (Windows/macOS filesystems are
        // commonly case-insensitive).
        auto key = [](const juce::String& s) { return s.toLowerCase(); };

        if (used.find(key(filename)) == used.end())
        {
            used.insert(key(filename));
            return filename;
        }

        const juce::String stem = filename.upToLastOccurrenceOf(".", false, false);
        const juce::String ext  = filename.fromLastOccurrenceOf(".", true, false); // includes dot

        for (int suffix = 1; suffix < INT_MAX; ++suffix)
        {
            juce::String candidate = stem + "_" + juce::String(suffix) + ext;
            if (used.find(key(candidate)) == used.end())
            {
                used.insert(key(candidate));
                return candidate;
            }
        }

        // Practically unreachable; fall back to the original.
        return filename;
    }
}

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
        DBG("RegionExporter: No regions to export");
        return 0;
    }

    if (!settings.outputDirectory.exists() || !settings.outputDirectory.isDirectory())
    {
        DBG("RegionExporter: Invalid output directory");
        return -1;
    }

    int successCount = 0;

    // H15: track chosen filenames so two regions sanitizing to the same name
    // get _1/_2 suffixes instead of silently overwriting each other.
    std::set<juce::String> usedFilenames;

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
                DBG("RegionExporter: Export cancelled by user");
                break;
            }
        }

        // Generate filename using full settings (supports templates), then
        // escape Windows-reserved stems (H16) and dedupe collisions (H15).
        juce::String filename = generateFilename(sourceFile, *region, i, settings);
        filename = escapeReservedFilename(filename);
        filename = makeUniqueFilename(filename, usedFilenames);

        juce::File outputFile = settings.outputDirectory.getChildFile(filename);

        // Export region
        juce::String errorMessage;
        bool success = exportSingleRegion(buffer, sampleRate, *region,
                                          outputFile, settings.bitDepth,
                                          errorMessage);

        if (success)
        {
            ++successCount;
            DBG("RegionExporter: Exported region " +
                                     juce::String(i + 1) + "/" + juce::String(numRegions) +
                                     " - " + outputFile.getFileName());
        }
        else
        {
            DBG("RegionExporter: Failed to export region " +
                                     juce::String(i + 1) + ": " + errorMessage);
        }
    }

    return successCount;
}

juce::String RegionExporter::generateFilename(const juce::File& sourceFile,
                                               const Region& region,
                                               int regionIndex,
                                               const ExportSettings& settings)
{
    // Base filename (without extension)
    juce::String baseName = sourceFile.getFileNameWithoutExtension();

    // Sanitize region name for filename (replace invalid characters)
    juce::String regionName = region.getName();
    regionName = regionName.replaceCharacters("/\\:*?\"<>|", "_________");

    // Prepare index strings (1-based for user-friendliness)
    int index1Based = regionIndex + 1;
    juce::String indexStr = juce::String(index1Based);
    juce::String paddedIndexStr = juce::String(index1Based).paddedLeft('0', 3);

    juce::String filename;

    // Check if custom template is provided
    juce::String customTemplate = settings.customTemplate.trim();

    if (customTemplate.isNotEmpty())
    {
        // Use template system with placeholder replacement
        filename = customTemplate;
        filename = filename.replace("{basename}", baseName);
        filename = filename.replace("{region}", regionName);
        filename = filename.replace("{index}", indexStr);
        filename = filename.replace("{N}", paddedIndexStr);
    }
    else
    {
        // Legacy naming for backward compatibility
        filename = baseName;

        if (settings.includeRegionName && regionName.isNotEmpty())
        {
            filename += "_" + regionName;
        }

        // Handle suffix placement relative to index
        juce::String suffix = settings.suffix.trim();

        if (settings.suffixBeforeIndex && suffix.isNotEmpty())
        {
            // Add suffix BEFORE index
            filename += "_" + suffix;
        }

        if (settings.includeIndex)
        {
            // Use padded or non-padded index based on settings
            filename += settings.usePaddedIndex ? "_" + paddedIndexStr : "_" + indexStr;
        }

        if (!settings.suffixBeforeIndex && suffix.isNotEmpty())
        {
            // Add suffix AFTER index (default behavior)
            filename += "_" + suffix;
        }
    }

    // Apply prefix (applies to both template and legacy modes)
    juce::String prefix = settings.prefix.trim();

    if (prefix.isNotEmpty())
    {
        filename = prefix + "_" + filename;
    }

    // Add .wav extension
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

    // Calculate region length (kept in int64 to avoid the narrowing that
    // produced a negative buffer size / crash on very long regions - C17).
    const int64_t regionLength = endSample - startSample;
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

    // C17: stream the region to disk in chunks instead of allocating one
    // int-sized temporary buffer. This both avoids the int64->int overflow
    // (a region longer than INT_MAX samples wrapped negative) and keeps peak
    // memory bounded for long-form files. We write directly from a view into
    // the source buffer per chunk, so no full copy is made.
    const int kChunkSamples = 1 << 20;  // ~1M samples per write
    const int numChannels = buffer.getNumChannels();

    bool writeSuccess = true;
    int64_t written = 0;

    while (written < regionLength && writeSuccess)
    {
        const int thisChunk =
            static_cast<int>(std::min<int64_t>(kChunkSamples, regionLength - written));

        // Build a lightweight buffer that points at the source samples for
        // this chunk (no deep copy). setDataToReferTo needs a channel pointer
        // array offset to (startSample + written).
        juce::AudioBuffer<float> chunk(numChannels, thisChunk);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            chunk.copyFrom(ch, 0, buffer, ch,
                           static_cast<int>(startSample + written), thisChunk);
        }

        writeSuccess = writer->writeFromAudioSampleBuffer(chunk, 0, thisChunk);
        written += thisChunk;
    }

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

    // Create output stream - need OutputStream base type for JUCE 8 API
    std::unique_ptr<juce::OutputStream> outputStream(outputFile.createOutputStream());
    if (outputStream == nullptr)
    {
        DBG("RegionExporter: Failed to create output stream for " +
                                 outputFile.getFullPathName());
        return nullptr;
    }

    // Determine bits per sample (8, 16, 24, or 32-bit)
    int bitsPerSample = bitDepth;
    if (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32)
    {
        DBG("RegionExporter: Invalid bit depth " +
                                 juce::String(bitDepth) + ", using 24-bit");
        bitsPerSample = 24;
    }

    // Build writer options using JUCE 8 AudioFormatWriterOptions API
    auto options = juce::AudioFormatWriterOptions()
        .withSampleRate(sampleRate)
        .withNumChannels(numChannels)
        .withBitsPerSample(bitsPerSample)
        .withQualityOptionIndex(0)
        .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::integral);  // Force PCM for 8-bit

    // Create writer using JUCE 8 API
    // Note: outputStream ownership is transferred via non-const lvalue reference
    return wavFormat.createWriterFor(outputStream, options);
}
