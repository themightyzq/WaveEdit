/*
  ==============================================================================

    ShortcutEditorPanel.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "ShortcutEditorPanel.h"
#include "../Commands/CommandIDs.h"
#include <juce_data_structures/juce_data_structures.h>

//==============================================================================
// KeyPressCaptureWindow - Modal dialog for capturing keyboard shortcuts
//==============================================================================

/**
 * Modal window that captures a single keypress for shortcut assignment.
 * Displays instructions and accepts any keyboard input (except Escape to cancel).
 */
class KeyPressCaptureWindow : public juce::DialogWindow,
                               public juce::KeyListener
{
public:
    KeyPressCaptureWindow(const juce::String& commandName)
        : juce::DialogWindow("Set Keyboard Shortcut", juce::Colours::darkgrey, true)
    {
        setUsingNativeTitleBar(true);

        // Create instruction label
        auto* content = new juce::Component();
        content->setSize(400, 150);

        m_instructionLabel.setText(
            "Press a key combination to assign to:\n\n" + commandName +
            "\n\nPress ESC to cancel\nPress DELETE/BACKSPACE to clear shortcut",
            juce::dontSendNotification
        );
        m_instructionLabel.setFont(juce::FontOptions(16.0f));
        m_instructionLabel.setJustificationType(juce::Justification::centred);
        m_instructionLabel.setBounds(10, 10, 380, 130);

        content->addAndMakeVisible(m_instructionLabel);

        setContentOwned(content, true);
        centreWithSize(getWidth(), getHeight());

        // Add key listener to capture keypresses
        addKeyListener(this);
        setWantsKeyboardFocus(true);
        grabKeyboardFocus();
    }

    ~KeyPressCaptureWindow() override
    {
        removeKeyListener(this);
    }

    /**
     * Get the captured keypress.
     * Returns invalid KeyPress if cancelled or cleared.
     */
    juce::KeyPress getCapturedKeyPress() const { return m_capturedKey; }

    /**
     * Check if user wants to clear the shortcut.
     */
    bool shouldClearShortcut() const { return m_clearShortcut; }

    // KeyListener interface
    bool keyPressed(const juce::KeyPress& key, juce::Component* /*originatingComponent*/) override
    {
        // ESC cancels
        if (key == juce::KeyPress::escapeKey)
        {
            m_capturedKey = juce::KeyPress();
            exitModalState(0);
            return true;
        }

        // DELETE or BACKSPACE clears shortcut
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            m_clearShortcut = true;
            m_capturedKey = juce::KeyPress();
            exitModalState(1);
            return true;
        }

        // Capture the keypress
        m_capturedKey = key;
        exitModalState(1);
        return true;
    }

private:
    juce::Label m_instructionLabel;
    juce::KeyPress m_capturedKey;
    bool m_clearShortcut {false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyPressCaptureWindow)
};

//==============================================================================
// ShortcutEditorPanel Implementation
//==============================================================================

ShortcutEditorPanel::ShortcutEditorPanel(juce::ApplicationCommandManager& commandManager)
    : m_commandManager(commandManager)
{
    // Title label
    m_titleLabel.setText("Keyboard Shortcuts", juce::dontSendNotification);
    m_titleLabel.setFont(juce::FontOptions(20.0f).withStyle("Bold"));
    m_titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_titleLabel);

    // Search label and box
    m_searchLabel.setText("Search:", juce::dontSendNotification);
    m_searchLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_searchLabel);

    m_searchBox.setTextToShowWhenEmpty("Filter commands...", juce::Colours::grey);
    m_searchBox.addListener(this);
    addAndMakeVisible(m_searchBox);

    // Table setup
    m_table.setModel(this);
    m_table.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1e1e1e));
    m_table.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff3e3e3e));
    m_table.getHeader().addColumn("Category", 1, 120, 80, 200, juce::TableHeaderComponent::notSortable);
    m_table.getHeader().addColumn("Command", 2, 280, 150, 400, juce::TableHeaderComponent::notSortable);
    m_table.getHeader().addColumn("Shortcut", 3, 180, 100, 300, juce::TableHeaderComponent::notSortable);
    m_table.setHeaderHeight(24);
    m_table.setRowHeight(22);
    addAndMakeVisible(m_table);

    // Buttons
    m_exportButton.addListener(this);
    addAndMakeVisible(m_exportButton);

    m_importButton.addListener(this);
    addAndMakeVisible(m_importButton);

    m_resetButton.addListener(this);
    addAndMakeVisible(m_resetButton);

    // Load commands from manager
    refreshCommandList();

    setSize(800, 600);
}

ShortcutEditorPanel::~ShortcutEditorPanel()
{
    m_searchBox.removeListener(this);
    m_exportButton.removeListener(this);
    m_importButton.removeListener(this);
    m_resetButton.removeListener(this);
}

