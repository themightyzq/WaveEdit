/*
  ==============================================================================

    Settings.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

/**
 * Manages application settings and persistent state.
 *
 * Settings are stored as JSON in platform-specific locations:
 * - macOS: ~/Library/Application Support/WaveEdit/
 * - Windows: %APPDATA%/WaveEdit/
 * - Linux: ~/.config/WaveEdit/
 *
 * Phase 1: Recent files list
 * Phase 2+: Auto-save settings, keyboard bindings, UI preferences
 */
class Settings
{
public:
    Settings();
    ~Settings();

    /**
     * Gets the singleton instance of Settings.
     *
     * @return Reference to the settings instance
     */
    static Settings& getInstance();

    //==============================================================================
    // File Management

    /**
     * Gets the settings directory path.
     *
     * @return Platform-specific settings directory
     */
    juce::File getSettingsDirectory() const;

    /**
     * Gets the settings file path.
     *
     * @return Full path to settings.json
     */
    juce::File getSettingsFile() const;

    /**
     * Loads settings from disk.
     *
     * @return true if load succeeded, false otherwise
     */
    bool load();

    /**
     * Saves settings to disk.
     *
     * @return true if save succeeded, false otherwise
     */
    bool save();

    //==============================================================================
    // Recent Files

    /**
     * Adds a file to the recent files list.
     * Maintains a maximum of 10 recent files.
     *
     * @param file The file to add
     */
    void addRecentFile(const juce::File& file);

    /**
     * Gets the list of recent files.
     *
     * @return Array of recent file paths (most recent first)
     */
    juce::StringArray getRecentFiles() const;

    /**
     * Clears the recent files list.
     */
    void clearRecentFiles();

    /**
     * Removes invalid/deleted files from the recent files list.
     *
     * @return Number of files removed
     */
    int cleanupRecentFiles();

    //==============================================================================
    // General Settings

    /**
     * Gets a setting value as a var.
     *
     * @param key The setting key (dot-separated path, e.g., "audio.sampleRate")
     * @param defaultValue Default value if setting doesn't exist
     * @return The setting value
     */
    juce::var getSetting(const juce::String& key, const juce::var& defaultValue = juce::var()) const;

    /**
     * Sets a setting value.
     *
     * @param key The setting key (dot-separated path)
     * @param value The value to set
     */
    void setSetting(const juce::String& key, const juce::var& value);

private:
    //==============================================================================
    // Private Members

    juce::ValueTree m_settingsTree;
    juce::File m_settingsFile;

    static constexpr int MAX_RECENT_FILES = 10;

    //==============================================================================
    // Private Methods

    /**
     * Creates default settings structure.
     */
    void createDefaultSettings();

    /**
     * Gets or creates a child ValueTree by path.
     *
     * @param path Dot-separated path (e.g., "audio.sampleRate")
     * @param createIfMissing Create the path if it doesn't exist
     * @return The ValueTree at the path, or invalid tree if not found
     */
    juce::ValueTree getTreeByPath(const juce::String& path, bool createIfMissing = false);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Settings)
};
