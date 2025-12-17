/*
  ==============================================================================

    OfflinePluginDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "OfflinePluginDialog.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioBufferManager.h"
#include "../Plugins/PluginManager.h"
#include "../Plugins/PluginChainRenderer.h"

//==============================================================================
OfflinePluginDialog::OfflinePluginDialog(AudioEngine* audioEngine,
                                         AudioBufferManager* bufferManager,
                                         int64_t selectionStart,
                                         int64_t selectionEnd)
    : m_audioEngine(audioEngine),
      m_bufferManager(bufferManager),
      m_selectionStart(selectionStart),
      m_selectionEnd(selectionEnd),
      m_isPreviewActive(false),
      m_isPreviewPlaying(false)
{
    // Title label
    m_titleLabel.setText("Offline Plugin", juce::dontSendNotification);
    m_titleLabel.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Search box
    m_searchBox.setTextToShowWhenEmpty("Filter plugins...", juce::Colours::grey);
    m_searchBox.onTextChange = [this]() { onSearchTextChanged(); };
    m_searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    m_searchBox.setColour(juce::TextEditor::textColourId, m_textColour);
    m_searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3a3a3a));
    addAndMakeVisible(m_searchBox);

    // Plugin browser table
    m_pluginTable.setModel(this);
    m_pluginTable.setRowHeight(kBrowserRowHeight);
    m_pluginTable.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1e1e1e));
    m_pluginTable.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff444444));
    m_pluginTable.getHeader().addColumn("Name", NameColumn, 250, 100, 400);
    m_pluginTable.getHeader().addColumn("Manufacturer", ManufacturerColumn, 120, 80, 200);
    m_pluginTable.getHeader().addColumn("Type", FormatColumn, 60, 50, 100);
    m_pluginTable.getHeader().setStretchToFitActive(true);
    addAndMakeVisible(m_pluginTable);

    // Rescan button
    m_rescanButton.setButtonText("Rescan");
    m_rescanButton.setTooltip("Scan for new or updated plugins");
    m_rescanButton.onClick = [this]()
    {
        auto& pm = PluginManager::getInstance();
        if (!pm.isScanInProgress())
        {
            pm.forceRescan(nullptr, [this](bool, int) { refreshPluginList(); });
        }
    };
    addAndMakeVisible(m_rescanButton);

    // Editor viewport and container
    m_editorContainer = std::make_unique<juce::Component>();
    m_editorViewport.setViewedComponent(m_editorContainer.get(), false);
    m_editorViewport.setScrollBarsShown(true, true);
    addAndMakeVisible(m_editorViewport);

    // "No plugin selected" label
    m_noPluginLabel.setText("Select a plugin from the list",
                            juce::dontSendNotification);
    m_noPluginLabel.setJustificationType(juce::Justification::centred);
    m_noPluginLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    m_editorContainer->addAndMakeVisible(m_noPluginLabel);

    // Render Options Group
    m_renderOptionsGroup.setText("Render Options");
    m_renderOptionsGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xff444444));
    m_renderOptionsGroup.setColour(juce::GroupComponent::textColourId, m_textColour);
    addAndMakeVisible(m_renderOptionsGroup);

    // Convert to stereo checkbox
    m_convertToStereoCheckbox.setButtonText("Convert to stereo");
    m_convertToStereoCheckbox.setTooltip("Convert mono file to stereo before processing (preserves stereo plugin effects)");
    m_convertToStereoCheckbox.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(m_convertToStereoCheckbox);

    // Include tail checkbox
    m_includeTailCheckbox.setButtonText("Include effect tail");
    m_includeTailCheckbox.setTooltip("Extend selection to include reverb/delay tail");
    m_includeTailCheckbox.setToggleState(false, juce::dontSendNotification);
    m_includeTailCheckbox.onClick = [this]()
    {
        bool enabled = m_includeTailCheckbox.getToggleState();
        m_tailLengthSlider.setEnabled(enabled);
        m_tailLengthLabel.setEnabled(enabled);
    };
    addAndMakeVisible(m_includeTailCheckbox);

    // Tail length label
    m_tailLengthLabel.setText("Tail:", juce::dontSendNotification);
    m_tailLengthLabel.setColour(juce::Label::textColourId, m_textColour);
    m_tailLengthLabel.setEnabled(false);
    addAndMakeVisible(m_tailLengthLabel);

    // Tail length slider
    m_tailLengthSlider.setRange(0.5, 10.0, 0.1);
    m_tailLengthSlider.setValue(2.0);
    m_tailLengthSlider.setTextValueSuffix(" sec");
    m_tailLengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    m_tailLengthSlider.setColour(juce::Slider::textBoxTextColourId, m_textColour);
    m_tailLengthSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff2a2a2a));
    m_tailLengthSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff3a3a3a));
    m_tailLengthSlider.setEnabled(false);
    addAndMakeVisible(m_tailLengthSlider);

    // Check if source is mono
    m_isSourceMono = (m_bufferManager && m_bufferManager->getNumChannels() == 1);
    m_convertToStereoCheckbox.setEnabled(m_isSourceMono);
    if (!m_isSourceMono)
    {
        m_convertToStereoCheckbox.setTooltip("Source is already stereo");
    }

    // Loop checkbox
    m_loopCheckbox.setButtonText("Loop");
    m_loopCheckbox.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(m_loopCheckbox);

    // Preview button
    m_previewButton.setButtonText("Preview");
    m_previewButton.onClick = [this]() { onPreviewClicked(); };
    m_previewButton.setEnabled(false);
    addAndMakeVisible(m_previewButton);

    // Cancel button
    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    // Apply button
    m_applyButton.setButtonText("Apply");
    m_applyButton.onClick = [this]() { onApplyClicked(); };
    m_applyButton.setEnabled(false);
    addAndMakeVisible(m_applyButton);

    // Load plugin list
    refreshPluginList();

    // Set initial size - side-by-side layout (editor on left, browser on right)
    // Width = padding + editor + divider + browser + padding
    // Height = padding + title + spacing + main content + spacing + render options + spacing + buttons + padding
    const int initialWidth = kPadding + kMinEditorWidth + kDividerWidth + kBrowserWidth + kPadding;
    const int initialHeight = kPadding + 30 + kSpacing + kMinEditorHeight + kSpacing +
                               kRenderOptionsHeight + kSpacing + kButtonRowHeight + kPadding;
    setSize(initialWidth, initialHeight);
}

OfflinePluginDialog::~OfflinePluginDialog()
{
    disablePreview();
    unloadCurrentPlugin();
}

//==============================================================================
std::optional<OfflinePluginDialog::Result> OfflinePluginDialog::showDialog(
    AudioEngine* audioEngine,
    AudioBufferManager* bufferManager,
    int64_t selectionStart,
    int64_t selectionEnd)
{
    OfflinePluginDialog dialog(audioEngine, bufferManager, selectionStart, selectionEnd);

    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(&dialog);
    options.dialogTitle = "Offline Plugin";
    options.dialogBackgroundColour = juce::Colour(0xff2b2b2b);
    options.escapeKeyTriggersCloseButton = false;
    options.useNativeTitleBar = false;
    options.resizable = true;
    options.componentToCentreAround = nullptr;

#if JUCE_MODAL_LOOPS_PERMITTED
    int result = options.runModal();

    // Ensure preview is disabled when dialog closes
    dialog.disablePreview();

    // Return the result only if Apply was clicked (result == 1)
    return (result == 1) ? dialog.m_result : std::nullopt;
#else
    jassertfalse; // Modal loops required for this dialog
    return std::nullopt;
#endif
}

//==============================================================================
void OfflinePluginDialog::paint(juce::Graphics& g)
{
    g.fillAll(m_backgroundColour);

    // Draw border around editor area
    auto editorBounds = m_editorViewport.getBounds().expanded(1);
    g.setColour(juce::Colour(0xff444444));
    g.drawRect(editorBounds, 1);

    // Draw border around browser table
    auto tableBounds = m_pluginTable.getBounds().expanded(1);
    g.setColour(juce::Colour(0xff444444));
    g.drawRect(tableBounds, 1);
}

void OfflinePluginDialog::resized()
{
    auto bounds = getLocalBounds().reduced(kPadding);

    // Title row at top
    m_titleLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(kSpacing);

    // Button row at bottom
    auto buttonRow = bounds.removeFromBottom(kButtonRowHeight);
    const int buttonWidth = 90;
    const int buttonSpacing = 10;

    // Left side: Preview and Loop
    m_previewButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(buttonSpacing);
    m_loopCheckbox.setBounds(buttonRow.removeFromLeft(80));

    // Right side: Cancel and Apply
    m_applyButton.setBounds(buttonRow.removeFromRight(buttonWidth));
    buttonRow.removeFromRight(buttonSpacing);
    m_cancelButton.setBounds(buttonRow.removeFromRight(buttonWidth));

    bounds.removeFromBottom(kSpacing);

    // Render options section above buttons (spans full width)
    auto optionsArea = bounds.removeFromBottom(kRenderOptionsHeight);
    m_renderOptionsGroup.setBounds(optionsArea);

    // Layout render options inside the group - two rows with checkboxes on the left half
    auto optionsInner = optionsArea.reduced(10, 20);  // Account for group border
    auto row1 = optionsInner.removeFromTop(26);
    optionsInner.removeFromTop(4);  // spacing
    auto row2 = optionsInner.removeFromTop(26);

    // Row 1: Convert to stereo checkbox (full row)
    m_convertToStereoCheckbox.setBounds(row1);

    // Row 2: Include tail checkbox + tail length controls (same row, matching checkbox width)
    m_includeTailCheckbox.setBounds(row2.removeFromLeft(180));  // Same width as stereo checkbox text
    row2.removeFromLeft(10);
    m_tailLengthLabel.setBounds(row2.removeFromLeft(35));
    m_tailLengthSlider.setBounds(row2);

    bounds.removeFromBottom(kSpacing);

    // Main content area: side-by-side layout (editor on left, browser on right)
    // Plugin browser panel on RIGHT side
    auto browserPanel = bounds.removeFromRight(kBrowserWidth);
    bounds.removeFromRight(kDividerWidth);  // Divider space

    // Browser panel layout: search row at top, then table
    auto searchRow = browserPanel.removeFromTop(kSearchRowHeight);
    m_rescanButton.setBounds(searchRow.removeFromRight(60));
    searchRow.removeFromRight(kSpacing);
    m_searchBox.setBounds(searchRow);

    browserPanel.removeFromTop(kSpacing);
    m_pluginTable.setBounds(browserPanel);

    // Editor viewport takes remaining space on LEFT side
    m_editorViewport.setBounds(bounds);

    // Update editor container size
    if (m_pluginEditor != nullptr)
    {
        auto editorSize = m_pluginEditor->getBounds();
        m_editorContainer->setSize(
            juce::jmax(bounds.getWidth(), editorSize.getWidth()),
            juce::jmax(bounds.getHeight(), editorSize.getHeight()));
        m_pluginEditor->setBounds(0, 0, editorSize.getWidth(), editorSize.getHeight());
    }
    else
    {
        m_editorContainer->setSize(bounds.getWidth(), bounds.getHeight());
        m_noPluginLabel.setBounds(m_editorContainer->getLocalBounds());
    }
}

//==============================================================================
void OfflinePluginDialog::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    // Plugin scan complete - refresh list
    refreshPluginList();
}

//==============================================================================
// TableListBoxModel overrides

int OfflinePluginDialog::getNumRows()
{
    return m_filteredPlugins.size();
}

void OfflinePluginDialog::paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/,
                                              bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(m_selectedRowColour);
    else if (rowNumber % 2 == 1)
        g.fillAll(m_alternateRowColour);
}

void OfflinePluginDialog::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                                     int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= m_filteredPlugins.size())
        return;

    const auto& filteredPlugin = m_filteredPlugins[rowNumber];
    const auto* desc = filteredPlugin.description;
    if (desc == nullptr)
        return;

    g.setColour(m_textColour);
    g.setFont(14.0f);

    juce::String text;
    switch (columnId)
    {
        case NameColumn:
            text = desc->name;
            break;
        case ManufacturerColumn:
            text = desc->manufacturerName;
            break;
        case FormatColumn:
            text = desc->pluginFormatName;
            break;
        default:
            break;
    }

    g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
}

void OfflinePluginDialog::cellClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent& /*event*/)
{
    if (rowNumber >= 0 && rowNumber < m_filteredPlugins.size())
    {
        m_pluginTable.selectRow(rowNumber);
    }
}

