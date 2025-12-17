/*
  ==============================================================================

    PluginScanState.h
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
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

/**
 * Represents the result of scanning a single plugin.
 */
struct PluginScanResult
{
    enum class Status
    {
        Pending,        // Not yet scanned
        Success,        // Scanned successfully
        Failed,         // Scan failed (plugin reported error)
        Crashed,        // Plugin crashed the scanner
        Timeout,        // Scan timed out
        Skipped,        // User chose to skip
        Blacklisted,    // Already blacklisted
        Cached          // Loaded from cache (not re-scanned)
    };

    juce::String pluginPath;                          // Full path to plugin file
    juce::String pluginName;                          // Short display name
    Status status = Status::Pending;
    juce::String errorMessage;                        // Error description if failed
    juce::Array<juce::PluginDescription> descriptions; // Found plugins (if any)
    juce::Time lastModified;                          // File modification time
    juce::int64 fileSize = 0;                         // File size for change detection

    bool isSuccess() const { return status == Status::Success || status == Status::Cached; }
    bool isFailed() const { return status == Status::Failed || status == Status::Crashed || status == Status::Timeout; }
};

/**
 * Snapshot of scan state suitable for passing to callbacks.
 * This is a copyable struct containing only the results, not the control state.
 */
struct PluginScanSummary
{
    juce::Array<PluginScanResult> results;
    juce::Time scanStartTime;
    int successCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    int cachedCount = 0;

    int getTotalPluginsFound() const
    {
        int count = 0;
        for (const auto& result : results)
            count += result.descriptions.size();
        return count;
    }

    juce::Array<PluginScanResult> getFailedResults() const
    {
        juce::Array<PluginScanResult> failed;
        for (const auto& result : results)
        {
            if (result.isFailed())
                failed.add(result);
        }
        return failed;
    }

    juce::RelativeTime getScanDuration() const
    {
        return juce::Time::getCurrentTime() - scanStartTime;
    }
};

/**
 * Tracks the state of a plugin scan session.
 *
 * This class is used to:
 * - Track progress of the current scan
 * - Store results for each plugin (success/failure/reason)
 * - Support incremental scanning (detect new/changed plugins)
 * - Generate summary data for the end-of-scan dialog
 *
 * Thread Safety:
 * - m_isCancelled and m_isPaused are std::atomic<bool> for safe cross-thread access
 * - Other members require external synchronization
 *
 * Note: This class is NOT copyable due to atomic members.
 * Use createSummary() to get a copyable snapshot for callbacks.
 */
class PluginScanState
{
public:
    //==============================================================================
    PluginScanState() = default;

    //==============================================================================
    // Scan Session Management
    //==============================================================================

    /** Reset state for a new scan session */
    void reset()
    {
        m_results.clear();
        m_currentIndex = 0;
        m_isCancelled.store(false);
        m_isPaused.store(false);
        m_scanStartTime = juce::Time::getCurrentTime();
    }

    /** Add a plugin to the scan queue */
    void addPluginToQueue(const juce::String& pluginPath,
                          const juce::Time& lastModified = juce::Time(),
                          juce::int64 fileSize = 0)
    {
        PluginScanResult result;
        result.pluginPath = pluginPath;
        result.pluginName = juce::File(pluginPath).getFileNameWithoutExtension();
        result.lastModified = lastModified;
        result.fileSize = fileSize;
        m_results.add(result);
    }

    /** Mark a plugin as already cached (won't be re-scanned) */
    void markAsCached(const juce::String& pluginPath,
                      const juce::Array<juce::PluginDescription>& descriptions)
    {
        for (auto& result : m_results)
        {
            if (result.pluginPath == pluginPath)
            {
                result.status = PluginScanResult::Status::Cached;
                result.descriptions = descriptions;
                return;
            }
        }
    }

    /** Mark a plugin as blacklisted (won't be scanned) */
    void markAsBlacklisted(const juce::String& pluginPath)
    {
        for (auto& result : m_results)
        {
            if (result.pluginPath == pluginPath)
            {
                result.status = PluginScanResult::Status::Blacklisted;
                return;
            }
        }
    }

    //==============================================================================
    // Progress Tracking
    //==============================================================================

    /** Get total number of plugins to scan */
    int getTotalCount() const { return m_results.size(); }

    /** Get current plugin index being scanned */
    int getCurrentIndex() const { return m_currentIndex; }

    /** Get overall progress (0.0 to 1.0) */
    float getProgress() const
    {
        if (m_results.isEmpty())
            return 1.0f;
        return static_cast<float>(m_currentIndex) / static_cast<float>(m_results.size());
    }

    /** Get the current plugin being scanned */
    PluginScanResult* getCurrentPlugin()
    {
        if (m_currentIndex >= 0 && m_currentIndex < m_results.size())
            return &m_results.getReference(m_currentIndex);
        return nullptr;
    }

    /** Advance to the next plugin */
    void moveToNext()
    {
        m_currentIndex++;
    }

    /** Check if there are more plugins to scan */
    bool hasMore() const
    {
        return m_currentIndex < m_results.size();
    }

    //==============================================================================
    // Result Recording
    //==============================================================================

    /** Record a successful scan result */
    void recordSuccess(const juce::String& pluginPath,
                       const juce::Array<juce::PluginDescription>& descriptions)
    {
        for (auto& result : m_results)
        {
            if (result.pluginPath == pluginPath)
            {
                result.status = PluginScanResult::Status::Success;
                result.descriptions = descriptions;
                result.errorMessage.clear();
                return;
            }
        }
    }

