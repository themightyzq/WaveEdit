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
// Generic settings serialization helpers.
//
// setSetting()/getSetting() store every key as a nested subtree under
// m_settingsTree (see getTreeByPath): "display.theme" becomes
// display -> theme, with the value held in a "value" property on the leaf
// node. A single node can be BOTH a leaf and a parent -- e.g.
// "display.waveformColor" (a leaf) and "display.waveformColor.ch0" (its
// child) coexist. To round-trip ANY current or future key without per-key
// code, we mirror the tree into nested JSON objects: each node becomes an
// object, its own leaf value (if any) is stored under the reserved
// "__value__" key, and each child becomes a nested object keyed by the
// child's type name. The reserved key can never collide with a real setting
// token (no key uses "__value__"). juce::var preserves bool/int/double/String
// through JSON, so types survive the round-trip.
namespace
{
    constexpr const char* kLeafValueKey = "__value__";

    void serializeSettingsNode(const juce::ValueTree& node, juce::DynamicObject& obj)
    {
        if (node.hasProperty("value"))
            obj.setProperty(juce::Identifier(kLeafValueKey), node.getProperty("value"));

        for (int i = 0; i < node.getNumChildren(); ++i)
        {
            auto child = node.getChild(i);
            auto childObj = std::make_unique<juce::DynamicObject>();
            serializeSettingsNode(child, *childObj);
            obj.setProperty(child.getType(), juce::var(childObj.release()));
        }
    }

    void deserializeSettingsNode(juce::ValueTree& node, const juce::DynamicObject& obj)
    {
        for (const auto& prop : obj.getProperties())
        {
            if (prop.name == juce::Identifier(kLeafValueKey))
            {
                node.setProperty("value", prop.value, nullptr);
                continue;
            }

            if (auto* childObj = prop.value.getDynamicObject())
            {
                auto child = node.getOrCreateChildWithName(prop.name, nullptr);
                deserializeSettingsNode(child, *childObj);
            }
        }
    }
}  // namespace

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

Settings::Settings(const juce::File& settingsFile)
    : m_settingsTree("WaveEditSettings")
{
    // Explicit-path construction: bind straight to the given file. No
    // singleton, no legacy migration, no touching the shared user location.
    m_settingsFile = settingsFile;
    settingsFile.getParentDirectory().createDirectory();

    if (!load())
        createDefaultSettings();
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
    // Once bound to a concrete file (default ctor after line-75 init, or the
    // explicit-path ctor used by tests), report THAT file -- never recompute
    // the shared per-user path, or callers/tests reading the bound path back
    // get the wrong location. Empty only during default construction, when we
    // must compute the canonical path to bind to.
    if (m_settingsFile != juce::File())
        return m_settingsFile;

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

    auto* obj = parsedData.getDynamicObject();
    if (obj == nullptr)
    {
        juce::Logger::writeToLog("Settings::load - Failed to parse settings JSON (not an object)");
        return false;
    }

    // Rebuild the tree from scratch and repopulate it generically. This
    // restores EVERYTHING the last save wrote (version, recent files, root
    // properties, and every setSetting key). Old settings.json files that
    // only carried version + recentFiles still load: the extra sections are
    // simply absent and skipped.
    m_settingsTree = juce::ValueTree("WaveEditSettings");

    // version (backward-compatible top-level key)
    if (obj->hasProperty("version"))
        m_settingsTree.setProperty("version", obj->getProperty("version"), nullptr);

    // recentFiles (legacy format: a flat array of path strings)
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

    // rootProperties (generic scalar properties set directly on the root
    // tree, e.g. lastFileDirectory). Held in a named local so the parsed
    // DynamicObject outlives the loop.
    if (obj->hasProperty("rootProperties"))
    {
        auto rootPropsVar = obj->getProperty("rootProperties");
        if (auto* rootProps = rootPropsVar.getDynamicObject())
            for (const auto& prop : rootProps->getProperties())
                m_settingsTree.setProperty(prop.name, prop.value, nullptr);
    }

    // settings (generic nested tree of every setSetting key)
    if (obj->hasProperty("settings"))
    {
        auto settingsVar = obj->getProperty("settings");
        if (auto* settingsObj = settingsVar.getDynamicObject())
            deserializeSettingsNode(m_settingsTree, *settingsObj);
    }

    DBG("Settings loaded from: " + m_settingsFile.getFullPathName());
    return true;
}

bool Settings::save()
{
    // Create JSON object
    auto jsonObject = std::make_unique<juce::DynamicObject>();

    // version -- preserve whatever is on the tree (default "1.0"); kept as a
    // top-level key for backward compatibility with old settings.json files.
    juce::var versionVar = m_settingsTree.getProperty("version");
    jsonObject->setProperty("version", versionVar.isVoid() ? juce::var("1.0") : versionVar);

    // Add recent files (legacy format: a flat array of path strings).
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

    // Root-level scalar properties other than version (e.g. lastFileDirectory).
    auto rootProps = std::make_unique<juce::DynamicObject>();
    for (int i = 0; i < m_settingsTree.getNumProperties(); ++i)
    {
        auto name = m_settingsTree.getPropertyName(i);
        if (name.toString() == "version")
            continue;  // written explicitly above
        rootProps->setProperty(name, m_settingsTree.getProperty(name));
    }
    if (rootProps->getProperties().size() > 0)
        jsonObject->setProperty("rootProperties", juce::var(rootProps.release()));

    // Every setSetting() key, serialized generically so current AND future
    // keys persist without per-key code (see the helpers at the top of file).
    auto settingsObj = std::make_unique<juce::DynamicObject>();
    for (int i = 0; i < m_settingsTree.getNumChildren(); ++i)
    {
        auto child = m_settingsTree.getChild(i);
        if (child.getType().toString() == "recentFiles")
            continue;  // handled above in the legacy array format

        auto childObj = std::make_unique<juce::DynamicObject>();
        serializeSettingsNode(child, *childObj);
        settingsObj->setProperty(child.getType(), juce::var(childObj.release()));
    }
    if (settingsObj->getProperties().size() > 0)
        jsonObject->setProperty("settings", juce::var(settingsObj.release()));

    // Convert to JSON string
    juce::var jsonVar(jsonObject.release());
    juce::String jsonString = juce::JSON::toString(jsonVar, true);

    // Write to file (atomic replace)
    if (!m_settingsFile.replaceWithText(jsonString))
    {
        juce::Logger::writeToLog("Settings::save - Failed to save settings to: " + m_settingsFile.getFullPathName());
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
    // "__value__" is the serializer's reserved leaf sentinel; a key segment
    // with that name would corrupt a sibling scalar on save. Reject it.
    if (key == "__value__" || key.contains(".__value__") || key.startsWith("__value__."))
    {
        jassertfalse;
        juce::Logger::writeToLog("Settings::setSetting - rejected reserved key: " + key);
        return;
    }

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
    // UX25: default true. For SFX slicing, snapping region edges to zero
    // crossings prevents edge clicks/pops on the exported slices.
    return getSetting("region.snapToZeroCrossings", true);
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
