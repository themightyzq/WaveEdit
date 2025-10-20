/*
  ==============================================================================

    FilePropertiesDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "FilePropertiesDialog.h"

// Dialog dimensions
namespace
{
    constexpr int DIALOG_WIDTH = 600;
    constexpr int DIALOG_HEIGHT = 400;
    constexpr int ROW_HEIGHT = 30;
    constexpr int LABEL_WIDTH = 150;
    constexpr int SPACING = 10;
    constexpr int BUTTON_HEIGHT = 30;
    constexpr int BUTTON_WIDTH = 100;
}

//==============================================================================
FilePropertiesDialog::FilePropertiesDialog(const Document& document)
{
    // Setup property name labels (left column)
    auto setupLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        label.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(label);
    };

    setupLabel(m_filenameLabel, "Filename:");
    setupLabel(m_filePathLabel, "File Path:");
    setupLabel(m_fileSizeLabel, "File Size:");
    setupLabel(m_dateCreatedLabel, "Date Created:");
    setupLabel(m_dateModifiedLabel, "Date Modified:");
    setupLabel(m_sampleRateLabel, "Sample Rate:");
    setupLabel(m_bitDepthLabel, "Bit Depth:");
    setupLabel(m_channelsLabel, "Channels:");
    setupLabel(m_durationLabel, "Duration:");
    setupLabel(m_codecLabel, "Format:");

    // Setup value labels (right column)
    auto setupValueLabel = [this](juce::Label& label)
    {
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        label.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(label);
    };

    setupValueLabel(m_filenameValue);
    setupValueLabel(m_filePathValue);
    setupValueLabel(m_fileSizeValue);
    setupValueLabel(m_dateCreatedValue);
    setupValueLabel(m_dateModifiedValue);
    setupValueLabel(m_sampleRateValue);
    setupValueLabel(m_bitDepthValue);
    setupValueLabel(m_channelsValue);
    setupValueLabel(m_durationValue);
    setupValueLabel(m_codecValue);

    // Make file path value label support text selection for copying
    m_filePathValue.setEditable(false, false, true);

    // Setup Close button
    m_closeButton.setButtonText("Close");
    m_closeButton.addListener(this);
    addAndMakeVisible(m_closeButton);

    // Load properties from document
    loadProperties(document);

    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);
}

FilePropertiesDialog::~FilePropertiesDialog()
{
}

//==============================================================================
void FilePropertiesDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));

    // Draw section headers
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText("File Information", SPACING, SPACING, DIALOG_WIDTH - 2 * SPACING, 20,
               juce::Justification::centred, false);

    g.drawText("Audio Information", SPACING, 6 * ROW_HEIGHT + SPACING,
               DIALOG_WIDTH - 2 * SPACING, 20, juce::Justification::centred, false);

    // Draw separator lines
    g.setColour(juce::Colours::grey);
    g.drawLine(SPACING, 2 * ROW_HEIGHT + SPACING,
               DIALOG_WIDTH - SPACING, 2 * ROW_HEIGHT + SPACING, 1.0f);
    g.drawLine(SPACING, 7 * ROW_HEIGHT + SPACING,
               DIALOG_WIDTH - SPACING, 7 * ROW_HEIGHT + SPACING, 1.0f);
}

void FilePropertiesDialog::resized()
{
    auto bounds = getLocalBounds().reduced(SPACING);

    // Skip section header
    bounds.removeFromTop(ROW_HEIGHT);
    bounds.removeFromTop(SPACING);

    // File information section
    auto layoutRow = [&](juce::Label& nameLabel, juce::Label& valueLabel)
    {
        auto row = bounds.removeFromTop(ROW_HEIGHT);
        nameLabel.setBounds(row.removeFromLeft(LABEL_WIDTH));
        row.removeFromLeft(SPACING);
        valueLabel.setBounds(row);
    };

    layoutRow(m_filenameLabel, m_filenameValue);
    layoutRow(m_filePathLabel, m_filePathValue);
    layoutRow(m_fileSizeLabel, m_fileSizeValue);
    layoutRow(m_dateCreatedLabel, m_dateCreatedValue);
    layoutRow(m_dateModifiedLabel, m_dateModifiedValue);

    // Skip section header for audio info
    bounds.removeFromTop(ROW_HEIGHT);
    bounds.removeFromTop(SPACING);

    // Audio information section
    layoutRow(m_sampleRateLabel, m_sampleRateValue);
    layoutRow(m_bitDepthLabel, m_bitDepthValue);
    layoutRow(m_channelsLabel, m_channelsValue);
    layoutRow(m_durationLabel, m_durationValue);
    layoutRow(m_codecLabel, m_codecValue);

    // Close button at bottom center
    auto buttonArea = getLocalBounds().removeFromBottom(BUTTON_HEIGHT + SPACING);
    m_closeButton.setBounds(buttonArea.withSizeKeepingCentre(BUTTON_WIDTH, BUTTON_HEIGHT));
}

//==============================================================================
void FilePropertiesDialog::buttonClicked(juce::Button* button)
{
    if (button == &m_closeButton)
    {
        // Close dialog
        if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
        {
            dialogWindow->exitModalState(0);
        }
    }
}

//==============================================================================
void FilePropertiesDialog::loadProperties(const Document& document)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    const juce::File& file = document.getFile();

    // Check if audio engine has a file loaded
    if (!document.getAudioEngine().isFileLoaded())
    {
        m_filenameValue.setText("(No file loaded)", juce::dontSendNotification);
        m_filePathValue.setText("", juce::dontSendNotification);
        m_fileSizeValue.setText("N/A", juce::dontSendNotification);
        m_dateCreatedValue.setText("N/A", juce::dontSendNotification);
        m_dateModifiedValue.setText("N/A", juce::dontSendNotification);
        m_sampleRateValue.setText("N/A", juce::dontSendNotification);
        m_bitDepthValue.setText("N/A", juce::dontSendNotification);
        m_channelsValue.setText("N/A", juce::dontSendNotification);
        m_durationValue.setText("N/A", juce::dontSendNotification);
        m_codecValue.setText("N/A", juce::dontSendNotification);
        return;
    }

    // File information
    m_filenameValue.setText(document.getFilename(), juce::dontSendNotification);
    m_filePathValue.setText(file.getFullPathName(), juce::dontSendNotification);

    // Check if file exists before accessing file properties
    if (file.existsAsFile())
    {
        m_fileSizeValue.setText(formatFileSize(file.getSize()), juce::dontSendNotification);
        m_dateCreatedValue.setText(formatDateTime(file.getCreationTime()), juce::dontSendNotification);
        m_dateModifiedValue.setText(formatDateTime(file.getLastModificationTime()), juce::dontSendNotification);
    }
    else
    {
        m_fileSizeValue.setText("File not found", juce::dontSendNotification);
        m_dateCreatedValue.setText("N/A", juce::dontSendNotification);
        m_dateModifiedValue.setText("N/A", juce::dontSendNotification);
    }

    // Audio information
    const AudioEngine& audioEngine = document.getAudioEngine();
    const AudioBufferManager& bufferManager = document.getBufferManager();

    double sampleRate = audioEngine.getSampleRate();
    int bitDepth = audioEngine.getBitDepth();
    int numChannels = audioEngine.getNumChannels();
    int64_t numSamples = bufferManager.getNumSamples();

    m_sampleRateValue.setText(juce::String(sampleRate, 1) + " Hz", juce::dontSendNotification);
    m_bitDepthValue.setText(juce::String(bitDepth) + " bit", juce::dontSendNotification);

    // Format channels
    juce::String channelsStr = juce::String(numChannels);
    if (numChannels == 1)
        channelsStr += " (Mono)";
    else if (numChannels == 2)
        channelsStr += " (Stereo)";
    m_channelsValue.setText(channelsStr, juce::dontSendNotification);

    // Calculate and format duration
    double durationSeconds = (sampleRate > 0) ? numSamples / sampleRate : 0.0;
    m_durationValue.setText(formatDuration(durationSeconds), juce::dontSendNotification);

    // Determine codec
    m_codecValue.setText(determineCodec(file, bitDepth), juce::dontSendNotification);
}

juce::String FilePropertiesDialog::formatDuration(double durationSeconds)
{
    int hours = static_cast<int>(durationSeconds / 3600.0);
    int minutes = static_cast<int>((durationSeconds - hours * 3600) / 60.0);
    int seconds = static_cast<int>(durationSeconds) % 60;
    int milliseconds = static_cast<int>((durationSeconds - static_cast<int>(durationSeconds)) * 1000);

    return juce::String::formatted("%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
}

juce::String FilePropertiesDialog::formatFileSize(int64_t bytes)
{
    if (bytes < 1024)
        return juce::String(bytes) + " bytes";
    else if (bytes < 1024 * 1024)
        return juce::String(bytes / 1024.0, 2) + " KB";
    else if (bytes < 1024 * 1024 * 1024)
        return juce::String(bytes / (1024.0 * 1024.0), 2) + " MB";
    else
        return juce::String(bytes / (1024.0 * 1024.0 * 1024.0), 2) + " GB";
}

juce::String FilePropertiesDialog::formatDateTime(const juce::Time& time)
{
    return time.formatted("%Y-%m-%d %H:%M:%S");
}

juce::String FilePropertiesDialog::determineCodec(const juce::File& file, int bitDepth)
{
    juce::String extension = file.getFileExtension().toLowerCase();

    // WAV files can be PCM or IEEE float
    if (extension == ".wav")
    {
        if (bitDepth == 32)
            return "WAV (IEEE Float 32-bit)";
        else if (bitDepth == 24)
            return "WAV (PCM 24-bit)";
        else if (bitDepth == 16)
            return "WAV (PCM 16-bit)";
        else
            return "WAV (PCM)";
    }
    else if (extension == ".aiff" || extension == ".aif")
    {
        return "AIFF (PCM " + juce::String(bitDepth) + "-bit)";
    }
    else if (extension == ".flac")
    {
        return "FLAC (Lossless " + juce::String(bitDepth) + "-bit)";
    }
    else if (extension == ".mp3")
    {
        return "MP3 (Lossy)";
    }
    else if (extension == ".ogg")
    {
        return "Ogg Vorbis (Lossy)";
    }
    else
    {
        return "Unknown Format";
    }
}

//==============================================================================
void FilePropertiesDialog::showDialog(juce::Component* parentComponent, const Document& document)
{
    auto* propertiesDialog = new FilePropertiesDialog(document);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(propertiesDialog);
    options.dialogTitle = "File Properties";
    options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.useBottomRightCornerResizer = false;

    // Center over parent component
    if (parentComponent != nullptr)
    {
        auto parentBounds = parentComponent->getScreenBounds();
        auto dialogBounds = juce::Rectangle<int>(0, 0, DIALOG_WIDTH, DIALOG_HEIGHT);
        dialogBounds.setCentre(parentBounds.getCentre());
        options.content->setBounds(dialogBounds);
    }

    // Launch dialog
    options.launchAsync();
}
