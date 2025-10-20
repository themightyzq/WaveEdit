/*
  ==============================================================================

    BatchExportDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "BatchExportDialog.h"
#include <optional>

BatchExportDialog::BatchExportDialog(const juce::File& sourceFile, const RegionManager& regionManager)
    : m_titleLabel("titleLabel", "Batch Export Regions"),
      m_outputDirLabel("outputDirLabel", "Output Directory:"),
      m_browseButton("Browse..."),
      m_namingOptionsLabel("namingOptionsLabel", "Naming Options:"),
      m_includeRegionNameToggle("Include region name"),
      m_includeIndexToggle("Include region index"),
      m_previewLabel("previewLabel", "Preview:"),
      m_exportButton("Export"),
      m_cancelButton("Cancel"),
      m_sourceFile(sourceFile),
      m_regionManager(regionManager)
{
    // Title label
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    #pragma GCC diagnostic pop
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Output directory label
    m_outputDirLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_outputDirLabel);

    // Output directory editor (read-only)
    m_outputDirEditor.setReadOnly(true);
    m_outputDirEditor.setJustification(juce::Justification::centredLeft);

    // Default to same directory as source file
    m_outputDirectory = sourceFile.getParentDirectory();
    m_outputDirEditor.setText(m_outputDirectory.getFullPathName());
    addAndMakeVisible(m_outputDirEditor);

    // Browse button
    m_browseButton.onClick = [this]() { onBrowseClicked(); };
    addAndMakeVisible(m_browseButton);

    // Naming options label
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_namingOptionsLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    #pragma GCC diagnostic pop
    addAndMakeVisible(m_namingOptionsLabel);

    // Include region name toggle
    m_includeRegionNameToggle.setToggleState(true, juce::dontSendNotification);
    m_includeRegionNameToggle.onClick = [this]() { onNamingOptionChanged(); };
    addAndMakeVisible(m_includeRegionNameToggle);

    // Include index toggle
    m_includeIndexToggle.setToggleState(true, juce::dontSendNotification);
    m_includeIndexToggle.onClick = [this]() { onNamingOptionChanged(); };
    addAndMakeVisible(m_includeIndexToggle);

    // Preview label
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_previewLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    #pragma GCC diagnostic pop
    addAndMakeVisible(m_previewLabel);

    // Preview list (multi-line, read-only)
    m_previewList.setMultiLine(true);
    m_previewList.setReadOnly(true);
    m_previewList.setScrollbarsShown(true);
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_previewList.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    #pragma GCC diagnostic pop
    addAndMakeVisible(m_previewList);

    // Export button
    m_exportButton.onClick = [this]() { onExportClicked(); };
    addAndMakeVisible(m_exportButton);

    // Cancel button
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    // Initial preview update
    updatePreviewList();

    setSize(500, 450);
}

std::optional<BatchExportDialog::ExportSettings> BatchExportDialog::showDialog(
    const juce::File& sourceFile,
    const RegionManager& regionManager)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Check if there are any regions to export
    if (regionManager.getNumRegions() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Regions to Export",
            "There are no regions defined in this file.\n\n"
            "Create regions first using:\n"
            "  • R - Create region from selection\n"
            "  • Cmd+Shift+R - Auto-create regions (Strip Silence)",
            "OK"
        );
        return std::nullopt;
    }

    // Create the dialog content
    std::unique_ptr<BatchExportDialog> dialog(new BatchExportDialog(sourceFile, regionManager));

    // Create the dialog window
    juce::DialogWindow dlg("Batch Export Regions", juce::Colours::darkgrey, true, false);
    dlg.setContentOwned(dialog.release(), true);
    dlg.centreWithSize(500, 450);
    dlg.setResizable(false, false);
    dlg.setUsingNativeTitleBar(false);

    // Add to desktop so it appears as a separate window (required on macOS)
    dlg.addToDesktop(juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowHasCloseButton);

    // Make dialog visible BEFORE entering modal state
    dlg.setVisible(true);

    // Bring to front
    dlg.toFront(true);

    // Show dialog modally and run modal loop
    // This makes the dialog modal and blocks until it's closed
    dlg.enterModalState(true);

    int result = 0;
    #if JUCE_MODAL_LOOPS_PERMITTED
        // Use modal loop for synchronous behavior (JUCE 6+ style)
        juce::Logger::writeToLog("BatchExportDialog: Using modal loops (JUCE_MODAL_LOOPS_PERMITTED=1)");
        result = dlg.runModalLoop();
        juce::Logger::writeToLog("BatchExportDialog: Modal loop returned with result: " + juce::String(result));
    #else
        // For JUCE 7+ without modal loops, we need to use launchAsync pattern
        // This is a limitation - should be refactored to async in future
        juce::Logger::writeToLog("ERROR: BatchExportDialog - JUCE_MODAL_LOOPS_PERMITTED is not defined! Dialog cannot be shown.");
        juce::Logger::writeToLog("       Modal loops are required for this dialog. Check CMakeLists.txt configuration.");
        jassert(false && "Modal loops not permitted in this JUCE build");
        return std::nullopt;
    #endif

    // Get the dialog content back to access the result
    auto* dialogContent = dynamic_cast<BatchExportDialog*>(dlg.getContentComponent());

    // Return result based on exit code
    // exitModalState(1) = Export clicked
    // exitModalState(0) = Cancel clicked or window closed
    if (result == 1 && dialogContent != nullptr && dialogContent->m_result.has_value())
    {
        return dialogContent->m_result;
    }

    return std::nullopt;
}

void BatchExportDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));

    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawRect(getLocalBounds(), 1);
}

void BatchExportDialog::resized()
{
    auto area = getLocalBounds().reduced(15);

    // Title
    m_titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10); // Spacing

    // Output directory row
    auto dirRow = area.removeFromTop(30);
    m_outputDirLabel.setBounds(dirRow.removeFromLeft(120));
    dirRow.removeFromLeft(10); // Spacing
    m_browseButton.setBounds(dirRow.removeFromRight(80));
    dirRow.removeFromRight(10); // Spacing
    m_outputDirEditor.setBounds(dirRow);

    area.removeFromTop(15); // Spacing

    // Naming options
    m_namingOptionsLabel.setBounds(area.removeFromTop(25));
    m_includeRegionNameToggle.setBounds(area.removeFromTop(25).reduced(10, 0));
    m_includeIndexToggle.setBounds(area.removeFromTop(25).reduced(10, 0));

    area.removeFromTop(15); // Spacing

    // Preview
    m_previewLabel.setBounds(area.removeFromTop(25));
    m_previewList.setBounds(area.removeFromTop(150));

    area.removeFromTop(15); // Spacing

    // Buttons
    auto buttonRow = area.removeFromTop(30);
    auto buttonWidth = 100;
    auto buttonSpacing = 10;

    m_cancelButton.setBounds(buttonRow.removeFromRight(buttonWidth));
    buttonRow.removeFromRight(buttonSpacing);
    m_exportButton.setBounds(buttonRow.removeFromRight(buttonWidth));
}

juce::String BatchExportDialog::generatePreviewFilename(int regionIndex, const Region& region) const
{
    // Base filename (without extension)
    juce::String baseName = m_sourceFile.getFileNameWithoutExtension();

    juce::String filename = baseName;

    if (m_includeRegionNameToggle.getToggleState())
    {
        // Sanitize region name for filename (replace invalid characters)
        juce::String regionName = region.getName();
        regionName = regionName.replaceCharacters("/\\:*?\"<>|", "_________");

        if (regionName.isNotEmpty())
        {
            filename += "_" + regionName;
        }
    }

    if (m_includeIndexToggle.getToggleState())
    {
        // Add region index (1-based for user-friendliness)
        filename += "_" + juce::String(regionIndex + 1);
    }

    filename += ".wav";

    return filename;
}

void BatchExportDialog::updatePreviewList()
{
    juce::String preview;

    int numRegions = m_regionManager.getNumRegions();

    if (numRegions == 0)
    {
        preview = "(No regions to export)";
    }
    else
    {
        preview = "Output files (" + juce::String(numRegions) + " regions):\n\n";

        for (int i = 0; i < numRegions; ++i)
        {
            auto* region = m_regionManager.getRegion(i);
            if (region != nullptr)
            {
                juce::String filename = generatePreviewFilename(i, *region);
                preview += filename + "\n";
            }
        }
    }

    m_previewList.setText(preview);
}

void BatchExportDialog::onBrowseClicked()
{
    // Use heap allocated FileChooser for async safety (required in JUCE 6+)
    m_fileChooser = std::make_unique<juce::FileChooser>("Select Output Directory",
                                                         m_outputDirectory,
                                                         "",
                                                         true);

    auto chooserFlags = juce::FileBrowserComponent::openMode |
                        juce::FileBrowserComponent::canSelectDirectories;

    m_fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result.exists() && result.isDirectory())
        {
            m_outputDirectory = result;
            m_outputDirEditor.setText(m_outputDirectory.getFullPathName());
        }
    });
}

void BatchExportDialog::onNamingOptionChanged()
{
    updatePreviewList();
}

void BatchExportDialog::onExportClicked()
{
    if (!validateExport())
    {
        return;
    }

    // Populate result
    ExportSettings settings;
    settings.outputDirectory = m_outputDirectory;
    settings.includeRegionName = m_includeRegionNameToggle.getToggleState();
    settings.includeIndex = m_includeIndexToggle.getToggleState();

    m_result = settings;

    // Close dialog
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
    {
        dw->exitModalState(1);
    }
}

void BatchExportDialog::onCancelClicked()
{
    m_result = std::nullopt;

    // Close dialog
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
    {
        dw->exitModalState(0);
    }
}

bool BatchExportDialog::validateExport()
{
    // Check output directory exists and is writable
    if (!m_outputDirectory.exists())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Invalid Output Directory",
            "The selected output directory does not exist:\n" +
            m_outputDirectory.getFullPathName(),
            "OK"
        );
        return false;
    }

    if (!m_outputDirectory.isDirectory())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Invalid Output Directory",
            "The selected path is not a directory:\n" +
            m_outputDirectory.getFullPathName(),
            "OK"
        );
        return false;
    }

    // Check for write permissions by attempting to create a temp file
    auto testFile = m_outputDirectory.getChildFile(".waveedit_write_test");
    if (!testFile.create())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Permission Denied",
            "Cannot write to the selected directory:\n" +
            m_outputDirectory.getFullPathName() +
            "\n\nPlease select a directory where you have write permission.",
            "OK"
        );
        return false;
    }
    testFile.deleteFile(); // Clean up test file

    // Check if any regions exist
    if (m_regionManager.getNumRegions() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Regions",
            "There are no regions to export.",
            "OK"
        );
        return false;
    }

    // Check for filename conflicts
    juce::StringArray existingFiles;
    for (int i = 0; i < m_regionManager.getNumRegions(); ++i)
    {
        auto* region = m_regionManager.getRegion(i);
        if (region != nullptr)
        {
            juce::String filename = generatePreviewFilename(i, *region);
            auto outputFile = m_outputDirectory.getChildFile(filename);

            if (outputFile.existsAsFile())
            {
                existingFiles.add(filename);
            }
        }
    }

    if (existingFiles.size() > 0)
    {
        int numFiles = existingFiles.size();
        juce::String message = juce::String(numFiles) + " file" +
                               (numFiles > 1 ? "s" : "") +
                               " will be overwritten:\n\n";

        // Show first few files
        int maxShow = juce::jmin(5, numFiles);
        for (int i = 0; i < maxShow; ++i)
        {
            message += "  • " + existingFiles[i] + "\n";
        }

        if (numFiles > maxShow)
        {
            message += "  • ... and " + juce::String(numFiles - maxShow) + " more\n";
        }

        message += "\nDo you want to proceed?";

        bool proceed = juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon,
            "Confirm Overwrite",
            message,
            "Overwrite",
            "Cancel",
            nullptr,
            nullptr
        );

        if (!proceed)
        {
            return false;
        }
    }

    return true;
}