void OfflinePluginDialog::cellDoubleClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent& /*event*/)
{
    // Double-click is the primary method for loading a plugin
    // The rowNumber parameter is reliable here - it's the actual row that was clicked
    if (rowNumber >= 0 && rowNumber < static_cast<int>(m_filteredPlugins.size()))
    {
        DBG("[PLUGIN SELECT] cellDoubleClicked: rowNumber=" + juce::String(rowNumber)
            + " -> originalIndex=" + juce::String(m_filteredPlugins[rowNumber].originalIndex)
            + " (" + m_filteredPlugins[rowNumber].description->name + ")");
        onPluginSelected(m_filteredPlugins[rowNumber].originalIndex);
    }
}

void OfflinePluginDialog::selectedRowsChanged(int lastRowSelected)
{
    // NOTE: This callback fires on EVERY selection change, including during double-clicks.
    // We intentionally do NOT auto-load plugins here to avoid race conditions with cellDoubleClicked.
    // The user must double-click to load a plugin.
    //
    // If we wanted single-click to load plugins, we'd need to debounce or use a timer
    // to avoid loading the wrong plugin during rapid selection changes.

    const int selectedRow = m_pluginTable.getSelectedRow();
    DBG("[PLUGIN SELECT] selectedRowsChanged: lastRowSelected=" + juce::String(lastRowSelected)
        + ", getSelectedRow()=" + juce::String(selectedRow));

    // Don't auto-load - require double-click
    juce::ignoreUnused(lastRowSelected);
}

