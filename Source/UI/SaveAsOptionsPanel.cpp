//
// SaveAsOptionsPanel.cpp
// WaveEdit by ZQ SFX
//
// Copyright (c) 2025 ZQ SFX
// Licensed under GPL v3
//

#include "SaveAsOptionsPanel.h"

// Static method to show dialog
std::optional<SaveAsOptionsPanel::SaveSettings> SaveAsOptionsPanel::showDialog(
    double sourceSampleRate,
    int sourceChannels,
    const juce::File& currentFile)
{
    // CRITICAL: UI components must be created on message thread
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Create dialog content (we keep ownership with unique_ptr)
    std::unique_ptr<SaveAsOptionsPanel> content(new SaveAsOptionsPanel(sourceSampleRate, sourceChannels, currentFile));

    // Create modal dialog window
    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(content.get());  // Don't transfer ownership - prevents use-after-free
    options.dialogTitle = "Save Audio File As";
    options.dialogBackgroundColour = juce::Colours::darkgrey;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.useBottomRightCornerResizer = false;

    // Show modal and wait for result
    options.runModal();

    // Copy result while panel is still alive
    std::optional<SaveSettings> result = content->m_result;

    // unique_ptr destructs panel cleanly here
    return result;
}

SaveAsOptionsPanel::SaveAsOptionsPanel(double sourceSampleRate, int sourceChannels, const juce::File& currentFile)
    : m_sourceSampleRate(sourceSampleRate)
    , m_sourceChannels(sourceChannels)
    , m_currentFile(currentFile)
    , m_targetDirectory(currentFile.existsAsFile()
                        ? currentFile.getParentDirectory()
                        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory))
{
    // Filename input
    m_filenameLabel.setText("Filename:", juce::dontSendNotification);
    m_filenameLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_filenameLabel);

    // Set filename: use current file's name or "Untitled" for new files
    juce::String defaultFilename = currentFile.existsAsFile()
                                   ? currentFile.getFileNameWithoutExtension()
                                   : "Untitled";
    m_filenameEditor.setText(defaultFilename, false);
    m_filenameEditor.setMultiLine(false);
    m_filenameEditor.setReturnKeyStartsNewLine(false);
    m_filenameEditor.onReturnKey = [this]() { onSaveClicked(); };
    addAndMakeVisible(m_filenameEditor);

    m_browseButton.setButtonText("Browse...");
    m_browseButton.onClick = [this]() { onBrowseClicked(); };
    addAndMakeVisible(m_browseButton);

    // Folder location display
    m_folderLocationLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    m_folderLocationLabel.setJustificationType(juce::Justification::centredLeft);
    m_folderLocationLabel.setFont(juce::Font(11.0f));
    m_folderLocationLabel.setText("Save to: " + m_targetDirectory.getFullPathName(), juce::dontSendNotification);
    addAndMakeVisible(m_folderLocationLabel);

    // Format dropdown
    m_formatLabel.setText("Format:", juce::dontSendNotification);
    m_formatLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_formatLabel);

    m_formatDropdown.addItem("WAV (Uncompressed)", 1);
    m_formatDropdown.addItem("FLAC (Lossless)", 2);
    m_formatDropdown.addItem("OGG Vorbis (Lossy)", 3);
    // MP3 encoding not supported by JUCE (decoding only)
    // m_formatDropdown.addItem("MP3 (Lossy)", 4);

    // Set default based on current format
    juce::String ext = currentFile.getFileExtension().toLowerCase();
    if (ext == ".wav" || ext.isEmpty())
        m_formatDropdown.setSelectedId(1, juce::dontSendNotification);
    else if (ext == ".flac")
        m_formatDropdown.setSelectedId(2, juce::dontSendNotification);
    else if (ext == ".ogg")
        m_formatDropdown.setSelectedId(3, juce::dontSendNotification);
    else if (ext == ".mp3")
        m_formatDropdown.setSelectedId(4, juce::dontSendNotification);
    else
        m_formatDropdown.setSelectedId(1, juce::dontSendNotification);

    m_formatDropdown.onChange = [this]() { updateUIForFormat(); updatePreview(); };
    addAndMakeVisible(m_formatDropdown);

    // Bit depth dropdown (WAV only)
    m_bitDepthLabel.setText("Bit Depth:", juce::dontSendNotification);
    m_bitDepthLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_bitDepthLabel);

    m_bitDepthDropdown.addItem("8-bit PCM", 1);
    m_bitDepthDropdown.addItem("16-bit PCM", 2);
    m_bitDepthDropdown.addItem("24-bit PCM", 3);
    m_bitDepthDropdown.addItem("32-bit Float", 4);
    m_bitDepthDropdown.setSelectedId(3, juce::dontSendNotification);  // Default: 24-bit
    m_bitDepthDropdown.onChange = [this]() { updatePreview(); };
    addAndMakeVisible(m_bitDepthDropdown);

    // Sample rate dropdown
    m_sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
    m_sampleRateLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_sampleRateLabel);

    m_sampleRateDropdown.addItem(juce::String(sourceSampleRate, 0) + " Hz (source)", 1);
    m_sampleRateDropdown.addItem("8000 Hz", 2);
    m_sampleRateDropdown.addItem("11025 Hz", 3);
    m_sampleRateDropdown.addItem("16000 Hz", 4);
    m_sampleRateDropdown.addItem("22050 Hz", 5);
    m_sampleRateDropdown.addItem("32000 Hz", 6);
    m_sampleRateDropdown.addItem("44100 Hz", 7);
    m_sampleRateDropdown.addItem("48000 Hz", 8);
    m_sampleRateDropdown.addItem("88200 Hz", 9);
    m_sampleRateDropdown.addItem("96000 Hz", 10);
    m_sampleRateDropdown.addItem("176400 Hz", 11);
    m_sampleRateDropdown.addItem("192000 Hz", 12);
    m_sampleRateDropdown.setSelectedId(1, juce::dontSendNotification);  // Default: source rate
    m_sampleRateDropdown.onChange = [this]() { updatePreview(); };
    addAndMakeVisible(m_sampleRateDropdown);

    // Quality slider (compressed formats only)
    m_qualityLabel.setText("Quality:", juce::dontSendNotification);
    m_qualityLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_qualityLabel);

    m_qualitySlider.setRange(0.0, 10.0, 1.0);
    m_qualitySlider.setValue(10.0, juce::dontSendNotification);  // Default: highest quality
    m_qualitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_qualitySlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_qualitySlider.onValueChange = [this]()
    {
        m_qualityValueLabel.setText(formatQualityDescription((int)m_qualitySlider.getValue()),
                                     juce::dontSendNotification);
        updatePreview();
    };
    addAndMakeVisible(m_qualitySlider);

    m_qualityValueLabel.setText(formatQualityDescription(10), juce::dontSendNotification);
    m_qualityValueLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_qualityValueLabel);

    // Metadata checkboxes (WAV only)
    m_includeBWFCheckbox.setButtonText("Include BWF metadata");
    m_includeBWFCheckbox.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(m_includeBWFCheckbox);

    m_includeiXMLCheckbox.setButtonText("Include iXML metadata");
    m_includeiXMLCheckbox.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(m_includeiXMLCheckbox);

    // Warning label (for MP3 without LAME)
    m_warningLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    m_warningLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_warningLabel);

    // Preview label
    m_previewLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    m_previewLabel.setJustificationType(juce::Justification::centred);
    m_previewLabel.setFont(juce::Font(12.0f, juce::Font::italic));
    addAndMakeVisible(m_previewLabel);

    // Action buttons
    m_saveButton.setButtonText("Save");
    m_saveButton.onClick = [this]() { onSaveClicked(); };
    addAndMakeVisible(m_saveButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    // Initial UI state
    updateUIForFormat();
    updatePreview();

    setSize(500, 400);
}

