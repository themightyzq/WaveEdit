/*
  ==============================================================================

    ToolbarConfig.h
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
#include <juce_data_structures/juce_data_structures.h>

/**
 * Defines the type of item that can appear in the toolbar.
 */
enum class ToolbarButtonType
{
    COMMAND,        // Executes a CommandID (e.g., processFadeIn, viewZoomIn)
    PLUGIN,         // Opens a specific plugin by identifier
    SEPARATOR,      // Visual separator bar
    SPACER,         // Flexible space
    TRANSPORT       // Embedded compact transport widget
};

/**
 * Configuration for a single toolbar button.
 *
 * Used for both built-in and user-defined toolbar items.
 * Serializes to/from JSON for template persistence.
 */
struct ToolbarButtonConfig
{
    juce::String id;                    // Unique identifier within the layout (e.g., "fadeIn", "sep1")
    ToolbarButtonType type = ToolbarButtonType::COMMAND;
    juce::String commandName;           // For COMMAND type: command name (e.g., "processFadeIn")
    juce::String pluginIdentifier;      // For PLUGIN type: plugin identifier string
    juce::String iconName;              // Optional: custom icon name (empty = derive from command)
    juce::String tooltip;               // Optional: custom tooltip (empty = use command description)
    int width = 28;                     // Button width in pixels (height is layout height)

    ToolbarButtonConfig() = default;

    /**
     * Create a command button configuration.
     *
     * @param buttonId Unique identifier
     * @param cmdName Command name from KeymapManager (e.g., "processFadeIn")
     * @param customWidth Optional custom width (default 28)
     */
    static ToolbarButtonConfig command(const juce::String& buttonId,
                                        const juce::String& cmdName,
                                        int customWidth = 28);

    /**
     * Create a plugin button configuration.
     *
     * @param buttonId Unique identifier
     * @param pluginId Plugin identifier string
     * @param customTooltip Optional tooltip text
     * @param customWidth Optional custom width (default 28)
     */
    static ToolbarButtonConfig plugin(const juce::String& buttonId,
                                       const juce::String& pluginId,
                                       const juce::String& customTooltip = {},
                                       int customWidth = 28);

    /**
     * Create a separator configuration.
     *
     * @param separatorId Unique identifier (e.g., "sep1")
     * @param customWidth Optional width (default 8)
     */
    static ToolbarButtonConfig separator(const juce::String& separatorId,
                                          int customWidth = 8);

    /**
     * Create a spacer configuration (flexible space).
     *
     * @param spacerId Unique identifier (e.g., "spacer1")
     * @param minWidth Minimum width (default 16)
     */
    static ToolbarButtonConfig spacer(const juce::String& spacerId,
                                       int minWidth = 16);

    /**
     * Create a transport widget configuration.
     *
     * @param transportId Unique identifier (typically "transport")
     * @param customWidth Width for compact transport (default 180)
     */
    static ToolbarButtonConfig transport(const juce::String& transportId = "transport",
                                          int customWidth = 180);

    /**
     * Load from JSON var object.
     */
    static ToolbarButtonConfig fromVar(const juce::var& json);

    /**
     * Convert to JSON var object.
     */
    juce::var toVar() const;

    /**
     * Get string representation of button type.
     */
    static juce::String typeToString(ToolbarButtonType type);

    /**
     * Parse button type from string.
     */
    static ToolbarButtonType stringToType(const juce::String& typeStr);
};

/**
 * Configuration for a complete toolbar layout.
 *
 * Contains metadata and the list of buttons to display.
 * Supports JSON serialization for template files.
 *
 * JSON Template Format:
 * {
 *   "name": "Default",
 *   "description": "Transport + common operations",
 *   "version": "1.0",
 *   "height": 36,
 *   "showLabels": false,
 *   "buttons": [
 *     {"id": "transport", "type": "transport", "width": 180},
 *     {"id": "sep1", "type": "separator"},
 *     {"id": "zoomIn", "type": "command", "commandName": "viewZoomIn"},
 *     {"id": "fadeIn", "type": "command", "commandName": "processFadeIn"}
 *   ]
 * }
 */
struct ToolbarLayout
{
    juce::String name;                  // Layout name for display and selection
    juce::String description;           // Brief description of the layout
    juce::String version;               // Version string (e.g., "1.0")
    int height = 36;                    // Toolbar height in pixels
    bool showLabels = false;            // Show text labels under icons
    std::vector<ToolbarButtonConfig> buttons;  // Ordered list of buttons

    ToolbarLayout() = default;

    /**
     * Load layout from JSON file.
     *
     * @param file Path to JSON file
     * @return ToolbarLayout object (empty if failed)
     */
    static ToolbarLayout fromJSON(const juce::File& file);

    /**
     * Save layout to JSON file.
     *
     * @param file Destination file path
     * @return true if successful, false on error
     */
    bool saveToJSON(const juce::File& file) const;

    /**
     * Load layout from JSON var object.
     */
    static ToolbarLayout fromVar(const juce::var& json);

    /**
     * Convert to JSON var object.
     */
    juce::var toVar() const;

    /**
     * Validate layout for correctness.
     *
     * Checks:
     * - Name is not empty
     * - At least one button defined
     * - No duplicate IDs
     * - Valid command names for COMMAND type buttons
     *
     * @param errors Output array of error messages
     * @return true if valid, false if errors found
     */
    bool validate(juce::StringArray& errors) const;

    /**
     * Check if layout has a transport widget.
     */
    bool hasTransport() const;

    /**
     * Get button config by ID.
     *
     * @param buttonId Button identifier
     * @return Pointer to config or nullptr if not found
     */
    const ToolbarButtonConfig* getButton(const juce::String& buttonId) const;

    /**
     * Calculate total minimum width of all buttons.
     */
    int calculateMinimumWidth() const;
};
