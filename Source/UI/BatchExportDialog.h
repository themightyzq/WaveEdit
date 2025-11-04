/*
  ==============================================================================

    BatchExportDialog.h
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
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Utils/RegionManager.h"

/**
 * Modal dialog for batch exporting regions as separate audio files.
 *
 * Features:
 * - Directory selection for output files
 * - File naming template: {filename}_{regionName}_{index}.wav
 * - Option to include/exclude region names in filenames
 * - Preview list showing resulting filenames
 * - Progress bar during export
 *
 * Workflow:
 * 1. User creates regions (manually or via Strip Silence)
 * 2. User triggers Cmd+Shift+E or File → Export Regions As Files...
 * 3. Dialog shows:
 *    - Output directory picker
 *    - Naming options (include region name checkbox)
 *    - Preview list of output files
 * 4. User clicks Export
 * 5. Progress bar shows export status
 * 6. Dialog closes on completion
 *
 * Use Cases:
 * - Podcast: Export intro, interview, ads, outro as separate files
 * - Game Audio: Export sound effect variations for batch processing
 * - Voice Acting: Export different takes/lines for editing
 */
class BatchExportDialog : public juce::Component
{
public:
    /**
     * Export result structure.
     */
    struct ExportSettings
    {
        juce::File outputDirectory;      // Where to save files
        bool includeRegionName;          // Include region name in filename
        bool includeIndex;               // Include region index in filename
        juce::String customTemplate;     // Custom filename template (e.g., "{basename}_{region}_{index}")
        juce::String prefix;             // Prefix to add to filenames
        juce::String suffix;             // Suffix to add before extension
        bool usePaddedIndex;             // Use padded index (001 vs 1)
        bool suffixBeforeIndex;          // Place suffix before index (true) or after (false)

        ExportSettings()
            : includeRegionName(true),
              includeIndex(true),
              usePaddedIndex(false),
              suffixBeforeIndex(false)
        {
        }
    };

    /**
     * Creates the batch export dialog.
     *
     * @param sourceFile The original audio file (for base filename)
     * @param regionManager The region manager containing regions to export
     */
    BatchExportDialog(const juce::File& sourceFile, const RegionManager& regionManager);

    ~BatchExportDialog() override = default;

    /**
     * Shows the dialog modally and returns the user's export settings.
     *
     * @param sourceFile The original audio file
     * @param regionManager The region manager containing regions
     * @return std::optional<ExportSettings> containing settings if user clicked Export,
     *         std::nullopt if user clicked Cancel
     */
    static std::optional<ExportSettings> showDialog(const juce::File& sourceFile,
                                                     const RegionManager& regionManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    /**
     * Generates preview filename for a region.
     *
     * @param regionIndex Region index (0-based)
     * @param region The region
     * @return Preview filename string
     */
    juce::String generatePreviewFilename(int regionIndex, const Region& region) const;

    /**
     * Updates the preview list with generated filenames.
     */
    void updatePreviewList();

    /**
     * Called when the output directory browse button is clicked.
     */
    void onBrowseClicked();

    /**
     * Called when naming options change.
     */
    void onNamingOptionChanged();

    /**
     * Called when output directory text changes.
     */
    void onOutputDirTextChanged();

    /**
     * Called when template text changes.
     */
    void onTemplateTextChanged();

    /**
     * Called when Export button is clicked.
     */
    void onExportClicked();

    /**
     * Called when Cancel button is clicked.
     */
    void onCancelClicked();

    /**
     * Validates that export can proceed.
     *
     * @return true if ready to export, false if validation fails
     */
    bool validateExport();

    // UI Components
    juce::Label m_titleLabel;
    juce::Label m_outputDirLabel;
    juce::TextEditor m_outputDirEditor;
    juce::TextButton m_browseButton;

    juce::Label m_namingOptionsLabel;
    juce::ToggleButton m_includeRegionNameToggle;
    juce::ToggleButton m_includeIndexToggle;

    juce::Label m_templateLabel;
    juce::TextEditor m_templateEditor;
    juce::Label m_templateHelpLabel;

    juce::Label m_prefixLabel;
    juce::TextEditor m_prefixEditor;

    juce::Label m_suffixLabel;
    juce::TextEditor m_suffixEditor;

    juce::ToggleButton m_paddedIndexToggle;
    juce::ToggleButton m_suffixBeforeIndexToggle;

    juce::Label m_previewLabel;
    juce::TextEditor m_previewList;  // Multi-line read-only preview

    juce::TextButton m_exportButton;
    juce::TextButton m_cancelButton;

    // Data
    juce::File m_sourceFile;
    const RegionManager& m_regionManager;
    std::optional<ExportSettings> m_result;
    juce::File m_outputDirectory;

    // File chooser (needs to stay alive during async operation)
    std::unique_ptr<juce::FileChooser> m_fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BatchExportDialog)
};
