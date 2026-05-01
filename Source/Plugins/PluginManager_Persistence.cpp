/*
  ==============================================================================

    PluginManager_Persistence.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Split out from PluginManager.cpp under CLAUDE.md §7.5 (file-size cap).
    Hosts the disk-persistence surface — the plugin-list cache (XML),
    blacklist text file, custom-search-path text file, and the
    incremental-cache XML — plus the default-blacklist seeding stub.
    The scanning thread, plugin-instance creation, and search/discovery
    APIs all stay in PluginManager.cpp.

  ==============================================================================
*/

#include "PluginManager.h"

//==============================================================================
// Cache Management
//==============================================================================
juce::File PluginManager::getPluginCacheFile() const
{
    // Use app data directory
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("WaveEdit");

    if (!appDataDir.exists())
    {
        appDataDir.createDirectory();
    }

    return appDataDir.getChildFile("plugins.xml");
}

void PluginManager::saveCache()
{
    const juce::ScopedLock sl(m_lock);

    auto cacheFile = getPluginCacheFile();

    // Create XML from plugin list
    auto xml = m_knownPluginList.createXml();
    if (xml != nullptr)
    {
        if (xml->writeTo(cacheFile))
        {
            m_lastScanDate = juce::Time::getCurrentTime();
            DBG("PluginManager: Saved plugin cache to " + cacheFile.getFullPathName());
        }
        else
        {
            DBG("PluginManager: Failed to save plugin cache");
        }
    }
}

bool PluginManager::loadCache()
{
    const juce::ScopedLock sl(m_lock);

    try
    {
        auto cacheFile = getPluginCacheFile();

        if (!cacheFile.existsAsFile())
        {
            DBG("PluginManager: No plugin cache found");
            return false;
        }

        auto xml = juce::XmlDocument::parse(cacheFile);
        if (xml != nullptr)
        {
            m_knownPluginList.recreateFromXml(*xml);
            m_lastScanDate = cacheFile.getLastModificationTime();

            DBG("PluginManager: Loaded " + juce::String(m_knownPluginList.getTypes().size()) + " plugins from cache");
            return true;
        }

        DBG("PluginManager: Failed to parse plugin cache");
        return false;
    }
    catch (const std::exception& e)
    {
        // Cache might be corrupted - delete it and start fresh
        DBG("PluginManager: Exception loading cache: " + juce::String(e.what()));
        DBG("PluginManager: Will delete corrupted cache and rescan");

        try
        {
            auto cacheFile = getPluginCacheFile();
            if (cacheFile.existsAsFile())
            {
                cacheFile.deleteFile();
            }
        }
        catch (...) {}

        return false;
    }
    catch (...)
    {
        DBG("PluginManager: Unknown exception loading cache");
        return false;
    }
}

bool PluginManager::clearCache()
{
    // Cancel any in-progress scan first to avoid race conditions
    // Always call cancelScan() to avoid TOCTOU race - it's safe to call even when no scan is running
    cancelScan();

    const juce::ScopedLock sl(m_lock);

    bool allDeleted = true;
    juce::StringArray failedFiles;

    // 1. Delete main cache file (plugins.xml)
    auto cacheFile = getPluginCacheFile();
    if (cacheFile.existsAsFile())
    {
        if (!cacheFile.deleteFile())
        {
            failedFiles.add(cacheFile.getFileName());
            allDeleted = false;
        }
    }

    // 2. Delete dead-mans-pedal file (scan_in_progress.tmp)
    if (m_deadMansPedalFile.existsAsFile())
    {
        if (!m_deadMansPedalFile.deleteFile())
        {
            failedFiles.add(m_deadMansPedalFile.getFileName());
            allDeleted = false;
        }
    }

    // 3. Delete incremental cache file (plugin_incremental_cache.xml)
    auto incrementalFile = getIncrementalCacheFile();
    if (incrementalFile.existsAsFile())
    {
        if (!incrementalFile.deleteFile())
        {
            failedFiles.add(incrementalFile.getFileName());
            allDeleted = false;
        }
    }

    // 4. Delete blacklist file (plugins_blacklist.xml)
    // This allows previously problematic plugins to be rescanned fresh
    auto blacklistFile = getBlacklistFile();
    if (blacklistFile.existsAsFile())
    {
        if (!blacklistFile.deleteFile())
        {
            failedFiles.add(blacklistFile.getFileName());
            allDeleted = false;
        }
    }

    // Clear in-memory state
    m_knownPluginList.clear();
    m_lastScanDate = juce::Time();  // Reset scan date
    m_incrementalCache.clear();     // Clear incremental cache map

    // Log results
    if (allDeleted)
    {
        DBG("PluginManager: Successfully cleared all plugin cache files");
    }
    else
    {
        DBG("PluginManager::clearCache() - Failed to delete: " + failedFiles.joinIntoString(", "));
    }

    return allDeleted;
}