//==============================================================================
// Component overrides

void ShortcutEditorPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2e2e2e));

    // Draw separator line below title
    g.setColour(juce::Colour(0xff3e3e3e));
    g.drawLine(10.0f, 50.0f, (float)getWidth() - 10.0f, 50.0f, 1.0f);
}

void ShortcutEditorPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Title at top
    m_titleLabel.setBounds(bounds.removeFromTop(30));

    bounds.removeFromTop(20); // Spacing

    // Search row
    auto searchRow = bounds.removeFromTop(28);
    m_searchLabel.setBounds(searchRow.removeFromLeft(80));
    searchRow.removeFromLeft(5);
    m_searchBox.setBounds(searchRow.removeFromLeft(300));

    bounds.removeFromTop(10); // Spacing

    // Button row at bottom
    auto buttonRow = bounds.removeFromBottom(32);
    m_resetButton.setBounds(buttonRow.removeFromRight(150));
    buttonRow.removeFromRight(10);
    m_importButton.setBounds(buttonRow.removeFromRight(100));
    buttonRow.removeFromRight(10);
    m_exportButton.setBounds(buttonRow.removeFromRight(100));

    bounds.removeFromBottom(10); // Spacing

    // Table fills remaining space
    m_table.setBounds(bounds);
}

//==============================================================================
// TableListBoxModel overrides

int ShortcutEditorPanel::getNumRows()
{
    return m_filteredCommands.size();
}

void ShortcutEditorPanel::paintRowBackground(juce::Graphics& g, int rowNumber,
                                              int /*width*/, int /*height*/, bool rowIsSelected)
{
    // Check if this command has conflicts
    bool hasConflict = false;
    if (rowNumber >= 0 && rowNumber < m_filteredCommands.size())
    {
        hasConflict = m_filteredCommands.getReference(rowNumber).hasConflict;
    }

    if (rowIsSelected)
    {
        g.fillAll(juce::Colour(0xff4e4e4e));
    }
    else if (hasConflict)
    {
        // Red background for conflicts
        g.fillAll(juce::Colour(0xff4e2e2e));
    }
    else if (rowNumber % 2 == 0)
    {
        g.fillAll(juce::Colour(0xff2a2a2a));
    }
    else
    {
        g.fillAll(juce::Colour(0xff1e1e1e));
    }
}

void ShortcutEditorPanel::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                                     int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= m_filteredCommands.size())
        return;

    const auto& cmd = m_filteredCommands.getReference(rowNumber);

    g.setColour(rowIsSelected ? juce::Colours::white : juce::Colour(0xffcccccc));
    g.setFont(14.0f);

    juce::String text;
    switch (columnId)
    {
        case 1: // Category
            text = cmd.category;
            break;
        case 2: // Command Name
            text = cmd.commandName;
            break;
        case 3: // Shortcut
            text = cmd.keyPress.isValid() ? cmd.keyPress.getTextDescription() : "(none)";
            if (cmd.hasConflict)
            {
                text += " ⚠️"; // Warning icon for conflicts
            }
            break;
    }

    g.drawText(text, 5, 0, width - 10, height, juce::Justification::centredLeft, true);
}

void ShortcutEditorPanel::cellClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent& /*event*/)
{
    m_selectedRow = rowNumber;
}

void ShortcutEditorPanel::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& /*event*/)
{
    if (columnId == 3) // Only allow editing shortcut column
    {
        showKeypressCaptureDialog(rowNumber);
    }
}

juce::String ShortcutEditorPanel::getCellTooltip(int rowNumber, int columnId)
{
    if (rowNumber < 0 || rowNumber >= m_filteredCommands.size())
        return {};

    const auto& cmd = m_filteredCommands.getReference(rowNumber);

    if (columnId == 3 && cmd.hasConflict)
    {
        juce::String tooltip = "Shortcut conflict detected with:\n";
        for (auto conflictID : cmd.conflictingCommands)
        {
            tooltip += "  • " + getCommandName(conflictID) + "\n";
        }
        return tooltip;
    }

    if (columnId == 2 && cmd.description.isNotEmpty())
    {
        return cmd.description;
    }

    return {};
}

//==============================================================================
// Button::Listener overrides

void ShortcutEditorPanel::buttonClicked(juce::Button* button)
{
    if (button == &m_exportButton)
    {
        exportKeybindings();
    }
    else if (button == &m_importButton)
    {
        importKeybindings();
    }
    else if (button == &m_resetButton)
    {
        // Confirm reset
        auto result = juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::QuestionIcon,
            "Reset to Defaults",
            "Are you sure you want to reset all keyboard shortcuts to Sound Forge Pro defaults?\n\n"
            "This will discard any customizations you've made.",
            "Reset",
            "Cancel"
        );

        if (result)
        {
            resetToDefaults();
        }
    }
}

