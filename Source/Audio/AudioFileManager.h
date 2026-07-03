/*
  ==============================================================================

    AudioFileManager.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

/**
 * Audio file format information structure.
 */
struct AudioFileInfo
{
    juce::String filename;
    juce::String fullPath;
    double sampleRate;
    int numChannels;
    int bitDepth;
    juce::int64 lengthInSamples;
    double lengthInSeconds;
    juce::String formatName;
    juce::StringPairArray metadata;  // BWF and other metadata

    AudioFileInfo()
        : sampleRate(0.0),
          numChannels(0),
          bitDepth(0),
          lengthInSamples(0),
          lengthInSeconds(0.0)
    {
    }
};

/**
 * A single-point cue destined for (or read from) a WAV RIFF "cue " point with
 * a matching "labl". Corresponds to a WaveEdit Marker. Name is ASCII/Latin-1
 * (labl is not Unicode); callers transliterate before writing.
 */
struct WavCueMarker
{
    juce::String name;
    juce::int64 position = 0;  // sample offset within the data chunk
};

/**
 * A ranged cue destined for (or read from) a WAV "cue " point plus an "ltxt"
 * of purpose 'rgn '. Corresponds to a WaveEdit Region.
 */
struct WavCueRegion
{
    juce::String name;
    juce::int64 start = 0;   // sample offset of the region start
    juce::int64 length = 0;  // region length in samples (ltxt dwSampleLength)
};

/**
 * The marker/region payload embedded in (or extracted from) a WAV file's
 * RIFF cue + LIST-adtl chunks.
 */
struct WavCueData
{
    juce::Array<WavCueMarker> markers;
    juce::Array<WavCueRegion> regions;

    bool isEmpty() const { return markers.isEmpty() && regions.isEmpty(); }
};

/**
 * Manages audio file I/O operations for WaveEdit.
 *
 * Handles:
 * - Loading audio files with validation
 * - Saving audio files in various formats
 * - File format detection and validation
 * - Error handling for corrupted or invalid files
 *
 * Supported Formats:
 * - WAV (8-bit, 16-bit, 24-bit, 32-bit float)
 * - FLAC (lossless compression)
 * - OGG Vorbis (lossy compression)
 * - MP3 (read-only - JUCE limitation)
 */
class AudioFileManager
{
public:
    AudioFileManager();
    ~AudioFileManager();

    //==============================================================================
    // File Information

    /**
     * Gets detailed information about an audio file without loading it.
     *
     * @param file The file to inspect
     * @param outInfo Structure to fill with file information
     * @return true if file info was successfully retrieved, false otherwise
     */
    bool getFileInfo(const juce::File& file, AudioFileInfo& outInfo);

    /**
     * Validates that a file is a supported audio file format.
     *
     * @param file The file to validate
     * @return true if file is valid and supported, false otherwise
     */
    bool isValidAudioFile(const juce::File& file);

    /**
     * Gets a list of supported file extensions.
     *
     * @return String containing supported extensions (e.g., "*.wav")
     */
    juce::String getSupportedExtensions() const;

    //==============================================================================
    // File Loading

    /**
     * Loads an audio file into an audio buffer.
     * This is used for editing operations.
     *
     * @param file The file to load
     * @param outBuffer Buffer to fill with audio data
     * @return true if load succeeded, false otherwise
     */
    bool loadIntoBuffer(const juce::File& file, juce::AudioBuffer<float>& outBuffer);

    /**
     * Loads an audio file into a buffer WITHOUT the supported-sample-rate
     * whitelist that loadIntoBuffer applies. Used for crash recovery: the
     * app must be able to reload an auto-save it wrote itself even if that
     * file's sample rate is outside the normal open-validation whitelist
     * (M6). Still requires a decodable file (a valid reader).
     *
     * @return true on success; outBuffer is left untouched on failure.
     */
    bool loadIntoBufferUnchecked(const juce::File& file, juce::AudioBuffer<float>& outBuffer);

    /**
     * Creates an audio format reader for streaming playback.
     * Caller takes ownership of the returned pointer.
     *
     * @param file The file to create a reader for
     * @return Audio format reader, or nullptr on failure
     */
    juce::AudioFormatReader* createReaderFor(const juce::File& file);

    //==============================================================================
    // File Saving

    /**
     * Saves an audio buffer to a WAV file.
     *
     * @param file The destination file
     * @param buffer The audio data to save
     * @param sampleRate Sample rate of the audio
     * @param bitDepth Bit depth (16, 24, or 32)
     * @param metadata Optional BWF metadata to embed in file
     * @return true if save succeeded, false otherwise
     */
    bool saveAsWav(const juce::File& file,
                   const juce::AudioBuffer<float>& buffer,
                   double sampleRate,
                   int bitDepth = 16,
                   const juce::StringPairArray& metadata = {});