    /** Record a failed scan */
    void recordFailure(const juce::String& pluginPath,
                       const juce::String& errorMessage,
                       PluginScanResult::Status failureType = PluginScanResult::Status::Failed)
    {
        for (auto& result : m_results)
        {
            if (result.pluginPath == pluginPath)
            {
                result.status = failureType;
                result.errorMessage = errorMessage;
                result.descriptions.clear();
                return;
            }
        }
    }

    /** Record that user skipped a plugin */
    void recordSkipped(const juce::String& pluginPath)
    {
        for (auto& result : m_results)
        {
            if (result.pluginPath == pluginPath)
            {
                result.status = PluginScanResult::Status::Skipped;
                result.errorMessage = "Skipped by user";
                return;
            }
        }
    }

    //==============================================================================
    // Cancellation / Pause (thread-safe via atomics)
    //==============================================================================

    void cancel() { m_isCancelled.store(true); }
    bool isCancelled() const { return m_isCancelled.load(); }

    void pause() { m_isPaused.store(true); }
    void resume() { m_isPaused.store(false); }
    bool isPaused() const { return m_isPaused.load(); }

    //==============================================================================
    // Summary Generation
    //==============================================================================

    /** Get count of successfully scanned plugins */
    int getSuccessCount() const
    {
        int count = 0;
        for (const auto& result : m_results)
        {
            if (result.isSuccess())
                count++;
        }
        return count;
    }

    /** Get count of failed plugins */
    int getFailedCount() const
    {
        int count = 0;
        for (const auto& result : m_results)
        {
            if (result.isFailed())
                count++;
        }
        return count;
    }

    /** Get count of skipped plugins */
    int getSkippedCount() const
    {
        int count = 0;
        for (const auto& result : m_results)
        {
            if (result.status == PluginScanResult::Status::Skipped ||
                result.status == PluginScanResult::Status::Blacklisted)
                count++;
        }
        return count;
    }

    /** Get count of cached (not re-scanned) plugins */
    int getCachedCount() const
    {
        int count = 0;
        for (const auto& result : m_results)
        {
            if (result.status == PluginScanResult::Status::Cached)
                count++;
        }
        return count;
    }

    /** Get total number of plugin descriptions found */
    int getTotalPluginsFound() const
    {
        int count = 0;
        for (const auto& result : m_results)
        {
            count += result.descriptions.size();
        }
        return count;
    }

    /** Get all failed results for summary dialog */
    juce::Array<PluginScanResult> getFailedResults() const
    {
        juce::Array<PluginScanResult> failed;
        for (const auto& result : m_results)
        {
            if (result.isFailed())
                failed.add(result);
        }
        return failed;
    }

    /** Get all results */
    const juce::Array<PluginScanResult>& getAllResults() const { return m_results; }

    /** Get scan duration */
    juce::RelativeTime getScanDuration() const
    {
        return juce::Time::getCurrentTime() - m_scanStartTime;
    }

    /** Create a copyable snapshot of the scan state for callbacks */
    PluginScanSummary createSummary() const
    {
        PluginScanSummary summary;
        summary.results = m_results;
        summary.scanStartTime = m_scanStartTime;
        summary.successCount = getSuccessCount();
        summary.failedCount = getFailedCount();
        summary.skippedCount = getSkippedCount();
        summary.cachedCount = getCachedCount();
        return summary;
    }

private:
    juce::Array<PluginScanResult> m_results;
    int m_currentIndex = 0;
    std::atomic<bool> m_isCancelled{false};
    std::atomic<bool> m_isPaused{false};
    juce::Time m_scanStartTime;
};

/**
 * Persistent cache entry for incremental scanning.
 * Stores file metadata to detect changes without re-scanning.
 */
struct PluginCacheEntry
{
    juce::String pluginPath;
    juce::Time lastModified;
    juce::int64 fileSize = 0;
    juce::Time lastScanned;
    juce::Array<juce::PluginDescription> descriptions;

    /** Serialize to XML */
    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("PluginCacheEntry");
        xml->setAttribute("path", pluginPath);
        xml->setAttribute("lastModified", static_cast<double>(lastModified.toMilliseconds()));
        xml->setAttribute("fileSize", static_cast<double>(fileSize));
        xml->setAttribute("lastScanned", static_cast<double>(lastScanned.toMilliseconds()));

        for (const auto& desc : descriptions)
        {
            auto* descXml = desc.createXml().release();
            if (descXml)
                xml->addChildElement(descXml);
        }

        return xml;
    }

    /** Deserialize from XML */
    static PluginCacheEntry fromXml(const juce::XmlElement& xml)
    {
        PluginCacheEntry entry;
        entry.pluginPath = xml.getStringAttribute("path");
        entry.lastModified = juce::Time(static_cast<juce::int64>(xml.getDoubleAttribute("lastModified")));
        entry.fileSize = static_cast<juce::int64>(xml.getDoubleAttribute("fileSize"));
        entry.lastScanned = juce::Time(static_cast<juce::int64>(xml.getDoubleAttribute("lastScanned")));

        for (auto* child : xml.getChildIterator())
        {
            juce::PluginDescription desc;
            if (desc.loadFromXml(*child))
                entry.descriptions.add(desc);
        }

        return entry;
    }

    /** Check if the plugin file has changed since last scan */
    bool hasFileChanged(const juce::File& file) const
    {
        if (!file.existsAsFile())
            return true;

        // Check modification time and size
        if (file.getLastModificationTime() != lastModified)
            return true;

        if (file.getSize() != fileSize)
            return true;

        return false;
    }
};
