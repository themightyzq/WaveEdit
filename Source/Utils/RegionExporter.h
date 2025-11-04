/*
  ==============================================================================

    RegionExporter.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "RegionManager.h"

/**
 * Utility class for exporting regions as separate audio files.
 *
 * Handles:
 * - Extracting audio data for each region from the main buffer
 * - Writing regions to separate WAV files
 * - Filename generation based on templates
 * - Progress reporting via callback
 * - Error handling and validation
 *
 * Thread Safety: All methods must be called from the message thread.
 */
class RegionExporter
{
public:
    /**
     * Export settings structure.
     */
    struct ExportSettings
    {
        juce::File outputDirectory;      // Where to save files
        bool includeRegionName;          // Include region name in filename
        bool includeIndex;               // Include region index in filename
        int bitDepth;                    // Bit depth (16, 24, 32)
        juce::String customTemplate;     // Custom filename template (e.g., "{basename}_{region}_{index}")
        juce::String prefix;             // Prefix to add to filenames
        juce::String suffix;             // Suffix to add before extension
        bool usePaddedIndex;             // Use padded index (001 vs 1)
        bool suffixBeforeIndex;          // Place suffix before index (true) or after (false)

        ExportSettings()
            : includeRegionName(true),
              includeIndex(true),
              bitDepth(24),
              usePaddedIndex(false),
              suffixBeforeIndex(false)
        {
        }
    };

    /**
     * Progress callback function type.
     * Parameters: current region index, total regions, current region name
     * Returns: true to continue, false to cancel export
     */
    using ProgressCallback = std::function<bool(int, int, const juce::String&)>;

    /**
     * Exports all regions from the audio buffer to separate files.
     *
     * @param buffer The audio buffer containing the complete audio data
     * @param sampleRate Sample rate of the audio
     * @param regionManager Region manager containing regions to export
     * @param sourceFile Original source file (for base filename)
     * @param settings Export settings (output directory, naming options, etc.)
     * @param progressCallback Optional callback for progress updates
     * @return Number of regions successfully exported, -1 on error
     */
    static int exportRegions(const juce::AudioBuffer<float>& buffer,
                             double sampleRate,
                             const RegionManager& regionManager,
                             const juce::File& sourceFile,
                             const ExportSettings& settings,
                             ProgressCallback progressCallback = nullptr);

    /**
     * Generates filename for a region based on naming template.
     *
     * Supports template placeholders:
     * - {basename}: Original filename without extension
     * - {region}: Region name (sanitized)
     * - {index}: Region index (1-based, non-padded)
     * - {N}: Region index (1-based, zero-padded to 3 digits)
     *
     * Also applies prefix, suffix, and padded index settings.
     *
     * @param sourceFile Original source file
     * @param region The region
     * @param regionIndex Region index (0-based)
     * @param settings Export settings including template, prefix, suffix options
     * @return Generated filename (without directory path)
     */
    static juce::String generateFilename(const juce::File& sourceFile,
                                          const Region& region,
                                          int regionIndex,
                                          const ExportSettings& settings);

    /**
     * Exports a single region to a file.
     *
     * @param buffer The complete audio buffer
     * @param sampleRate Sample rate
     * @param region The region to export
     * @param outputFile Output file path
     * @param bitDepth Bit depth (16, 24, 32)
     * @param errorMessage Set to error message if export fails
     * @return true if successful, false on error
     */
    static bool exportSingleRegion(const juce::AudioBuffer<float>& buffer,
                                    double sampleRate,
                                    const Region& region,
                                    const juce::File& outputFile,
                                    int bitDepth,
                                    juce::String& errorMessage);

private:
    /**
     * Creates an audio format writer for WAV files.
     *
     * @param outputFile Output file
     * @param sampleRate Sample rate
     * @param numChannels Number of channels
     * @param bitDepth Bit depth (16, 24, 32)
     * @return Unique pointer to writer, or nullptr on failure
     */
    static std::unique_ptr<juce::AudioFormatWriter> createWavWriter(
        const juce::File& outputFile,
        double sampleRate,
        int numChannels,
        int bitDepth);

    JUCE_DECLARE_NON_COPYABLE(RegionExporter)
};
