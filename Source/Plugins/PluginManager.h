/*
  ==============================================================================

    PluginManager.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <map>
#include <memory>

#include "PluginScanState.h"

// Forward declarations
class OutOfProcessPluginScanner;
class PluginScanProgressDialog;

/**
 * Singleton manager for VST3/AU plugin discovery, caching, and instantiation.
 *
 * Thread Safety:
 * - Plugin scanning runs on background thread
 * - Plugin instantiation must be called from message thread
 * - KnownPluginList access is thread-safe via internal locking
 *
 * Usage:
 * @code
 * // Get singleton instance
 * auto& pm = PluginManager::getInstance();
 *
 * // Start async scan (call from message thread)
 * pm.startScanAsync([](float progress, const String& name) {
 *     // Update UI with progress
 * });
 *
 * // Get available plugins
 * auto plugins = pm.getAvailablePlugins();
 *
 * // Create plugin instance
 * auto instance = pm.createPluginInstance(description, sampleRate, blockSize);
 * @endcode
 */
class PluginManager : public juce::DeletedAtShutdown
{
public:
    //==============================================================================
    /** Progress callback for plugin scanning */
    using ScanProgressCallback = std::function<void(float progress, const juce::String& currentPlugin)>;

    /** Completion callback for plugin scanning */
    using ScanCompleteCallback = std::function<void(bool success, int numPluginsFound)>;

    /**
     * Extended scan options for more control over the scanning process.
     *
     * Note: Interactive error dialogs have been removed for reliability.
     * All failures are logged and shown in the summary dialog at the end.
     */
    struct ScanOptions
    {
        bool forceRescan = false;           // If true, ignore cache and rescan everything
        bool showProgressDialog = false;     // If true, show a modal progress dialog
        bool showSummaryDialog = false;      // If true, show summary at end (auto-shown on failures)
        bool useIncrementalScan = true;     // If true, only scan new/changed plugins
    };

    /**
     * Extended completion callback with scan summary for detailed results.
     * Uses PluginScanSummary which is a copyable snapshot of the scan state.
     */
    using ExtendedScanCompleteCallback = std::function<void(const PluginScanSummary& summary)>;

    //==============================================================================
    /** Get the singleton instance */
    static PluginManager& getInstance();

    /** Destructor - saves plugin cache */
    ~PluginManager() override;

    //==============================================================================
    // Plugin Scanning
    //==============================================================================

    /**
     * Start asynchronous plugin scanning on a background thread.
     * Safe to call from any thread.
     *
     * @param progressCallback Called periodically with scan progress (on message thread)
     * @param completeCallback Called when scan completes (on message thread)
     */
    void startScanAsync(ScanProgressCallback progressCallback = nullptr,
                        ScanCompleteCallback completeCallback = nullptr);

    /** Cancel any in-progress scan */
    void cancelScan();

    /** Check if a scan is currently in progress */
    bool isScanInProgress() const { return m_scanInProgress.load(); }

    /** Force a full rescan, ignoring cache */
    void forceRescan(ScanProgressCallback progressCallback = nullptr,
                     ScanCompleteCallback completeCallback = nullptr);

    /**
     * Start a scan with extended options.
     *
     * This is the preferred method for UI-driven scans as it supports:
     * - Incremental scanning (only new/changed plugins)
     * - Progress dialog with cancel support
     * - Per-plugin error dialogs (Retry/Skip/Cancel)
     * - Summary dialog at completion
     *
     * @param options Control how the scan behaves
     * @param progressCallback Called with progress updates (on message thread)
     * @param completeCallback Called when scan completes with full scan state
     */
    void startScanWithOptions(const ScanOptions& options,
                              ScanProgressCallback progressCallback = nullptr,
                              ExtendedScanCompleteCallback completeCallback = nullptr);

    /**
     * Check if this is the first scan (no cache exists).
     * Use this to decide whether to show a "first-time scan" dialog.
     */
    bool isFirstScan() const;

    /**
     * Get the current scan state (read-only).
     * Returns nullptr if no scan is in progress.
     */
    const PluginScanState* getCurrentScanState() const;

    //==============================================================================
    // Plugin Discovery
    //==============================================================================

    /** Get list of all discovered plugins (thread-safe) */
    juce::Array<juce::PluginDescription> getAvailablePlugins() const;

    /** Get plugins filtered by category */
    juce::Array<juce::PluginDescription> getPluginsByCategory(const juce::String& category) const;

    /** Get plugins filtered by manufacturer */
    juce::Array<juce::PluginDescription> getPluginsByManufacturer(const juce::String& manufacturer) const;

    /** Get all unique plugin categories */
    juce::StringArray getCategories() const;

    /** Get all unique plugin manufacturers */
    juce::StringArray getManufacturers() const;

    /** Search plugins by name (case-insensitive) */
    juce::Array<juce::PluginDescription> searchPlugins(const juce::String& searchTerm) const;

    /** Get plugin description by unique identifier */
    std::optional<juce::PluginDescription> getPluginByIdentifier(const juce::String& identifier) const;