//==============================================================================
// TextEditor::Listener overrides

void ShortcutEditorPanel::textEditorTextChanged(juce::TextEditor& /*editor*/)
{
    updateFilteredCommands();
}

//==============================================================================
// Shortcut management

void ShortcutEditorPanel::refreshCommandList()
{
    m_allCommands.clear();
    m_originalCommands.clear();

    loadCommandsFromManager();

    // Save original state for revert
    m_originalCommands = m_allCommands;

    updateFilteredCommands();
    detectConflicts();

    m_hasUnsavedChanges = false;
}

void ShortcutEditorPanel::applyChanges()
{
    auto* keyMappings = m_commandManager.getKeyMappings();
    if (keyMappings == nullptr)
        return;

    // Apply each command's shortcut to the command manager
    for (const auto& cmd : m_allCommands)
    {
        keyMappings->clearAllKeyPresses(cmd.commandID);

        if (cmd.keyPress.isValid())
        {
            keyMappings->addKeyPress(cmd.commandID, cmd.keyPress);
        }
    }

    // Save original state
    m_originalCommands = m_allCommands;
    m_hasUnsavedChanges = false;

    juce::Logger::writeToLog("Keyboard shortcuts applied successfully");
}

void ShortcutEditorPanel::revertChanges()
{
    // Restore from original state
    m_allCommands = m_originalCommands;

    updateFilteredCommands();
    detectConflicts();
    m_table.repaint();

    m_hasUnsavedChanges = false;

    juce::Logger::writeToLog("Keyboard shortcuts reverted to saved state");
}

void ShortcutEditorPanel::resetToDefaults()
{
    // Reset all commands to their default keypresses
    for (auto& cmd : m_allCommands)
    {
        cmd.keyPress = getDefaultKeypressForCommand(cmd.commandID);
        cmd.hasConflict = false;
        cmd.conflictingCommands.clear();
    }

    updateFilteredCommands();
    detectConflicts();
    m_table.repaint();

    m_hasUnsavedChanges = true;

    juce::Logger::writeToLog("Keyboard shortcuts reset to Sound Forge Pro defaults");
}

bool ShortcutEditorPanel::exportKeybindings()
{
    juce::FileChooser chooser("Export Keyboard Shortcuts",
                              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                              "*.json");

    if (chooser.browseForFileToSave(true))
    {
        auto file = chooser.getResult();

        // Create JSON structure
        juce::var keybindingsObject = new juce::DynamicObject();
        auto* obj = keybindingsObject.getDynamicObject();

        obj->setProperty("version", "1.0");
        obj->setProperty("profile", "Custom");

        juce::Array<juce::var> bindings;
        for (const auto& cmd : m_allCommands)
        {
            if (cmd.keyPress.isValid())
            {
                juce::var binding = new juce::DynamicObject();
                auto* bindingObj = binding.getDynamicObject();

                bindingObj->setProperty("commandID", (int)cmd.commandID);
                bindingObj->setProperty("commandName", cmd.commandName);
                bindingObj->setProperty("keyPress", cmd.keyPress.getTextDescription());

                bindings.add(binding);
            }
        }

        obj->setProperty("keybindings", bindings);

        // Write to file
        auto json = juce::JSON::toString(keybindingsObject, true);

        if (file.replaceWithText(json))
        {
            juce::Logger::writeToLog("Keyboard shortcuts exported to: " + file.getFullPathName());

            juce::AlertWindow::showMessageBox(
                juce::AlertWindow::InfoIcon,
                "Export Successful",
                "Keyboard shortcuts exported to:\n" + file.getFullPathName()
            );

            return true;
        }
        else
        {
            juce::Logger::writeToLog("Failed to export keyboard shortcuts to: " + file.getFullPathName());

            juce::AlertWindow::showMessageBox(
                juce::AlertWindow::WarningIcon,
                "Export Failed",
                "Failed to write keyboard shortcuts to file."
            );

            return false;
        }
    }

    return false;
}

