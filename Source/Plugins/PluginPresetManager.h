/*
  ==============================================================================

    PluginPresetManager.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include "PluginChain.h"

/**
 * Manages saving and loading plugin chain presets.
 *
 * Presets are stored as JSON files containing:
 * - Chain configuration (list of plugins)
 * - Plugin states (base64-encoded binary data)
 * - Bypass states
 *
 * Default preset location: ~/Library/Application Support/WaveEdit/Presets/PluginChains/
 */
class PluginPresetManager
{
public:
    //==============================================================================
    PluginPresetManager();
    ~PluginPresetManager();

    //==============================================================================
    // Preset Directories

    /** Get the preset directory for plugin chains */
    static juce::File getPresetDirectory();

    /** Ensure the preset directory exists */
    static bool ensurePresetDirectoryExists();

    //==============================================================================
    // Preset Operations

    /**
     * Save a plugin chain to a preset file.
     * @param chain The chain to save
     * @param presetName Name for the preset (without extension)
     * @return true if saved successfully
     */
    static bool savePreset(const PluginChain& chain, const juce::String& presetName);

    /**
     * Load a plugin chain from a preset file.
     * @param chain The chain to load into (existing plugins will be cleared)
     * @param presetName Name of the preset to load
     * @return true if loaded successfully
     */
    static bool loadPreset(PluginChain& chain, const juce::String& presetName);

    /**
     * Delete a preset file.
     * @param presetName Name of the preset to delete
     * @return true if deleted successfully
     */
    static bool deletePreset(const juce::String& presetName);

    /**
     * Get a list of available preset names.
     * @return Array of preset names (without extensions)
     */
    static juce::StringArray getAvailablePresets();

    /**
     * Check if a preset exists.
     * @param presetName Name of the preset to check
     * @return true if preset exists
     */
    static bool presetExists(const juce::String& presetName);

    //==============================================================================
    // Export/Import

    /**
     * Export a chain preset to a specified file location.
     * @param chain The chain to export
     * @param file The destination file
     * @return true if exported successfully
     */
    static bool exportPreset(const PluginChain& chain, const juce::File& file);

    /**
     * Import a chain preset from a file.
     * @param chain The chain to load into
     * @param file The source file
     * @return true if imported successfully
     */
    static bool importPreset(PluginChain& chain, const juce::File& file);

private:
    //==============================================================================
    static juce::File getPresetFile(const juce::String& presetName);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPresetManager)
};