SaveAsOptionsPanel::SaveSettings SaveAsOptionsPanel::getSettings() const
{
    SaveSettings settings;

    // Format
    int formatId = m_formatDropdown.getSelectedId();
    switch (formatId)
    {
        case 1: settings.format = "wav"; break;
        case 2: settings.format = "flac"; break;
        case 3: settings.format = "ogg"; break;
        case 4: settings.format = "mp3"; break;
        default: settings.format = "wav"; break;
    }

    // Build target file with proper extension
    juce::String filename = m_filenameEditor.getText().trim();
    if (filename.isEmpty())
        filename = m_currentFile.getFileNameWithoutExtension();

    settings.targetFile = m_targetDirectory.getChildFile(filename).withFileExtension(settings.format);

    // Bit depth (WAV only)
    int bitDepthId = m_bitDepthDropdown.getSelectedId();
    switch (bitDepthId)
    {
        case 1: settings.bitDepth = 8; break;
        case 2: settings.bitDepth = 16; break;
        case 3: settings.bitDepth = 24; break;
        case 4: settings.bitDepth = 32; break;
        default: settings.bitDepth = 24; break;
    }

    // Quality (compressed formats)
    settings.quality = (int)m_qualitySlider.getValue();

    // Sample rate
    int rateId = m_sampleRateDropdown.getSelectedId();
    switch (rateId)
    {
        case 1: settings.targetSampleRate = 0.0; break;  // 0 = preserve source
        case 2: settings.targetSampleRate = 8000.0; break;
        case 3: settings.targetSampleRate = 11025.0; break;
        case 4: settings.targetSampleRate = 16000.0; break;
        case 5: settings.targetSampleRate = 22050.0; break;
        case 6: settings.targetSampleRate = 32000.0; break;
        case 7: settings.targetSampleRate = 44100.0; break;
        case 8: settings.targetSampleRate = 48000.0; break;
        case 9: settings.targetSampleRate = 88200.0; break;
        case 10: settings.targetSampleRate = 96000.0; break;
        case 11: settings.targetSampleRate = 176400.0; break;
        case 12: settings.targetSampleRate = 192000.0; break;
        default: settings.targetSampleRate = 0.0; break;
    }

    // Metadata (WAV only)
    settings.includeBWFMetadata = m_includeBWFCheckbox.getToggleState();
    settings.includeiXMLMetadata = m_includeiXMLCheckbox.getToggleState();

    return settings;
}