bool ShortcutEditorPanel::importKeybindings()
{
    juce::FileChooser chooser("Import Keyboard Shortcuts",
                              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                              "*.json");

    if (chooser.browseForFileToOpen())
    {
        auto file = chooser.getResult();

        // Read JSON file
        auto jsonText = file.loadFileAsString();
        auto result = juce::JSON::parse(jsonText);

        if (!result.isObject())
        {
            juce::AlertWindow::showMessageBox(
                juce::AlertWindow::WarningIcon,
                "Import Failed",
                "Invalid JSON format in keybindings file."
            );
            return false;
        }

        auto* obj = result.getDynamicObject();
        if (obj == nullptr)
        {
            juce::AlertWindow::showMessageBox(
                juce::AlertWindow::WarningIcon,
                "Import Failed",
                "Invalid keybindings file format."
            );
            return false;
        }

        // Verify version
        auto version = obj->getProperty("version").toString();
        if (version != "1.0")
        {
            juce::AlertWindow::showMessageBox(
                juce::AlertWindow::WarningIcon,
                "Import Failed",
                "Unsupported keybindings file version: " + version
            );
            return false;
        }

        // Parse keybindings
        auto* bindingsArray = obj->getProperty("keybindings").getArray();
        if (bindingsArray == nullptr)
        {
            juce::AlertWindow::showMessageBox(
                juce::AlertWindow::WarningIcon,
                "Import Failed",
                "No keybindings found in file."
            );
            return false;
        }

        // Clear all current shortcuts
        for (auto& cmd : m_allCommands)
        {
            cmd.keyPress = juce::KeyPress();
            cmd.hasConflict = false;
            cmd.conflictingCommands.clear();
        }

        // Apply imported bindings
        int importedCount = 0;
        for (const auto& bindingVar : *bindingsArray)
        {
            auto* bindingObj = bindingVar.getDynamicObject();
            if (bindingObj == nullptr)
                continue;

            auto commandID = (juce::CommandID)((int)bindingObj->getProperty("commandID"));
            auto keyPressText = bindingObj->getProperty("keyPress").toString();

            // Find command in our list
            for (auto& cmd : m_allCommands)
            {
                if (cmd.commandID == commandID)
                {
                    cmd.keyPress = juce::KeyPress::createFromDescription(keyPressText);
                    importedCount++;
                    break;
                }
            }
        }

        updateFilteredCommands();
        detectConflicts();
        m_table.repaint();

        m_hasUnsavedChanges = true;

        juce::Logger::writeToLog("Imported " + juce::String(importedCount) +
                                " keyboard shortcuts from: " + file.getFullPathName());

        juce::AlertWindow::showMessageBox(
            juce::AlertWindow::InfoIcon,
            "Import Successful",
            "Imported " + juce::String(importedCount) + " keyboard shortcuts.\n\n"
            "Click 'Apply' to save changes or 'Revert' to undo."
        );

        return true;
    }

    return false;
}

bool ShortcutEditorPanel::hasUnsavedChanges() const
{
    return m_hasUnsavedChanges;
}

//==============================================================================
// Private methods

