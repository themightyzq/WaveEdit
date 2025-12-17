/*
  ==============================================================================

    PluginScannerCoordinator.h
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
#include "PluginScannerProtocol.h"
#include <functional>
#include <memory>
#include <atomic>

/**
 * Coordinates out-of-process plugin scanning by managing worker subprocesses.
 *
 * This class spawns separate processes to scan plugins, so that crashes
 * don't affect the main application. If a worker crashes while scanning
 * a plugin, that plugin is automatically added to the blacklist and
 * scanning continues with the remaining plugins.
 *
 * Thread Safety:
 * - Scanning runs on a background thread
 * - Callbacks are invoked on the message thread
 * - The class itself should only be used from the message thread
 *
 * Usage:
 * @code
 * PluginScannerCoordinator coordinator;
 *
 * coordinator.scanPluginsAsync(
 *     pluginFiles,
 *     formatName,
 *     [](float progress, const String& name) { updateProgressUI(progress, name); },
 *     [](bool success, const Array<PluginDescription>& plugins) { handleResults(plugins); },
 *     [](const String& crashedPlugin) { addToBlacklist(crashedPlugin); }
 * );
 * @endcode
 */
class PluginScannerCoordinator : public juce::ChildProcessCoordinator,
                                  private juce::Timer
{
public:
    //==============================================================================
    /** Progress callback: (progress 0.0-1.0, current plugin name) */
    using ProgressCallback = std::function<void(float progress, const juce::String& currentPlugin)>;

    /** Completion callback: (success, found plugin descriptions) */
    using CompletionCallback = std::function<void(bool success,
                                                   const juce::Array<juce::PluginDescription>& plugins)>;

    /** Crash callback: (crashed plugin path) - called when worker crashes scanning a plugin */
    using CrashCallback = std::function<void(const juce::String& crashedPlugin)>;

    //==============================================================================
    PluginScannerCoordinator();
    ~PluginScannerCoordinator() override;

    //==============================================================================
    // Scanning API
    //==============================================================================

    /**
     * Start asynchronous plugin scanning.
     *
     * @param pluginFiles    List of plugin file paths to scan
     * @param formatName     Plugin format (e.g., "VST3", "AudioUnit"), empty for auto-detect
     * @param progressCallback Called with progress updates (on message thread)
     * @param completionCallback Called when scan completes (on message thread)
     * @param crashCallback  Called when a plugin causes worker crash (on message thread)
     */
    void scanPluginsAsync(const juce::StringArray& pluginFiles,
                          const juce::String& formatName,
                          ProgressCallback progressCallback,
                          CompletionCallback completionCallback,
                          CrashCallback crashCallback);

    /** Cancel any in-progress scan */
    void cancelScan();

    /** Check if a scan is currently in progress */
    bool isScanInProgress() const { return m_scanInProgress.load(); }

protected:
    //==============================================================================
    // ChildProcessCoordinator overrides
    //==============================================================================

    /** Called when a message arrives from the worker */
    void handleMessageFromWorker(const juce::MemoryBlock& data) override;

    /** Called when connection to worker is lost (crash or exit) */
    void handleConnectionLost() override;

private:
    //==============================================================================
    // Internal Scanning State
    //==============================================================================

    struct ScanState
    {
        juce::StringArray pluginFiles;      // All files to scan
        juce::String formatName;            // Format to use
        int currentIndex = 0;               // Current file being scanned
        juce::String currentPlugin;         // Path of plugin currently being scanned
        juce::Array<juce::PluginDescription> foundPlugins;  // Accumulated results
        bool workerReady = false;           // Worker sent Ready message
        bool waitingForScanResult = false;  // Waiting for scan response

        ProgressCallback progressCallback;
        CompletionCallback completionCallback;
        CrashCallback crashCallback;
    };

    std::unique_ptr<ScanState> m_scanState;
    std::atomic<bool> m_scanInProgress{false};

    //==============================================================================
    // Worker Management
    //==============================================================================

    /** Start a new worker process */
    bool startWorker();

    /** Stop the worker process */
    void stopWorker();

    /** Send the next scan request to worker */
    void scanNextPlugin();

    /** Handle scan completion (success or failure) */
    void finishScan(bool success);

    //==============================================================================
    // Message Handlers
    //==============================================================================

    void handleWorkerReady();
    void handleScanStarted(const juce::XmlElement& xml);
    void handleScanComplete(const juce::XmlElement& xml);
    void handleScanFailed(const juce::XmlElement& xml);
    void handleWorkerError(const juce::XmlElement& xml);

    //==============================================================================
    // Timer callback for heartbeat
    //==============================================================================

    void timerCallback() override;

    //==============================================================================
    juce::CriticalSection m_lock;
    juce::uint32 m_lastHeartbeatTime = 0;
    std::atomic<bool> m_workerConnected{false};

    /** Check if worker is connected */
    bool isWorkerConnected() const { return m_workerConnected.load(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginScannerCoordinator)
};

/**
 * Custom scanner that integrates with JUCE's KnownPluginList.
 *
 * This class implements KnownPluginList::CustomScanner to redirect
 * plugin scanning through the out-of-process coordinator.
 */
class OutOfProcessPluginScanner : public juce::KnownPluginList::CustomScanner
{
public:
    OutOfProcessPluginScanner();
    ~OutOfProcessPluginScanner() override;

    /** Set callback for when a plugin causes a crash (for blacklisting) */
    void setCrashCallback(std::function<void(const juce::String&)> callback);

    //==============================================================================
    // KnownPluginList::CustomScanner overrides
    //==============================================================================

    /**
     * Scan for plugins of a given format.
     * This is called by KnownPluginList::scanAndAddFile().
     *
     * @return true if scanning should continue, false to abort
     */
    bool findPluginTypesFor(juce::AudioPluginFormat& format,
                            juce::OwnedArray<juce::PluginDescription>& result,
                            const juce::String& fileOrIdentifier) override;

    /** Called when the scan should be cancelled */
    void scanFinished() override;

private:
    PluginScannerCoordinator m_coordinator;
    std::function<void(const juce::String&)> m_crashCallback;

    // Synchronization for blocking scan
    juce::WaitableEvent m_scanComplete;
    juce::Array<juce::PluginDescription> m_scanResults;
    bool m_scanSuccess = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutOfProcessPluginScanner)
};
