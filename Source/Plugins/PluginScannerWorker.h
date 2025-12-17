/*
  ==============================================================================

    PluginScannerWorker.h
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

/**
 * Plugin scanner worker that runs in a separate subprocess.
 *
 * This worker is spawned by the main WaveEdit application to scan plugins
 * in isolation. If a plugin causes a crash (std::terminate, access violation,
 * etc.), only this worker process dies - the main application survives and
 * can detect which plugin caused the crash.
 *
 * Usage:
 * The main application spawns WaveEdit with --waveedit-plugin-scanner argument.
 * This class handles all communication and plugin scanning in that subprocess.
 *
 * IPC is handled via JUCE's ChildProcessWorker using pipes/sockets.
 */
class PluginScannerWorker : public juce::ChildProcessWorker
{
public:
    PluginScannerWorker();
    ~PluginScannerWorker() override;

    /**
     * Run the worker's main loop.
     * @param commandLine The command line string from main() - required because
     *                    JUCE isn't initialized yet when running as scanner worker
     * @return Exit code (0 = success)
     */
    int run(const juce::String& commandLine);

protected:
    //==============================================================================
    // ChildProcessWorker overrides
    //==============================================================================

    /** Called when a message arrives from the coordinator (main app) */
    void handleMessageFromCoordinator(const juce::MemoryBlock& data) override;

    /** Called when connection to coordinator is lost */
    void handleConnectionLost() override;

private:
    //==============================================================================
    // Message Handlers
    //==============================================================================

    void handleScanPlugin(const juce::XmlElement& xml);
    void handleShutdown();
    void handleHeartbeat();

    //==============================================================================
    // Plugin Scanning
    //==============================================================================

    /** Scan a single plugin file and return descriptions */
    juce::Array<juce::PluginDescription> scanPluginFile(const juce::String& pluginPath,
                                                         const juce::String& formatName);

    //==============================================================================
    juce::AudioPluginFormatManager m_formatManager;
    std::atomic<bool> m_shouldShutdown{false};
    std::atomic<bool> m_connectionLost{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginScannerWorker)
};

/**
 * Entry point for the worker process.
 * Called from main() when --waveedit-plugin-scanner is detected.
 *
 * IMPORTANT: This function is called BEFORE JUCE is initialized, so it cannot
 * use JUCEApplication methods. The command line must be passed from main().
 *
 * @param commandLine The raw command line string from main()
 * @return Exit code (0 = success, non-zero = error)
 */
int runPluginScannerWorker(const juce::String& commandLine);