void ShortcutEditorPanel::loadCommandsFromManager()
{
    // Get all command IDs by iterating through CommandIDs namespace
    // Since we don't have direct access to ApplicationCommandTarget, we'll use known command IDs
    juce::Array<juce::CommandID> commandIDs;

    // File Operations (0x1000 - 0x10FF)
    commandIDs.add(CommandIDs::fileNew);
    commandIDs.add(CommandIDs::fileOpen);
    commandIDs.add(CommandIDs::fileSave);
    commandIDs.add(CommandIDs::fileSaveAs);
    commandIDs.add(CommandIDs::fileClose);
    commandIDs.add(CommandIDs::fileProperties);
    commandIDs.add(CommandIDs::fileExit);
    commandIDs.add(CommandIDs::filePreferences);

    // Edit Operations (0x2000 - 0x20FF)
    commandIDs.add(CommandIDs::editUndo);
    commandIDs.add(CommandIDs::editRedo);
    commandIDs.add(CommandIDs::editCut);
    commandIDs.add(CommandIDs::editCopy);
    commandIDs.add(CommandIDs::editPaste);
    commandIDs.add(CommandIDs::editDelete);
    commandIDs.add(CommandIDs::editSelectAll);
    commandIDs.add(CommandIDs::editSilence);
    commandIDs.add(CommandIDs::editTrim);

    // Playback Operations (0x3000 - 0x30FF)
    commandIDs.add(CommandIDs::playbackPlay);
    commandIDs.add(CommandIDs::playbackPause);
    commandIDs.add(CommandIDs::playbackStop);
    commandIDs.add(CommandIDs::playbackLoop);
    commandIDs.add(CommandIDs::playbackRecord);

    // View Operations (0x4000 - 0x40FF)
    commandIDs.add(CommandIDs::viewZoomIn);
    commandIDs.add(CommandIDs::viewZoomOut);
    commandIDs.add(CommandIDs::viewZoomFit);
    commandIDs.add(CommandIDs::viewZoomSelection);
    commandIDs.add(CommandIDs::viewZoomOneToOne);
    commandIDs.add(CommandIDs::viewCycleTimeFormat);
    commandIDs.add(CommandIDs::viewAutoScroll);
    commandIDs.add(CommandIDs::viewZoomToRegion);
    commandIDs.add(CommandIDs::viewAutoPreviewRegions);

    // Processing Operations (0x5000 - 0x50FF)
    commandIDs.add(CommandIDs::processFadeIn);
    commandIDs.add(CommandIDs::processFadeOut);
    commandIDs.add(CommandIDs::processNormalize);
    commandIDs.add(CommandIDs::processDCOffset);
    commandIDs.add(CommandIDs::processGain);
    commandIDs.add(CommandIDs::processIncreaseGain);
    commandIDs.add(CommandIDs::processDecreaseGain);

    // Navigation Operations (0x6000 - 0x60FF)
    commandIDs.add(CommandIDs::navigateLeft);
    commandIDs.add(CommandIDs::navigateRight);
    commandIDs.add(CommandIDs::navigateStart);
    commandIDs.add(CommandIDs::navigateEnd);
    commandIDs.add(CommandIDs::navigatePageLeft);
    commandIDs.add(CommandIDs::navigatePageRight);
    commandIDs.add(CommandIDs::navigateHomeVisible);
    commandIDs.add(CommandIDs::navigateEndVisible);
    commandIDs.add(CommandIDs::navigateCenterView);
    commandIDs.add(CommandIDs::navigateGoToPosition);

    // Selection Operations (0x7000 - 0x70FF)
    commandIDs.add(CommandIDs::selectExtendLeft);
    commandIDs.add(CommandIDs::selectExtendRight);
    commandIDs.add(CommandIDs::selectExtendStart);
    commandIDs.add(CommandIDs::selectExtendEnd);
    commandIDs.add(CommandIDs::selectExtendPageLeft);
    commandIDs.add(CommandIDs::selectExtendPageRight);

    // Snap Operations (0x8000 - 0x80FF)
    commandIDs.add(CommandIDs::snapCycleMode);
    commandIDs.add(CommandIDs::snapToggleZeroCrossing);
    commandIDs.add(CommandIDs::snapPreferences);

    // Help Operations (0x9000 - 0x90FF)
    commandIDs.add(CommandIDs::helpAbout);
    commandIDs.add(CommandIDs::helpShortcuts);

    // Tab Operations (0xA000 - 0xA0FF)
    commandIDs.add(CommandIDs::tabClose);
    commandIDs.add(CommandIDs::tabCloseAll);
    commandIDs.add(CommandIDs::tabNext);
    commandIDs.add(CommandIDs::tabPrevious);
    commandIDs.add(CommandIDs::tabSelect1);
    commandIDs.add(CommandIDs::tabSelect2);
    commandIDs.add(CommandIDs::tabSelect3);
    commandIDs.add(CommandIDs::tabSelect4);
    commandIDs.add(CommandIDs::tabSelect5);
    commandIDs.add(CommandIDs::tabSelect6);
    commandIDs.add(CommandIDs::tabSelect7);
    commandIDs.add(CommandIDs::tabSelect8);
    commandIDs.add(CommandIDs::tabSelect9);

    // Region Operations (0xB000 - 0xB0FF)
    commandIDs.add(CommandIDs::regionAdd);
    commandIDs.add(CommandIDs::regionDelete);
    commandIDs.add(CommandIDs::regionNext);
    commandIDs.add(CommandIDs::regionPrevious);
    commandIDs.add(CommandIDs::regionSelectInverse);
    commandIDs.add(CommandIDs::regionSelectAll);
    commandIDs.add(CommandIDs::regionStripSilence);
    commandIDs.add(CommandIDs::regionExportAll);
    commandIDs.add(CommandIDs::regionShowList);
    commandIDs.add(CommandIDs::regionSnapToZeroCrossing);
    commandIDs.add(CommandIDs::regionNudgeStartLeft);
    commandIDs.add(CommandIDs::regionNudgeStartRight);
    commandIDs.add(CommandIDs::regionNudgeEndLeft);
    commandIDs.add(CommandIDs::regionNudgeEndRight);
    commandIDs.add(CommandIDs::regionBatchRename);
    commandIDs.add(CommandIDs::regionMerge);
    commandIDs.add(CommandIDs::regionSplit);
    commandIDs.add(CommandIDs::regionCopy);
    commandIDs.add(CommandIDs::regionPaste);

    // Marker Operations (0xC000 - 0xC0FF)
    commandIDs.add(CommandIDs::markerAdd);
    commandIDs.add(CommandIDs::markerDelete);
    commandIDs.add(CommandIDs::markerNext);
    commandIDs.add(CommandIDs::markerPrevious);
    commandIDs.add(CommandIDs::markerShowList);

    // Now populate command info for each ID
    for (auto commandID : commandIDs)
    {
        // Get current keypress from KeyMappings
        auto* keyMappings = m_commandManager.getKeyMappings();
        juce::KeyPress currentKey;

        if (keyMappings != nullptr)
        {
            auto keys = keyMappings->getKeyPressesAssignedToCommand(commandID);
            if (!keys.isEmpty())
            {
                currentKey = keys[0]; // Use first assigned keypress
            }
        }

        // Get command name using the command manager's target
        juce::String commandName = getCommandName(commandID);

        // If no current key, try to get default
        if (!currentKey.isValid())
        {
            currentKey = getDefaultKeypressForCommand(commandID);
        }

        CommandShortcut cmd(
            commandID,
            commandName,
            getCategoryForCommand(commandID),
            currentKey,
            "" // Description can be empty for now
        );

        m_allCommands.add(cmd);
    }

    juce::Logger::writeToLog("Loaded " + juce::String(m_allCommands.size()) + " commands from ApplicationCommandManager");
}