//==============================================================================
// Blacklist Management
//==============================================================================
juce::File PluginManager::getBlacklistFile() const
{
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("WaveEdit");

    if (!appDataDir.exists())
    {
        appDataDir.createDirectory();
    }

    return appDataDir.getChildFile("plugin_blacklist.txt");
}

void PluginManager::loadBlacklist()
{
    const juce::ScopedLock sl(m_lock);

    auto blacklistFile = getBlacklistFile();
    if (blacklistFile.existsAsFile())
    {
        juce::StringArray lines;
        blacklistFile.readLines(lines);

        m_blacklist.clear();
        for (const auto& line : lines)
        {
            juce::String trimmed = line.trim();
            if (trimmed.isNotEmpty() && !trimmed.startsWith("#"))
            {
                m_blacklist.addIfNotAlreadyThere(trimmed);
            }
        }

        DBG("PluginManager: Loaded " + juce::String(m_blacklist.size()) + " blacklisted plugins");
    }
}

void PluginManager::saveBlacklist()
{
    const juce::ScopedLock sl(m_lock);

    auto blacklistFile = getBlacklistFile();

    juce::String content;
    content += "# WaveEdit Plugin Blacklist\n";
    content += "# Plugins listed here will be skipped during scanning\n";
    content += "# (usually because they caused crashes)\n";
    content += "#\n";

    for (const auto& plugin : m_blacklist)
    {
        content += plugin + "\n";
    }

    // Use explicit UTF-8 encoding for consistency with loadFileAsString()
    if (blacklistFile.replaceWithText(content, false, false, "\n"))
    {
        DBG("PluginManager: Saved blacklist with " + juce::String(m_blacklist.size()) + " entries");
    }
    else
    {
        DBG("PluginManager: Failed to save blacklist");
    }
}

void PluginManager::addToBlacklist(const juce::String& fileOrIdentifier)
{
    if (fileOrIdentifier.isEmpty())
        return;

    {
        const juce::ScopedLock sl(m_lock);
        m_blacklist.addIfNotAlreadyThere(fileOrIdentifier);
    }

    saveBlacklist();
    DBG("PluginManager: Added to blacklist: " + fileOrIdentifier);
}

void PluginManager::removeFromBlacklist(const juce::String& fileOrIdentifier)
{
    {
        const juce::ScopedLock sl(m_lock);
        m_blacklist.removeString(fileOrIdentifier);
    }

    saveBlacklist();
    DBG("PluginManager: Removed from blacklist: " + fileOrIdentifier);
}

bool PluginManager::isBlacklisted(const juce::String& fileOrIdentifier) const
{
    const juce::ScopedLock sl(m_lock);

    // Simple exact match or contains check
    // Since we only scan VST3 now, we don't need complex pattern matching
    if (m_blacklist.contains(fileOrIdentifier))
        return true;

    // Also check if any blacklist entry is contained in the identifier (for path matches)
    for (const auto& entry : m_blacklist)
    {
        if (fileOrIdentifier.contains(entry) || entry.contains(fileOrIdentifier))
            return true;
    }

    return false;
}

juce::StringArray PluginManager::getBlacklist() const
{
    const juce::ScopedLock sl(m_lock);
    return m_blacklist;
}

void PluginManager::clearBlacklist()
{
    {
        const juce::ScopedLock sl(m_lock);
        m_blacklist.clear();
    }

    saveBlacklist();
    DBG("PluginManager: Cleared blacklist");
}

//==============================================================================
// Crash Notification Methods
//==============================================================================

juce::StringArray PluginManager::getAndClearNewlyBlacklistedPlugins()
{
    const juce::ScopedLock sl(m_lock);

    juce::StringArray result = m_newlyBlacklistedPlugins;
    m_newlyBlacklistedPlugins.clear();

    return result;
}

bool PluginManager::hasNewlyBlacklistedPlugins() const
{
    const juce::ScopedLock sl(m_lock);
    return !m_newlyBlacklistedPlugins.isEmpty();
}

//==============================================================================
// Custom Search Paths Persistence
//==============================================================================

