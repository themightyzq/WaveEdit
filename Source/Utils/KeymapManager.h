/*
  ==============================================================================

    KeymapManager.h
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
#include <juce_data_structures/juce_data_structures.h>
#include "../Commands/CommandIDs.h"

/**
 * Manages keyboard shortcut templates (keymaps) for WaveEdit.
 *
 * Features:
 * - Load/save keymap templates from JSON files
 * - Switch between templates at runtime
 * - Validate templates for conflicts
 * - Import/export custom user templates
 * - Built-in templates: Default, WaveEdit Classic, Sound Forge, Pro Tools
 *
 * Template Format (JSON):
 * {
 *   "name": "Default",
 *   "description": "WaveEdit family-based shortcuts",
 *   "version": "1.0",
 *   "shortcuts": {
 *     "playbackPlay": { "key": "Space", "modifiers": [] },
 *     "processGain": { "key": "G", "modifiers": [] }
 *   }
 * }
 *
 * Thread Safety: UI thread only (JUCE component integration)
 */
class KeymapManager
{
public:
    /**
     * Represents a single keyboard shortcut mapping.
     */
    struct Shortcut
    {
        juce::String key;                     // Single character or special key name
        juce::Array<juce::String> modifiers;  // "cmd", "shift", "alt", "ctrl"

        Shortcut() = default;
        Shortcut(const juce::String& k, const juce::Array<juce::String>& mods)
            : key(k), modifiers(mods) {}

        /**
         * Convert to JUCE KeyPress object.
         */
        juce::KeyPress toKeyPress() const;

        /**
         * Create from JUCE KeyPress object.
         */
        static Shortcut fromKeyPress(const juce::KeyPress& keyPress);

        /**
         * Get human-readable description (e.g., "Cmd+Shift+G").
         */
        juce::String getDescription() const;
    };

    /**
     * Represents a complete keymap template.
     */
    struct Template
    {
        juce::String name;
        juce::String description;
        juce::String version;
        std::map<juce::String, Shortcut> shortcuts;  // commandName -> Shortcut

        Template() = default;

        /**
         * Load template from JSON file.
         */
        static Template fromJSON(const juce::File& file);

        /**
         * Save template to JSON file.
         */
        bool saveToJSON(const juce::File& file) const;

        /**
         * Load template from JSON var object.
         */
        static Template fromVar(const juce::var& json);

        /**
         * Convert to JSON var object.
         */
        juce::var toVar() const;

        /**
         * Validate template for conflicts and completeness.
         *
         * @param errors Output array of error messages
         * @return true if valid, false if errors found
         */
        bool validate(juce::StringArray& errors) const;
    };

    //==============================================================================

    /**
     * Constructor - requires reference to ApplicationCommandManager to apply shortcuts.
     *
     * @param commandManager Reference to the application's command manager
     */
    KeymapManager(juce::ApplicationCommandManager& commandManager);
    ~KeymapManager();

    //==============================================================================
    // Template Management

    /**
     * Get list of all available template names (built-in + user).
     */
    juce::StringArray getAvailableTemplates() const;

    /**
     * Get currently active template name.
     */
    juce::String getCurrentTemplateName() const;

    /**
     * Load and activate a template by name.
     *
     * @param templateName Name of template to load
     * @return true if successful, false if template not found or invalid
     */
    bool loadTemplate(const juce::String& templateName);

    /**
     * Get the currently active template.
     */
    const Template& getCurrentTemplate() const;

    /**
     * Check if a template exists.
     */
    bool templateExists(const juce::String& templateName) const;

    //==============================================================================
    // Import/Export

    /**
     * Import a template from a JSON file.
     *
     * @param file Path to JSON file
     * @param makeActive If true, activate the imported template immediately
     * @return true if successful, false on error
     */
    bool importTemplate(const juce::File& file, bool makeActive = false);

    /**
     * Export the current template to a JSON file.
     *
     * @param file Destination file path
     * @return true if successful, false on error
     */
    bool exportCurrentTemplate(const juce::File& file) const;

    /**
     * Export a specific template by name.
     */
    bool exportTemplate(const juce::String& templateName, const juce::File& file) const;

    //==============================================================================
    // Shortcut Queries

    /**
     * Get the shortcut for a specific command in the current template.
     *
     * @param commandName Command name (e.g., "playbackPlay")
     * @return Shortcut object, or empty shortcut if not found
     */
    Shortcut getShortcut(const juce::String& commandName) const;

    /**
     * Get the KeyPress for a specific command ID in the current template.
     */
    juce::KeyPress getKeyPress(juce::CommandID commandID) const;

    /**
     * Check if a shortcut is assigned to any command.
     *
     * @param shortcut Shortcut to check
     * @return Command name if found, empty string if not assigned
     */
    juce::String findCommandForShortcut(const Shortcut& shortcut) const;

    //==============================================================================
    // Settings Persistence

    /**
     * Save current template selection to app settings.
     */
    void saveToSettings();

    /**
     * Load template selection from app settings.
     */
    void loadFromSettings();

    /**
     * Get the templates directory path.
     */
    static juce::File getTemplatesDirectory();

    /**
     * Apply the current template's shortcuts to the ApplicationCommandManager.
     *
     * This is the key integration method that updates the actual shortcuts
     * in the application when a template is loaded. Called automatically
     * by loadTemplate() after updating internal state.
     */
    void applyTemplateToCommandManager();

private:
    //==============================================================================
    // Private members

    juce::ApplicationCommandManager& m_commandManager;  // Reference to command manager for applying shortcuts
    Template m_currentTemplate;
    juce::String m_currentTemplateName;
    std::map<juce::String, Template> m_builtInTemplates;
    std::map<juce::String, juce::File> m_userTemplates;

    //==============================================================================
    // Private methods

    /**
     * Load all built-in templates (Default, Classic, Sound Forge, Pro Tools).
     */
    void loadBuiltInTemplates();

    /**
     * Scan for user-installed templates in the templates directory.
     */
    void scanUserTemplates();

    /**
     * Get command name from command ID.
     */
    static juce::String getCommandName(juce::CommandID commandID);

    /**
     * Get command ID from command name.
     */
    static juce::CommandID getCommandID(const juce::String& commandName);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeymapManager)
};
