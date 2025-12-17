/*
  ==============================================================================

    ToolbarManager.h
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
#include "ToolbarConfig.h"

/**
 * Manages toolbar layout templates for WaveEdit.
 *
 * Features:
 * - Load/save toolbar layouts from JSON files
 * - Switch between layouts at runtime
 * - Built-in templates: Default, Compact, DSPFocused, SoundForge
 * - Import/export custom user layouts
 * - Settings persistence
 *
 * Pattern: Mirrors KeymapManager for consistency.
 *
 * Template Format (JSON):
 * {
 *   "name": "Default",
 *   "description": "Transport + common operations",
 *   "version": "1.0",
 *   "height": 36,
 *   "showLabels": false,
 *   "buttons": [
 *     {"id": "transport", "type": "transport", "width": 180},
 *     {"id": "sep1", "type": "separator"},
 *     {"id": "zoomIn", "type": "command", "commandName": "viewZoomIn"}
 *   ]
 * }
 *
 * Thread Safety: UI thread only (JUCE component integration)
 */
class ToolbarManager
{
public:
    //==============================================================================

    /**
     * Constructor - loads built-in templates and user settings.
     */
    ToolbarManager();

    ~ToolbarManager();

    //==============================================================================
    // Layout Management

    /**
     * Get list of all available layout names (built-in + user).
     */
    juce::StringArray getAvailableLayouts() const;

    /**
     * Get currently active layout name.
     */
    juce::String getCurrentLayoutName() const;

    /**
     * Load and activate a layout by name.
     *
     * @param layoutName Name of layout to load
     * @return true if successful, false if layout not found or invalid
     */
    bool loadLayout(const juce::String& layoutName);

    /**
     * Get the currently active layout.
     */
    const ToolbarLayout& getCurrentLayout() const;

    /**
     * Check if a layout exists.
     */
    bool layoutExists(const juce::String& layoutName) const;

    //==============================================================================
    // Import/Export

    /**
     * Import a layout from a JSON file.
     *
     * @param file Path to JSON file
     * @param makeActive If true, activate the imported layout immediately
     * @return true if successful, false on error
     */
    bool importLayout(const juce::File& file, bool makeActive = false);

    /**
     * Export the current layout to a JSON file.
     *
     * @param file Destination file path
     * @return true if successful, false on error
     */
    bool exportCurrentLayout(const juce::File& file) const;

    /**
     * Export a specific layout by name.
     */
    bool exportLayout(const juce::String& layoutName, const juce::File& file) const;

    //==============================================================================
    // Layout Modification

    /**
     * Save the current layout under a new name (create custom layout).
     *
     * @param newName Name for the saved layout
     * @return true if successful
     */
    bool saveCurrentLayoutAs(const juce::String& newName);

    /**
     * Update the current layout in-place (used by customization dialog).
     * This updates the active layout and notifies listeners, but doesn't
     * save to disk unless the layout is a user layout.
     *
     * @param layout The new layout configuration
     */
    void updateCurrentLayout(const ToolbarLayout& layout);

    /**
     * Delete a user layout (cannot delete built-in layouts).
     *
     * @param layoutName Name of layout to delete
     * @return true if deleted, false if built-in or not found
     */
    bool deleteLayout(const juce::String& layoutName);

    /**
     * Check if a layout is built-in (cannot be deleted/modified).
     */
    bool isBuiltInLayout(const juce::String& layoutName) const;

    //==============================================================================
    // Settings Persistence

    /**
     * Save current layout selection to app settings.
     */
    void saveToSettings();

    /**
     * Load layout selection from app settings.
     */
    void loadFromSettings();

    /**
     * Get the toolbars directory path.
     */
    static juce::File getToolbarsDirectory();

    //==============================================================================
    // Layout Listener

    /**
     * Listener interface for layout changes.
     */
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void toolbarLayoutChanged(const ToolbarLayout& newLayout) = 0;
    };

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    //==============================================================================
    // Private members

    ToolbarLayout m_currentLayout;
    juce::String m_currentLayoutName;
    std::map<juce::String, ToolbarLayout> m_builtInLayouts;
    std::map<juce::String, juce::File> m_userLayouts;
    juce::ListenerList<Listener> m_listeners;

    //==============================================================================
    // Private methods

    /**
     * Create all built-in layouts programmatically.
     */
    void createBuiltInLayouts();

    /**
     * Load built-in templates from bundled JSON files.
     */
    void loadBuiltInTemplates();

    /**
     * Scan for user-installed layouts in the toolbars directory.
     */
    void scanUserLayouts();

    /**
     * Notify listeners of layout change.
     */
    void notifyListeners();

    /**
     * Create the default built-in layout.
     */
    static ToolbarLayout createDefaultLayout();

    /**
     * Create the compact (transport-only) layout.
     */
    static ToolbarLayout createCompactLayout();

    /**
     * Create the DSP-focused layout.
     */
    static ToolbarLayout createDSPFocusedLayout();

    /**
     * Create the Sound Forge-style layout.
     */
    static ToolbarLayout createSoundForgeLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolbarManager)
};