juce::KeyPress ShortcutEditorPanel::getDefaultKeypressForCommand(juce::CommandID commandID)
{
    // CRITICAL FIX: Do NOT query command targets for info!
    // The original code used invokeDirectly() which actually executed commands,
    // causing infinite recursion when querying filePreferences:
    // ShortcutEditor queries filePreferences → opens Preferences → creates ShortcutEditor → repeat
    //
    // We rely on the ApplicationCommandManager's existing key mappings instead.
    // The KeymapManager (in Main.cpp) has already set up all shortcuts from templates.

    // Get the current key mappings from the command manager
    auto mappings = m_commandManager.getKeyMappings();
    if (mappings != nullptr)
    {
        // Get the array of keys assigned to this command
        auto keys = mappings->getKeyPressesAssignedToCommand(commandID);
        if (!keys.isEmpty())
            return keys.getFirst();
    }

    return juce::KeyPress();
}

juce::String ShortcutEditorPanel::getCommandName(juce::CommandID commandID)
{
    // Map command IDs to readable names
    // This is a simple lookup - ideally we'd query the ApplicationCommandTarget
    switch (commandID)
    {
        // File Operations
        case CommandIDs::fileNew: return "New";
        case CommandIDs::fileOpen: return "Open...";
        case CommandIDs::fileSave: return "Save";
        case CommandIDs::fileSaveAs: return "Save As...";
        case CommandIDs::fileClose: return "Close";
        case CommandIDs::fileProperties: return "Properties";
        case CommandIDs::fileExit: return "Quit";
        case CommandIDs::filePreferences: return "Preferences...";

        // Edit Operations
        case CommandIDs::editUndo: return "Undo";
        case CommandIDs::editRedo: return "Redo";
        case CommandIDs::editCut: return "Cut";
        case CommandIDs::editCopy: return "Copy";
        case CommandIDs::editPaste: return "Paste";
        case CommandIDs::editDelete: return "Delete";
        case CommandIDs::editSelectAll: return "Select All";
        case CommandIDs::editSilence: return "Insert Silence";
        case CommandIDs::editTrim: return "Trim";

        // Playback Operations
        case CommandIDs::playbackPlay: return "Play";
        case CommandIDs::playbackPause: return "Pause";
        case CommandIDs::playbackStop: return "Stop";
        case CommandIDs::playbackLoop: return "Loop";
        case CommandIDs::playbackRecord: return "Record";

        // View Operations
        case CommandIDs::viewZoomIn: return "Zoom In";
        case CommandIDs::viewZoomOut: return "Zoom Out";
        case CommandIDs::viewZoomFit: return "Zoom to Fit";
        case CommandIDs::viewZoomSelection: return "Zoom to Selection";
        case CommandIDs::viewZoomOneToOne: return "Zoom 1:1";
        case CommandIDs::viewCycleTimeFormat: return "Cycle Time Format";
        case CommandIDs::viewAutoScroll: return "Auto-Scroll";
        case CommandIDs::viewZoomToRegion: return "Zoom to Region";
        case CommandIDs::viewAutoPreviewRegions: return "Auto-Preview Regions";

        // Processing Operations
        case CommandIDs::processFadeIn: return "Fade In";
        case CommandIDs::processFadeOut: return "Fade Out";
        case CommandIDs::processNormalize: return "Normalize";
        case CommandIDs::processDCOffset: return "DC Offset";
        case CommandIDs::processGain: return "Gain...";
        case CommandIDs::processIncreaseGain: return "Increase Gain (+1dB)";
        case CommandIDs::processDecreaseGain: return "Decrease Gain (-1dB)";

        // Navigation Operations
        case CommandIDs::navigateLeft: return "Move Left";
        case CommandIDs::navigateRight: return "Move Right";
        case CommandIDs::navigateStart: return "Go to Start";
        case CommandIDs::navigateEnd: return "Go to End";
        case CommandIDs::navigatePageLeft: return "Page Left";
        case CommandIDs::navigatePageRight: return "Page Right";
        case CommandIDs::navigateHomeVisible: return "Home (Visible Start)";
        case CommandIDs::navigateEndVisible: return "End (Visible End)";
        case CommandIDs::navigateCenterView: return "Center View";
        case CommandIDs::navigateGoToPosition: return "Go to Position...";

        // Selection Operations
        case CommandIDs::selectExtendLeft: return "Extend Selection Left";
        case CommandIDs::selectExtendRight: return "Extend Selection Right";
        case CommandIDs::selectExtendStart: return "Extend to Visible Start";
        case CommandIDs::selectExtendEnd: return "Extend to Visible End";
        case CommandIDs::selectExtendPageLeft: return "Extend Selection Page Left";
        case CommandIDs::selectExtendPageRight: return "Extend Selection Page Right";

        // Snap Operations
        case CommandIDs::snapCycleMode: return "Toggle Snap";
        case CommandIDs::snapToggleZeroCrossing: return "Toggle Zero Crossing";
        case CommandIDs::snapPreferences: return "Snap Preferences...";

        // Help Operations
        case CommandIDs::helpAbout: return "About WaveEdit";
        case CommandIDs::helpShortcuts: return "Keyboard Shortcuts";

        // Tab Operations
        case CommandIDs::tabClose: return "Close Tab";
        case CommandIDs::tabCloseAll: return "Close All Tabs";
        case CommandIDs::tabNext: return "Next Tab";
        case CommandIDs::tabPrevious: return "Previous Tab";
        case CommandIDs::tabSelect1: return "Jump to Tab 1";
        case CommandIDs::tabSelect2: return "Jump to Tab 2";
        case CommandIDs::tabSelect3: return "Jump to Tab 3";
        case CommandIDs::tabSelect4: return "Jump to Tab 4";
        case CommandIDs::tabSelect5: return "Jump to Tab 5";
        case CommandIDs::tabSelect6: return "Jump to Tab 6";
        case CommandIDs::tabSelect7: return "Jump to Tab 7";
        case CommandIDs::tabSelect8: return "Jump to Tab 8";
        case CommandIDs::tabSelect9: return "Jump to Tab 9";

        // Region Operations
        case CommandIDs::regionAdd: return "Add Region";
        case CommandIDs::regionDelete: return "Delete Region";
        case CommandIDs::regionNext: return "Next Region";
        case CommandIDs::regionPrevious: return "Previous Region";
        case CommandIDs::regionSelectInverse: return "Select Inverse";
        case CommandIDs::regionSelectAll: return "Select All Regions";
        case CommandIDs::regionStripSilence: return "Strip Silence...";
        case CommandIDs::regionExportAll: return "Export All Regions...";
        case CommandIDs::regionShowList: return "Show Region List";
        case CommandIDs::regionSnapToZeroCrossing: return "Snap Regions to Zero Crossing";
        case CommandIDs::regionNudgeStartLeft: return "Nudge Region Start Left";
        case CommandIDs::regionNudgeStartRight: return "Nudge Region Start Right";
        case CommandIDs::regionNudgeEndLeft: return "Nudge Region End Left";
        case CommandIDs::regionNudgeEndRight: return "Nudge Region End Right";
        case CommandIDs::regionBatchRename: return "Batch Rename Regions...";
        case CommandIDs::regionMerge: return "Merge Regions";
        case CommandIDs::regionSplit: return "Split Region";
        case CommandIDs::regionCopy: return "Copy Region Definitions";
        case CommandIDs::regionPaste: return "Paste Region Definitions";

        // Marker Operations
        case CommandIDs::markerAdd: return "Add Marker";
        case CommandIDs::markerDelete: return "Delete Marker";
        case CommandIDs::markerNext: return "Next Marker";
        case CommandIDs::markerPrevious: return "Previous Marker";
        case CommandIDs::markerShowList: return "Show Marker List";

        default: return "Unknown Command";
    }
}

