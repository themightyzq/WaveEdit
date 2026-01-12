/*
  ==============================================================================

    BatchPresetManager.cpp
    Created: 2025
    Author:  ZQ SFX

  ==============================================================================
*/

#include "BatchPresetManager.h"

namespace waveedit
{

// =============================================================================
// BatchPreset
// =============================================================================

juce::var BatchPreset::toVar() const
{
    auto obj = new juce::DynamicObject();
    obj->setProperty("name", name);
    obj->setProperty("description", description);
    obj->setProperty("createdTime", createdTime.toMilliseconds());
    obj->setProperty("modifiedTime", modifiedTime.toMilliseconds());
    obj->setProperty("isFactoryPreset", isFactoryPreset);
    obj->setProperty("settings", settings.toVar());
    return juce::var(obj);
}

BatchPreset BatchPreset::fromVar(const juce::var& v)
{
    BatchPreset preset;
    if (auto* obj = v.getDynamicObject())
    {
        preset.name = obj->getProperty("name").toString();
        preset.description = obj->getProperty("description").toString();
        preset.createdTime = juce::Time(static_cast<juce::int64>(obj->getProperty("createdTime")));
        preset.modifiedTime = juce::Time(static_cast<juce::int64>(obj->getProperty("modifiedTime")));
        preset.isFactoryPreset = obj->getProperty("isFactoryPreset");
        preset.settings = BatchProcessorSettings::fromVar(obj->getProperty("settings"));
    }
    return preset;
}

// =============================================================================
// BatchPresetManager
// =============================================================================

BatchPresetManager::BatchPresetManager()
{
    // Ensure preset directory exists
    getPresetDirectory().createDirectory();

    // Create factory presets if needed
    createFactoryPresets();

    // Load all presets
    loadPresets();
}

juce::File BatchPresetManager::getPresetDirectory()
{
    auto appData = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory);

#if JUCE_MAC
    return appData.getChildFile("Application Support/WaveEdit/Presets/Batch");
#elif JUCE_WINDOWS
    return appData.getChildFile("WaveEdit/Presets/Batch");
#else
    return appData.getChildFile(".waveedit/presets/batch");
#endif
}

void BatchPresetManager::loadPresets()
{
    m_presets.clear();

    juce::File presetDir = getPresetDirectory();
    if (!presetDir.exists())
        return;

    // Find all preset files
    auto files = presetDir.findChildFiles(
        juce::File::findFiles, false, "*" + getFileExtension());

    for (const auto& file : files)
    {
        BatchPreset preset;
        if (loadPresetFromFile(file, preset))
        {
            m_presets.push_back(preset);
        }
    }

    // Sort by name
    std::sort(m_presets.begin(), m_presets.end(),
        [](const BatchPreset& a, const BatchPreset& b)
        {
            // Factory presets first
            if (a.isFactoryPreset != b.isFactoryPreset)
                return a.isFactoryPreset;
            return a.name.compareIgnoreCase(b.name) < 0;
        });
}

juce::StringArray BatchPresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (const auto& preset : m_presets)
        names.add(preset.name);
    return names;
}

const BatchPreset* BatchPresetManager::getPreset(const juce::String& name) const
{
    for (const auto& preset : m_presets)
    {
        if (preset.name.equalsIgnoreCase(name))
            return &preset;
    }
    return nullptr;
}

bool BatchPresetManager::presetExists(const juce::String& name) const
{
    return getPreset(name) != nullptr;
}

bool BatchPresetManager::isFactoryPreset(const juce::String& name) const
{
    auto* preset = getPreset(name);
    return preset && preset->isFactoryPreset;
}

