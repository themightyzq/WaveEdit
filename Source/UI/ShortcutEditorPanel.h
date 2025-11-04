/*
  ==============================================================================

    ShortcutEditorPanel.h
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

/**
 * Keyboard shortcut customization panel for WaveEdit.
 *
 * Features:
 * - Searchable list of all commands with current shortcuts
 * - Click to rebind any shortcut
 * - Conflict detection with visual warnings
 * - Export/Import keybindings to/from JSON
 * - Reset to Sound Forge defaults
 *
 * Accessed via Preferences → Keyboard Shortcuts tab
 *
 * Thread Safety: UI thread only (JUCE component)
 */
class ShortcutEditorPanel : public juce::Component,
                            public juce::TableListBoxModel,
                            public juce::Button::Listener,
                            public juce::TextEditor::Listener
{
public:
    /**
     * Constructor.
     *
     * @param commandManager Reference to the application's ApplicationCommandManager
     */
    explicit ShortcutEditorPanel(juce::ApplicationCommandManager& commandManager);
    ~ShortcutEditorPanel() override;

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // TableListBoxModel overrides

    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    juce::String getCellTooltip(int rowNumber, int columnId) override;

    //==============================================================================
    // Button::Listener overrides

    void buttonClicked(juce::Button* button) override;

    //==============================================================================
    // TextEditor::Listener overrides

    void textEditorTextChanged(juce::TextEditor& editor) override;

    //==============================================================================
    // Shortcut management

    /**
     * Refreshes the command list from the ApplicationCommandManager.
     * Call this after modifying shortcuts externally.
     */
    void refreshCommandList();

    /**
     * Applies current shortcut changes to the ApplicationCommandManager.
     */
    void applyChanges();

    /**
     * Reverts all changes back to current ApplicationCommandManager state.
     */
    void revertChanges();

    /**
     * Resets all shortcuts to Sound Forge Pro defaults.
     */
    void resetToDefaults();

    /**
     * Exports current keybindings to a JSON file.
     *
     * @return true if export succeeded, false otherwise
     */
    bool exportKeybindings();

    /**
     * Imports keybindings from a JSON file.
     *
     * @return true if import succeeded, false otherwise
     */
    bool importKeybindings();

    /**
     * Checks if there are unsaved changes.
     *
     * @return true if shortcuts have been modified but not applied
     */
    bool hasUnsavedChanges() const;

    /**
     * Loads all commands from the ApplicationCommandManager.
     * Used to refresh the shortcut display after template changes.
     */
    void loadCommandsFromManager();

private:
    //==============================================================================
    // Private structures

    /**
     * Represents a single command with its shortcut binding.
     */
    struct CommandShortcut
    {
        juce::CommandID commandID;
        juce::String commandName;
        juce::String category;
        juce::KeyPress keyPress;
        juce::String description;
        bool hasConflict {false};
        juce::Array<juce::CommandID> conflictingCommands;

        CommandShortcut() : commandID(0) {}

        CommandShortcut(juce::CommandID id, const juce::String& name, const juce::String& cat,
                        const juce::KeyPress& key, const juce::String& desc)
            : commandID(id), commandName(name), category(cat), keyPress(key), description(desc)
        {
        }

        bool matchesFilter(const juce::String& filterText) const
        {
            if (filterText.isEmpty())
                return true;

            return commandName.containsIgnoreCase(filterText)
                || category.containsIgnoreCase(filterText)
                || keyPress.getTextDescription().containsIgnoreCase(filterText);
        }
    };

    //==============================================================================
    // Private members

    juce::ApplicationCommandManager& m_commandManager;

    // UI Components
    juce::Label m_titleLabel;
    juce::Label m_searchLabel;
    juce::TextEditor m_searchBox;
    juce::TableListBox m_table;

    juce::TextButton m_exportButton {"Export..."};
    juce::TextButton m_importButton {"Import..."};
    juce::TextButton m_resetButton {"Reset to Defaults"};

    // Command list (all commands from ApplicationCommandManager)
    juce::Array<CommandShortcut> m_allCommands;

    // Filtered command list (based on search text)
    juce::Array<CommandShortcut> m_filteredCommands;

    // Original shortcuts (for revert functionality)
    juce::Array<CommandShortcut> m_originalCommands;

    // Currently selected row for editing
    int m_selectedRow {-1};

    // Flag for unsaved changes
    bool m_hasUnsavedChanges {false};

    //==============================================================================
    // Private methods

    /**
     * Gets the default keypress for a command ID.
     * This extracts the default from getCommandInfo().
     *
     * @param commandID The command ID
     * @return The default KeyPress, or invalid KeyPress if none
     */
    juce::KeyPress getDefaultKeypressForCommand(juce::CommandID commandID);

    /**
     * Gets the command name from ApplicationCommandManager.
     *
     * @param commandID The command ID
     * @return The command name
     */
    juce::String getCommandName(juce::CommandID commandID);

    /**
     * Gets the category for a command based on its ID range.
     *
     * @param commandID The command ID
     * @return The category name (e.g., "File", "Edit", "Playback")
     */
    juce::String getCategoryForCommand(juce::CommandID commandID);

    /**
     * Updates the filtered command list based on search text.
     */
    void updateFilteredCommands();

    /**
     * Detects conflicts in current shortcut assignments.
     */
    void detectConflicts();

    /**
     * Shows a dialog to capture a new keyboard shortcut.
     *
     * @param rowNumber The row to assign the new shortcut to
     */
    void showKeypressCaptureDialog(int rowNumber);

    /**
     * Removes a shortcut assignment for a command.
     *
     * @param rowNumber The row to clear
     */
    void clearShortcut(int rowNumber);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShortcutEditorPanel)
};