bool SaveAsOptionsPanel::isMP3EncoderAvailable()
{
    // Check if LAME encoder is available by actually testing writer creation
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();

    // IMPORTANT: registerBasicFormats() doesn't include MP3!
    // We need to manually register MP3AudioFormat
    manager.registerFormat(new juce::MP3AudioFormat(), true);

    // Now check if MP3 format is registered
    auto* mp3Format = manager.findFormatForFileExtension(".mp3");
    if (mp3Format == nullptr)
    {
        juce::Logger::writeToLog("MP3 format not found in AudioFormatManager after manual registration");
        return false;
    }

    // Actually test if we can create a writer (verifies LAME is functional)
    // Use a MemoryOutputStream to avoid file system I/O during detection
    juce::MemoryOutputStream dummyStream;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        mp3Format->createWriterFor(&dummyStream,
                                    44100.0,  // sample rate
                                    2,        // channels
                                    16,       // bits per sample
                                    {},       // metadata
                                    5)        // quality (0-10)
    );

    if (writer == nullptr)
    {
        juce::Logger::writeToLog("MP3 format found but writer creation failed (LAME not available)");
        return false;
    }

    juce::Logger::writeToLog("MP3 encoder available and functional: " + mp3Format->getFormatName());
    return true;
}

void SaveAsOptionsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(15);
    int rowHeight = 28;
    int labelWidth = 100;
    int spacing = 8;

    // Filename row
    auto filenameRow = bounds.removeFromTop(rowHeight);
    m_filenameLabel.setBounds(filenameRow.removeFromLeft(labelWidth));
    filenameRow.removeFromLeft(spacing);
    m_browseButton.setBounds(filenameRow.removeFromRight(80));
    filenameRow.removeFromRight(spacing);
    m_filenameEditor.setBounds(filenameRow);
    bounds.removeFromTop(spacing);

    // Folder location row
    auto folderRow = bounds.removeFromTop(rowHeight - 4);  // Slightly smaller
    folderRow.removeFromLeft(labelWidth + spacing);  // Indent to align with filename editor
    m_folderLocationLabel.setBounds(folderRow);
    bounds.removeFromTop(spacing * 2);

    // Format dropdown
    auto formatRow = bounds.removeFromTop(rowHeight);
    m_formatLabel.setBounds(formatRow.removeFromLeft(labelWidth));
    formatRow.removeFromLeft(spacing);
    m_formatDropdown.setBounds(formatRow);
    bounds.removeFromTop(spacing);

    // Bit depth dropdown (WAV only)
    auto bitDepthRow = bounds.removeFromTop(rowHeight);
    m_bitDepthLabel.setBounds(bitDepthRow.removeFromLeft(labelWidth));
    bitDepthRow.removeFromLeft(spacing);
    m_bitDepthDropdown.setBounds(bitDepthRow);
    bounds.removeFromTop(spacing);

    // Sample rate dropdown
    auto sampleRateRow = bounds.removeFromTop(rowHeight);
    m_sampleRateLabel.setBounds(sampleRateRow.removeFromLeft(labelWidth));
    sampleRateRow.removeFromLeft(spacing);
    m_sampleRateDropdown.setBounds(sampleRateRow);
    bounds.removeFromTop(spacing);

    // Quality slider (compressed formats only)
    auto qualityRow = bounds.removeFromTop(rowHeight);
    m_qualityLabel.setBounds(qualityRow.removeFromLeft(labelWidth));
    qualityRow.removeFromLeft(spacing);
    auto sliderArea = qualityRow.removeFromLeft(220);
    m_qualitySlider.setBounds(sliderArea);
    qualityRow.removeFromLeft(spacing);
    m_qualityValueLabel.setBounds(qualityRow);
    bounds.removeFromTop(spacing);

    // Metadata checkboxes (WAV only)
    auto bwfRow = bounds.removeFromTop(rowHeight);
    bwfRow.removeFromLeft(labelWidth + spacing);
    m_includeBWFCheckbox.setBounds(bwfRow);
    bounds.removeFromTop(spacing / 2);

    auto ixmlRow = bounds.removeFromTop(rowHeight);
    ixmlRow.removeFromLeft(labelWidth + spacing);
    m_includeiXMLCheckbox.setBounds(ixmlRow);
    bounds.removeFromTop(spacing * 2);

    // Warning label
    auto warningRow = bounds.removeFromTop(rowHeight);
    m_warningLabel.setBounds(warningRow);
    bounds.removeFromTop(spacing / 2);

    // Preview label
    auto previewRow = bounds.removeFromTop(rowHeight);
    m_previewLabel.setBounds(previewRow);
    bounds.removeFromTop(spacing * 2);

    // Action buttons (bottom right)
    auto buttonRow = bounds.removeFromBottom(30);
    m_cancelButton.setBounds(buttonRow.removeFromRight(80));
    buttonRow.removeFromRight(spacing);
    m_saveButton.setBounds(buttonRow.removeFromRight(80));
}

