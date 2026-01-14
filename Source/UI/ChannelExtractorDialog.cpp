/*
  ==============================================================================

    ChannelExtractorDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "ChannelExtractorDialog.h"
#include "UIConstants.h"

namespace
{
    constexpr int DIALOG_WIDTH = 480;
    constexpr int DIALOG_HEIGHT = 555;  // Increased to accommodate format selector
    constexpr int MARGIN = 20;
    constexpr int ROW_HEIGHT = 28;
    constexpr int BUTTON_WIDTH = 90;
    constexpr int BUTTON_HEIGHT = 30;
}

ChannelExtractorDialog::ChannelExtractorDialog(int currentChannels, const juce::String& sourceFileName)
    : m_currentChannels(currentChannels)
    , m_sourceFileName(sourceFileName)
{
    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);

    // Title
    m_titleLabel.setText("Channel Extractor", juce::dontSendNotification);
    m_titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Source file info
    m_sourceLabel.setText("Source:", juce::dontSendNotification);
    m_sourceLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_sourceLabel);

    m_sourceValueLabel.setText(sourceFileName + " (" + juce::String(currentChannels) + " channels)",
                                juce::dontSendNotification);
    m_sourceValueLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    addAndMakeVisible(m_sourceValueLabel);

    // Channel selection group
    m_channelGroup.setText("Select Channels to Extract");
    addAndMakeVisible(m_channelGroup);

    populateChannelCheckboxes();

    // Selection count
    m_selectionCountLabel.setText("0 channels selected", juce::dontSendNotification);
    m_selectionCountLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    addAndMakeVisible(m_selectionCountLabel);

    // Select All / Select None buttons
    m_selectAllButton.setButtonText("Select All");
    m_selectAllButton.onClick = [this]()
    {
        for (auto* cb : m_channelCheckboxes)
            cb->setToggleState(true, juce::sendNotification);
    };
    addAndMakeVisible(m_selectAllButton);

    m_selectNoneButton.setButtonText("Select None");
    m_selectNoneButton.onClick = [this]()
    {
        for (auto* cb : m_channelCheckboxes)
            cb->setToggleState(false, juce::sendNotification);
    };
    addAndMakeVisible(m_selectNoneButton);

    // Export mode
    m_exportModeLabel.setText("Export As:", juce::dontSendNotification);
    m_exportModeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_exportModeLabel);

    m_individualFilesButton.setButtonText("Individual mono files");
    m_individualFilesButton.setRadioGroupId(1);
    m_individualFilesButton.setToggleState(true, juce::dontSendNotification);
    m_individualFilesButton.onClick = [this]() { onExportModeChanged(); };
    addAndMakeVisible(m_individualFilesButton);

    m_combinedFileButton.setButtonText("Combined multi-channel file");
    m_combinedFileButton.setRadioGroupId(1);
    m_combinedFileButton.onClick = [this]() { onExportModeChanged(); };
    addAndMakeVisible(m_combinedFileButton);

    // Export format
    m_formatLabel.setText("Format:", juce::dontSendNotification);
    m_formatLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_formatLabel);

    m_formatCombo.addItem("WAV", static_cast<int>(ExportFormat::WAV));
    m_formatCombo.addItem("FLAC", static_cast<int>(ExportFormat::FLAC));
    m_formatCombo.addItem("OGG", static_cast<int>(ExportFormat::OGG));
    m_formatCombo.setSelectedId(static_cast<int>(ExportFormat::WAV));
    m_formatCombo.onChange = [this]() { updateFilenamePreview(); };
    addAndMakeVisible(m_formatCombo);

    // Output directory
    m_outputLabel.setText("Output Folder:", juce::dontSendNotification);
    m_outputLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_outputLabel);

    m_outputDirButton.setButtonText("Choose...");
    m_outputDirButton.onClick = [this]() { onChooseOutputDirectory(); };
    addAndMakeVisible(m_outputDirButton);

    m_outputDirLabel.setText("(Not selected)", juce::dontSendNotification);
    m_outputDirLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(m_outputDirLabel);

    // Filename preview
    m_previewLabel.setText("Files to create:", juce::dontSendNotification);
    addAndMakeVisible(m_previewLabel);

    m_filenamePreview.setMultiLine(true, true);
    m_filenamePreview.setReadOnly(true);
    m_filenamePreview.setScrollbarsShown(true);
    m_filenamePreview.setFont(juce::FontOptions(11.0f));
    m_filenamePreview.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1E1E1E));
    m_filenamePreview.setColour(juce::TextEditor::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(m_filenamePreview);

    // Buttons
    m_applyButton.setButtonText("Extract");
    m_applyButton.onClick = [this]() { onApplyClicked(); };
    addAndMakeVisible(m_applyButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    updateSelectionCount();
    updateFilenamePreview();
}

void ChannelExtractorDialog::populateChannelCheckboxes()
{
    waveedit::ChannelLayout layout = waveedit::ChannelLayout::fromChannelCount(m_currentChannels);

    for (int ch = 0; ch < m_currentChannels; ++ch)
    {
        auto* checkbox = new juce::ToggleButton();
        const auto& info = layout.getChannelInfo(ch);

        juce::String label = "Ch " + juce::String(ch + 1);
        if (!info.shortLabel.isEmpty())
            label += " (" + info.shortLabel + ")";
        if (!info.fullName.isEmpty())
            label += " - " + info.fullName;

        checkbox->setButtonText(label);
        checkbox->onClick = [this]() { onChannelCheckboxChanged(); };
        addAndMakeVisible(checkbox);
        m_channelCheckboxes.add(checkbox);
    }
}

void ChannelExtractorDialog::onChannelCheckboxChanged()
{
    updateSelectionCount();
    updateFilenamePreview();
}

void ChannelExtractorDialog::onExportModeChanged()
{
    updateFilenamePreview();
}

void ChannelExtractorDialog::onChooseOutputDirectory()
{
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Select Output Folder",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*",
        true);

    m_fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result.isDirectory())
            {
                m_outputDirectory = result;
                m_outputDirLabel.setText(result.getFileName(), juce::dontSendNotification);
                m_outputDirLabel.setColour(juce::Label::textColourId, juce::Colours::white);
                m_outputDirLabel.setTooltip(result.getFullPathName());
                updateFilenamePreview();
            }
        });
}

void ChannelExtractorDialog::updateSelectionCount()
{
    int count = 0;
    for (auto* cb : m_channelCheckboxes)
    {
        if (cb->getToggleState())
            count++;
    }

    m_selectionCountLabel.setText(juce::String(count) + " channel" + (count != 1 ? "s" : "") + " selected",
                                   juce::dontSendNotification);
}

void ChannelExtractorDialog::updateFilenamePreview()
{
    std::vector<int> selectedChannels;
    for (int i = 0; i < m_channelCheckboxes.size(); ++i)
    {
        if (m_channelCheckboxes[i]->getToggleState())
            selectedChannels.push_back(i);
    }

    if (selectedChannels.empty())
    {
        m_filenamePreview.setText("(Select channels to see preview)", juce::dontSendNotification);
        return;
    }

    juce::String baseName = m_sourceFileName;
    if (baseName.contains("."))
        baseName = baseName.upToLastOccurrenceOf(".", false, false);

    waveedit::ChannelLayout layout = waveedit::ChannelLayout::fromChannelCount(m_currentChannels);
    juce::String preview;

    // Get file extension based on selected format
    juce::String extension;
    switch (static_cast<ExportFormat>(m_formatCombo.getSelectedId()))
    {
        case ExportFormat::FLAC: extension = ".flac"; break;
        case ExportFormat::OGG:  extension = ".ogg";  break;
        case ExportFormat::WAV:
        default:                 extension = ".wav";  break;
    }

    if (m_individualFilesButton.getToggleState())
    {
        // Individual mono files
        for (int ch : selectedChannels)
        {
            juce::String channelLabel = layout.getShortLabel(ch);
            preview += baseName + "_Ch" + juce::String(ch + 1) + "_" + channelLabel + extension + "\n";
        }
    }
    else
    {
        // Combined multi-channel file
        juce::String channelSuffix;
        for (size_t i = 0; i < selectedChannels.size(); ++i)
        {
            if (i > 0) channelSuffix += "-";
            channelSuffix += layout.getShortLabel(selectedChannels[i]);
        }
        preview = baseName + "_" + channelSuffix + "_extracted" + extension;
    }

    m_filenamePreview.setText(preview, juce::dontSendNotification);
}

void ChannelExtractorDialog::onApplyClicked()
{
    // Validate: at least one channel selected
    std::vector<int> selectedChannels;
    for (int i = 0; i < m_channelCheckboxes.size(); ++i)
    {
        if (m_channelCheckboxes[i]->getToggleState())
            selectedChannels.push_back(i);
    }

    if (selectedChannels.empty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "No Channels Selected",
            "Please select at least one channel to extract.");
        return;
    }

    // Validate: output directory selected
    if (!m_outputDirectory.isDirectory())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Output Folder Required",
            "Please choose an output folder for the extracted files.");
        return;
    }

    m_result = Result{
        selectedChannels,
        m_individualFilesButton.getToggleState() ? ExportMode::IndividualMono : ExportMode::CombinedMulti,
        static_cast<ExportFormat>(m_formatCombo.getSelectedId()),
        m_outputDirectory
    };

    if (auto* dlg = findParentComponentOfClass<juce::DialogWindow>())
        dlg->exitModalState(1);
}

void ChannelExtractorDialog::onCancelClicked()
{
    m_result = std::nullopt;

    if (auto* dlg = findParentComponentOfClass<juce::DialogWindow>())
        dlg->exitModalState(0);
}

void ChannelExtractorDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(waveedit::ui::kBackgroundPrimary));
}

void ChannelExtractorDialog::resized()
{
    auto bounds = getLocalBounds().reduced(MARGIN);

    // Title
    m_titleLabel.setBounds(bounds.removeFromTop(35));
    bounds.removeFromTop(5);

    // Source file row
    auto sourceRow = bounds.removeFromTop(ROW_HEIGHT);
    m_sourceLabel.setBounds(sourceRow.removeFromLeft(80));
    sourceRow.removeFromLeft(10);
    m_sourceValueLabel.setBounds(sourceRow);
    bounds.removeFromTop(10);

    // Channel selection group
    int groupHeight = 140;  // Space for up to 8 channels in 2 columns
    auto groupBounds = bounds.removeFromTop(groupHeight);
    m_channelGroup.setBounds(groupBounds);

    auto groupContent = groupBounds.reduced(15, 25);

    // Arrange checkboxes in two columns
    int checkboxHeight = 24;
    int col1Width = groupContent.getWidth() / 2 - 5;

    for (int i = 0; i < m_channelCheckboxes.size(); ++i)
    {
        int col = i % 2;
        int row = i / 2;

        auto checkboxBounds = juce::Rectangle<int>(
            groupContent.getX() + col * (col1Width + 10),
            groupContent.getY() + row * (checkboxHeight + 2),
            col1Width,
            checkboxHeight
        );

        m_channelCheckboxes[i]->setBounds(checkboxBounds);
    }

    bounds.removeFromTop(8);

    // Selection count and buttons row
    auto selectionRow = bounds.removeFromTop(ROW_HEIGHT);
    m_selectionCountLabel.setBounds(selectionRow.removeFromLeft(150));
    selectionRow.removeFromLeft(10);
    m_selectAllButton.setBounds(selectionRow.removeFromLeft(80));
    selectionRow.removeFromLeft(5);
    m_selectNoneButton.setBounds(selectionRow.removeFromLeft(90));
    bounds.removeFromTop(15);

    // Export mode row
    auto modeRow = bounds.removeFromTop(ROW_HEIGHT);
    m_exportModeLabel.setBounds(modeRow.removeFromLeft(90));
    modeRow.removeFromLeft(10);
    m_individualFilesButton.setBounds(modeRow.removeFromLeft(160));
    modeRow.removeFromLeft(10);
    m_combinedFileButton.setBounds(modeRow);
    bounds.removeFromTop(8);

    // Export format row
    auto formatRow = bounds.removeFromTop(ROW_HEIGHT);
    m_formatLabel.setBounds(formatRow.removeFromLeft(90));
    formatRow.removeFromLeft(10);
    m_formatCombo.setBounds(formatRow.removeFromLeft(100));
    bounds.removeFromTop(10);

    // Output directory row
    auto outputRow = bounds.removeFromTop(ROW_HEIGHT);
    m_outputLabel.setBounds(outputRow.removeFromLeft(90));
    outputRow.removeFromLeft(10);
    m_outputDirButton.setBounds(outputRow.removeFromLeft(80));
    outputRow.removeFromLeft(10);
    m_outputDirLabel.setBounds(outputRow);
    bounds.removeFromTop(15);

    // Filename preview
    m_previewLabel.setBounds(bounds.removeFromTop(22));
    bounds.removeFromTop(3);

    // Reserve space for buttons at bottom
    auto buttonRow = bounds.removeFromBottom(BUTTON_HEIGHT);
    bounds.removeFromBottom(10);

    // Preview takes remaining space
    m_filenamePreview.setBounds(bounds);

    // Buttons
    m_applyButton.setBounds(buttonRow.removeFromRight(BUTTON_WIDTH));
    buttonRow.removeFromRight(10);
    m_cancelButton.setBounds(buttonRow.removeFromRight(BUTTON_WIDTH));
}

std::optional<ChannelExtractorDialog::Result> ChannelExtractorDialog::showDialog(
    int currentChannels, const juce::String& sourceFileName)
{
    // Use stack allocation to avoid use-after-free bug
    // setOwned() would delete the dialog when runModal() returns,
    // making dialog->m_result access freed memory
    ChannelExtractorDialog dialog(currentChannels, sourceFileName);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Channel Extractor";
    options.dialogBackgroundColour = juce::Colour(waveedit::ui::kBackgroundPrimary);
    options.content.setNonOwned(&dialog);  // Use setNonOwned for stack object
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

#if JUCE_MODAL_LOOPS_PERMITTED
    options.runModal();
    return dialog.m_result;
#else
    jassertfalse; // Modal loops required for this dialog
    return std::nullopt;
#endif
}