    /**
     * Overwrites an existing file with new audio data.
     * This is the "Save" operation (not "Save As").
     *
     * @param file The file to overwrite
     * @param buffer The audio data to save
     * @param sampleRate Sample rate of the audio
     * @param bitDepth Bit depth (16, 24, or 32)
     * @param metadata Optional BWF metadata to embed in file
     * @return true if save succeeded, false otherwise
     */
    bool overwriteFile(const juce::File& file,
                       const juce::AudioBuffer<float>& buffer,
                       double sampleRate,
                       int bitDepth,
                       const juce::StringPairArray& metadata = {});

    /**
     * Appends an iXML chunk to an existing WAV file.
     * Must be called AFTER the file has been written with JUCE's writer.
     *
     * @param file The WAV file to append the chunk to
     * @param ixmlData The iXML metadata as XML string
     * @return true if chunk was successfully appended, false otherwise
     */
    bool appendiXMLChunk(const juce::File& file, const juce::String& ixmlData);

    /**
     * Reads an iXML chunk from a WAV file.
     * JUCE doesn't read custom chunks, so we must read them manually.
     *
     * @param file The WAV file to read from
     * @param outData String to fill with iXML data
     * @return true if iXML chunk was found and read, false otherwise
     */
    bool readiXMLChunk(const juce::File& file, juce::String& outData);

    //==============================================================================
    // WAV cue / adtl chunk embedding (markers + regions)
    //
    // Implemented in AudioFileManager_Cues.cpp. Composes with appendiXMLChunk /
    // the BWF bext chunk: the writer copies every existing chunk EXCEPT a prior
    // "cue " / LIST-adtl (which it replaces), so bext, INFO and iXML survive.

    /**
     * Embeds the given markers and regions into an existing WAV file as RIFF
     * "cue " + LIST-adtl chunks (Sound Forge compatible: one cue point per
     * marker, one cue point + ltxt of purpose 'rgn ' per region, labl for every
     * name). Any pre-existing cue/adtl chunks are REPLACED, never duplicated.
     * Odd-sized chunks are word-aligned and the RIFF size is fixed up. Writes
     * atomically via a sibling temp file.
     *
     * If @p data is empty and the file has no existing cue/adtl chunks this is a
     * no-op (returns true without rewriting).
     *
     * @param wavFile The WAV file to embed into (must already exist).
     * @param data    Markers and regions to embed (names should be ASCII).
     * @return true on success, false on error (see getLastError()).
     */
    bool writeCueChunks(const juce::File& wavFile, const WavCueData& data);

    /**
     * Parses a WAV file's embedded cue + LIST-adtl chunks into markers and
     * regions. A cue point with a matching nonzero-length 'rgn ' ltxt becomes a
     * region; a cue point without one becomes a marker. Names come from labl.
     *
     * @param wavFile  The WAV file to read.
     * @param outData  Filled with the parsed markers/regions (cleared first).
     * @return true if any cue data was found, false otherwise / on error.
     */
    bool readCueChunks(const juce::File& wavFile, WavCueData& outData);

    /**
     * Saves an audio buffer to any supported format (WAV, FLAC, OGG, MP3).
     * Format is auto-detected from file extension.
     *
     * @param file The destination file
     * @param buffer The audio data to save
     * @param sampleRate Sample rate of the audio
     * @param bitDepth Bit depth (16, 24, or 32) - ignored for compressed formats
     * @param qualityOptionIndex Quality setting (0-10) for compressed formats, ignored for WAV
     * @param metadata Optional metadata to embed (WAV only)
     * @return true if save succeeded, false otherwise
     */
    bool saveAudioFile(const juce::File& file,
                       const juce::AudioBuffer<float>& buffer,
                       double sampleRate,
                       int bitDepth = 16,
                       int qualityOptionIndex = 5,
                       const juce::StringPairArray& metadata = {});

    //==============================================================================
    // Error Handling

    /**
     * Gets the last error message from a failed operation.
     *
     * @return Error message string
     */
    juce::String getLastError() const;

    /**
     * Clears the last error message.
     */
    void clearError();

    //==============================================================================
    // Sample Rate Conversion

    /**
     * Resamples an audio buffer to a different sample rate using high-quality interpolation.
     *
     * @param sourceBuffer The input audio buffer to resample
     * @param sourceSampleRate The current sample rate of the audio
     * @param targetSampleRate The desired output sample rate
     * @return New buffer with resampled audio
     */
    static juce::AudioBuffer<float> resampleBuffer(const juce::AudioBuffer<float>& sourceBuffer,
                                                    double sourceSampleRate,
                                                    double targetSampleRate);

private:
    //==============================================================================
    // Private Members

    juce::AudioFormatManager m_formatManager;
    juce::String m_lastError;

    //==============================================================================
    // Private Methods

    /**
     * Sets the last error message.
     */
    void setError(const juce::String& errorMessage);

    /**
     * Validates audio buffer parameters before saving.
     */
    bool validateBufferForSaving(const juce::AudioBuffer<float>& buffer,
                                  double sampleRate,
                                  int bitDepth);

private:
    /**
     * Gets the appropriate audio format for a file extension.
     */
    juce::AudioFormat* getFormatForFile(const juce::File& file);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileManager)
};
