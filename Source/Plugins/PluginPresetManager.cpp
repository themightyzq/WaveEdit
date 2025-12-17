/*
  ==============================================================================

    PluginPresetManager.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "PluginPresetManager.h"

static const juce::String kPresetExtension = ".wepchain";  // WaveEdit Plugin Chain

//==============================================================================
PluginPresetManager::PluginPresetManager()
{
}

PluginPresetManager::~PluginPresetManager()
{
}

//==============================================================================
// Preset Directories

juce::File PluginPresetManager::getPresetDirectory()
{
#if JUCE_MAC
    auto appSupport = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return appSupport.getChildFile("WaveEdit/Presets/PluginChains");
#elif JUCE_WINDOWS
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return appData.getChildFile("WaveEdit/Presets/PluginChains");
#else
    auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    return home.getChildFile(".waveedit/presets/plugin_chains");
#endif
}

bool PluginPresetManager::ensurePresetDirectoryExists()
{
    auto dir = getPresetDirectory();
    if (!dir.exists())
    {
        return dir.createDirectory();
    }
    return true;
}

//==============================================================================
// Preset Operations

bool PluginPresetManager::savePreset(const PluginChain& chain, const juce::String& presetName)
{
    if (presetName.isEmpty())
    {
        DBG("PluginPresetManager::savePreset - Empty preset name");
        return false;
    }

    if (!ensurePresetDirectoryExists())
    {
        DBG("PluginPresetManager::savePreset - Failed to create preset directory");
        return false;
    }

    auto file = getPresetFile(presetName);
    auto json = chain.saveToJson();

    if (json.isVoid())
    {
        DBG("PluginPresetManager::savePreset - Failed to serialize chain");
        return false;
    }

    // Add preset metadata
    auto* obj = json.getDynamicObject();
    if (obj != nullptr)
    {
        obj->setProperty("presetName", presetName);
        obj->setProperty("createdAt", juce::Time::getCurrentTime().toISO8601(true));
        obj->setProperty("version", "1.0");
    }

    auto jsonString = juce::JSON::toString(json, true);
    return file.replaceWithText(jsonString);
}

bool PluginPresetManager::loadPreset(PluginChain& chain, const juce::String& presetName)
{
    auto file = getPresetFile(presetName);
    return importPreset(chain, file);
}

bool PluginPresetManager::deletePreset(const juce::String& presetName)
{
    auto file = getPresetFile(presetName);
    if (file.existsAsFile())
    {
        return file.deleteFile();
    }
    return false;
}

juce::StringArray PluginPresetManager::getAvailablePresets()
{
    juce::StringArray presets;

    auto dir = getPresetDirectory();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*" + kPresetExtension);
        for (const auto& file : files)
        {
            presets.add(file.getFileNameWithoutExtension());
        }
    }

    presets.sort(true);  // Case-insensitive alphabetical sort
    return presets;
}

bool PluginPresetManager::presetExists(const juce::String& presetName)
{
    return getPresetFile(presetName).existsAsFile();
}

//==============================================================================
// Export/Import

bool PluginPresetManager::exportPreset(const PluginChain& chain, const juce::File& file)
{
    auto json = chain.saveToJson();

    if (json.isVoid())
    {
        DBG("PluginPresetManager::exportPreset - Failed to serialize chain");
        return false;
    }

    // Add export metadata
    auto* obj = json.getDynamicObject();
    if (obj != nullptr)
    {
        obj->setProperty("exportedAt", juce::Time::getCurrentTime().toISO8601(true));
        obj->setProperty("version", "1.0");
    }

    auto jsonString = juce::JSON::toString(json, true);
    return file.replaceWithText(jsonString);
}

bool PluginPresetManager::importPreset(PluginChain& chain, const juce::File& file)
{
    if (!file.existsAsFile())
    {
        DBG("PluginPresetManager::importPreset - File not found: " + file.getFullPathName());
        return false;
    }

    auto jsonString = file.loadFileAsString();
    if (jsonString.isEmpty())
    {
        DBG("PluginPresetManager::importPreset - Empty file");
        return false;
    }

    auto json = juce::JSON::parse(jsonString);
    if (json.isVoid())
    {
        DBG("PluginPresetManager::importPreset - Invalid JSON");
        return false;
    }

    return chain.loadFromJson(json);
}

//==============================================================================
// Private

juce::File PluginPresetManager::getPresetFile(const juce::String& presetName)
{
    auto dir = getPresetDirectory();
    return dir.getChildFile(presetName + kPresetExtension);
}