//==============================================================================
void OfflinePluginDialog::refreshPluginList()
{
    auto& pm = PluginManager::getInstance();
    auto allPlugins = pm.getAvailablePlugins();

    // Filter to effects only (no instruments)
    m_availablePlugins.clear();
    for (const auto& desc : allPlugins)
    {
        if (!desc.isInstrument)
        {
            m_availablePlugins.add(desc);
        }
    }

    // Sort alphabetically by name
    std::sort(m_availablePlugins.begin(), m_availablePlugins.end(),
              [](const juce::PluginDescription& a, const juce::PluginDescription& b)
              {
                  return a.name.compareIgnoreCase(b.name) < 0;
              });

    updateFilteredPlugins();
}

void OfflinePluginDialog::updateFilteredPlugins()
{
    m_filteredPlugins.clear();

    for (int i = 0; i < m_availablePlugins.size(); ++i)
    {
        const auto& desc = m_availablePlugins.getReference(i);

        // Apply text filter
        if (m_filterText.isEmpty())
        {
            m_filteredPlugins.add({ i, &desc });
        }
        else
        {
            juce::String searchText = m_filterText.toLowerCase();
            if (desc.name.toLowerCase().contains(searchText) ||
                desc.manufacturerName.toLowerCase().contains(searchText) ||
                desc.pluginFormatName.toLowerCase().contains(searchText))
            {
                m_filteredPlugins.add({ i, &desc });
            }
        }
    }

    m_pluginTable.updateContent();
    m_pluginTable.repaint();
}