bool BatchPresetManager::savePreset(const juce::String& name,
                                     const juce::String& description,
                                     const BatchProcessorSettings& settings)
{
    // Don't allow overwriting factory presets
    if (isFactoryPreset(name))
    {
        DBG("BatchPresetManager: Cannot overwrite factory preset: " + name);
        return false;
    }

    BatchPreset preset;
    preset.name = name;
    preset.description = description;
    preset.settings = settings;
    preset.isFactoryPreset = false;

    // Set timestamps
    auto now = juce::Time::getCurrentTime();
    if (presetExists(name))
    {
        // Keep original creation time
        auto* existing = getPreset(name);
        if (existing)
            preset.createdTime = existing->createdTime;
        else
            preset.createdTime = now;
    }
    else
    {
        preset.createdTime = now;
    }
    preset.modifiedTime = now;

    if (savePresetToFile(preset))
    {
        // Update or add to our list
        bool found = false;
        for (auto& p : m_presets)
        {
            if (p.name.equalsIgnoreCase(name))
            {
                p = preset;
                found = true;
                break;
            }
        }

        if (!found)
            m_presets.push_back(preset);

        DBG("BatchPresetManager: Saved preset: " + name);
        return true;
    }

    return false;
}

bool BatchPresetManager::deletePreset(const juce::String& name)
{
    // Don't allow deleting factory presets
    if (isFactoryPreset(name))
    {
        DBG("BatchPresetManager: Cannot delete factory preset: " + name);
        return false;
    }

    juce::File file = getPresetFile(name);
    if (file.existsAsFile())
    {
        if (file.deleteFile())
        {
            // Remove from our list
            m_presets.erase(
                std::remove_if(m_presets.begin(), m_presets.end(),
                    [&name](const BatchPreset& p) {
                        return p.name.equalsIgnoreCase(name);
                    }),
                m_presets.end());

            DBG("BatchPresetManager: Deleted preset: " + name);
            return true;
        }
    }

    return false;
}

bool BatchPresetManager::renamePreset(const juce::String& oldName, const juce::String& newName)
{
    // Don't allow renaming factory presets
    if (isFactoryPreset(oldName))
    {
        DBG("BatchPresetManager: Cannot rename factory preset: " + oldName);
        return false;
    }

    // Check if new name already exists
    if (presetExists(newName))
    {
        DBG("BatchPresetManager: Preset already exists: " + newName);
        return false;
    }

    auto* preset = const_cast<BatchPreset*>(getPreset(oldName));
    if (!preset)
        return false;

    // Delete old file
    juce::File oldFile = getPresetFile(oldName);

    // Update name and save
    preset->name = newName;
    preset->modifiedTime = juce::Time::getCurrentTime();

    if (savePresetToFile(*preset))
    {
        oldFile.deleteFile();
        DBG("BatchPresetManager: Renamed preset: " + oldName + " -> " + newName);
        return true;
    }

    // Restore old name on failure
    preset->name = oldName;
    return false;
}

bool BatchPresetManager::exportPreset(const juce::String& name, const juce::File& destinationFile)
{
    const BatchPreset* preset = getPreset(name);
    if (!preset)
    {
        DBG("BatchPresetManager: Cannot export - preset not found: " + name);
        return false;
    }

    juce::String json = juce::JSON::toString(preset->toVar(), true);
    if (destinationFile.replaceWithText(json))
    {
        DBG("BatchPresetManager: Exported preset '" + name + "' to " + destinationFile.getFullPathName());
        return true;
    }

    return false;
}