juce::String ShortcutEditorPanel::getCategoryForCommand(juce::CommandID commandID)
{
    // Determine category based on command ID ranges (from CommandIDs.h)
    if (commandID >= 0x1000 && commandID < 0x2000)
        return "File";
    else if (commandID >= 0x2000 && commandID < 0x3000)
        return "Edit";
    else if (commandID >= 0x3000 && commandID < 0x4000)
        return "Playback";
    else if (commandID >= 0x4000 && commandID < 0x5000)
        return "View";
    else if (commandID >= 0x5000 && commandID < 0x6000)
        return "Processing";
    else if (commandID >= 0x6000 && commandID < 0x7000)
        return "Navigation";
    else if (commandID >= 0x7000 && commandID < 0x8000)
        return "Selection";
    else if (commandID >= 0x8000 && commandID < 0x9000)
        return "Snap";
    else if (commandID >= 0x9000 && commandID < 0xA000)
        return "Help";
    else if (commandID >= 0xA000 && commandID < 0xB000)
        return "Tab";
    else if (commandID >= 0xB000 && commandID < 0xC000)
        return "Region";
    else if (commandID >= 0xC000 && commandID < 0xD000)
        return "Marker";
    else
        return "Other";
}

void ShortcutEditorPanel::updateFilteredCommands()
{
    m_filteredCommands.clear();

    juce::String filterText = m_searchBox.getText();

    for (const auto& cmd : m_allCommands)
    {
        if (cmd.matchesFilter(filterText))
        {
            m_filteredCommands.add(cmd);
        }
    }

    m_table.updateContent();
    m_table.repaint();
}

