/*
  ==============================================================================

    Settings.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "Settings.h"

//==============================================================================
Settings::Settings()
    : m_settingsTree("WaveEditSettings")
{
    // Get platform-specific settings directory
    m_settingsFile = getSettingsFile();

    // Create settings directory if it doesn't exist
    auto settingsDir = getSettingsDirectory();
    if (!settingsDir.exists())
    {
        settingsDir.createDirectory();
    }

    // Load existing settings or create defaults
    if (!load())
    {
        createDefaultSettings();
        save();
    }
}

Settings::~Settings()
{
    // Save settings on exit
    save();
}

Settings& Settings::getInstance()
{
    static Settings instance;
    return instance;
}

//==============================================================================
// File Management

juce::File Settings::getSettingsDirectory() const
{
    // Get platform-specific application data directory
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);

   #if JUCE_MAC
    // macOS: ~/Library/Application Support/WaveEdit/
    return appDataDir.getChildFile("WaveEdit");
   #elif JUCE_WINDOWS
    // Windows: %APPDATA%/WaveEdit/
    return appDataDir.getChildFile("WaveEdit");
   #else
    // Linux: ~/.config/WaveEdit/
    auto configDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                         .getChildFile(".config");
    return configDir.getChildFile("WaveEdit");
   #endif
}

juce::File Settings::getSettingsFile() const
{
    return getSettingsDirectory().getChildFile("settings.json");
}

bool Settings::load()
{
    if (!m_settingsFile.existsAsFile())
    {
        juce::Logger::writeToLog("Settings file does not exist: " + m_settingsFile.getFullPathName());
        return false;
    }

    // Read the file
    juce::String fileContent = m_settingsFile.loadFileAsString();

    if (fileContent.isEmpty())
    {
        juce::Logger::writeToLog("Settings file is empty");
        return false;
    }

    // Parse JSON
    juce::var parsedData = juce::JSON::parse(fileContent);

    if (parsedData.isVoid())
    {
        juce::Logger::writeToLog("Failed to parse settings JSON");
        return false;
    }

    // Convert to ValueTree
    m_settingsTree = juce::ValueTree::fromXml(parsedData.toString());

    if (!m_settingsTree.isValid())
    {
        // Try alternate parsing method
        auto* obj = parsedData.getDynamicObject();
        if (obj != nullptr)
        {
            m_settingsTree = juce::ValueTree("WaveEditSettings");

            // Extract recentFiles array
            if (obj->hasProperty("recentFiles"))
            {
                auto recentFilesVar = obj->getProperty("recentFiles");
                if (recentFilesVar.isArray())
                {
                    auto* arr = recentFilesVar.getArray();
                    juce::ValueTree recentFilesTree("recentFiles");

                    for (int i = 0; i < arr->size(); ++i)
                    {
                        juce::ValueTree fileTree("file");
                        fileTree.setProperty("path", arr->getReference(i).toString(), nullptr);
                        recentFilesTree.appendChild(fileTree, nullptr);
                    }

                    m_settingsTree.appendChild(recentFilesTree, nullptr);
                }
            }

            // Extract version
            if (obj->hasProperty("version"))
            {
                m_settingsTree.setProperty("version", obj->getProperty("version").toString(), nullptr);
            }
        }
    }

    juce::Logger::writeToLog("Settings loaded from: " + m_settingsFile.getFullPathName());
    return true;
}

bool Settings::save()
{
    // Create JSON object
    auto jsonObject = std::make_unique<juce::DynamicObject>();
    jsonObject->setProperty("version", "1.0");

    // Add recent files
    auto recentFilesTree = m_settingsTree.getChildWithName("recentFiles");
    if (recentFilesTree.isValid())
    {
        juce::Array<juce::var> recentFilesArray;

        for (int i = 0; i < recentFilesTree.getNumChildren(); ++i)
        {
            auto fileTree = recentFilesTree.getChild(i);
            juce::String path = fileTree.getProperty("path").toString();

            if (path.isNotEmpty())
            {
                recentFilesArray.add(path);
            }
        }

        jsonObject->setProperty("recentFiles", recentFilesArray);
    }

    // Convert to JSON string
    juce::var jsonVar(jsonObject.release());
    juce::String jsonString = juce::JSON::toString(jsonVar, true);

    // Write to file
    if (!m_settingsFile.replaceWithText(jsonString))
    {
        juce::Logger::writeToLog("Failed to save settings to: " + m_settingsFile.getFullPathName());
        return false;
    }

    juce::Logger::writeToLog("Settings saved to: " + m_settingsFile.getFullPathName());
    return true;
}

//==============================================================================
// Recent Files

void Settings::addRecentFile(const juce::File& file)
{
    if (!file.existsAsFile())
    {
        return;
    }

    juce::String filePath = file.getFullPathName();

    // Get or create recent files tree
    auto recentFilesTree = m_settingsTree.getChildWithName("recentFiles");
    if (!recentFilesTree.isValid())
    {
        recentFilesTree = juce::ValueTree("recentFiles");
        m_settingsTree.appendChild(recentFilesTree, nullptr);
    }

    // Check if file is already in the list
    for (int i = 0; i < recentFilesTree.getNumChildren(); ++i)
    {
        auto fileTree = recentFilesTree.getChild(i);
        if (fileTree.getProperty("path").toString() == filePath)
        {
            // Move to front
            recentFilesTree.removeChild(i, nullptr);
            break;
        }
    }

    // Add to front of list
    juce::ValueTree newFileTree("file");
    newFileTree.setProperty("path", filePath, nullptr);
    recentFilesTree.addChild(newFileTree, 0, nullptr);

    // Limit to MAX_RECENT_FILES
    while (recentFilesTree.getNumChildren() > MAX_RECENT_FILES)
    {
        recentFilesTree.removeChild(recentFilesTree.getNumChildren() - 1, nullptr);
    }

    // Save immediately
    save();

    juce::Logger::writeToLog("Added to recent files: " + filePath);
}

juce::StringArray Settings::getRecentFiles() const
{
    juce::StringArray recentFiles;

    auto recentFilesTree = m_settingsTree.getChildWithName("recentFiles");
    if (!recentFilesTree.isValid())
    {
        return recentFiles;
    }

    for (int i = 0; i < recentFilesTree.getNumChildren(); ++i)
    {
        auto fileTree = recentFilesTree.getChild(i);
        juce::String path = fileTree.getProperty("path").toString();

        if (path.isNotEmpty())
        {
            recentFiles.add(path);
        }
    }

    return recentFiles;
}

void Settings::clearRecentFiles()
{
    auto recentFilesTree = m_settingsTree.getChildWithName("recentFiles");
    if (recentFilesTree.isValid())
    {
        m_settingsTree.removeChild(recentFilesTree, nullptr);
    }

    save();
    juce::Logger::writeToLog("Recent files list cleared");
}

int Settings::cleanupRecentFiles()
{
    auto recentFilesTree = m_settingsTree.getChildWithName("recentFiles");
    if (!recentFilesTree.isValid())
    {
        return 0;
    }

    int removedCount = 0;

    // Iterate backwards to safely remove items
    for (int i = recentFilesTree.getNumChildren() - 1; i >= 0; --i)
    {
        auto fileTree = recentFilesTree.getChild(i);
        juce::String path = fileTree.getProperty("path").toString();

        juce::File file(path);
        if (!file.existsAsFile())
        {
            recentFilesTree.removeChild(i, nullptr);
            removedCount++;
        }
    }

    if (removedCount > 0)
    {
        save();
        juce::Logger::writeToLog("Removed " + juce::String(removedCount) + " invalid files from recent list");
    }

    return removedCount;
}

//==============================================================================
// General Settings

juce::var Settings::getSetting(const juce::String& key, const juce::var& defaultValue) const
{
    juce::ScopedLock lock(m_settingsLock);  // Thread-safe access

    auto tree = const_cast<Settings*>(this)->getTreeByPath(key, false);

    if (!tree.isValid())
    {
        return defaultValue;
    }

    return tree.getProperty("value", defaultValue);
}

void Settings::setSetting(const juce::String& key, const juce::var& value)
{
    juce::ScopedLock lock(m_settingsLock);  // Thread-safe access

    auto tree = getTreeByPath(key, true);

    if (tree.isValid())
    {
        tree.setProperty("value", value, nullptr);
        save();
    }
}

//==============================================================================
// Region Settings (Phase 3.3)

bool Settings::getSnapRegionsToZeroCrossings() const
{
    // Default: false (snap disabled by default)
    return getSetting("region.snapToZeroCrossings", false);
}

void Settings::setSnapRegionsToZeroCrossings(bool enabled)
{
    setSetting("region.snapToZeroCrossings", enabled);
    juce::Logger::writeToLog("Region zero-crossing snap: " + juce::String(enabled ? "enabled" : "disabled"));
}

bool Settings::getAutoPreviewRegions() const
{
    // Default: false (auto-preview disabled by default)
    return getSetting("region.autoPreview", false);
}

void Settings::setAutoPreviewRegions(bool enabled)
{
    setSetting("region.autoPreview", enabled);
    juce::Logger::writeToLog("Region auto-preview: " + juce::String(enabled ? "enabled" : "disabled"));
}

//==============================================================================
// Private Methods

void Settings::createDefaultSettings()
{
    m_settingsTree = juce::ValueTree("WaveEditSettings");
    m_settingsTree.setProperty("version", "1.0", nullptr);

    // Create empty recent files list
    auto recentFilesTree = juce::ValueTree("recentFiles");
    m_settingsTree.appendChild(recentFilesTree, nullptr);

    juce::Logger::writeToLog("Created default settings");
}

juce::ValueTree Settings::getTreeByPath(const juce::String& path, bool createIfMissing)
{
    auto pathComponents = juce::StringArray::fromTokens(path, ".", "");

    if (pathComponents.isEmpty())
    {
        return {};
    }

    juce::ValueTree current = m_settingsTree;

    for (const auto& component : pathComponents)
    {
        auto child = current.getChildWithName(component);

        if (!child.isValid())
        {
            if (createIfMissing)
            {
                child = juce::ValueTree(component);
                current.appendChild(child, nullptr);
            }
            else
            {
                return {};
            }
        }

        current = child;
    }

    return current;
}