bool BatchPresetManager::importPreset(const juce::File& sourceFile, juce::String& importedName)
{
    if (!sourceFile.existsAsFile())
    {
        DBG("BatchPresetManager: Import file does not exist: " + sourceFile.getFullPathName());
        return false;
    }

    juce::String content = sourceFile.loadFileAsString();
    if (content.isEmpty())
    {
        DBG("BatchPresetManager: Import file is empty");
        return false;
    }

    auto parsed = juce::JSON::parse(content);
    if (parsed.isVoid())
    {
        DBG("BatchPresetManager: Import file is not valid JSON");
        return false;
    }

    BatchPreset preset = BatchPreset::fromVar(parsed);
    if (preset.name.isEmpty())
    {
        DBG("BatchPresetManager: Import file has no preset name");
        return false;
    }

    // Generate unique name if preset already exists
    juce::String baseName = preset.name;
    int suffix = 1;
    while (presetExists(preset.name))
    {
        preset.name = baseName + " (" + juce::String(suffix++) + ")";
    }

    // Mark as user preset, not factory
    preset.isFactoryPreset = false;
    preset.modifiedTime = juce::Time::getCurrentTime();

    if (savePresetToFile(preset))
    {
        m_presets.push_back(preset);

        // Sort the presets
        std::sort(m_presets.begin(), m_presets.end(),
            [](const BatchPreset& a, const BatchPreset& b)
            {
                if (a.isFactoryPreset != b.isFactoryPreset)
                    return a.isFactoryPreset;
                return a.name.compareIgnoreCase(b.name) < 0;
            });

        importedName = preset.name;
        DBG("BatchPresetManager: Imported preset as '" + preset.name + "'");
        return true;
    }

    return false;
}

void BatchPresetManager::createFactoryPresets()
{
    juce::File presetDir = getPresetDirectory();
    presetDir.createDirectory();

    // Normalize Only preset
    {
        std::vector<BatchDSPSettings> chain;
        BatchDSPSettings normalize;
        normalize.operation = BatchDSPOperation::NORMALIZE;
        normalize.enabled = true;
        normalize.normalizeTargetDb = -0.3f;
        chain.push_back(normalize);

        createFactoryPreset("Normalize Only",
                            "Normalize audio to -0.3dB peak",
                            chain);
    }

    // Normalize + DC Offset preset
    {
        std::vector<BatchDSPSettings> chain;

        BatchDSPSettings dcOffset;
        dcOffset.operation = BatchDSPOperation::DC_OFFSET;
        dcOffset.enabled = true;
        chain.push_back(dcOffset);

        BatchDSPSettings normalize;
        normalize.operation = BatchDSPOperation::NORMALIZE;
        normalize.enabled = true;
        normalize.normalizeTargetDb = -0.3f;
        chain.push_back(normalize);

        createFactoryPreset("Normalize + DC Offset",
                            "Remove DC offset and normalize to -0.3dB",
                            chain);
    }

    // Broadcast Ready preset
    {
        std::vector<BatchDSPSettings> chain;

        BatchDSPSettings dcOffset;
        dcOffset.operation = BatchDSPOperation::DC_OFFSET;
        dcOffset.enabled = true;
        chain.push_back(dcOffset);

        BatchDSPSettings normalize;
        normalize.operation = BatchDSPOperation::NORMALIZE;
        normalize.enabled = true;
        normalize.normalizeTargetDb = -3.0f;
        chain.push_back(normalize);

        BatchDSPSettings fadeIn;
        fadeIn.operation = BatchDSPOperation::FADE_IN;
        fadeIn.enabled = true;
        fadeIn.fadeDurationMs = 10.0f;
        fadeIn.fadeType = 0; // Linear
        chain.push_back(fadeIn);

        BatchDSPSettings fadeOut;
        fadeOut.operation = BatchDSPOperation::FADE_OUT;
        fadeOut.enabled = true;
        fadeOut.fadeDurationMs = 10.0f;
        fadeOut.fadeType = 0; // Linear
        chain.push_back(fadeOut);

        createFactoryPreset("Broadcast Ready",
                            "Clean up and normalize for broadcast (-3dB, short fades)",
                            chain);
    }

    // Podcast Prep preset
    {
        std::vector<BatchDSPSettings> chain;

        BatchDSPSettings dcOffset;
        dcOffset.operation = BatchDSPOperation::DC_OFFSET;
        dcOffset.enabled = true;
        chain.push_back(dcOffset);

        BatchDSPSettings normalize;
        normalize.operation = BatchDSPOperation::NORMALIZE;
        normalize.enabled = true;
        normalize.normalizeTargetDb = -1.0f;
        chain.push_back(normalize);

        createFactoryPreset("Podcast Prep",
                            "Prepare audio for podcast publishing (-1dB peak)",
                            chain);
    }

    // Game Audio Export preset
    {
        std::vector<BatchDSPSettings> chain;

        BatchDSPSettings dcOffset;
        dcOffset.operation = BatchDSPOperation::DC_OFFSET;
        dcOffset.enabled = true;
        chain.push_back(dcOffset);

        BatchDSPSettings normalize;
        normalize.operation = BatchDSPOperation::NORMALIZE;
        normalize.enabled = true;
        normalize.normalizeTargetDb = -0.1f;
        chain.push_back(normalize);

        BatchDSPSettings fadeIn;
        fadeIn.operation = BatchDSPOperation::FADE_IN;
        fadeIn.enabled = true;
        fadeIn.fadeDurationMs = 5.0f;
        fadeIn.fadeType = 1; // Exponential
        chain.push_back(fadeIn);

        BatchDSPSettings fadeOut;
        fadeOut.operation = BatchDSPOperation::FADE_OUT;
        fadeOut.enabled = true;
        fadeOut.fadeDurationMs = 20.0f;
        fadeOut.fadeType = 1; // Exponential
        chain.push_back(fadeOut);

        createFactoryPreset("Game Audio Export",
                            "Maximize volume with micro-fades for game audio",
                            chain);
    }

    // Gain +6dB preset
    {
        std::vector<BatchDSPSettings> chain;
        BatchDSPSettings gain;
        gain.operation = BatchDSPOperation::GAIN;
        gain.enabled = true;
        gain.gainDb = 6.0f;
        chain.push_back(gain);

        createFactoryPreset("Gain +6dB",
                            "Increase volume by 6dB",
                            chain);
    }

    // Gain -6dB preset
    {
        std::vector<BatchDSPSettings> chain;
        BatchDSPSettings gain;
        gain.operation = BatchDSPOperation::GAIN;
        gain.enabled = true;
        gain.gainDb = -6.0f;
        chain.push_back(gain);

        createFactoryPreset("Gain -6dB",
                            "Decrease volume by 6dB",
                            chain);
    }
}

