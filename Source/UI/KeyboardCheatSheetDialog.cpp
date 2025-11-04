/*
  ==============================================================================

    KeyboardCheatSheetDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "KeyboardCheatSheetDialog.h"

//==============================================================================
// Constructor / Destructor

KeyboardCheatSheetDialog::KeyboardCheatSheetDialog(KeymapManager& keymapManager,
                                                     juce::ApplicationCommandManager& commandManager)
    : m_keymapManager(keymapManager),
      m_commandManager(commandManager),
      m_sortColumnId(ColumnCategory),
      m_sortForwards(true)
{
    // Title label
    m_titleLabel.setText("Keyboard Shortcuts", juce::dontSendNotification);
    m_titleLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Info label (shows current template)
    juce::String templateInfo = "Current Template: " + m_keymapManager.getCurrentTemplateName();
    m_infoLabel.setText(templateInfo, juce::dontSendNotification);
    m_infoLabel.setFont(juce::Font(12.0f));
    m_infoLabel.setJustificationType(juce::Justification::centred);
    m_infoLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(m_infoLabel);

    // Search label
    m_searchLabel.setText("Search:", juce::dontSendNotification);
    m_searchLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_searchLabel);

    // Search editor
    m_searchEditor.setTextToShowWhenEmpty("Type to filter shortcuts...", juce::Colours::grey);
    m_searchEditor.addListener(this);
    addAndMakeVisible(m_searchEditor);

    // Shortcuts table
    m_shortcutsTable.setModel(this);
    m_shortcutsTable.setColour(juce::ListBox::outlineColourId, juce::Colours::grey);
    m_shortcutsTable.setOutlineThickness(1);
    m_shortcutsTable.setMultipleSelectionEnabled(false);
    m_shortcutsTable.setRowHeight(24);
    m_shortcutsTable.getHeader().addColumn("Category", ColumnCategory, 120, 80, 200, juce::TableHeaderComponent::defaultFlags);
    m_shortcutsTable.getHeader().addColumn("Command", ColumnCommand, 300, 200, 400, juce::TableHeaderComponent::defaultFlags);
    m_shortcutsTable.getHeader().addColumn("Shortcut", ColumnShortcut, 180, 100, 250, juce::TableHeaderComponent::defaultFlags);
    m_shortcutsTable.getHeader().setSortColumnId(ColumnCategory, true);
    addAndMakeVisible(m_shortcutsTable);

    // Close button
    m_closeButton.setButtonText("Close");
    m_closeButton.onClick = [this]() {
        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState(0);
    };
    addAndMakeVisible(m_closeButton);

    // Load shortcuts
    loadShortcuts();
    applyFilter();

    // Set size
    setSize(700, 600);
}

KeyboardCheatSheetDialog::~KeyboardCheatSheetDialog()
{
    m_searchEditor.removeListener(this);
    m_shortcutsTable.setModel(nullptr);
}

//==============================================================================
// Component overrides

void KeyboardCheatSheetDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void KeyboardCheatSheetDialog::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Title
    m_titleLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(5);

    // Info label
    m_infoLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(10);

    // Search row
    auto searchRow = bounds.removeFromTop(30);
    m_searchLabel.setBounds(searchRow.removeFromLeft(60));
    m_searchEditor.setBounds(searchRow);
    bounds.removeFromTop(10);

    // Close button (bottom)
    auto buttonRow = bounds.removeFromBottom(30);
    bounds.removeFromBottom(10);
    m_closeButton.setBounds(buttonRow.removeFromRight(100));

    // Table (remaining space)
    m_shortcutsTable.setBounds(bounds);
}

//==============================================================================
// TableListBoxModel overrides

int KeyboardCheatSheetDialog::getNumRows()
{
    return m_filteredShortcuts.size();
}

void KeyboardCheatSheetDialog::paintRowBackground(juce::Graphics& g, int rowNumber,
                                                    int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(juce::Colour(0xff3a3a3a));
    else if (rowNumber % 2 == 0)
        g.fillAll(juce::Colour(0xff2a2a2a));
    else
        g.fillAll(juce::Colour(0xff252525));
}

void KeyboardCheatSheetDialog::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                                          int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= m_filteredShortcuts.size())
        return;

    const auto& entry = m_filteredShortcuts.getReference(rowNumber);

    g.setColour(rowIsSelected ? juce::Colours::white : juce::Colour(0xffcccccc));
    g.setFont(juce::Font(13.0f));

    juce::String text;
    switch (columnId)
    {
        case ColumnCategory:
            text = entry.category;
            break;
        case ColumnCommand:
            text = entry.commandName;
            break;
        case ColumnShortcut:
            text = entry.shortcut;
            g.setFont(juce::Font("Monospace", 13.0f, juce::Font::plain));
            break;
        default:
            break;
    }

    g.drawText(text, 2, 0, width - 4, height, juce::Justification::centredLeft, true);
}

juce::Component* KeyboardCheatSheetDialog::refreshComponentForCell(int rowNumber, int columnId,
                                                                     bool isRowSelected,
                                                                     juce::Component* existingComponentToUpdate)
{
    // No custom components needed for this simple table
    jassert(existingComponentToUpdate == nullptr);
    return nullptr;
}

void KeyboardCheatSheetDialog::sortOrderChanged(int newSortColumnId, bool isForwards)
{
    m_sortColumnId = newSortColumnId;
    m_sortForwards = isForwards;

    // Sort filtered shortcuts
    struct Sorter
    {
        int sortColumn;
        bool forwards;

        int compareElements(const ShortcutEntry& first, const ShortcutEntry& second) const
        {
            int result = 0;

            switch (sortColumn)
            {
                case ColumnCategory:
                    result = first.category.compareIgnoreCase(second.category);
                    if (result == 0)
                        result = first.commandName.compareIgnoreCase(second.commandName);
                    break;
                case ColumnCommand:
                    result = first.commandName.compareIgnoreCase(second.commandName);
                    break;
                case ColumnShortcut:
                    result = first.shortcut.compareIgnoreCase(second.shortcut);
                    break;
                default:
                    break;
            }

            return forwards ? result : -result;
        }
    };

    Sorter sorter = { m_sortColumnId, m_sortForwards };
    m_filteredShortcuts.sort(sorter);

    m_shortcutsTable.updateContent();
}

//==============================================================================
// TextEditor::Listener overrides

void KeyboardCheatSheetDialog::textEditorTextChanged(juce::TextEditor& editor)
{
    m_searchText = editor.getText();
    applyFilter();
}

//==============================================================================
// Static factory method

void KeyboardCheatSheetDialog::showDialog(juce::Component* parentComponent,
                                           KeymapManager& keymapManager,
                                           juce::ApplicationCommandManager& commandManager)
{
    auto* dialog = new KeyboardCheatSheetDialog(keymapManager, commandManager);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = "Keyboard Shortcuts";
    options.componentToCentreAround = parentComponent;
    options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;

    auto* window = options.launchAsync();
    if (window != nullptr)
        window->centreWithSize(700, 600);
}

//==============================================================================
// Helper methods

void KeyboardCheatSheetDialog::loadShortcuts()
{
    m_allShortcuts.clear();

    // Get current template from KeymapManager
    const auto& currentTemplate = m_keymapManager.getCurrentTemplate();

    // Get all command IDs from CommandIDs namespace
    juce::Array<int> commandIDs;
    commandIDs.add(CommandIDs::fileNew);
    commandIDs.add(CommandIDs::fileOpen);
    commandIDs.add(CommandIDs::fileSave);
    commandIDs.add(CommandIDs::fileSaveAs);
    commandIDs.add(CommandIDs::fileClose);
    commandIDs.add(CommandIDs::fileProperties);
    commandIDs.add(CommandIDs::fileExit);
    commandIDs.add(CommandIDs::filePreferences);

    commandIDs.add(CommandIDs::editUndo);
    commandIDs.add(CommandIDs::editRedo);
    commandIDs.add(CommandIDs::editCut);
    commandIDs.add(CommandIDs::editCopy);
    commandIDs.add(CommandIDs::editPaste);
    commandIDs.add(CommandIDs::editDelete);
    commandIDs.add(CommandIDs::editSelectAll);
    commandIDs.add(CommandIDs::editSilence);
    commandIDs.add(CommandIDs::editTrim);

    commandIDs.add(CommandIDs::playbackPlay);
    commandIDs.add(CommandIDs::playbackPause);
    commandIDs.add(CommandIDs::playbackStop);
    commandIDs.add(CommandIDs::playbackLoop);

    commandIDs.add(CommandIDs::viewZoomIn);
    commandIDs.add(CommandIDs::viewZoomOut);
    commandIDs.add(CommandIDs::viewZoomFit);
    commandIDs.add(CommandIDs::viewZoomSelection);
    commandIDs.add(CommandIDs::viewZoomOneToOne);
    commandIDs.add(CommandIDs::viewCycleTimeFormat);
    commandIDs.add(CommandIDs::viewAutoScroll);
    commandIDs.add(CommandIDs::viewZoomToRegion);
    commandIDs.add(CommandIDs::viewAutoPreviewRegions);

    commandIDs.add(CommandIDs::processFadeIn);
    commandIDs.add(CommandIDs::processFadeOut);
    commandIDs.add(CommandIDs::processNormalize);
    commandIDs.add(CommandIDs::processDCOffset);
    commandIDs.add(CommandIDs::processGain);
    commandIDs.add(CommandIDs::processIncreaseGain);
    commandIDs.add(CommandIDs::processDecreaseGain);

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

    commandIDs.add(CommandIDs::selectExtendLeft);
    commandIDs.add(CommandIDs::selectExtendRight);
    commandIDs.add(CommandIDs::selectExtendStart);
    commandIDs.add(CommandIDs::selectExtendEnd);
    commandIDs.add(CommandIDs::selectExtendPageLeft);
    commandIDs.add(CommandIDs::selectExtendPageRight);

    commandIDs.add(CommandIDs::snapCycleMode);
    commandIDs.add(CommandIDs::snapToggleZeroCrossing);

    commandIDs.add(CommandIDs::helpAbout);
    commandIDs.add(CommandIDs::helpShortcuts);

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

    commandIDs.add(CommandIDs::regionAdd);
    commandIDs.add(CommandIDs::regionDelete);
    commandIDs.add(CommandIDs::regionNext);
    commandIDs.add(CommandIDs::regionPrevious);
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

    commandIDs.add(CommandIDs::markerAdd);
    commandIDs.add(CommandIDs::markerDelete);
    commandIDs.add(CommandIDs::markerNext);
    commandIDs.add(CommandIDs::markerPrevious);

    // Load shortcut for each command
    for (int commandID : commandIDs)
    {
        // Get shortcut from KeymapManager
        juce::KeyPress keyPress = m_keymapManager.getKeyPress(commandID);

        if (keyPress.isValid())
        {
            juce::String shortcutText = keyPress.getTextDescription();
            juce::String commandName = getCommandName(commandID, m_commandManager);
            juce::String category = getCategoryForCommand(commandID);

            m_allShortcuts.add(ShortcutEntry(category, commandName, shortcutText, commandID));
        }
    }

    // Sort by category by default
    struct Sorter
    {
        int compareElements(const ShortcutEntry& first, const ShortcutEntry& second) const
        {
            int result = first.category.compareIgnoreCase(second.category);
            if (result == 0)
                result = first.commandName.compareIgnoreCase(second.commandName);
            return result;
        }
    };

    Sorter sorter;
    m_allShortcuts.sort(sorter);
}

void KeyboardCheatSheetDialog::applyFilter()
{
    m_filteredShortcuts.clear();

    if (m_searchText.isEmpty())
    {
        // No filter - show all
        m_filteredShortcuts = m_allShortcuts;
    }
    else
    {
        // Apply case-insensitive search filter
        juce::String searchLower = m_searchText.toLowerCase();

        for (const auto& entry : m_allShortcuts)
        {
            // Search in category, command name, and shortcut
            if (entry.category.toLowerCase().contains(searchLower) ||
                entry.commandName.toLowerCase().contains(searchLower) ||
                entry.shortcut.toLowerCase().contains(searchLower))
            {
                m_filteredShortcuts.add(entry);
            }
        }
    }

    // Re-apply current sort
    sortOrderChanged(m_sortColumnId, m_sortForwards);
}

juce::String KeyboardCheatSheetDialog::getCategoryForCommand(int commandID)
{
    // Group commands by ID range
    if (commandID >= 0x1000 && commandID < 0x2000)
        return "File";
    else if (commandID >= 0x2000 && commandID < 0x3000)
        return "Edit";
    else if (commandID >= 0x3000 && commandID < 0x4000)
        return "Playback";
    else if (commandID >= 0x4000 && commandID < 0x5000)
        return "View";
    else if (commandID >= 0x5000 && commandID < 0x6000)
        return "Process";
    else if (commandID >= 0x6000 && commandID < 0x7000)
        return "Navigate";
    else if (commandID >= 0x7000 && commandID < 0x8000)
        return "Selection";
    else if (commandID >= 0x8000 && commandID < 0x9000)
        return "Snap";
    else if (commandID >= 0x9000 && commandID < 0xA000)
        return "Help";
    else if (commandID >= 0xA000 && commandID < 0xB000)
        return "Tabs";
    else if (commandID >= 0xB000 && commandID < 0xC000)
        return "Regions";
    else if (commandID >= 0xC000 && commandID < 0xD000)
        return "Markers";
    else
        return "Other";
}

juce::String KeyboardCheatSheetDialog::getCommandName(int commandID,
                                                       juce::ApplicationCommandManager& commandManager)
{
    // Static mapping of command IDs to display names (matches Main.cpp)
    switch (commandID)
    {
        // File commands
        case CommandIDs::fileOpen: return "Open...";
        case CommandIDs::fileSave: return "Save";
        case CommandIDs::fileSaveAs: return "Save As...";
        case CommandIDs::fileClose: return "Close";
        case CommandIDs::fileProperties: return "Properties...";
        case CommandIDs::fileExit: return "Exit";
        case CommandIDs::filePreferences: return "Preferences...";
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

        // Edit commands
        case CommandIDs::editUndo: return "Undo";
        case CommandIDs::editRedo: return "Redo";
        case CommandIDs::editSelectAll: return "Select All";
        case CommandIDs::editCut: return "Cut";
        case CommandIDs::editCopy: return "Copy";
        case CommandIDs::editPaste: return "Paste";
        case CommandIDs::editDelete: return "Delete";
        case CommandIDs::editSilence: return "Silence";
        case CommandIDs::editTrim: return "Trim";

        // Playback commands
        case CommandIDs::playbackPlay: return "Play";
        case CommandIDs::playbackPause: return "Pause";
        case CommandIDs::playbackStop: return "Stop";
        case CommandIDs::playbackLoop: return "Loop";

        // View commands
        case CommandIDs::viewZoomIn: return "Zoom In";
        case CommandIDs::viewZoomOut: return "Zoom Out";
        case CommandIDs::viewZoomFit: return "Zoom to Fit";
        case CommandIDs::viewZoomSelection: return "Zoom to Selection";
        case CommandIDs::viewZoomOneToOne: return "Zoom 1:1";
        case CommandIDs::viewCycleTimeFormat: return "Cycle Time Format";
        case CommandIDs::viewAutoScroll: return "Auto-Scroll During Playback";
        case CommandIDs::viewZoomToRegion: return "Zoom to Region";
        case CommandIDs::viewAutoPreviewRegions: return "Auto-Preview Regions";

        // Navigation commands
        case CommandIDs::navigateLeft: return "Navigate Left";
        case CommandIDs::navigateRight: return "Navigate Right";
        case CommandIDs::navigateStart: return "Jump to Start";
        case CommandIDs::navigateEnd: return "Jump to End";
        case CommandIDs::navigatePageLeft: return "Page Left";
        case CommandIDs::navigatePageRight: return "Page Right";
        case CommandIDs::navigateHomeVisible: return "Jump to Visible Start";
        case CommandIDs::navigateEndVisible: return "Jump to Visible End";
        case CommandIDs::navigateCenterView: return "Center View";
        case CommandIDs::navigateGoToPosition: return "Go To Position...";

        // Selection commands
        case CommandIDs::selectExtendLeft: return "Extend Selection Left";
        case CommandIDs::selectExtendRight: return "Extend Selection Right";
        case CommandIDs::selectExtendStart: return "Extend to Visible Start";
        case CommandIDs::selectExtendEnd: return "Extend to Visible End";
        case CommandIDs::selectExtendPageLeft: return "Extend Selection Page Left";
        case CommandIDs::selectExtendPageRight: return "Extend Selection Page Right";

        // Snap commands
        case CommandIDs::snapCycleMode: return "Toggle Snap";
        case CommandIDs::snapToggleZeroCrossing: return "Toggle Zero Crossing Snap";

        // Process commands
        case CommandIDs::processGain: return "Gain...";
        case CommandIDs::processIncreaseGain: return "Increase Gain";
        case CommandIDs::processDecreaseGain: return "Decrease Gain";
        case CommandIDs::processNormalize: return "Normalize...";
        case CommandIDs::processFadeIn: return "Fade In";
        case CommandIDs::processFadeOut: return "Fade Out";
        case CommandIDs::processDCOffset: return "Remove DC Offset";

        // Region commands
        case CommandIDs::regionAdd: return "Add Region";
        case CommandIDs::regionDelete: return "Delete Region";
        case CommandIDs::regionNext: return "Next Region";
        case CommandIDs::regionPrevious: return "Previous Region";
        case CommandIDs::regionSelectInverse: return "Select Inverse of Regions";
        case CommandIDs::regionSelectAll: return "Select All Regions";
        case CommandIDs::regionStripSilence: return "Strip Silence (Auto-Create Regions)";
        case CommandIDs::regionExportAll: return "Export Regions As Files";
        case CommandIDs::regionBatchRename: return "Batch Rename Regions";
        case CommandIDs::regionMerge: return "Merge Regions";
        case CommandIDs::regionSplit: return "Split Region at Cursor";
        case CommandIDs::regionCopy: return "Copy Region";
        case CommandIDs::regionPaste: return "Paste Regions at Cursor";
        case CommandIDs::regionShowList: return "Show Region List";
        case CommandIDs::regionSnapToZeroCrossing: return "Snap to Zero Crossings";
        case CommandIDs::regionNudgeStartLeft: return "Nudge Region Start Left";
        case CommandIDs::regionNudgeStartRight: return "Nudge Region Start Right";
        case CommandIDs::regionNudgeEndLeft: return "Nudge Region End Left";
        case CommandIDs::regionNudgeEndRight: return "Nudge Region End Right";

        // Marker commands
        case CommandIDs::markerAdd: return "Add Marker";
        case CommandIDs::markerDelete: return "Delete Marker";
        case CommandIDs::markerNext: return "Next Marker";
        case CommandIDs::markerPrevious: return "Previous Marker";
        case CommandIDs::markerShowList: return "Show Marker List";

        // Help commands
        case CommandIDs::helpAbout: return "About WaveEdit";
        case CommandIDs::helpShortcuts: return "Keyboard Shortcuts";

        default:
            return "Unknown Command";
    }
}
