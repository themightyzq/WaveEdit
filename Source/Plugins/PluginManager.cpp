/*
  ==============================================================================

    PluginManager.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "PluginManager.h"
#include "PluginScannerCoordinator.h"
#include "PluginScanDialogs.h"

#include <future>
#include <chrono>

//==============================================================================
// Background Scanner Thread
//==============================================================================
class PluginManager::ScannerThread : public juce::Thread
{
public:
    ScannerThread(PluginManager& owner,
                  juce::AudioPluginFormatManager& formatManager,
                  juce::KnownPluginList& knownPlugins,
                  const juce::File& deadMansPedal,
                  ScanProgressCallback progressCallback,
                  ScanCompleteCallback completeCallback,
                  bool forceRescan)
        : juce::Thread("VST3 Plugin Scanner"),
          m_owner(owner),
          m_formatManager(formatManager),
          m_knownPlugins(knownPlugins),
          m_deadMansPedal(deadMansPedal),
          m_progressCallback(std::move(progressCallback)),
          m_completeCallback(std::move(completeCallback)),
          m_forceRescan(forceRescan)
    {
    }

    void run() override
    {
        int pluginsFound = 0;
        bool success = true;

        try
        {
            // Get search paths
            auto searchPaths = m_owner.getDefaultSearchPaths();

            // Add custom paths
            for (const auto& path : m_owner.getCustomSearchPaths())
            {
                searchPaths.add(juce::File(path));
            }

            // Scan each format
            for (int i = 0; i < m_formatManager.getNumFormats(); ++i)
            {
                if (threadShouldExit())
                    break;

                auto* format = m_formatManager.getFormat(i);
                if (format == nullptr)
                    continue;

                // Get format-specific search paths (VST3 only)
                juce::FileSearchPath formatPaths;

                #if JUCE_PLUGINHOST_VST3
                if (format->getName() == "VST3")
                {
                    formatPaths = m_owner.getVST3SearchPaths();
                }
                #endif

                // NOTE: AudioUnit support removed - we only scan VST3

                // Use default if no format-specific paths
                if (formatPaths.getNumPaths() == 0)
                {
                    formatPaths = format->getDefaultLocationsToSearch();
                }

                // Create scanner
                juce::PluginDirectoryScanner scanner(
                    m_knownPlugins,
                    *format,
                    formatPaths,
                    true,  // Recursive
                    m_deadMansPedal,
                    !m_forceRescan  // Allow plugins from cache if not forcing rescan
                );

                juce::String pluginBeingScanned;
                float progress = 0.0f;

                // Scan all plugins in this format
                while (true)
                {
                    if (threadShouldExit())
                        break;

                    // Get the next plugin that will be scanned (before scanning it)
                    juce::String nextPlugin = scanner.getNextPluginFileThatWillBeScanned();
                    if (nextPlugin.isEmpty())
                        break;  // No more plugins to scan

                    // Check if this plugin is blacklisted
                    if (m_owner.isBlacklisted(nextPlugin))
                    {
                        DBG("PluginManager: Skipping blacklisted plugin: " + nextPlugin);
                        // Skip this plugin without scanning it
                        if (!scanner.skipNextFile())
                            break;  // No more files
                        continue;
                    }

                    // Write current plugin to dead-mans-pedal file BEFORE scanning
                    // If we crash during scan, on next startup we'll know which plugin caused it
                    // Use explicit UTF-8 encoding (false = don't use UTF-16, false = don't write BOM)
                    // This ensures loadFileAsString() can read it correctly on all platforms
                    m_deadMansPedal.replaceWithText(nextPlugin, false, false, "\n");

                    // Report progress on message thread (before actual scan starts)
                    progress = scanner.getProgress();
                    if (m_progressCallback)
                    {
                        juce::MessageManager::callAsync([callback = m_progressCallback,
                                                         progress,
                                                         name = nextPlugin]()
                        {
                            callback(progress, name);
                        });
                    }

                    // Now actually scan this plugin (this is where crashes can happen)
                    // scanNextFile returns false when there are no more files
                    if (!scanner.scanNextFile(true, pluginBeingScanned))
                        break;

                    // If we get here, the plugin scanned without crashing
                    pluginsFound = static_cast<int>(m_knownPlugins.getTypes().size());
                }

                // Clear the dead-mans-pedal after successful scan of this format
                m_deadMansPedal.deleteFile();

                // Check for any failures
                auto failedFiles = scanner.getFailedFiles();
                if (failedFiles.size() > 0)
                {
                    DBG("Plugin scan: " + juce::String(failedFiles.size()) + " plugins failed to load");
                }
            }
        }
        catch (const std::exception& e)
        {
            DBG("Plugin scan exception: " + juce::String(e.what()));
            success = false;
        }
        catch (...)
        {
            // Catch any other exception types (some plugins can throw non-std exceptions)
            DBG("Plugin scan: Unknown exception during scanning");
            success = false;
        }

        // Final count
        pluginsFound = static_cast<int>(m_knownPlugins.getTypes().size());

        // Save cache
        m_owner.saveCache();

        // Report completion on message thread
        if (m_completeCallback)
        {
            juce::MessageManager::callAsync([callback = m_completeCallback, success, pluginsFound]()
            {
                callback(success, pluginsFound);
            });
        }

        // Mark scan as complete
        m_owner.m_scanInProgress.store(false);
    }

private:
    PluginManager& m_owner;
    juce::AudioPluginFormatManager& m_formatManager;
    juce::KnownPluginList& m_knownPlugins;
    juce::File m_deadMansPedal;
    ScanProgressCallback m_progressCallback;
    ScanCompleteCallback m_completeCallback;
    bool m_forceRescan;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScannerThread)
};

//==============================================================================
// PluginManager Implementation
//==============================================================================

PluginManager::PluginManager()
{
    try
    {
        initializeFormats();

        // Set up dead-mans-pedal file for crash recovery
        m_deadMansPedalFile = getPluginCacheFile().getSiblingFile("scan_in_progress.tmp");

        // Load blacklist (safe, won't crash on missing file)
        loadBlacklist();

        // Pre-blacklist known problematic plugins that crash during scanning
        // These plugins have bugs like duplicate Objective-C classes that cause
        // crashes when multiple iZotope plugins are loaded together
        initializeDefaultBlacklist();

        // Check if we crashed during a previous scan
        if (m_deadMansPedalFile.existsAsFile())
        {
            juce::String crashedPlugin = m_deadMansPedalFile.loadFileAsString().trim();
            if (crashedPlugin.isNotEmpty())
            {
                DBG("PluginManager: Detected crash while scanning: " + crashedPlugin);
                DBG("PluginManager: Adding to blacklist");
                addToBlacklist(crashedPlugin);

                // Track for user notification (use lock for thread safety)
                {
                    const juce::ScopedLock sl(m_lock);
                    m_newlyBlacklistedPlugins.addIfNotAlreadyThere(crashedPlugin);
                }
            }
            m_deadMansPedalFile.deleteFile();
        }

        // Load cached plugin list (safe, handles corrupt/missing cache)
        loadCache();

        // Load custom search paths
        loadCustomSearchPaths();

        // Load incremental cache for faster subsequent scans
        loadIncrementalCache();
    }
    catch (const std::exception& e)
    {
        // Log but don't crash - plugin system can initialize later
        DBG("PluginManager: Exception during initialization: " + juce::String(e.what()));
        DBG("PluginManager: Plugin system may have limited functionality");
    }
    catch (...)
    {
        // Catch any other exceptions to prevent startup crash
        DBG("PluginManager: Unknown exception during initialization");
        DBG("PluginManager: Plugin system may have limited functionality");
    }
}

// Destructor moved after ExtendedScannerThread definition
// to avoid incomplete type issues

PluginManager& PluginManager::getInstance()
{
    static PluginManager* instance = new PluginManager();
    return *instance;
}

void PluginManager::initializeFormats()
{
    // IMPORTANT: Only add VST3 format - NOT AudioUnit
    // AudioUnits (especially Apple system AUs like HRTFPanner, AUSpatialMixer, etc.)
    // cause hangs during scanning because they require special system resources
    // and run loop handling that isn't available in background scanning threads.
    // User explicitly requested VST3 only.
    try
    {
        #if JUCE_PLUGINHOST_VST3
        m_formatManager.addFormat(new juce::VST3PluginFormat());
        DBG("PluginManager: Added VST3 format");
        #endif

        // NOTE: AudioUnit format intentionally NOT added
        // If you need to re-enable AU scanning in the future, uncomment:
        // #if JUCE_PLUGINHOST_AU && JUCE_MAC
        // m_formatManager.addFormat(new juce::AudioUnitPluginFormat());
        // #endif
    }
    catch (const std::exception& e)
    {
        DBG("PluginManager: Exception adding formats: " + juce::String(e.what()));
        // Continue without full format support
    }

    DBG("PluginManager: Initialized with " + juce::String(m_formatManager.getNumFormats()) + " plugin format(s) (VST3 only):");
    for (int i = 0; i < m_formatManager.getNumFormats(); ++i)
    {
        auto* format = m_formatManager.getFormat(i);
        if (format != nullptr)
        {
            DBG("  - " + format->getName());
        }
    }

    // Set up out-of-process scanner for crash isolation
    // This is critical - badly behaved plugins can call std::terminate()
    // which would crash the entire app without out-of-process scanning
    try
    {
        auto customScanner = std::make_unique<OutOfProcessPluginScanner>();

        // Set up crash callback to auto-blacklist crashing plugins
        customScanner->setCrashCallback([this](const juce::String& crashedPlugin)
        {
            DBG("PluginManager: Plugin crashed scanner: " + crashedPlugin);
            addToBlacklist(crashedPlugin);

            // Track for user notification (thread-safe via addToBlacklist lock)
            {
                const juce::ScopedLock sl(m_lock);
                m_newlyBlacklistedPlugins.addIfNotAlreadyThere(crashedPlugin);
            }
        });

        m_knownPluginList.setCustomScanner(std::move(customScanner));
        DBG("PluginManager: Out-of-process scanner configured");
    }
    catch (const std::exception& e)
    {
        // If out-of-process scanner fails, we'll use in-process scanning
        // This is less safe but still functional
        DBG("PluginManager: Failed to set up out-of-process scanner: " + juce::String(e.what()));
        DBG("PluginManager: Falling back to in-process scanning (less safe)");
    }
}

//==============================================================================
// Plugin Scanning
//==============================================================================
void PluginManager::startScanAsync(ScanProgressCallback progressCallback,
                                    ScanCompleteCallback completeCallback)
{
    // Use the Extended scanner which is thread-safe and non-blocking
    ScanOptions options;
    options.forceRescan = false;
    options.useIncrementalScan = true;
    options.showSummaryDialog = false;  // Caller handles completion

    // Wrap the simple callback in an extended callback
    ExtendedScanCompleteCallback extendedCallback = nullptr;
    if (completeCallback)
    {
        extendedCallback = [completeCallback](const PluginScanSummary& summary)
        {
            bool success = summary.failedCount == 0;
            int numFound = summary.getTotalPluginsFound();
            completeCallback(success, numFound);
        };
    }

    startScanWithOptions(options, progressCallback, extendedCallback);
}

void PluginManager::forceRescan(ScanProgressCallback progressCallback,
                                 ScanCompleteCallback completeCallback)
{
    // Use the Extended scanner which is thread-safe and non-blocking
    ScanOptions options;
    options.forceRescan = true;
    options.useIncrementalScan = false;  // Force rescan ignores cache
    options.showSummaryDialog = true;    // Show results at end

    // Wrap the simple callback in an extended callback
    ExtendedScanCompleteCallback extendedCallback = nullptr;
    if (completeCallback)
    {
        extendedCallback = [completeCallback](const PluginScanSummary& summary)
        {
            bool success = summary.failedCount == 0;
            int numFound = summary.getTotalPluginsFound();
            completeCallback(success, numFound);
        };
    }

    startScanWithOptions(options, progressCallback, extendedCallback);
}

//==============================================================================
// Plugin Discovery
//==============================================================================
juce::Array<juce::PluginDescription> PluginManager::getAvailablePlugins() const
{
    const juce::ScopedLock sl(m_lock);

    juce::Array<juce::PluginDescription> plugins;
    for (const auto& desc : m_knownPluginList.getTypes())
    {
        // Filter out instrument plugins (effects only)
        if (!desc.isInstrument)
        {
            plugins.add(desc);
        }
    }

    return plugins;
}

juce::Array<juce::PluginDescription> PluginManager::getPluginsByCategory(const juce::String& category) const
{
    const juce::ScopedLock sl(m_lock);

    juce::Array<juce::PluginDescription> plugins;
    for (const auto& desc : m_knownPluginList.getTypes())
    {
        if (!desc.isInstrument && desc.category.containsIgnoreCase(category))
        {
            plugins.add(desc);
        }
    }

    return plugins;
}

juce::Array<juce::PluginDescription> PluginManager::getPluginsByManufacturer(const juce::String& manufacturer) const
{
    const juce::ScopedLock sl(m_lock);

    juce::Array<juce::PluginDescription> plugins;
    for (const auto& desc : m_knownPluginList.getTypes())
    {
        if (!desc.isInstrument && desc.manufacturerName.containsIgnoreCase(manufacturer))
        {
            plugins.add(desc);
        }
    }

    return plugins;
}

juce::StringArray PluginManager::getCategories() const
{
    const juce::ScopedLock sl(m_lock);

    juce::StringArray categories;
    for (const auto& desc : m_knownPluginList.getTypes())
    {
        if (!desc.isInstrument && desc.category.isNotEmpty())
        {
            categories.addIfNotAlreadyThere(desc.category);
        }
    }

    categories.sort(true);
    return categories;
}

juce::StringArray PluginManager::getManufacturers() const
{
    const juce::ScopedLock sl(m_lock);

    juce::StringArray manufacturers;
    for (const auto& desc : m_knownPluginList.getTypes())
    {
        if (!desc.isInstrument && desc.manufacturerName.isNotEmpty())
        {
            manufacturers.addIfNotAlreadyThere(desc.manufacturerName);
        }
    }

    manufacturers.sort(true);
    return manufacturers;
}

juce::Array<juce::PluginDescription> PluginManager::searchPlugins(const juce::String& searchTerm) const
{
    const juce::ScopedLock sl(m_lock);

    juce::Array<juce::PluginDescription> plugins;
    juce::String searchLower = searchTerm.toLowerCase();

    for (const auto& desc : m_knownPluginList.getTypes())
    {
        if (!desc.isInstrument)
        {
            if (desc.name.toLowerCase().contains(searchLower) ||
                desc.manufacturerName.toLowerCase().contains(searchLower) ||
                desc.category.toLowerCase().contains(searchLower))
            {
                plugins.add(desc);
            }
        }
    }

    return plugins;
}

std::optional<juce::PluginDescription> PluginManager::getPluginByIdentifier(const juce::String& identifier) const
{
    const juce::ScopedLock sl(m_lock);

    for (const auto& desc : m_knownPluginList.getTypes())
    {
        if (desc.createIdentifierString() == identifier)
        {
            return desc;
        }
    }

    return std::nullopt;
}

//==============================================================================
// Plugin Instantiation
//==============================================================================
std::unique_ptr<juce::AudioPluginInstance> PluginManager::createPluginInstance(
    const juce::PluginDescription& description,
    double sampleRate,
    int blockSize)
{
    // Plugin instantiation can throw or crash - wrap with exception handling
    // This is especially important for VST3 plugins which can have initialization bugs
    try
    {
        juce::String errorMessage;

        auto instance = m_formatManager.createPluginInstance(
            description,
            sampleRate,
            blockSize,
            errorMessage
        );

        if (instance == nullptr)
        {
            DBG("PluginManager: Failed to create plugin instance: " + errorMessage);
            // Don't blacklist on simple load failure - might be temporary
        }
        else
        {
            DBG("PluginManager: Created plugin instance: " + description.name);
        }

        return instance;
    }
    catch (const std::exception& e)
    {
        DBG("PluginManager: Exception creating plugin instance '" + description.name + "': " + juce::String(e.what()));
        // Plugin threw during instantiation - this is a problematic plugin
        // Consider adding to a "problematic plugins" list in future
        return nullptr;
    }
    catch (...)
    {
        DBG("PluginManager: Unknown exception creating plugin instance: " + description.name);
        return nullptr;
    }
}

std::unique_ptr<juce::AudioPluginInstance> PluginManager::createPluginInstance(
    const juce::String& identifier,
    double sampleRate,
    int blockSize)
{
    auto desc = getPluginByIdentifier(identifier);
    if (!desc.has_value())
    {
        DBG("PluginManager: Plugin not found: " + identifier);
        return nullptr;
    }

    return createPluginInstance(*desc, sampleRate, blockSize);
}

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

void PluginManager::clearCache()
{
    // Cancel any in-progress scan first to avoid race conditions
    // Always call cancelScan() to avoid TOCTOU race - it's safe to call even when no scan is running
    cancelScan();

    const juce::ScopedLock sl(m_lock);

    auto cacheFile = getPluginCacheFile();
    if (cacheFile.existsAsFile())
    {
        if (cacheFile.deleteFile())
            DBG("PluginManager: Deleted cache file: " + cacheFile.getFullPathName());
        else
            DBG("PluginManager: Warning - failed to delete cache file");
    }

    // Also clean up dead-mans-pedal file if it exists
    if (m_deadMansPedalFile.existsAsFile())
    {
        if (m_deadMansPedalFile.deleteFile())
            DBG("PluginManager: Deleted dead-mans-pedal file");
        else
            DBG("PluginManager: Warning - failed to delete dead-mans-pedal file");
    }

    m_knownPluginList.clear();
    m_lastScanDate = juce::Time();  // Reset scan date
    DBG("PluginManager: Cleared plugin cache and reset scan state");
}

//==============================================================================
// Scan Paths
//==============================================================================
juce::FileSearchPath PluginManager::getVST3SearchPaths() const
{
    juce::FileSearchPath paths;

    #if JUCE_MAC
    // macOS VST3 paths
    paths.add(juce::File("/Library/Audio/Plug-Ins/VST3"));
    paths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                  .getChildFile("Library/Audio/Plug-Ins/VST3"));
    #elif JUCE_WINDOWS
    // Windows VST3 paths
    paths.add(juce::File("C:/Program Files/Common Files/VST3"));
    paths.add(juce::File("C:/Program Files (x86)/Common Files/VST3"));
    #elif JUCE_LINUX
    // Linux VST3 paths
    paths.add(juce::File("/usr/lib/vst3"));
    paths.add(juce::File("/usr/local/lib/vst3"));
    paths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                  .getChildFile(".vst3"));
    #endif

    return paths;
}

juce::FileSearchPath PluginManager::getAUSearchPaths() const
{
    juce::FileSearchPath paths;

    #if JUCE_MAC
    // macOS AudioUnit paths
    paths.add(juce::File("/Library/Audio/Plug-Ins/Components"));
    paths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                  .getChildFile("Library/Audio/Plug-Ins/Components"));
    #endif

    return paths;
}

juce::FileSearchPath PluginManager::getDefaultSearchPaths() const
{
    // VST3 only - AudioUnit support removed
    return getVST3SearchPaths();
}

void PluginManager::addCustomSearchPath(const juce::File& path)
{
    const juce::ScopedLock sl(m_lock);

    if (path.isDirectory())
    {
        m_customSearchPaths.addIfNotAlreadyThere(path.getFullPathName());
    }
}

void PluginManager::removeCustomSearchPath(const juce::File& path)
{
    const juce::ScopedLock sl(m_lock);

    m_customSearchPaths.removeString(path.getFullPathName());
}

juce::StringArray PluginManager::getCustomSearchPaths() const
{
    const juce::ScopedLock sl(m_lock);
    return m_customSearchPaths;
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

// NOTE: initializeDefaultBlacklist() has been removed since we only scan VST3 now
// (AudioUnit format is not registered, so Apple system AU blacklisting is unnecessary)

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
// Extended Scanner Thread (with incremental scanning - NO blocking dialogs)
//==============================================================================
class PluginManager::ExtendedScannerThread : public juce::Thread
{
public:
    ExtendedScannerThread(PluginManager& owner,
                          const ScanOptions& options,
                          ScanProgressCallback progressCallback,
                          ExtendedScanCompleteCallback completeCallback)
        : juce::Thread("Extended Plugin Scanner"),
          m_owner(owner),
          m_options(options),
          m_progressCallback(std::move(progressCallback)),
          m_completeCallback(std::move(completeCallback))
    {
    }

    void run() override
    {
        try
        {
            // Initialize scan state (do this under lock for thread safety)
            {
                const juce::ScopedLock sl(m_owner.m_lock);
                m_owner.m_scanState = std::make_unique<PluginScanState>();
            }

            auto& scanState = *m_owner.m_scanState;
            scanState.reset();

            // Discover plugin files (doesn't need lock - only writes to scanState)
            discoverPluginFiles(scanState);

            if (threadShouldExit())
            {
                finishScan(scanState, false);
                return;
            }

            // Scan each plugin
            while (scanState.hasMore() && !threadShouldExit())
            {
                // Check cancellation (atomic, no lock needed)
                if (scanState.isCancelled())
                    break;

                auto* currentPlugin = scanState.getCurrentPlugin();
                if (currentPlugin == nullptr)
                    break;

                // Check if blacklisted (needs lock for blacklist access)
                bool isBlacklisted = false;
                {
                    const juce::ScopedLock sl(m_owner.m_lock);
                    isBlacklisted = m_owner.isBlacklisted(currentPlugin->pluginPath);
                }

                if (isBlacklisted)
                {
                    scanState.markAsBlacklisted(currentPlugin->pluginPath);
                    scanState.moveToNext();
                    continue;
                }

                // Check if we can use cached result (incremental scan)
                if (m_options.useIncrementalScan && !m_options.forceRescan)
                {
                    bool usedCache = false;
                    {
                        const juce::ScopedLock sl(m_owner.m_lock);
                        auto it = m_owner.m_incrementalCache.find(currentPlugin->pluginPath);
                        if (it != m_owner.m_incrementalCache.end())
                        {
                            juce::File pluginFile(currentPlugin->pluginPath);
                            if (!it->second.hasFileChanged(pluginFile))
                            {
                                // Plugin unchanged - use cached result
                                scanState.markAsCached(currentPlugin->pluginPath, it->second.descriptions);

                                // Add to known plugin list
                                for (const auto& desc : it->second.descriptions)
                                {
                                    if (m_owner.m_knownPluginList.getTypeForFile(desc.fileOrIdentifier) == nullptr)
                                    {
                                        m_owner.m_knownPluginList.addType(desc);
                                    }
                                }
                                usedCache = true;
                            }
                        }
                    } // Release lock before continuing

                    if (usedCache)
                    {
                        scanState.moveToNext();
                        reportProgress(scanState);
                        continue;
                    }
                }

                // Report progress before scanning (async, no blocking)
                reportProgress(scanState);

                // Actually scan this plugin (lock released during plugin load)
                scanPlugin(currentPlugin->pluginPath, scanState);

                // On failure, just log and continue - NO interactive dialogs
                // All failures will be shown in the summary dialog at the end

                scanState.moveToNext();
            }

            finishScan(scanState, !scanState.isCancelled());
        }
        catch (const std::exception& e)
        {
            DBG("ExtendedScannerThread: Exception: " + juce::String(e.what()));
            if (m_owner.m_scanState)
                finishScan(*m_owner.m_scanState, false);
        }
        catch (...)
        {
            DBG("ExtendedScannerThread: Unknown exception");
            if (m_owner.m_scanState)
                finishScan(*m_owner.m_scanState, false);
        }
    }

private:
    void discoverPluginFiles(PluginScanState& scanState)
    {
        // Get all search paths (need lock for custom paths)
        juce::FileSearchPath searchPaths;
        juce::StringArray customPaths;

        {
            const juce::ScopedLock sl(m_owner.m_lock);
            searchPaths = m_owner.getVST3SearchPaths();
            customPaths = m_owner.m_customSearchPaths;
        }

        // Add custom paths
        for (const auto& path : customPaths)
        {
            juce::File customDir(path);
            if (customDir.isDirectory())
                searchPaths.add(customDir);
        }

        // Find all VST3 files (no lock needed - file system access)
        for (int i = 0; i < searchPaths.getNumPaths(); ++i)
        {
            if (threadShouldExit())
                return;

            juce::File dir = searchPaths[i];
            if (!dir.isDirectory())
                continue;

            // Find VST3 bundles (directories ending in .vst3)
            juce::Array<juce::File> vst3Files;
            dir.findChildFiles(vst3Files, juce::File::findDirectories, true, "*.vst3");

            for (const auto& file : vst3Files)
            {
                scanState.addPluginToQueue(file.getFullPathName(),
                                           file.getLastModificationTime(),
                                           file.getSize());
            }
        }

        DBG("ExtendedScannerThread: Found " + juce::String(scanState.getTotalCount()) + " plugin files");
    }

    bool scanPlugin(const juce::String& pluginPath, PluginScanState& scanState)
    {
        // Timeout for plugin scanning via out-of-process worker
        // 90 seconds is reasonable - complex plugins (AI/ML, license validation)
        // can legitimately take this long on first load, and out-of-process has overhead
        static constexpr int kScanTimeoutMs = 90000;  // 90 seconds

        try
        {
            // Write to dead-mans-pedal before scanning (no lock needed - atomic file write)
            m_owner.m_deadMansPedalFile.replaceWithText(pluginPath, false, false, "\n");

            // Find the format name for this plugin
            juce::String formatName;
            {
                const juce::ScopedLock sl(m_owner.m_lock);
                for (int i = 0; i < m_owner.m_formatManager.getNumFormats(); ++i)
                {
                    auto* fmt = m_owner.m_formatManager.getFormat(i);
                    if (fmt != nullptr && fmt->fileMightContainThisPluginType(pluginPath))
                    {
                        formatName = fmt->getName();
                        break;
                    }
                }
            }

            if (formatName.isEmpty())
            {
                scanState.recordFailure(pluginPath, "Unknown plugin format");
                return false;
            }

            // OUT-OF-PROCESS SCANNING
            // This is the key fix: we scan plugins in a separate subprocess so that
            // crashes (from PACE/iLok, heap corruption, etc.) only kill the worker,
            // not WaveEdit itself. The coordinator handles crash recovery automatically.

            DBG("ExtendedScannerThread: Scanning (out-of-process) " + pluginPath);
            std::cerr << "[SCAN] Scanning (out-of-process): " << pluginPath.toStdString() << std::endl;

            // Create a coordinator for this single-plugin scan
            // Using a per-scan coordinator ensures clean state and proper cleanup
            std::cerr << "[SCAN] Creating coordinator..." << std::endl;
            auto coordinator = std::make_shared<PluginScannerCoordinator>();
            std::cerr << "[SCAN] Coordinator created" << std::endl;
            auto scanComplete = std::make_shared<juce::WaitableEvent>();
            auto scanSuccess = std::make_shared<std::atomic<bool>>(false);
            auto workerCrashed = std::make_shared<std::atomic<bool>>(false);

            // Thread-safe results container - completion callback writes, scanner thread reads
            // Access is synchronized via scanComplete event (write before signal, read after wait)
            auto scanResults = std::make_shared<juce::Array<juce::PluginDescription>>();

            // Start the out-of-process scan
            juce::StringArray pluginFiles;
            pluginFiles.add(pluginPath);

            // Must be called from message thread since coordinator uses IPC
            std::cerr << "[SCAN] About to callAsync for scanPluginsAsync..." << std::endl;
            juce::MessageManager::callAsync([coordinator, pluginFiles, formatName,
                                              scanComplete, scanSuccess, scanResults,
                                              workerCrashed]()
            {
                std::cerr << "[SCAN] Inside callAsync - calling scanPluginsAsync..." << std::endl;
                coordinator->scanPluginsAsync(
                    pluginFiles,
                    formatName,
                    nullptr,  // No progress callback for single file
                    [scanComplete, scanSuccess, scanResults, workerCrashed]
                    (bool success, const juce::Array<juce::PluginDescription>& plugins)
                    {
                        // Completion callback - always called, even after crashes
                        // Write results BEFORE setting success flag and signaling
                        *scanResults = plugins;

                        // Only mark success if we got results AND no crash occurred
                        bool realSuccess = success && !plugins.isEmpty() &&
                                          !workerCrashed->load(std::memory_order_acquire);
                        scanSuccess->store(realSuccess, std::memory_order_release);
                        scanComplete->signal();
                    },
                    [workerCrashed]
                    (const juce::String& crashed)
                    {
                        // Crash callback - worker process died while scanning this plugin
                        // The completion callback will be called after this by the coordinator
                        DBG("ExtendedScannerThread: Worker crashed scanning: " + crashed);
                        workerCrashed->store(true, std::memory_order_release);
                        // Note: completion callback will signal scanComplete
                    }
                );
            });

            // Wait for completion with timeout
            std::cerr << "[SCAN] Waiting for scan completion (timeout " << kScanTimeoutMs << "ms)..." << std::endl;
            bool completed = scanComplete->wait(kScanTimeoutMs);
            std::cerr << "[SCAN] Wait finished - completed=" << (completed ? "true" : "false") << std::endl;

            if (!completed)
            {
                // Plugin is taking too long - show dialog to ask user what to do
                DBG("ExtendedScannerThread: TIMEOUT scanning " + pluginPath + " (>" +
                    juce::String(kScanTimeoutMs) + "ms) - showing dialog");

                // Cancel the in-progress scan
                juce::MessageManager::callAsync([coordinator]() {
                    coordinator->cancelScan();
                });

                // Show timeout dialog on message thread and wait for response
                auto dialogResult = std::make_shared<std::atomic<int>>(-1);
                auto dialogComplete = std::make_shared<juce::WaitableEvent>();

                const int timeoutSec = kScanTimeoutMs / 1000;
                juce::MessageManager::callAsync([dialogResult, dialogComplete, pluginPath, timeoutSec_ = timeoutSec]()
                {
                    auto result = PluginTimeoutDialog::showDialog(pluginPath, timeoutSec_);
                    dialogResult->store(static_cast<int>(result), std::memory_order_release);
                    dialogComplete->signal();
                });

                // Wait for user to respond to dialog
                dialogComplete->wait(-1);

                auto result = static_cast<PluginTimeoutDialog::Result>(
                    dialogResult->load(std::memory_order_acquire));

                if (result == PluginTimeoutDialog::Result::WaitLonger)
                {
                    // User wants to wait longer - restart scan and wait again
                    DBG("ExtendedScannerThread: User chose to wait longer for " + pluginPath);

                    // Restart the out-of-process scan
                    auto scanComplete2 = std::make_shared<juce::WaitableEvent>();
                    auto scanSuccess2 = std::make_shared<std::atomic<bool>>(false);
                    auto scanResults2 = std::make_shared<juce::Array<juce::PluginDescription>>();
                    auto coordinator2 = std::make_shared<PluginScannerCoordinator>();

                    juce::MessageManager::callAsync([coordinator2, pluginFiles, formatName,
                                                      scanComplete2, scanSuccess2, scanResults2]()
                    {
                        coordinator2->scanPluginsAsync(
                            pluginFiles, formatName, nullptr,
                            [scanComplete2, scanSuccess2, scanResults2]
                            (bool success, const juce::Array<juce::PluginDescription>& plugins) {
                                *scanResults2 = plugins;
                                scanSuccess2->store(success && !plugins.isEmpty());
                                scanComplete2->signal();
                            },
                            nullptr  // Crash callback not needed for retry
                        );
                    });

                    completed = scanComplete2->wait(kScanTimeoutMs);

                    if (!completed)
                    {
                        scanState.recordFailure(pluginPath, "Plugin scan timed out after extended wait");
                        juce::MessageManager::callAsync([coordinator2]() { coordinator2->cancelScan(); });
                        return false;
                    }

                    // Use results from retry
                    if (scanSuccess2->load(std::memory_order_acquire) && !scanResults2->isEmpty())
                    {
                        // Success on retry - fall through to success handling
                        *scanResults = *scanResults2;
                        scanSuccess->store(true, std::memory_order_release);
                    }
                    else
                    {
                        scanState.recordFailure(pluginPath, "No valid plugins found after extended wait");
                        return false;
                    }
                }
                else if (result == PluginTimeoutDialog::Result::Blacklist)
                {
                    // User explicitly chose to blacklist this plugin
                    DBG("ExtendedScannerThread: User chose to blacklist " + pluginPath);
                    scanState.recordFailure(pluginPath, "Plugin scan timed out (user chose to blacklist)");

                    {
                        const juce::ScopedLock sl(m_owner.m_lock);
                        m_owner.m_blacklist.addIfNotAlreadyThere(pluginPath);
                        m_owner.m_newlyBlacklistedPlugins.addIfNotAlreadyThere(pluginPath);
                    }
                    m_owner.saveBlacklist();
                    return false;
                }
                else // Result::Skip
                {
                    // User chose to skip
                    DBG("ExtendedScannerThread: User chose to skip " + pluginPath);
                    scanState.recordFailure(pluginPath, "Plugin scan timed out (skipped by user)");
                    return false;
                }
            }

            // Check if worker crashed (indicated by workerCrashed flag)
            // The completion callback already checked this and set scanSuccess=false
            if (workerCrashed->load(std::memory_order_acquire))
            {
                // Worker crashed while scanning this plugin
                // With out-of-process scanning, WaveEdit survives but we record the failure
                DBG("ExtendedScannerThread: Worker crashed scanning " + pluginPath +
                    " - recording failure (WaveEdit continues)");
                scanState.recordFailure(pluginPath, "Plugin crashed during scan (isolated in worker process)");

                // Auto-blacklist crashed plugins so they don't crash again on next scan
                {
                    const juce::ScopedLock sl(m_owner.m_lock);
                    m_owner.m_blacklist.addIfNotAlreadyThere(pluginPath);
                    m_owner.m_newlyBlacklistedPlugins.addIfNotAlreadyThere(pluginPath);
                }
                m_owner.saveBlacklist();

                return false;
            }

            // Scan completed - check results (reads synchronized via scanComplete event)
            if (!scanSuccess->load(std::memory_order_acquire) || scanResults->isEmpty())
            {
                scanState.recordFailure(pluginPath, "No valid plugins found in file");
                return false;
            }

            // Success - add to known plugin list
            juce::Array<juce::PluginDescription> descriptions = *scanResults;

            {
                const juce::ScopedLock sl(m_owner.m_lock);
                for (const auto& desc : descriptions)
                {
                    if (m_owner.m_knownPluginList.getTypeForFile(desc.fileOrIdentifier) == nullptr)
                    {
                        m_owner.m_knownPluginList.addType(desc);
                    }
                }
            }

            // Update incremental cache
            {
                const juce::ScopedLock sl(m_owner.m_lock);

                juce::File pluginFile(pluginPath);
                PluginCacheEntry cacheEntry;
                cacheEntry.pluginPath = pluginPath;
                cacheEntry.lastModified = pluginFile.getLastModificationTime();
                cacheEntry.fileSize = pluginFile.getSize();
                cacheEntry.lastScanned = juce::Time::getCurrentTime();
                cacheEntry.descriptions = descriptions;
                m_owner.m_incrementalCache[pluginPath] = cacheEntry;
            }

            scanState.recordSuccess(pluginPath, descriptions);

            // Clear dead-mans-pedal on success
            m_owner.m_deadMansPedalFile.deleteFile();

            DBG("ExtendedScannerThread: Successfully scanned " + pluginPath +
                " (" + juce::String(descriptions.size()) + " plugins)");

            return true;
        }
        catch (const std::exception& e)
        {
            scanState.recordFailure(pluginPath, e.what());
            DBG("ExtendedScannerThread: Exception scanning " + pluginPath + ": " + e.what());
            return false;
        }
        catch (...)
        {
            scanState.recordFailure(pluginPath, "Unknown error during scan");
            DBG("ExtendedScannerThread: Unknown exception scanning " + pluginPath);
            return false;
        }
    }

    void reportProgress(PluginScanState& scanState)
    {
        if (m_progressCallback)
        {
            // Read progress data (scanState is only written by this thread)
            float progress = scanState.getProgress();
            juce::String currentPlugin;
            int currentIndex = scanState.getCurrentIndex();
            int totalCount = scanState.getTotalCount();

            auto* current = scanState.getCurrentPlugin();
            if (current)
                currentPlugin = current->pluginName;

            // Store values for thread-safe UI update via atomic members
            m_lastProgress.store(progress);
            m_lastCurrentIndex.store(currentIndex);
            m_lastTotalCount.store(totalCount);
            {
                const juce::ScopedLock sl(m_progressLock);
                m_lastPluginName = currentPlugin;
            }

            // Fire async callback - UI will read the atomic values
            juce::MessageManager::callAsync([callback = m_progressCallback,
                                              progress, currentPlugin]()
            {
                callback(progress, currentPlugin);
            });
        }
    }

    void finishScan(PluginScanState& scanState, bool success)
    {
        // Save caches (needs lock)
        m_owner.saveCache();
        m_owner.saveIncrementalCache();

        m_owner.m_scanInProgress.store(false);

        // Create a copyable summary for callbacks
        PluginScanSummary summary = scanState.createSummary();

        // Show summary dialog if requested (always show if there were failures)
        bool hasFailures = summary.failedCount > 0;
        if (m_options.showSummaryDialog || hasFailures)
        {
            juce::MessageManager::callAsync([summary]()
            {
                PluginScanSummaryDialog::showDialog(summary);
            });
        }

        // Call completion callback
        if (m_completeCallback)
        {
            juce::MessageManager::callAsync([callback = m_completeCallback, summary]()
            {
                callback(summary);
            });
        }
    }

    PluginManager& m_owner;
    ScanOptions m_options;
    ScanProgressCallback m_progressCallback;
    ExtendedScanCompleteCallback m_completeCallback;

    // Thread-safe progress data for UI updates
    std::atomic<float> m_lastProgress{0.0f};
    std::atomic<int> m_lastCurrentIndex{0};
    std::atomic<int> m_lastTotalCount{0};
    juce::CriticalSection m_progressLock;
    juce::String m_lastPluginName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExtendedScannerThread)
};

//==============================================================================
// PluginManager Destructor (defined after ExtendedScannerThread for complete type)
//==============================================================================

PluginManager::~PluginManager()
{
    // Cancel any in-progress scan
    cancelScan();

    // Stop extended scanner thread if running
    if (m_extendedScannerThread != nullptr)
    {
        m_extendedScannerThread->stopThread(2000);
        m_extendedScannerThread.reset();
    }

    // Save caches on shutdown
    saveCache();
    saveIncrementalCache();
    saveCustomSearchPaths();
}

//==============================================================================
// Extended Scanning Methods
//==============================================================================

void PluginManager::startScanWithOptions(const ScanOptions& options,
                                          ScanProgressCallback progressCallback,
                                          ExtendedScanCompleteCallback completeCallback)
{
    if (m_scanInProgress.load())
    {
        DBG("PluginManager: Scan already in progress");
        return;
    }

    m_scanInProgress.store(true);

    // Clear existing if force rescan
    if (options.forceRescan)
    {
        const juce::ScopedLock sl(m_lock);
        m_knownPluginList.clear();
        m_incrementalCache.clear();
    }

    // Stop any existing scanner thread
    if (m_extendedScannerThread != nullptr)
    {
        m_extendedScannerThread->stopThread(2000);
        m_extendedScannerThread.reset();
    }

    // Start new scanner thread
    m_extendedScannerThread = std::make_unique<ExtendedScannerThread>(
        *this,
        options,
        std::move(progressCallback),
        std::move(completeCallback)
    );

    m_extendedScannerThread->startThread();
}

void PluginManager::cancelScan()
{
    // Cancel extended scanner if running
    if (m_extendedScannerThread != nullptr)
    {
        m_extendedScannerThread->signalThreadShouldExit();
        if (m_scanState)
            m_scanState->cancel();
        m_extendedScannerThread->stopThread(2000);
        m_extendedScannerThread.reset();
    }

    // Also cancel old scanner if running (for backwards compatibility)
    if (m_scannerThread != nullptr)
    {
        m_scannerThread->signalThreadShouldExit();
        m_scannerThread->stopThread(2000);
        m_scannerThread.reset();
    }

    m_scanInProgress.store(false);
}

bool PluginManager::isFirstScan() const
{
    auto cacheFile = getPluginCacheFile();
    return !cacheFile.existsAsFile();
}

const PluginScanState* PluginManager::getCurrentScanState() const
{
    return m_scanState.get();
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
