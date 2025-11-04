/*
  ==============================================================================

    KeyboardCheatSheetDialog.h
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
#include "../Commands/CommandIDs.h"
#include "../Utils/KeymapManager.h"

/**
 * Keyboard shortcut cheat sheet dialog for WaveEdit.
 *
 * Features:
 * - Dynamically loads shortcuts from KeymapManager (shows current template)
 * - Search/filter functionality (real-time filtering)
 * - Grouped by category (File, Edit, Playback, View, etc.)
 * - Scrollable list with proper formatting
 * - Keyboard accessible (F1 or Cmd+/ to open, Escape to close)
 *
 * Accessed via:
 * - Keyboard shortcut: Cmd+/ (macOS) or Ctrl+/ (Windows/Linux)
 * - Menu: Help → Keyboard Shortcuts
 * - Alternative: F1 key
 */
class KeyboardCheatSheetDialog : public juce::Component,
                                  public juce::TableListBoxModel,
                                  public juce::TextEditor::Listener
{
public:
    /**
     * Constructor.
     *
     * @param keymapManager Reference to KeymapManager for loading shortcuts
     * @param commandManager Reference to ApplicationCommandManager for command info
     */
    explicit KeyboardCheatSheetDialog(KeymapManager& keymapManager,
                                       juce::ApplicationCommandManager& commandManager);
    ~KeyboardCheatSheetDialog() override;

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // TableListBoxModel overrides

    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, Component* existingComponentToUpdate) override;
    void sortOrderChanged(int newSortColumnId, bool isForwards) override;

    //==============================================================================
    // TextEditor::Listener overrides

    void textEditorTextChanged(juce::TextEditor& editor) override;

    //==============================================================================
    // Static factory method

    /**
     * Shows the keyboard cheat sheet dialog as a modal window.
     *
     * @param parentComponent Parent component to center the dialog over
     * @param keymapManager Reference to KeymapManager for loading shortcuts
     * @param commandManager Reference to ApplicationCommandManager for command info
     */
    static void showDialog(juce::Component* parentComponent,
                           KeymapManager& keymapManager,
                           juce::ApplicationCommandManager& commandManager);

private:
    //==============================================================================
    // Internal data structures

    /**
     * Represents a single shortcut entry for display.
     */
    struct ShortcutEntry
    {
        juce::String category;          // File, Edit, Playback, etc.
        juce::String commandName;       // Human-readable command name
        juce::String shortcut;          // Formatted shortcut (e.g., "Cmd+S")
        int commandID;                  // CommandID for reference

        ShortcutEntry() : commandID(0) {}
        ShortcutEntry(const juce::String& cat, const juce::String& name,
                      const juce::String& sc, int id)
            : category(cat), commandName(name), shortcut(sc), commandID(id) {}
    };

    //==============================================================================
    // Helper methods

    /**
     * Load all shortcuts from KeymapManager and ApplicationCommandManager.
     * Populates m_allShortcuts array.
     */
    void loadShortcuts();

    /**
     * Apply current search filter to shortcuts.
     * Filters m_allShortcuts into m_filteredShortcuts based on m_searchText.
     */
    void applyFilter();

    /**
     * Get category string for a command ID.
     *
     * @param commandID The command ID to categorize
     * @return Category name (e.g., "File", "Edit", "Playback")
     */
    static juce::String getCategoryForCommand(int commandID);

    /**
     * Get human-readable command name for a command ID.
     *
     * @param commandID The command ID
     * @param commandManager Reference to ApplicationCommandManager for command info
     * @return Human-readable command name
     */
    static juce::String getCommandName(int commandID, juce::ApplicationCommandManager& commandManager);

    //==============================================================================
    // UI Components

    juce::Label m_titleLabel;
    juce::Label m_searchLabel;
    juce::TextEditor m_searchEditor;
    juce::TableListBox m_shortcutsTable;
    juce::TextButton m_closeButton;
    juce::Label m_infoLabel;  // Shows current template name

    //==============================================================================
    // Data members

    KeymapManager& m_keymapManager;
    juce::ApplicationCommandManager& m_commandManager;

    juce::Array<ShortcutEntry> m_allShortcuts;       // All shortcuts loaded from KeymapManager
    juce::Array<ShortcutEntry> m_filteredShortcuts;  // Filtered shortcuts based on search

    juce::String m_searchText;  // Current search filter text

    int m_sortColumnId;     // Current sort column (1 = Category, 2 = Command, 3 = Shortcut)
    bool m_sortForwards;    // Sort direction

    //==============================================================================
    // Constants

    enum ColumnIds
    {
        ColumnCategory = 1,
        ColumnCommand = 2,
        ColumnShortcut = 3
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyboardCheatSheetDialog)
};