void SaveAsOptionsPanel::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    // Draw separator line above buttons
    g.setColour(juce::Colours::darkgrey);
    g.drawHorizontalLine(getHeight() - 60, 15.0f, (float)(getWidth() - 15));
}

void SaveAsOptionsPanel::updateUIForFormat()
{
    int formatId = m_formatDropdown.getSelectedId();
    bool isWAV = (formatId == 1);
    bool isCompressed = (formatId >= 2);  // FLAC, OGG

    // Show/hide WAV-specific options
    m_bitDepthLabel.setVisible(isWAV);
    m_bitDepthDropdown.setVisible(isWAV);
    m_includeBWFCheckbox.setVisible(isWAV);
    m_includeiXMLCheckbox.setVisible(isWAV);

    // Show/hide quality slider for compressed formats
    m_qualityLabel.setVisible(isCompressed);
    m_qualitySlider.setVisible(isCompressed);
    m_qualityValueLabel.setVisible(isCompressed);

    // Hide warning label (MP3 is no longer an option)
    m_warningLabel.setVisible(false);
}

void SaveAsOptionsPanel::updatePreview()
{
    auto settings = getSettings();

    juce::String preview = "Output: ";

    // Format description
    if (settings.format == "wav")
    {
        preview += juce::String(settings.bitDepth) + "-bit WAV";
    }
    else if (settings.format == "flac")
    {
        preview += "FLAC (Quality " + juce::String(settings.quality) + "/10)";
    }
    else if (settings.format == "ogg")
    {
        preview += "OGG Vorbis (Quality " + juce::String(settings.quality) + "/10)";
    }
    else if (settings.format == "mp3")
    {
        preview += "MP3 (Quality " + juce::String(settings.quality) + "/10)";
    }

    // Sample rate
    double targetRate = (settings.targetSampleRate > 0.0) ? settings.targetSampleRate : m_sourceSampleRate;
    preview += ", " + juce::String(targetRate, 0) + " Hz";

    if (settings.targetSampleRate > 0.0 && std::abs(settings.targetSampleRate - m_sourceSampleRate) > 0.01)
    {
        preview += " (converted from " + juce::String(m_sourceSampleRate, 0) + " Hz)";
    }

    // Estimated file size
    preview += ", ~" + estimateFileSize(settings.format, settings.bitDepth, targetRate, m_sourceChannels);

    m_previewLabel.setText(preview, juce::dontSendNotification);
}