    //==============================================================================
    // Plugin Instantiation
    //==============================================================================

    /**
     * Create a plugin instance from a description.
     * Must be called from message thread.
     *
     * @param description Plugin to instantiate
     * @param sampleRate Audio sample rate
     * @param blockSize Maximum block size
     * @return Unique pointer to plugin instance, or nullptr on failure
     */
    std::unique_ptr<juce::AudioPluginInstance> createPluginInstance(
        const juce::PluginDescription& description,
        double sampleRate,
        int blockSize);

    /**
     * Create a plugin instance by unique identifier.
     * Must be called from message thread.
     */
    std::unique_ptr<juce::AudioPluginInstance> createPluginInstance(
        const juce::String& identifier,
        double sampleRate,
        int blockSize);

    //==============================================================================
    // Cache Management
    //==============================================================================

    /** Get path to plugin cache file */
    juce::File getPluginCacheFile() const;

    /** Save plugin list to cache */
    void saveCache();

    /** Load plugin list from cache */
    bool loadCache();

    /** Clear the plugin cache file */
    void clearCache();

    /** Get the last scan date */
    juce::Time getLastScanDate() const { return m_lastScanDate; }

    //==============================================================================
    // Blacklist Management
    //==============================================================================

    /** Add a plugin to the blacklist (will be skipped during scanning) */
    void addToBlacklist(const juce::String& fileOrIdentifier);

    /** Remove a plugin from the blacklist */
    void removeFromBlacklist(const juce::String& fileOrIdentifier);

    /** Check if a plugin is blacklisted */
    bool isBlacklisted(const juce::String& fileOrIdentifier) const;

    /** Get all blacklisted plugins */
    juce::StringArray getBlacklist() const;

    /** Clear the blacklist */
    void clearBlacklist();

    /**
     * Get plugins that were auto-blacklisted due to crashes.
     * Called at startup to notify user about problematic plugins.
     * This clears the list after returning it.
     */
    juce::StringArray getAndClearNewlyBlacklistedPlugins();

    /**
     * Check if there are any plugins that crashed during last session.
     * Call this at startup to determine if user notification is needed.
     */
    bool hasNewlyBlacklistedPlugins() const;

    //==============================================================================
    // Scan Paths
    //==============================================================================

    /** Get VST3 plugin search paths */
    juce::FileSearchPath getVST3SearchPaths() const;

    /** Get AudioUnit plugin search paths (macOS only) */
    juce::FileSearchPath getAUSearchPaths() const;

    /** Add a custom plugin search path */
    void addCustomSearchPath(const juce::File& path);

    /** Remove a custom plugin search path */
    void removeCustomSearchPath(const juce::File& path);

    /** Get custom plugin search paths */
    juce::StringArray getCustomSearchPaths() const;

    /** Set custom plugin search paths (replaces existing) */
    void setCustomSearchPaths(const juce::StringArray& paths);

    /** Save custom search paths to disk */
    void saveCustomSearchPaths();

    /** Load custom search paths from disk */
    void loadCustomSearchPaths();

    //==============================================================================
    // Format Manager Access
    //==============================================================================

    /** Get the audio plugin format manager */
    juce::AudioPluginFormatManager& getFormatManager() { return m_formatManager; }

    /** Get the known plugin list (for JUCE PluginListComponent) */
    juce::KnownPluginList& getKnownPluginList() { return m_knownPluginList; }

private:
    //==============================================================================
    PluginManager();

    void initializeFormats();
    juce::FileSearchPath getDefaultSearchPaths() const;

    //==============================================================================
    // Background Scanner Thread
    class ScannerThread;
    std::unique_ptr<ScannerThread> m_scannerThread;

    // Extended scanner thread with error dialogs
    class ExtendedScannerThread;
    std::unique_ptr<ExtendedScannerThread> m_extendedScannerThread;

    //==============================================================================
    juce::AudioPluginFormatManager m_formatManager;
    juce::KnownPluginList m_knownPluginList;

    std::atomic<bool> m_scanInProgress{false};
    juce::Time m_lastScanDate;

    juce::StringArray m_customSearchPaths;
    juce::StringArray m_blacklist;

    // Plugins that were auto-blacklisted during this session or detected at startup
    // This is used to notify the user about problematic plugins
    juce::StringArray m_newlyBlacklistedPlugins;

    // Dead-mans-pedal file for crash recovery during scanning
    juce::File m_deadMansPedalFile;

    // Current scan state (for extended scans with dialogs)
    std::unique_ptr<PluginScanState> m_scanState;

    // Incremental scan cache (maps plugin paths to metadata)
    std::map<juce::String, PluginCacheEntry> m_incrementalCache;

    void loadBlacklist();
    void saveBlacklist();
    juce::File getBlacklistFile() const;

    // Incremental cache management
    void loadIncrementalCache();
    void saveIncrementalCache();
    juce::File getIncrementalCacheFile() const;

    // Custom paths persistence
    juce::File getCustomPathsFile() const;

    // Default blacklist for known problematic plugins
    void initializeDefaultBlacklist();

    mutable juce::CriticalSection m_lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginManager)
};
