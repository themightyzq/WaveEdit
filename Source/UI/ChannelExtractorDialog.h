/*
  ==============================================================================

    ChannelExtractorDialog.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>
#include "../Audio/ChannelLayout.h"

/**
 * Modal dialog for extracting/splitting audio channels to separate files.
 *
 * Features:
 * - Select specific channels to extract via checkboxes
 * - Export as individual mono files or combined multi-channel file
 * - Output directory selection
 * - Filename preview showing what will be created
 *
 * This is a focused tool for channel extraction to files.
 * For in-place channel conversion (downmix/upmix), use ChannelConverterDialog.
 */
class ChannelExtractorDialog : public juce::Component
{
public:
    /**
     * Export mode - how to export extracted channels
     */
    enum class ExportMode
    {
        IndividualMono,    // Each channel → separate mono file
        CombinedMulti      // All selected → single multi-channel file
    };

    /**
     * Export format - file format for extracted audio
     */
    enum class ExportFormat
    {
        WAV = 1,
        FLAC = 2,
        OGG = 3
    };

    /**
     * Result structure containing extraction parameters.
     */
    struct Result
    {
        std::vector<int> channels;      // Channel indices to extract (0-based)
        ExportMode exportMode;
        ExportFormat exportFormat;      // File format for exported audio
        juce::File outputDirectory;
    };

    /**
     * Creates a ChannelExtractorDialog.
     *
     * @param currentChannels Current number of channels in the audio
     * @param sourceFileName Name of the source file (for preview)
     */
    ChannelExtractorDialog(int currentChannels, const juce::String& sourceFileName);
    ~ChannelExtractorDialog() override = default;

    /**
     * Show the dialog modally and return the extraction settings.
     *
     * @param currentChannels Current number of channels
     * @param sourceFileName Name of the source file
     * @return std::optional<Result> containing settings if Apply clicked,
     *         std::nullopt if user cancelled
     */
    static std::optional<Result> showDialog(int currentChannels, const juce::String& sourceFileName);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void populateChannelCheckboxes();
    void onChannelCheckboxChanged();
    void onExportModeChanged();
    void onChooseOutputDirectory();
    void onApplyClicked();
    void onCancelClicked();
    void updateSelectionCount();
    void updateFilenamePreview();

    // UI Components - Header
    juce::Label m_titleLabel;
    juce::Label m_sourceLabel;
    juce::Label m_sourceValueLabel;

    // Channel selection
    juce::GroupComponent m_channelGroup;
    juce::OwnedArray<juce::ToggleButton> m_channelCheckboxes;
    juce::Label m_selectionCountLabel;
    juce::TextButton m_selectAllButton;
    juce::TextButton m_selectNoneButton;

    // Export mode
    juce::Label m_exportModeLabel;
    juce::ToggleButton m_individualFilesButton;
    juce::ToggleButton m_combinedFileButton;

    // Export format
    juce::Label m_formatLabel;
    juce::ComboBox m_formatCombo;

    // Output directory
    juce::Label m_outputLabel;
    juce::TextButton m_outputDirButton;
    juce::Label m_outputDirLabel;

    // Filename preview
    juce::Label m_previewLabel;
    juce::TextEditor m_filenamePreview;

    // Buttons
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;

    // State
    int m_currentChannels;
    juce::String m_sourceFileName;
    juce::File m_outputDirectory;
    std::unique_ptr<juce::FileChooser> m_fileChooser;
    std::optional<Result> m_result;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelExtractorDialog)
};