void SaveAsOptionsPanel::onBrowseClicked()
{
    // Show directory chooser
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Choose save location",
        m_targetDirectory,
        "",
        true);

    auto folderChooserFlags = juce::FileBrowserComponent::openMode |
                              juce::FileBrowserComponent::canSelectDirectories;

    m_fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& chooser)
    {
        auto result = chooser.getResult();
        if (result != juce::File())
        {
            m_targetDirectory = result;
            m_folderLocationLabel.setText("Save to: " + m_targetDirectory.getFullPathName(), juce::dontSendNotification);
            updatePreview();
        }
        m_fileChooser.reset();
    });
}

void SaveAsOptionsPanel::onSaveClicked()
{
    // Validate filename
    juce::String filename = m_filenameEditor.getText().trim();
    if (filename.isEmpty())
    {
        juce::AlertWindow::showMessageBox(
            juce::AlertWindow::WarningIcon,
            "Invalid Filename",
            "Please enter a filename.",
            "OK");
        return;
    }

    // Check for invalid filename characters (cross-platform)
    const juce::String invalidChars = "<>:\"/\\|?*";
    for (int i = 0; i < invalidChars.length(); ++i)
    {
        if (filename.containsChar(invalidChars[i]))
        {
            juce::AlertWindow::showMessageBox(
                juce::AlertWindow::WarningIcon,
                "Invalid Filename",
                "Filename contains invalid character: " + juce::String::charToString(invalidChars[i]) +
                "\n\nInvalid characters: < > : \" / \\ | ? *",
                "OK");
            return;
        }
    }

    // Validate target directory exists
    if (!m_targetDirectory.exists() || !m_targetDirectory.isDirectory())
    {
        juce::AlertWindow::showMessageBox(
            juce::AlertWindow::WarningIcon,
            "Invalid Directory",
            "Please click 'Browse...' to select a valid save location.",
            "OK");
        return;
    }

    // Check for MP3 without LAME
    int formatId = m_formatDropdown.getSelectedId();
    if (formatId == 4 && !isMP3EncoderAvailable())
    {
        juce::AlertWindow::showMessageBox(
            juce::AlertWindow::WarningIcon,
            "MP3 Encoder Not Available",
            "MP3 encoding requires the LAME encoder.\n\nInstall with: brew install lame\n\nThen restart WaveEdit.",
            "OK");
        return;
    }

    // Get settings
    auto settings = getSettings();

    // Check if target file already exists and warn user
    if (settings.targetFile.existsAsFile())
    {
        // Special case: if trying to save to the same file as source, show specific warning
        if (m_currentFile.existsAsFile() && settings.targetFile == m_currentFile)
        {
            int result = juce::AlertWindow::showYesNoCancelBox(
                juce::AlertWindow::WarningIcon,
                "Overwrite Source File?",
                "You are about to overwrite the original source file:\n\n" +
                settings.targetFile.getFullPathName() +
                "\n\nThis cannot be undone. Continue?",
                "Overwrite",
                "Cancel",
                juce::String());

            if (result != 1)  // 1 = Yes/Overwrite
                return;
        }
        else
        {
            // Generic file exists warning
            int result = juce::AlertWindow::showYesNoCancelBox(
                juce::AlertWindow::WarningIcon,
                "File Already Exists",
                "A file with this name already exists:\n\n" +
                settings.targetFile.getFullPathName() +
                "\n\nDo you want to replace it?",
                "Replace",
                "Cancel",
                juce::String());

            if (result != 1)  // 1 = Yes/Replace
                return;
        }
    }

    // Store settings and close dialog
    m_result = settings;

    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(1);
}

