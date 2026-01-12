/*
  ==============================================================================

    BatchPresetManager.h
    Created: 2025
    Author:  ZQ SFX

    Manages saving and loading of batch processing presets.
    Presets store DSP chain configuration, output settings, and naming patterns.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include "BatchProcessorSettings.h"

namespace waveedit
{

/**
 * @brief Represents a saved batch processing preset
 */
struct BatchPreset
{
    juce::String name;
    juce::String description;
    juce::Time createdTime;
    juce::Time modifiedTime;
    bool isFactoryPreset = false;

    // The actual settings
    BatchProcessorSettings settings;

    /**
     * @brief Convert preset to JSON var
     */
    juce::var toVar() const;

    /**
     * @brief Load preset from JSON var
     */
    static BatchPreset fromVar(const juce::var& v);
};

/**
 * @brief Manages batch processing presets
 */
class BatchPresetManager
{
public:
    BatchPresetManager();
    ~BatchPresetManager() = default;

    // =========================================================================
    // Preset Directory
    // =========================================================================

    /**
     * @brief Get the directory where presets are stored
     */
    static juce::File getPresetDirectory();

    /**
     * @brief Get the file extension for batch presets
     */
    static juce::String getFileExtension() { return ".webatch"; }

    // =========================================================================
    // Preset Management
    // =========================================================================

    /**
     * @brief Load all presets from disk
     */
    void loadPresets();

    /**
     * @brief Get all available presets
     */
    const std::vector<BatchPreset>& getPresets() const { return m_presets; }

    /**
     * @brief Get preset names
     */
    juce::StringArray getPresetNames() const;

    /**
     * @brief Get preset by name
     * @return Pointer to preset or nullptr if not found
     */
    const BatchPreset* getPreset(const juce::String& name) const;

    /**
     * @brief Check if a preset exists
     */
    bool presetExists(const juce::String& name) const;

    /**
     * @brief Check if preset is a factory preset
     */
    bool isFactoryPreset(const juce::String& name) const;

    // =========================================================================
    // Save/Delete
    // =========================================================================

    /**
     * @brief Save settings as a new preset
     * @param name Preset name
     * @param description Optional description
     * @param settings The settings to save
     * @return true if saved successfully
     */
    bool savePreset(const juce::String& name,
                    const juce::String& description,
                    const BatchProcessorSettings& settings);

    /**
     * @brief Delete a preset
     * @param name Preset name
     * @return true if deleted successfully
     */
    bool deletePreset(const juce::String& name);

    /**
     * @brief Rename a preset
     * @param oldName Current name
     * @param newName New name
     * @return true if renamed successfully
     */
    bool renamePreset(const juce::String& oldName, const juce::String& newName);

    // =========================================================================
    // Factory Presets
    // =========================================================================

    /**
     * @brief Create factory presets if they don't exist
     */
    void createFactoryPresets();

private:
    /**
     * @brief Get file path for a preset
     */
    juce::File getPresetFile(const juce::String& name) const;

    /**
     * @brief Load a single preset from file
     */
    bool loadPresetFromFile(const juce::File& file, BatchPreset& preset);

    /**
     * @brief Save a single preset to file
     */
    bool savePresetToFile(const BatchPreset& preset);

    /**
     * @brief Create a factory preset
     */
    void createFactoryPreset(const juce::String& name,
                             const juce::String& description,
                             const std::vector<BatchDSPSettings>& dspChain);

    // =========================================================================
    // Member variables
    // =========================================================================

    std::vector<BatchPreset> m_presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BatchPresetManager)
};

} // namespace waveedit
