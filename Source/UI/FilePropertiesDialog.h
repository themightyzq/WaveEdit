/*
  ==============================================================================

    FilePropertiesDialog.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/Document.h"

/**
 * File Properties dialog for WaveEdit.
 *
 * Displays comprehensive file and audio information:
 * - File metadata (name, path, size, dates)
 * - Audio format (sample rate, bit depth, channels)
 * - Duration (formatted as HH:MM:SS.mmm)
 * - Codec information (PCM, IEEE float, etc.)
 *
 * Accessed via Alt+Enter keyboard shortcut.
 */
class FilePropertiesDialog : public juce::Component,
                             public juce::Button::Listener
{
public:
    /**
     * Constructor.
     *
     * @param document Reference to the document whose properties to display
     */
    explicit FilePropertiesDialog(const Document& document);
    ~FilePropertiesDialog() override;

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // Button::Listener overrides

    void buttonClicked(juce::Button* button) override;

    //==============================================================================
    // Static factory method

    /**
     * Shows the file properties dialog as a modal window.
     *
     * @param parentComponent Parent component to center the dialog over
     * @param document Reference to the document whose properties to display
     */
    static void showDialog(juce::Component* parentComponent, const Document& document);

private:
    //==============================================================================
    // Helper methods

    /**
     * Populates all property labels from the document.
     */
    void loadProperties(const Document& document);

    /**
     * Formats a duration in seconds as HH:MM:SS.mmm.
     *
     * @param durationSeconds Duration in seconds
     * @return Formatted time string
     */
    static juce::String formatDuration(double durationSeconds);

    /**
     * Formats file size in human-readable format (KB, MB, GB).
     *
     * @param bytes File size in bytes
     * @return Formatted size string
     */
    static juce::String formatFileSize(int64_t bytes);

    /**
     * Formats a JUCE Time object as a readable date/time string.
     *
     * @param time The time to format
     * @return Formatted date/time string
     */
    static juce::String formatDateTime(const juce::Time& time);

    /**
     * Determines the audio codec/format description.
     *
     * @param file The audio file
     * @param bitDepth The bit depth
     * @return Codec description (e.g., "PCM 16-bit", "IEEE Float 32-bit")
     */
    static juce::String determineCodec(const juce::File& file, int bitDepth);

    //==============================================================================
    // UI Components

    // Property labels (left column - property names)
    juce::Label m_filenameLabel;
    juce::Label m_filePathLabel;
    juce::Label m_fileSizeLabel;
    juce::Label m_dateCreatedLabel;
    juce::Label m_dateModifiedLabel;

    juce::Label m_sampleRateLabel;
    juce::Label m_bitDepthLabel;
    juce::Label m_channelsLabel;
    juce::Label m_durationLabel;
    juce::Label m_codecLabel;

    // Value labels (right column - property values)
    juce::Label m_filenameValue;
    juce::Label m_filePathValue;
    juce::Label m_fileSizeValue;
    juce::Label m_dateCreatedValue;
    juce::Label m_dateModifiedValue;

    juce::Label m_sampleRateValue;
    juce::Label m_bitDepthValue;
    juce::Label m_channelsValue;
    juce::Label m_durationValue;
    juce::Label m_codecValue;

    // Close button
    juce::TextButton m_closeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilePropertiesDialog)
};
