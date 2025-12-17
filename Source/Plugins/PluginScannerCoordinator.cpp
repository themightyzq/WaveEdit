/*
  ==============================================================================

    PluginScannerCoordinator.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "PluginScannerCoordinator.h"
#include <iostream>

//==============================================================================
// PluginScannerCoordinator Implementation
//==============================================================================

PluginScannerCoordinator::PluginScannerCoordinator()
{
    std::cerr << "[COORD " << this << "] Constructor" << std::endl;
}

PluginScannerCoordinator::~PluginScannerCoordinator()
{
    std::cerr << "[COORD " << this << "] Destructor - calling cancelScan" << std::endl;
    cancelScan();
    std::cerr << "[COORD " << this << "] Destructor done" << std::endl;
}

//==============================================================================
// Scanning API
//==============================================================================

void PluginScannerCoordinator::scanPluginsAsync(const juce::StringArray& pluginFiles,
                                                  const juce::String& formatName,
                                                  ProgressCallback progressCallback,
                                                  CompletionCallback completionCallback,
                                                  CrashCallback crashCallback)
{
    if (m_scanInProgress.load())
    {
        DBG("PluginScannerCoordinator: Scan already in progress");
        return;
    }

    if (pluginFiles.isEmpty())
    {
        if (completionCallback)
        {
            juce::Array<juce::PluginDescription> empty;
            juce::MessageManager::callAsync([completionCallback, empty]()
            {
                completionCallback(true, empty);
            });
        }
        return;
    }

    m_scanInProgress.store(true);

    // Initialize scan state
    {
        const juce::ScopedLock sl(m_lock);
        m_scanState = std::make_unique<ScanState>();
        m_scanState->pluginFiles = pluginFiles;
        m_scanState->formatName = formatName;
        m_scanState->currentIndex = 0;
        m_scanState->progressCallback = std::move(progressCallback);
        m_scanState->completionCallback = std::move(completionCallback);
        m_scanState->crashCallback = std::move(crashCallback);
    }

    // Start worker process
    if (!startWorker())
    {
        DBG("PluginScannerCoordinator: Failed to start worker process");
        finishScan(false);
        return;
    }

    // Start heartbeat timer
    m_lastHeartbeatTime = juce::Time::getMillisecondCounter();
    startTimer(PluginScannerProtocol::kHeartbeatIntervalMs);
}

void PluginScannerCoordinator::cancelScan()
{
    stopTimer();
    stopWorker();

    const juce::ScopedLock sl(m_lock);
    m_scanState.reset();
    m_scanInProgress.store(false);
}

//==============================================================================
// Worker Management
//==============================================================================

bool PluginScannerCoordinator::startWorker()
{
    // Get path to current executable
    juce::File executable = PluginScannerProtocol::getExecutablePath();
    std::cerr << "[COORD] startWorker() - executable: " << executable.getFullPathName().toStdString() << std::endl;

    if (!executable.existsAsFile())
    {
        DBG("PluginScannerCoordinator: Executable not found: " + executable.getFullPathName());
        std::cerr << "[COORD] ERROR: Executable not found!" << std::endl;
        return false;
    }

    // Launch worker process with special argument
    std::cerr << "[COORD] Launching worker process with arg: " << PluginScannerProtocol::kWorkerProcessArg << std::endl;
    bool result = launchWorkerProcess(executable,
                               PluginScannerProtocol::kWorkerProcessArg,
                               PluginScannerProtocol::kConnectionTimeoutMs);
    std::cerr << "[COORD] launchWorkerProcess returned: " << (result ? "true" : "false") << std::endl;
    return result;
}

void PluginScannerCoordinator::stopWorker()
{
    // Try to gracefully shut down worker
    if (m_workerConnected.load())
    {
        sendMessageToWorker(PluginScannerProtocol::createShutdownMessage());
        juce::Thread::sleep(100);  // Give worker time to exit
    }

    // Force kill if still connected
    killWorkerProcess();
    m_workerConnected.store(false);
}

void PluginScannerCoordinator::scanNextPlugin()
{
    const juce::ScopedLock sl(m_lock);

    if (m_scanState == nullptr)
        return;

    // Check if we're done
    if (m_scanState->currentIndex >= m_scanState->pluginFiles.size())
    {
        // All plugins scanned
        finishScan(true);
        return;
    }

    // Get next plugin to scan
    m_scanState->currentPlugin = m_scanState->pluginFiles[m_scanState->currentIndex];
    m_scanState->waitingForScanResult = true;

    // Report progress
    float progress = static_cast<float>(m_scanState->currentIndex) /
                     static_cast<float>(m_scanState->pluginFiles.size());

    if (m_scanState->progressCallback)
    {
        auto callback = m_scanState->progressCallback;
        auto pluginName = m_scanState->currentPlugin;
        juce::MessageManager::callAsync([callback, progress, pluginName]()
        {
            callback(progress, pluginName);
        });
    }

    // Send scan request to worker
    sendMessageToWorker(PluginScannerProtocol::createScanPluginMessage(
        m_scanState->currentPlugin,
        m_scanState->formatName));

    m_lastHeartbeatTime = juce::Time::getMillisecondCounter();
}

void PluginScannerCoordinator::finishScan(bool success)
{
    std::cerr << "[COORD " << this << "] finishScan(" << (success ? "true" : "false") << ") - START" << std::endl;

    std::cerr << "[COORD " << this << "] finishScan - stopTimer" << std::endl;
    stopTimer();

    // IMPORTANT: Do NOT call stopWorker() here!
    // We're likely being called from handleMessageFromWorker which runs on a thread
    // that JUCE's IPC uses. Calling killWorkerProcess() from here would deadlock.
    // Instead, we just send the shutdown message and let the worker exit gracefully.
    // The worker will be killed when this PluginScannerCoordinator is destroyed.
    std::cerr << "[COORD " << this << "] finishScan - sending shutdown to worker" << std::endl;
    if (m_workerConnected.load())
    {
        sendMessageToWorker(PluginScannerProtocol::createShutdownMessage());
    }
    m_workerConnected.store(false);
    std::cerr << "[COORD " << this << "] finishScan - shutdown sent" << std::endl;

    juce::Array<juce::PluginDescription> results;
    CompletionCallback callback;

    {
        const juce::ScopedLock sl(m_lock);

        if (m_scanState != nullptr)
        {
            results = m_scanState->foundPlugins;
            callback = m_scanState->completionCallback;
            std::cerr << "[COORD " << this << "] finishScan - found " << results.size() << " plugins" << std::endl;
        }
        else
        {
            std::cerr << "[COORD " << this << "] finishScan - WARNING: m_scanState is null!" << std::endl;
        }

        m_scanState.reset();
    }

    m_scanInProgress.store(false);

    // Invoke completion callback on message thread
    if (callback)
    {
        std::cerr << "[COORD " << this << "] finishScan - calling completion callback via callAsync" << std::endl;
        juce::MessageManager::callAsync([callback, success, results]()
        {
            std::cerr << "[COORD] Completion callback running - success=" << (success ? "true" : "false")
                      << " plugins=" << results.size() << std::endl;
            callback(success, results);
            std::cerr << "[COORD] Completion callback finished" << std::endl;
        });
    }
    else
    {
        std::cerr << "[COORD " << this << "] finishScan - WARNING: no completion callback!" << std::endl;
    }
}

//==============================================================================
// ChildProcessCoordinator overrides
//==============================================================================

void PluginScannerCoordinator::handleMessageFromWorker(const juce::MemoryBlock& data)
{
    std::cerr << "[COORD " << this << "] handleMessageFromWorker - received " << data.getSize() << " bytes" << std::endl;

    auto xml = PluginScannerProtocol::parseMessage(data);
    if (xml == nullptr)
    {
        std::cerr << "[COORD " << this << "] handleMessageFromWorker - FAILED to parse message!" << std::endl;
        DBG("PluginScannerCoordinator: Failed to parse worker message");
        return;
    }

    juce::String messageType = PluginScannerProtocol::getMessageType(*xml);
    std::cerr << "[COORD " << this << "] handleMessageFromWorker - message type: " << messageType.toStdString() << std::endl;

    if (messageType == "Ready")
    {
        handleWorkerReady();
    }
    else if (messageType == "ScanStarted")
    {
        handleScanStarted(*xml);
    }
    else if (messageType == "ScanComplete")
    {
        handleScanComplete(*xml);
    }
    else if (messageType == "ScanFailed")
    {
        handleScanFailed(*xml);
    }
    else if (messageType == "HeartbeatAck")
    {
        m_lastHeartbeatTime = juce::Time::getMillisecondCounter();
    }
    else if (messageType == "Error")
    {
        handleWorkerError(*xml);
    }
    else
    {
        DBG("PluginScannerCoordinator: Unknown message type: " + messageType);
    }
}

void PluginScannerCoordinator::handleConnectionLost()
{
    m_workerConnected.store(false);

    const juce::ScopedLock sl(m_lock);

    if (m_scanState == nullptr || !m_scanInProgress.load())
        return;

    DBG("PluginScannerCoordinator: Connection to worker lost");

    // If we were waiting for a scan result, the plugin caused a crash
    if (m_scanState->waitingForScanResult && m_scanState->currentPlugin.isNotEmpty())
    {
        DBG("PluginScannerCoordinator: Plugin crashed worker: " + m_scanState->currentPlugin);

        // Notify about crash for blacklisting
        if (m_scanState->crashCallback)
        {
            auto callback = m_scanState->crashCallback;
            auto crashedPlugin = m_scanState->currentPlugin;
            juce::MessageManager::callAsync([callback, crashedPlugin]()
            {
                callback(crashedPlugin);
            });
        }

        // Move to next plugin
        m_scanState->currentIndex++;
        m_scanState->waitingForScanResult = false;

        // Restart worker and continue scanning
        if (m_scanState->currentIndex < m_scanState->pluginFiles.size())
        {
            if (startWorker())
            {
                // Worker will send Ready message, then we continue
                return;
            }
        }
    }

    // If we can't restart or no more plugins, finish
    finishScan(true);  // Still success - we got what we could
}

//==============================================================================
// Message Handlers
//==============================================================================

void PluginScannerCoordinator::handleWorkerReady()
{
    std::cerr << "[COORD " << this << "] handleWorkerReady - worker is ready!" << std::endl;
    DBG("PluginScannerCoordinator: Worker ready");

    m_workerConnected.store(true);

    {
        const juce::ScopedLock sl(m_lock);
        if (m_scanState != nullptr)
        {
            m_scanState->workerReady = true;
            std::cerr << "[COORD " << this << "] handleWorkerReady - about to scanNextPlugin" << std::endl;
        }
        else
        {
            std::cerr << "[COORD " << this << "] handleWorkerReady - ERROR: m_scanState is null!" << std::endl;
        }
    }

    // Start scanning
    scanNextPlugin();
}

void PluginScannerCoordinator::handleScanStarted(const juce::XmlElement& xml)
{
    juce::String pluginPath = xml.getStringAttribute("path");
    DBG("PluginScannerCoordinator: Scan started for: " + pluginPath);

    // Reset heartbeat timer
    m_lastHeartbeatTime = juce::Time::getMillisecondCounter();
}

void PluginScannerCoordinator::handleScanComplete(const juce::XmlElement& xml)
{
    juce::String pluginPath = xml.getStringAttribute("path");
    auto descriptions = PluginScannerProtocol::parsePluginDescriptions(xml);

    DBG("PluginScannerCoordinator: Scan complete for: " + pluginPath +
        " (" + juce::String(descriptions.size()) + " plugins found)");

    {
        const juce::ScopedLock sl(m_lock);

        if (m_scanState != nullptr)
        {
            // Add found plugins to results
            m_scanState->foundPlugins.addArray(descriptions);

            // Move to next plugin
            m_scanState->currentIndex++;
            m_scanState->waitingForScanResult = false;
        }
    }

    // Continue with next plugin
    scanNextPlugin();
}

void PluginScannerCoordinator::handleScanFailed(const juce::XmlElement& xml)
{
    juce::String pluginPath = xml.getStringAttribute("path");
    juce::String error = xml.getStringAttribute("error");

    DBG("PluginScannerCoordinator: Scan failed for: " + pluginPath + " - " + error);

    {
        const juce::ScopedLock sl(m_lock);

        if (m_scanState != nullptr)
        {
            // Move to next plugin
            m_scanState->currentIndex++;
            m_scanState->waitingForScanResult = false;
        }
    }

    // Continue with next plugin
    scanNextPlugin();
}

void PluginScannerCoordinator::handleWorkerError(const juce::XmlElement& xml)
{
    juce::String error = xml.getStringAttribute("message");
    DBG("PluginScannerCoordinator: Worker error: " + error);
}

//==============================================================================
// Timer callback
//==============================================================================

void PluginScannerCoordinator::timerCallback()
{
    if (!m_scanInProgress.load() || !m_workerConnected.load())
        return;

    auto now = juce::Time::getMillisecondCounter();

    // Check for timeout
    if (now - m_lastHeartbeatTime > static_cast<juce::uint32>(PluginScannerProtocol::kWorkerTimeoutMs))
    {
        DBG("PluginScannerCoordinator: Worker timeout - killing process");

        // Worker is stuck - treat as crash
        stopWorker();
        handleConnectionLost();
        return;
    }

    // Send heartbeat
    sendMessageToWorker(PluginScannerProtocol::createHeartbeatMessage());
}

//==============================================================================
// OutOfProcessPluginScanner Implementation
//==============================================================================

OutOfProcessPluginScanner::OutOfProcessPluginScanner()
{
}

OutOfProcessPluginScanner::~OutOfProcessPluginScanner()
{
    m_coordinator.cancelScan();
}

void OutOfProcessPluginScanner::setCrashCallback(std::function<void(const juce::String&)> callback)
{
    m_crashCallback = std::move(callback);
}

bool OutOfProcessPluginScanner::findPluginTypesFor(juce::AudioPluginFormat& format,
                                                     juce::OwnedArray<juce::PluginDescription>& result,
                                                     const juce::String& fileOrIdentifier)
{
    // Reset state
    m_scanComplete.reset();
    m_scanResults.clear();
    m_scanSuccess = false;

    // Start async scan
    juce::StringArray files;
    files.add(fileOrIdentifier);

    m_coordinator.scanPluginsAsync(
        files,
        format.getName(),
        nullptr,  // No progress callback for single file
        [this](bool success, const juce::Array<juce::PluginDescription>& plugins)
        {
            m_scanSuccess = success;
            m_scanResults = plugins;
            m_scanComplete.signal();
        },
        [this](const juce::String& crashedPlugin)
        {
            if (m_crashCallback)
            {
                m_crashCallback(crashedPlugin);
            }
        }
    );

    // Wait for completion (with timeout)
    if (!m_scanComplete.wait(PluginScannerProtocol::kWorkerTimeoutMs + 5000))
    {
        DBG("OutOfProcessPluginScanner: Timeout waiting for scan of " + fileOrIdentifier);
        m_coordinator.cancelScan();
        return true;  // Continue scanning other plugins
    }

    // Add results
    for (const auto& desc : m_scanResults)
    {
        result.add(new juce::PluginDescription(desc));
    }

    return true;  // Continue scanning
}

void OutOfProcessPluginScanner::scanFinished()
{
    m_coordinator.cancelScan();
}
