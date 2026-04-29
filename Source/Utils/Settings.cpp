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

    // One-time migration from the legacy macOS path (see notes in
    // getLegacySettingsDirectory()). No-op on other platforms and on
    // macOS once the new directory is populated.
    migrateLegacySettingsIfNeeded();

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
    // macOS: ~/Library/Application Support/WaveEdit/. Note that JUCE's
    // userApplicationDataDirectory resolves to ~/Library/, so we append
    // the canonical "Application Support/WaveEdit" segment ourselves.
    // This now matches where KeymapManager/ToolbarManager/Batch presets
    // live, so the entire user state is under one folder.
    return appDataDir.getChildFile("Application Support/WaveEdit");
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

juce::File Settings::getLegacySettingsDirectory() const
{
   #if JUCE_MAC
    // Pre-2026-04-29 builds wrote settings.json and the plugin scan
    // cache directly into ~/Library/WaveEdit/ (because
    // userApplicationDataDirectory resolves to ~/Library/, not
    // ~/Library/Application Support/). New builds use the canonical
    // Application Support path; this helper exposes the old path so
    // we can copy any pre-existing data forward.
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return appDataDir.getChildFile("WaveEdit");
   #else
    // Other platforms never had a separate legacy path — return an
    // empty File so the migration step is a no-op.
    return {};
   #endif
}

void Settings::migrateLegacySettingsIfNeeded()
{
    auto oldDir = getLegacySettingsDirectory();
    if (oldDir == juce::File{} || !oldDir.exists())
        return;

    auto newDir = getSettingsDirectory();
    if (oldDir == newDir)
        return;  // Same path on this platform — nothing to migrate.

    // Copy each top-level entry from the legacy directory into the new
    // one if (and only if) it isn't already present in the new
    // location. We never overwrite or delete from the legacy directory
    // — if anything goes wrong, the old data is still intact.
    auto entries = oldDir.findChildFiles(
        juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles, false);

    int migratedFiles = 0;
    int migratedDirs = 0;
    for (const auto& entry : entries)
    {
        auto target = newDir.getChildFile(entry.getFileName());
        if (target.exists())
            continue;  // user already has new version — leave it alone

        bool ok = false;
        if (entry.isDirectory())
        {
            ok = entry.copyDirectoryTo(target);
            if (ok) ++migratedDirs;
        }
        else
        {
            ok = entry.copyFileTo(target);
            if (ok) ++migratedFiles;
        }

        if (!ok)
            juce::Logger::writeToLog("Settings::migrateLegacySettingsIfNeeded: "
                                     "failed to copy " + entry.getFullPathName()
                                     + " — leaving in place at the old path");
    }

    if (migratedFiles + migratedDirs > 0)
    {
        juce::Logger::writeToLog("Settings: migrated " + juce::String(migratedFiles)
            + " file(s) and " + juce::String(migratedDirs) + " directory(ies) from "
            + oldDir.getFullPathName() + " to " + newDir.getFullPathName()
            + " (legacy directory left as backup; safe to delete manually)");
    }
}

juce::File Settings::getSettingsFile() const
{
    return getSettingsDirectory().getChildFile("settings.json");
}

bool Settings::load()
{
    if (!m_settingsFile.existsAsFile())
    {
        DBG("Settings file does not exist: " + m_settingsFile.getFullPathName());
        return false;
    }

    // Read the file
    juce::String fileContent = m_settingsFile.loadFileAsString();

    if (fileContent.isEmpty())
    {
        DBG("Settings file is empty");
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

    DBG("Settings loaded from: " + m_settingsFile.getFullPathName());
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

    DBG("Settings saved to: " + m_settingsFile.getFullPathName());
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

    DBG("Added to recent files: " + filePath);
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
    DBG("Recent files list cleared");
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
        DBG("Removed " + juce::String(removedCount) + " invalid files from recent list");
    }

    return removedCount;
}

void Settings::setLastFileDirectory(const juce::File& directory)
{
    if (!directory.isDirectory())
    {
        juce::Logger::writeToLog("Warning: Attempt to set non-directory as last file directory");
        return;
    }

    m_settingsTree.setProperty("lastFileDirectory", directory.getFullPathName(), nullptr);
    save();
}

juce::File Settings::getLastFileDirectory() const
{
    juce::String lastDir = m_settingsTree.getProperty("lastFileDirectory").toString();

    if (lastDir.isNotEmpty())
    {
        juce::File dir(lastDir);
        if (dir.isDirectory())
        {
            return dir;
        }
    }

    // Default to home directory if no last directory set or invalid
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
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
    DBG("Region zero-crossing snap: " + juce::String(enabled ? "enabled" : "disabled"));
}

bool Settings::getAutoPreviewRegions() const
{
    // Default: false (auto-preview disabled by default)
    return getSetting("region.autoPreview", false);
}

void Settings::setAutoPreviewRegions(bool enabled)
{
    setSetting("region.autoPreview", enabled);
    DBG("Region auto-preview: " + juce::String(enabled ? "enabled" : "disabled"));
}

bool Settings::getRegionsVisible() const
{
    // Default: true (regions visible by default)
    return getSetting("view.showRegions", true);
}

void Settings::setRegionsVisible(bool visible)
{
    setSetting("view.showRegions", visible);
    DBG("Regions visibility: " + juce::String(visible ? "visible" : "hidden"));
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

    DBG("Created default settings");
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