void ShortcutEditorPanel::detectConflicts()
{
    // Clear all existing conflict flags
    for (auto& cmd : m_allCommands)
    {
        cmd.hasConflict = false;
        cmd.conflictingCommands.clear();
    }

    for (auto& cmd : m_filteredCommands)
    {
        cmd.hasConflict = false;
        cmd.conflictingCommands.clear();
    }

    // Check for duplicate keypresses
    for (int i = 0; i < m_allCommands.size(); ++i)
    {
        auto& cmd1 = m_allCommands.getReference(i);

        if (!cmd1.keyPress.isValid())
            continue; // Skip commands with no shortcut

        for (int j = i + 1; j < m_allCommands.size(); ++j)
        {
            auto& cmd2 = m_allCommands.getReference(j);

            if (!cmd2.keyPress.isValid())
                continue;

            // Check if keypresses match
            if (cmd1.keyPress == cmd2.keyPress)
            {
                // Mark both as conflicting
                cmd1.hasConflict = true;
                cmd1.conflictingCommands.addIfNotAlreadyThere(cmd2.commandID);

                cmd2.hasConflict = true;
                cmd2.conflictingCommands.addIfNotAlreadyThere(cmd1.commandID);
            }
        }
    }

    // Update filtered list conflicts
    for (auto& filteredCmd : m_filteredCommands)
    {
        for (const auto& cmd : m_allCommands)
        {
            if (cmd.commandID == filteredCmd.commandID)
            {
                filteredCmd.hasConflict = cmd.hasConflict;
                filteredCmd.conflictingCommands = cmd.conflictingCommands;
                break;
            }
        }
    }

    // Log conflicts if any
    int conflictCount = 0;
    for (const auto& cmd : m_allCommands)
    {
        if (cmd.hasConflict)
            conflictCount++;
    }

    if (conflictCount > 0)
    {
        juce::Logger::writeToLog("Detected " + juce::String(conflictCount / 2) +
                                " keyboard shortcut conflicts");
    }
}

void ShortcutEditorPanel::showKeypressCaptureDialog(int rowNumber)
{
    if (rowNumber < 0 || rowNumber >= m_filteredCommands.size())
        return;

    auto& cmd = m_filteredCommands.getReference(rowNumber);

    // Create and show modal dialog
    KeyPressCaptureWindow dialog(cmd.commandName);

    bool modalResult = dialog.runModalLoop();

    if (modalResult)
    {
        if (dialog.shouldClearShortcut())
        {
            clearShortcut(rowNumber);
        }
        else
        {
            auto newKey = dialog.getCapturedKeyPress();

            if (newKey.isValid())
            {
                // Update the command in filtered list
                cmd.keyPress = newKey;

                // Update in main command list
                for (auto& mainCmd : m_allCommands)
                {
                    if (mainCmd.commandID == cmd.commandID)
                    {
                        mainCmd.keyPress = newKey;
                        break;
                    }
                }

                m_hasUnsavedChanges = true;
                detectConflicts();
                m_table.repaint();

                juce::Logger::writeToLog("Assigned shortcut '" + newKey.getTextDescription() +
                                        "' to command '" + cmd.commandName + "'");
            }
        }
    }
}

void ShortcutEditorPanel::clearShortcut(int rowNumber)
{
    if (rowNumber < 0 || rowNumber >= m_filteredCommands.size())
        return;

    auto& cmd = m_filteredCommands.getReference(rowNumber);

    // Clear keypress
    cmd.keyPress = juce::KeyPress();

    // Update in main command list
    for (auto& mainCmd : m_allCommands)
    {
        if (mainCmd.commandID == cmd.commandID)
        {
            mainCmd.keyPress = juce::KeyPress();
            break;
        }
    }

    m_hasUnsavedChanges = true;
    detectConflicts();
    m_table.repaint();

    juce::Logger::writeToLog("Cleared shortcut for command '" + cmd.commandName + "'");
}
