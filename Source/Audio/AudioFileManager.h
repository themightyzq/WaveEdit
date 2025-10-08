/*
  ==============================================================================

    AudioFileManager.h
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
 * Manages audio file I/O operations for WaveEdit.
 *
 * Handles:
 * - Loading audio files with validation
 * - Saving audio files in various formats
 * - File format detection and validation
 * - Error handling for corrupted or invalid files
 *
 * Phase 1: WAV files only (16-bit, 24-bit, 32-bit float)
 * Future phases: FLAC, MP3, OGG, AIFF support
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
     * @return true if save succeeded, false otherwise
     */
    bool saveAsWav(const juce::File& file,
                   const juce::AudioBuffer<float>& buffer,
                   double sampleRate,
                   int bitDepth = 16);

    /**
     * Overwrites an existing file with new audio data.
     * This is the "Save" operation (not "Save As").
     *
     * @param file The file to overwrite
     * @param buffer The audio data to save
     * @param sampleRate Sample rate of the audio
     * @param bitDepth Bit depth (16, 24, or 32)
     * @return true if save succeeded, false otherwise
     */
    bool overwriteFile(const juce::File& file,
                       const juce::AudioBuffer<float>& buffer,
                       double sampleRate,
                       int bitDepth);

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

    /**
     * Gets the appropriate audio format for a file extension.
     */
    juce::AudioFormat* getFormatForFile(const juce::File& file);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileManager)
};