juce::File BatchPresetManager::getPresetFile(const juce::String& name) const
{
    // Sanitize name for filename
    juce::String safeName = name.replaceCharacters("/\\:*?\"<>|", "_________");
    return getPresetDirectory().getChildFile(safeName + getFileExtension());
}

bool BatchPresetManager::loadPresetFromFile(const juce::File& file, BatchPreset& preset)
{
    if (!file.existsAsFile())
        return false;

    juce::String content = file.loadFileAsString();
    if (content.isEmpty())
        return false;

    auto parsed = juce::JSON::parse(content);
    if (parsed.isVoid())
        return false;

    preset = BatchPreset::fromVar(parsed);
    return !preset.name.isEmpty();
}

bool BatchPresetManager::savePresetToFile(const BatchPreset& preset)
{
    juce::File file = getPresetFile(preset.name);
    juce::String json = juce::JSON::toString(preset.toVar(), true);
    return file.replaceWithText(json);
}

void BatchPresetManager::createFactoryPreset(const juce::String& name,
                                              const juce::String& description,
                                              const std::vector<BatchDSPSettings>& dspChain)
{
    juce::File file = getPresetFile(name);

    // Only create if it doesn't exist
    if (file.existsAsFile())
        return;

    BatchPreset preset;
    preset.name = name;
    preset.description = description;
    preset.isFactoryPreset = true;
    preset.createdTime = juce::Time::getCurrentTime();
    preset.modifiedTime = preset.createdTime;

    // Set default output pattern
    preset.settings.outputPattern = "{filename}_processed";
    preset.settings.dspChain = dspChain;

    savePresetToFile(preset);

    DBG("BatchPresetManager: Created factory preset: " + name);
}

} // namespace waveedit