void OfflinePluginDialog::onSearchTextChanged()
{
    m_filterText = m_searchBox.getText();
    updateFilteredPlugins();
}

void OfflinePluginDialog::onPluginSelected(int pluginIndex)
{
    if (pluginIndex < 0 || pluginIndex >= m_availablePlugins.size())
    {
        unloadCurrentPlugin();
        m_applyButton.setEnabled(false);
        m_previewButton.setEnabled(false);
        return;
    }

    m_selectedPluginIndex = pluginIndex;
    m_selectedPluginDescription = m_availablePlugins[pluginIndex];
    loadSelectedPlugin();
}

void OfflinePluginDialog::loadSelectedPlugin()
{
    // Unload any existing plugin
    unloadCurrentPlugin();

    auto& pm = PluginManager::getInstance();

    // Get sample rate and block size from buffer manager or use defaults
    double sampleRate = m_bufferManager ? m_bufferManager->getSampleRate() : 44100.0;
    int blockSize = 512;

    // Create plugin instance
    m_pluginInstance = pm.createPluginInstance(m_selectedPluginDescription, sampleRate, blockSize);

    if (m_pluginInstance != nullptr)
    {
        // Configure for stereo (same as PluginChainRenderer)
        const int processChannels = 2;
        m_pluginInstance->setPlayConfigDetails(processChannels, processChannels,
                                               sampleRate, blockSize);
        m_pluginInstance->prepareToPlay(sampleRate, blockSize);

        // Create editor
        createPluginEditor();

        m_applyButton.setEnabled(true);
        m_previewButton.setEnabled(m_audioEngine != nullptr);
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Plugin Load Failed",
            "Failed to load plugin: " + m_selectedPluginDescription.name,
            "OK");
        m_applyButton.setEnabled(false);
        m_previewButton.setEnabled(false);
    }
}

