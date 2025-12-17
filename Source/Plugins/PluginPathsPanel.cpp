/*
  ==============================================================================

    PluginPathsPanel.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "PluginPathsPanel.h"

PluginPathsPanel::PluginPathsPanel()
{
    // Title
    m_titleLabel.setText("VST3 Plugin Search Paths", juce::dontSendNotification);
    m_titleLabel.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Description
    m_descriptionLabel.setText(
        "WaveEdit searches these directories for VST3 plugins. "
        "Default paths are always searched. You can add custom directories below.",
        juce::dontSendNotification);
    m_descriptionLabel.setFont(juce::FontOptions(11.0f));
    m_descriptionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    m_descriptionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_descriptionLabel);

    // Paths list
    m_pathsList.setModel(this);
    m_pathsList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1e1e1e));
    m_pathsList.setRowHeight(28);
    m_pathsList.setMultipleSelectionEnabled(false);
    addAndMakeVisible(m_pathsList);

    // Buttons
    m_addButton.setButtonText("Add Path...");
    m_addButton.onClick = [this]() { onAddPathClicked(); };
    addAndMakeVisible(m_addButton);

    m_removeButton.setButtonText("Remove");
    m_removeButton.onClick = [this]() { onRemovePathClicked(); };
    m_removeButton.setEnabled(false);
    addAndMakeVisible(m_removeButton);

    m_browseButton.setButtonText("Browse...");
    m_browseButton.onClick = [this]() { onBrowseClicked(); };
    addAndMakeVisible(m_browseButton);

    m_okButton.setButtonText("OK");
    m_okButton.onClick = [this]()
    {
        applyChanges();
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(1);
    };
    addAndMakeVisible(m_okButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]()
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(m_cancelButton);

    // Load paths
    refresh();

    setSize(500, 400);
}

PluginPathsPanel::~PluginPathsPanel() = default;

void PluginPathsPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));

    // Separator between default and custom paths
    auto bounds = getLocalBounds().reduced(20);
    bounds.removeFromTop(85);  // Skip title and description

    int separatorY = bounds.getY() + bounds.getHeight() / 2 - 50;
    g.setColour(juce::Colour(0xff444444));
    g.drawHorizontalLine(separatorY, 20.0f, static_cast<float>(getWidth() - 20));
}

void PluginPathsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // Title and description
    m_titleLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(5);
    m_descriptionLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(15);

    // Buttons at bottom
    auto buttonRow = bounds.removeFromBottom(35);
    m_cancelButton.setBounds(buttonRow.removeFromRight(80));
    buttonRow.removeFromRight(10);
    m_okButton.setBounds(buttonRow.removeFromRight(80));

    bounds.removeFromBottom(15);

    // Path management buttons
    auto pathButtonRow = bounds.removeFromBottom(30);
    m_addButton.setBounds(pathButtonRow.removeFromLeft(100));
    pathButtonRow.removeFromLeft(10);
    m_browseButton.setBounds(pathButtonRow.removeFromLeft(100));
    pathButtonRow.removeFromLeft(10);
    m_removeButton.setBounds(pathButtonRow.removeFromLeft(80));

    bounds.removeFromBottom(10);

    // Paths list takes remaining space
    m_pathsList.setBounds(bounds);
}

int PluginPathsPanel::getNumRows()
{
    return m_paths.size();
}

void PluginPathsPanel::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                          int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= m_paths.size())
        return;

    const auto& entry = m_paths[rowNumber];

    // Background
    if (rowIsSelected)
        g.fillAll(juce::Colour(0xff3a3a3a));
    else if (rowNumber % 2 == 1)
        g.fillAll(juce::Colour(0xff252525));
    else
        g.fillAll(juce::Colour(0xff1e1e1e));

    // Icon/indicator for default vs custom
    int iconWidth = 20;
    auto iconBounds = juce::Rectangle<int>(4, 0, iconWidth, height);

    if (entry.isDefault)
    {
        g.setColour(juce::Colours::grey);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("[S]", iconBounds, juce::Justification::centred); // System
    }
    else
    {
        g.setColour(juce::Colours::lightgreen);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("[C]", iconBounds, juce::Justification::centred); // Custom
    }

    // Path text
    g.setColour(entry.isDefault ? juce::Colours::grey : juce::Colours::white);
    g.setFont(juce::FontOptions(12.0f));

    // Check if path exists
    juce::File dir(entry.path);
    if (!dir.isDirectory())
    {
        g.setColour(juce::Colours::indianred);
    }

    g.drawText(entry.path, iconWidth + 8, 0, width - iconWidth - 12, height,
               juce::Justification::centredLeft, true);
}

void PluginPathsPanel::listBoxItemClicked(int row, const juce::MouseEvent& /*e*/)
{
    updateRemoveButtonState();
}

