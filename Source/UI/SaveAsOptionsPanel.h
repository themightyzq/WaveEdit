//
// SaveAsOptionsPanel.h
// WaveEdit by ZQ SFX
//
// Copyright (c) 2025 ZQ SFX
// Licensed under GPL v3
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <optional>

/**
 * @brief Save As dialog with format selection and encoding options
 *
 * Modal dialog that collects all save parameters before showing the file chooser.
 * Provides format selection, quality settings, and sample rate conversion options.
 */
class SaveAsOptionsPanel : public juce::Component
{
public:
    /**
     * @brief Save format settings structure
     */
    struct SaveSettings
    {
        juce::File targetFile;        // Target file path
        juce::String format;          // "wav", "flac", "ogg", "mp3"
        int bitDepth;                 // 16, 24, or 32 (WAV only)
        int quality;                  // 0-10 (FLAC/OGG/MP3)
        double targetSampleRate;      // Target sample rate (0 = preserve source)
        bool includeBWFMetadata;      // Include BWF chunk (WAV only)
        bool includeiXMLMetadata;     // Include iXML chunk (WAV only)
    };

    /**
     * @brief Show the Save As dialog and get user settings
     * @param sourceSampleRate The current audio file's sample rate
     * @param sourceChannels The current audio file's channel count
     * @param currentFile The current file (for default name and directory)
     * @return Settings if user clicked Save, nullopt if cancelled
     */
    static std::optional<SaveSettings> showDialog(double sourceSampleRate,
                                                   int sourceChannels,
                                                   const juce::File& currentFile);

    /**
     * @brief Check if MP3 encoder (LAME) is available
     */
    static bool isMP3EncoderAvailable();

private:
    /**
     * @brief Private constructor (use showDialog() instead)
     */
    SaveAsOptionsPanel(double sourceSampleRate, int sourceChannels, const juce::File& currentFile);

    /**
     * @brief Get current settings from UI
     */
    SaveSettings getSettings() const;

    // Component overrides
    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    // UI Components - File selection
    juce::Label m_filenameLabel;
    juce::TextEditor m_filenameEditor;
    juce::TextButton m_browseButton;
    juce::Label m_folderLocationLabel;  // Shows current save directory

    // UI Components - Format options
    juce::Label m_formatLabel;
    juce::ComboBox m_formatDropdown;

    juce::Label m_bitDepthLabel;
    juce::ComboBox m_bitDepthDropdown;

    juce::Label m_sampleRateLabel;
    juce::ComboBox m_sampleRateDropdown;

    juce::Label m_qualityLabel;
    juce::Slider m_qualitySlider;
    juce::Label m_qualityValueLabel;

    juce::ToggleButton m_includeBWFCheckbox;
    juce::ToggleButton m_includeiXMLCheckbox;

    juce::Label m_warningLabel;  // For MP3/LAME warning
    juce::Label m_previewLabel;  // Output format preview

    // Action buttons
    juce::TextButton m_saveButton;
    juce::TextButton m_cancelButton;

    // State
    double m_sourceSampleRate;
    int m_sourceChannels;
    juce::File m_currentFile;
    juce::File m_targetDirectory;
    std::optional<SaveSettings> m_result;
    std::unique_ptr<juce::FileChooser> m_fileChooser;

    // Helpers
    void updateUIForFormat();
    void updatePreview();
    void onBrowseClicked();
    void onSaveClicked();
    void onCancelClicked();
    juce::String formatQualityDescription(int quality) const;
    juce::String estimateFileSize(const juce::String& format, int bitDepth,
                                   double sampleRate, int channels) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaveAsOptionsPanel)
};