void OfflinePluginDialog::unloadCurrentPlugin()
{
    disablePreview();

    // Remove editor first
    if (m_pluginEditor != nullptr)
    {
        m_editorContainer->removeChildComponent(m_pluginEditor.get());
        m_pluginEditor.reset();
    }

    // Release plugin instance
    if (m_pluginInstance != nullptr)
    {
        m_pluginInstance->releaseResources();
        m_pluginInstance.reset();
    }

    m_selectedPluginIndex = -1;

    // Show "no plugin" label
    m_noPluginLabel.setVisible(true);
    resized();
}

void OfflinePluginDialog::createPluginEditor()
{
    if (m_pluginInstance == nullptr)
        return;

    // Hide "no plugin" label
    m_noPluginLabel.setVisible(false);

    // Create the plugin's editor
    m_pluginEditor.reset(m_pluginInstance->createEditor());

    if (m_pluginEditor != nullptr)
    {
        m_editorContainer->addAndMakeVisible(m_pluginEditor.get());

        // Size container to fit editor
        auto editorBounds = m_pluginEditor->getBounds();
        int editorWidth = juce::jmax(kMinEditorWidth, editorBounds.getWidth());
        int editorHeight = juce::jmax(kMinEditorHeight, editorBounds.getHeight());

        m_editorContainer->setSize(editorWidth, editorHeight);
        m_pluginEditor->setBounds(0, 0, editorBounds.getWidth(), editorBounds.getHeight());
    }
    else
    {
        // Plugin doesn't have a native editor - create generic
        m_pluginEditor.reset(new juce::GenericAudioProcessorEditor(*m_pluginInstance));
        m_editorContainer->addAndMakeVisible(m_pluginEditor.get());

        m_editorContainer->setSize(kMinEditorWidth, kMinEditorHeight);
        m_pluginEditor->setBounds(0, 0, kMinEditorWidth, kMinEditorHeight);
    }

    // Resize dialog to fit the plugin editor
    resizeToFitEditor();
}

void OfflinePluginDialog::resizeToFitEditor()
{
    if (m_pluginEditor == nullptr)
        return;

    // Get the plugin editor's preferred size
    auto editorBounds = m_pluginEditor->getBounds();
    int editorWidth = juce::jmax(kMinEditorWidth, editorBounds.getWidth());
    int editorHeight = juce::jmax(kMinEditorHeight, editorBounds.getHeight());

    // Calculate the new dialog size - side-by-side layout
    // Width: padding + editor + divider + browser + padding
    int dialogWidth = kPadding + editorWidth + kDividerWidth + kBrowserWidth + kPadding + 20;  // +20 for scrollbars
    dialogWidth = juce::jmax(dialogWidth, 750);  // Minimum width for side-by-side

    // Height: title + main content (max of editor height and browser) + render options + buttons
    int mainContentHeight = juce::jmax(editorHeight, 250);  // Min height for browser
    int dialogHeight = kPadding +                    // Top padding
                       30 +                          // Title
                       kSpacing +
                       mainContentHeight +           // Main content area
                       kSpacing +
                       kRenderOptionsHeight +        // Render options
                       kSpacing +
                       kButtonRowHeight +            // Buttons
                       kPadding;                     // Bottom padding

    // Set the size - this will update the parent DialogWindow
    setSize(dialogWidth, dialogHeight);

    // Force a resize to update layout
    resized();
}