void PluginPathsPanel::refresh()
{
    m_paths.clear();

    auto& pm = PluginManager::getInstance();

    // Add default paths (system, read-only)
    auto defaultPaths = pm.getVST3SearchPaths();
    for (int i = 0; i < defaultPaths.getNumPaths(); ++i)
    {
        PathEntry entry;
        entry.path = defaultPaths[i].getFullPathName();
        entry.isDefault = true;
        m_paths.add(entry);
    }

    // Add custom paths
    for (const auto& path : pm.getCustomSearchPaths())
    {
        PathEntry entry;
        entry.path = path;
        entry.isDefault = false;
        m_paths.add(entry);
    }

    m_pathsList.updateContent();
    m_pathsList.repaint();
    updateRemoveButtonState();
}

void PluginPathsPanel::applyChanges()
{
    if (!m_hasChanges)
        return;

    // Collect custom paths only
    juce::StringArray customPaths;
    for (const auto& entry : m_paths)
    {
        if (!entry.isDefault)
        {
            customPaths.add(entry.path);
        }
    }

    // Save to PluginManager
    PluginManager::getInstance().setCustomSearchPaths(customPaths);

    m_hasChanges = false;
}

void PluginPathsPanel::onAddPathClicked()
{
    // Show text input dialog
    juce::AlertWindow alert("Add Custom Path", "Enter the full path to a VST3 plugin directory:",
                            juce::AlertWindow::QuestionIcon);
    alert.addTextEditor("path", "", "Path:");
    alert.addButton("OK", 1);
    alert.addButton("Cancel", 0);

    // Add to desktop and run modal loop
    alert.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    alert.setVisible(true);
    alert.toFront(true);

#if JUCE_MODAL_LOOPS_PERMITTED
    alert.enterModalState(true);
    int result = alert.runModalLoop();
#else
    int result = 0;
    jassertfalse; // Modal loops required
#endif

    if (result == 1)
    {
        juce::String path = alert.getTextEditorContents("path").trim();
        if (path.isNotEmpty())
        {
            juce::File dir(path);
            if (dir.isDirectory())
            {
                // Check for duplicates
                for (const auto& entry : m_paths)
                {
                    if (entry.path == path)
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::WarningIcon,
                            "Duplicate Path",
                            "This path is already in the list.");
                        return;
                    }
                }

                PathEntry entry;
                entry.path = path;
                entry.isDefault = false;
                m_paths.add(entry);

                m_hasChanges = true;
                m_pathsList.updateContent();
                m_pathsList.repaint();
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Invalid Path",
                    "The specified directory does not exist:\n" + path);
            }
        }
    }
}

void PluginPathsPanel::onRemovePathClicked()
{
    int selectedRow = m_pathsList.getSelectedRow();
    if (selectedRow < 0 || selectedRow >= m_paths.size())
        return;

    const auto& entry = m_paths[selectedRow];

    // Can only remove custom paths
    if (entry.isDefault)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Cannot Remove",
            "System default paths cannot be removed.");
        return;
    }

    m_paths.remove(selectedRow);
    m_hasChanges = true;

    m_pathsList.updateContent();
    m_pathsList.repaint();
    updateRemoveButtonState();
}

void PluginPathsPanel::onBrowseClicked()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select VST3 Plugin Directory",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*", true);

    int flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;

    chooser->launchAsync(flags, [this, chooser](const juce::FileChooser& fc)
    {
        juce::File dir = fc.getResult();
        if (!dir.isDirectory())
            return;

        juce::String path = dir.getFullPathName();

        // Check for duplicates
        for (const auto& entry : m_paths)
        {
            if (entry.path == path)
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Duplicate Path",
                    "This path is already in the list.");
                return;
            }
        }

        PathEntry entry;
        entry.path = path;
        entry.isDefault = false;
        m_paths.add(entry);

        m_hasChanges = true;
        m_pathsList.updateContent();
        m_pathsList.repaint();
    });
}

void PluginPathsPanel::updateRemoveButtonState()
{
    int selectedRow = m_pathsList.getSelectedRow();
    bool canRemove = false;

    if (selectedRow >= 0 && selectedRow < m_paths.size())
    {
        canRemove = !m_paths[selectedRow].isDefault;
    }

    m_removeButton.setEnabled(canRemove);
}

void PluginPathsPanel::showDialog()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Create the dialog content
    std::unique_ptr<PluginPathsPanel> panel(new PluginPathsPanel());

    // Create the dialog window
    juce::DialogWindow dlg("VST3 Plugin Paths", juce::Colour(0xff2a2a2a), true, false);
    dlg.setContentOwned(panel.release(), true);
    dlg.centreWithSize(500, 400);
    dlg.setResizable(true, true);
    dlg.setUsingNativeTitleBar(true);

    // Add to desktop
    dlg.addToDesktop(juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowHasCloseButton);
    dlg.setVisible(true);
    dlg.toFront(true);

    // Run modal loop
#if JUCE_MODAL_LOOPS_PERMITTED
    dlg.enterModalState(true);
    dlg.runModalLoop();
#else
    jassertfalse; // Modal loops are required
#endif
}
