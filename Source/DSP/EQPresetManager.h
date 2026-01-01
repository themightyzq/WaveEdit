/*
  ==============================================================================

    EQPresetManager.h
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
#include "DynamicParametricEQ.h"

/**
 * Manages saving and loading EQ presets for the Graphical EQ.
 *
 * Presets are stored as JSON files containing:
 * - EQ band parameters (frequency, gain, Q, filter type, enabled)
 * - Output gain
 * - Preset metadata (name, creation date, version)
 *
 * Default preset location: ~/Library/Application Support/WaveEdit/Presets/EQ/
 * File extension: .weeq (WaveEdit EQ)
 */
class EQPresetManager
{
public:
    //==============================================================================
    EQPresetManager();
    ~EQPresetManager();

    //==============================================================================
    // Preset Directories

    /** Get the preset directory for EQ presets */
    static juce::File getPresetDirectory();

    /** Ensure the preset directory exists */
    static bool ensurePresetDirectoryExists();

    //==============================================================================
    // Preset Operations

    /**
     * Save EQ parameters to a preset file.
     * @param params The parameters to save
     * @param presetName Name for the preset (without extension)
     * @return true if saved successfully
     */
    static bool savePreset(const DynamicParametricEQ::Parameters& params,
                          const juce::String& presetName);

    /**
     * Load EQ parameters from a preset file.
     * @param params The parameters to load into
     * @param presetName Name of the preset to load
     * @return true if loaded successfully
     */
    static bool loadPreset(DynamicParametricEQ::Parameters& params,
                          const juce::String& presetName);

    /**
     * Delete a user preset file.
     * @param presetName Name of the preset to delete
     * @return true if deleted successfully
     */
    static bool deletePreset(const juce::String& presetName);

    /**
     * Get a list of available user preset names.
     * @return Array of preset names (without extensions)
     */
    static juce::StringArray getAvailablePresets();

    /**
     * Check if a user preset exists.
     * @param presetName Name of the preset to check
     * @return true if preset exists
     */
    static bool presetExists(const juce::String& presetName);

    //==============================================================================
    // Export/Import

    /**
     * Export EQ parameters to a specified file location.
     * @param params The parameters to export
     * @param file The destination file
     * @return true if exported successfully
     */
    static bool exportPreset(const DynamicParametricEQ::Parameters& params,
                            const juce::File& file);

    /**
     * Import EQ parameters from a file.
     * @param params The parameters to load into
     * @param file The source file
     * @return true if imported successfully
     */
    static bool importPreset(DynamicParametricEQ::Parameters& params,
                            const juce::File& file);

    //==============================================================================
    // Factory Presets

    /**
     * Get a factory preset by name.
     * @param name Name of the factory preset
     * @return EQ parameters for the preset, or flat preset if not found
     */
    static DynamicParametricEQ::Parameters getFactoryPreset(const juce::String& name);

    /**
     * Get list of factory preset names.
     * @return Array of factory preset names
     */
    static juce::StringArray getFactoryPresetNames();

    /**
     * Check if a preset name is a factory preset.
     * @param name Preset name to check
     * @return true if it's a factory preset
     */
    static bool isFactoryPreset(const juce::String& name);

private:
    //==============================================================================
    static juce::File getPresetFile(const juce::String& presetName);

    /** Convert EQ parameters to JSON */
    static juce::var parametersToJson(const DynamicParametricEQ::Parameters& params);

    /** Convert JSON to EQ parameters */
    static bool jsonToParameters(const juce::var& json,
                                DynamicParametricEQ::Parameters& params);

    /** Convert filter type to string for JSON */
    static juce::String filterTypeToString(DynamicParametricEQ::FilterType type);

    /** Convert string to filter type from JSON */
    static DynamicParametricEQ::FilterType stringToFilterType(const juce::String& str);

    //==============================================================================
    // Factory preset generators
    static DynamicParametricEQ::Parameters createFlatPreset();
    static DynamicParametricEQ::Parameters createDefaultPreset();
    static DynamicParametricEQ::Parameters createVocalPresencePreset();
    static DynamicParametricEQ::Parameters createDeMuddyPreset();
    static DynamicParametricEQ::Parameters createWarmthPreset();
    static DynamicParametricEQ::Parameters createBrightPreset();
    static DynamicParametricEQ::Parameters createBassBoostPreset();
    static DynamicParametricEQ::Parameters createLowShelfPreset();
    static DynamicParametricEQ::Parameters createLowCutPreset();
    static DynamicParametricEQ::Parameters createHighShelfPreset();
    static DynamicParametricEQ::Parameters createHighCutPreset();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQPresetManager)
};