void SaveAsOptionsPanel::onCancelClicked()
{
    m_result = std::nullopt;

    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(0);
}

juce::String SaveAsOptionsPanel::formatQualityDescription(int quality) const
{
    int formatId = m_formatDropdown.getSelectedId();

    if (formatId == 2)  // FLAC
    {
        if (quality <= 2) return juce::String(quality) + " (Fast)";
        if (quality <= 5) return juce::String(quality) + " (Default)";
        if (quality <= 8) return juce::String(quality) + " (High)";
        return juce::String(quality) + " (Best)";
    }
    else if (formatId == 3)  // OGG
    {
        if (quality <= 3) return juce::String(quality) + " (~64 kbps)";
        if (quality <= 6) return juce::String(quality) + " (~128 kbps)";
        if (quality <= 8) return juce::String(quality) + " (~192 kbps)";
        return juce::String(quality) + " (~256 kbps)";
    }
    else if (formatId == 4)  // MP3
    {
        if (quality <= 2) return juce::String(quality) + " (~96 kbps)";
        if (quality <= 4) return juce::String(quality) + " (~128 kbps)";
        if (quality <= 6) return juce::String(quality) + " (~192 kbps)";
        if (quality <= 8) return juce::String(quality) + " (~256 kbps)";
        return juce::String(quality) + " (~320 kbps)";
    }

    return juce::String(quality);
}

juce::String SaveAsOptionsPanel::estimateFileSize(const juce::String& format, int bitDepth,
                                                   double sampleRate, int channels) const
{
    // Rough estimates based on typical 1-minute stereo audio
    double durationMinutes = 1.0;  // Estimate for 1 minute
    double sizeBytes = 0.0;

    if (format == "wav")
    {
        // PCM: sample_rate * bit_depth/8 * channels * duration_seconds
        sizeBytes = sampleRate * (bitDepth / 8.0) * channels * (durationMinutes * 60.0);
    }
    else if (format == "flac")
    {
        // FLAC: typically 50-60% of uncompressed size
        double uncompressedSize = sampleRate * 2.0 * channels * (durationMinutes * 60.0);  // 16-bit equivalent
        sizeBytes = uncompressedSize * 0.55;
    }
    else if (format == "ogg" || format == "mp3")
    {
        // Quality 10 ≈ 256-320 kbps
        int quality = (int)m_qualitySlider.getValue();
        double bitrateKbps = 32.0 + (quality * 28.8);  // 32 kbps (quality 0) to 320 kbps (quality 10)
        sizeBytes = (bitrateKbps * 1000.0 / 8.0) * (durationMinutes * 60.0);
    }

    // Convert to human-readable format
    double sizeMB = sizeBytes / (1024.0 * 1024.0);

    if (sizeMB >= 1.0)
        return juce::String(sizeMB, 1) + " MB/min";
    else
        return juce::String(sizeBytes / 1024.0, 0) + " KB/min";
}