//==============================================================================
void OfflinePluginDialog::onPreviewClicked()
{
    if (!m_audioEngine || !m_pluginInstance)
        return;

    // Toggle behavior: if preview is playing, stop it
    if (m_isPreviewPlaying && m_audioEngine->isPlaying())
    {
        disablePreview();
        return;
    }

    // Enable real-time preview
    enableRealtimePreview();
}

void OfflinePluginDialog::enableRealtimePreview()
{
    if (!m_audioEngine || !m_pluginInstance || !m_bufferManager)
        return;

    // Stop any current playback first
    m_audioEngine->stop();

    // Get the selection range
    int64_t startSample = m_selectionStart;
    int64_t numSamples = (m_selectionEnd > m_selectionStart)
                             ? (m_selectionEnd - m_selectionStart)
                             : m_bufferManager->getNumSamples();

    if (numSamples <= 0)
        return;

    // Enable looping based on checkbox
    bool shouldLoop = m_loopCheckbox.getToggleState();
    m_audioEngine->setLooping(shouldLoop);

    // Register the plugin with AudioEngine for real-time preview processing
    // This allows the plugin to receive audio and show visualizations
    m_audioEngine->setPreviewPluginInstance(m_pluginInstance.get());
    m_audioEngine->setPreviewMode(PreviewMode::REALTIME_DSP);

    // Set up playback position
    if (m_selectionEnd > m_selectionStart)
    {
        // Preview selection - set position to selection start
        double startSec = static_cast<double>(startSample) / m_bufferManager->getSampleRate();
        m_audioEngine->setPosition(startSec);

        // Set loop points if looping is enabled
        if (shouldLoop)
        {
            double endSec = static_cast<double>(m_selectionEnd) / m_bufferManager->getSampleRate();
            m_audioEngine->setLoopPoints(startSec, endSec);
        }
    }
    else
    {
        // No selection - play from beginning
        m_audioEngine->setPosition(0.0);
    }

    // Start playback
    m_audioEngine->play();
    m_isPreviewPlaying = true;
    m_isPreviewActive = true;

    // Update button appearance
    m_previewButton.setButtonText("Stop Preview");
    m_previewButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
}

void OfflinePluginDialog::onApplyClicked()
{
    if (!m_pluginInstance)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Plugin Selected",
            "Please select a plugin before applying.",
            "OK");
        return;
    }

    // Stop preview before applying
    disablePreview();

    // Build result
    Result result;
    result.applied = true;
    result.pluginDescription = m_selectedPluginDescription;
    m_pluginInstance->getStateInformation(result.pluginState);

    // Capture render options
    result.renderOptions.convertToStereo = m_convertToStereoCheckbox.getToggleState();
    result.renderOptions.includeTail = m_includeTailCheckbox.getToggleState();
    result.renderOptions.tailLengthSeconds = m_tailLengthSlider.getValue();

    m_result = result;

    // Close dialog
    if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
    {
        parent->exitModalState(1);
    }
}

void OfflinePluginDialog::onCancelClicked()
{
    disablePreview();
    m_result = std::nullopt;

    if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
    {
        parent->exitModalState(0);
    }
}

void OfflinePluginDialog::disablePreview()
{
    if (m_audioEngine && m_isPreviewActive)
    {
        m_audioEngine->stop();
        m_audioEngine->clearLoopPoints();
        m_audioEngine->setLooping(false);
        m_audioEngine->setPreviewPluginInstance(nullptr);
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);

        m_isPreviewActive = false;
        m_isPreviewPlaying = false;

        m_previewButton.setButtonText("Preview");
        m_previewButton.setColour(juce::TextButton::buttonColourId,
                                  getLookAndFeel().findColour(juce::TextButton::buttonColourId));
    }
}