juce::File PluginManager::getCustomPathsFile() const
{
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("WaveEdit");

    if (!appDataDir.exists())
        appDataDir.createDirectory();

    return appDataDir.getChildFile("custom_plugin_paths.txt");
}

void PluginManager::setCustomSearchPaths(const juce::StringArray& paths)
{
    {
        const juce::ScopedLock sl(m_lock);
        m_customSearchPaths = paths;
    }

    saveCustomSearchPaths();
}

void PluginManager::saveCustomSearchPaths()
{
    const juce::ScopedLock sl(m_lock);

    auto pathsFile = getCustomPathsFile();
    juce::String content;

    content += "# WaveEdit Custom Plugin Search Paths\n";
    content += "# One path per line\n";
    content += "#\n";

    for (const auto& path : m_customSearchPaths)
    {
        content += path + "\n";
    }

    if (pathsFile.replaceWithText(content, false, false, "\n"))
    {
        DBG("PluginManager: Saved " + juce::String(m_customSearchPaths.size()) + " custom paths");
    }
}

void PluginManager::loadCustomSearchPaths()
{
    const juce::ScopedLock sl(m_lock);

    auto pathsFile = getCustomPathsFile();
    if (!pathsFile.existsAsFile())
        return;

    juce::StringArray lines;
    pathsFile.readLines(lines);

    m_customSearchPaths.clear();
    for (const auto& line : lines)
    {
        juce::String trimmed = line.trim();
        if (trimmed.isNotEmpty() && !trimmed.startsWith("#"))
        {
            // Validate path exists
            juce::File dir(trimmed);
            if (dir.isDirectory())
            {
                m_customSearchPaths.addIfNotAlreadyThere(trimmed);
            }
        }
    }

    DBG("PluginManager: Loaded " + juce::String(m_customSearchPaths.size()) + " custom paths");
}

//==============================================================================
// Incremental Cache Management
//==============================================================================

juce::File PluginManager::getIncrementalCacheFile() const
{
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("WaveEdit");

    if (!appDataDir.exists())
        appDataDir.createDirectory();

    return appDataDir.getChildFile("plugin_incremental_cache.xml");
}

void PluginManager::saveIncrementalCache()
{
    const juce::ScopedLock sl(m_lock);

    auto cacheFile = getIncrementalCacheFile();

    juce::XmlElement root("IncrementalPluginCache");
    root.setAttribute("version", 1);
    root.setAttribute("savedAt", static_cast<double>(juce::Time::getCurrentTime().toMilliseconds()));

    for (const auto& [path, entry] : m_incrementalCache)
    {
        auto entryXml = entry.toXml();
        if (entryXml)
            root.addChildElement(entryXml.release());
    }

    if (root.writeTo(cacheFile))
    {
        DBG("PluginManager: Saved incremental cache with " +
            juce::String(m_incrementalCache.size()) + " entries");
    }
}

void PluginManager::loadIncrementalCache()
{
    const juce::ScopedLock sl(m_lock);

    auto cacheFile = getIncrementalCacheFile();
    if (!cacheFile.existsAsFile())
        return;

    auto xml = juce::XmlDocument::parse(cacheFile);
    if (xml == nullptr || xml->getTagName() != "IncrementalPluginCache")
        return;

    m_incrementalCache.clear();

    for (auto* child : xml->getChildIterator())
    {
        if (child->getTagName() == "PluginCacheEntry")
        {
            auto entry = PluginCacheEntry::fromXml(*child);
            if (entry.pluginPath.isNotEmpty())
            {
                m_incrementalCache[entry.pluginPath] = entry;
            }
        }
    }

    DBG("PluginManager: Loaded incremental cache with " +
        juce::String(m_incrementalCache.size()) + " entries");
}

//==============================================================================
// Default Blacklist for Known Problematic Plugins
//==============================================================================

void PluginManager::initializeDefaultBlacklist()
{
    // NOTE: Auto-blacklisting has been removed in favor of out-of-process scanning.
    //
    // With out-of-process scanning (via PluginScannerCoordinator), crashed plugins
    // only crash the worker subprocess, not WaveEdit itself. The coordinator
    // automatically restarts the worker and continues scanning the remaining plugins.
    //
    // Commercial plugins (iZotope, Universal Audio, Baby Audio, etc.) should all
    // work correctly now - any that crash during scanning will be automatically
    // handled by the crash recovery system without requiring manual blacklisting.
    //
    // Users can still manually blacklist plugins via the Plugins menu if needed.

    DBG("PluginManager: Out-of-process scanning enabled - no auto-blacklisting needed");
}
