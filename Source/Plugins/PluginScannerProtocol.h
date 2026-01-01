/*
  ==============================================================================

    PluginScannerProtocol.h
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

#if JUCE_WINDOWS
  #include <windows.h>  // For GetCurrentProcessId()
#else
  #include <unistd.h>   // For getpid()
#endif

/**
 * Inter-process communication protocol for out-of-process plugin scanning.
 *
 * This protocol defines the messages exchanged between the coordinator (main app)
 * and worker (scanner subprocess) using JUCE's ChildProcess IPC mechanism.
 *
 * Out-of-process scanning is essential because:
 * 1. Badly behaved plugins can call std::terminate() which bypasses all C++ exception handling
 * 2. Plugin crashes would otherwise terminate the entire application
 * 3. Professional DAWs (Logic Pro, Pro Tools, Ableton) all use this approach
 *
 * Message Format:
 * All messages are sent as MemoryBlocks containing XML data for easy parsing
 * and future extensibility.
 */
namespace PluginScannerProtocol
{
    //==============================================================================
    // Protocol Constants
    //==============================================================================

    /** Command line argument to identify worker process */
    static const char* const kWorkerProcessArg = "--waveedit-plugin-scanner";

    /** Timeout for worker process to respond (milliseconds) */
    static constexpr int kWorkerTimeoutMs = 30000;  // 30 seconds per plugin

    /** Timeout for initial connection (milliseconds) */
    static constexpr int kConnectionTimeoutMs = 5000;

    /** Heartbeat interval (milliseconds) */
    static constexpr int kHeartbeatIntervalMs = 1000;

    //==============================================================================
    // Message Types
    //==============================================================================

    /** Messages from Coordinator -> Worker */
    enum class CoordinatorMessage
    {
        ScanPlugin,      // Request to scan a specific plugin file
        Shutdown,        // Request worker to exit cleanly
        Heartbeat        // Keep-alive ping
    };

    /** Messages from Worker -> Coordinator */
    enum class WorkerMessage
    {
        Ready,           // Worker has initialized and is ready
        ScanStarted,     // Worker is beginning to scan a plugin
        ScanComplete,    // Plugin scan completed successfully
        ScanFailed,      // Plugin scan failed (but worker survived)
        HeartbeatAck,    // Response to heartbeat
        Error            // General error message
    };

    //==============================================================================
    // Message Builders (Coordinator -> Worker)
    //==============================================================================

    /** Create a scan request message */
    inline juce::MemoryBlock createScanPluginMessage(const juce::String& pluginPath,
                                                      const juce::String& formatName)
    {
        juce::XmlElement xml("ScanPlugin");
        xml.setAttribute("path", pluginPath);
        xml.setAttribute("format", formatName);

        juce::MemoryOutputStream stream;
        xml.writeTo(stream);
        return stream.getMemoryBlock();
    }

    /** Create a shutdown request message */
    inline juce::MemoryBlock createShutdownMessage()
    {
        juce::XmlElement xml("Shutdown");

        juce::MemoryOutputStream stream;
        xml.writeTo(stream);
        return stream.getMemoryBlock();
    }

    /** Create a heartbeat message */
    inline juce::MemoryBlock createHeartbeatMessage()
    {
        juce::XmlElement xml("Heartbeat");
        xml.setAttribute("time", static_cast<double>(juce::Time::getCurrentTime().toMilliseconds()));

        juce::MemoryOutputStream stream;
        xml.writeTo(stream);
        return stream.getMemoryBlock();
    }

    //==============================================================================
    // Message Builders (Worker -> Coordinator)
    //==============================================================================

    /** Create a ready message */
    inline juce::MemoryBlock createReadyMessage()
    {
        juce::XmlElement xml("Ready");
        // Use getpid() on POSIX, GetCurrentProcessId() on Windows
        #if JUCE_WINDOWS
        xml.setAttribute("pid", static_cast<int>(GetCurrentProcessId()));
        #else
        xml.setAttribute("pid", static_cast<int>(getpid()));
        #endif

        juce::MemoryOutputStream stream;
        xml.writeTo(stream);
        return stream.getMemoryBlock();
    }

    /** Create a scan started message */
    inline juce::MemoryBlock createScanStartedMessage(const juce::String& pluginPath)
    {
        juce::XmlElement xml("ScanStarted");
        xml.setAttribute("path", pluginPath);
        xml.setAttribute("time", static_cast<double>(juce::Time::getCurrentTime().toMilliseconds()));

        juce::MemoryOutputStream stream;
        xml.writeTo(stream);
        return stream.getMemoryBlock();
    }

    /** Create a scan complete message with plugin descriptions */
    inline juce::MemoryBlock createScanCompleteMessage(const juce::String& pluginPath,
                                                        const juce::Array<juce::PluginDescription>& descriptions)
    {
        juce::XmlElement xml("ScanComplete");
        xml.setAttribute("path", pluginPath);
        xml.setAttribute("count", descriptions.size());

        // Add each plugin description as a child element
        for (const auto& desc : descriptions)
        {
            auto* descXml = desc.createXml().release();
            if (descXml != nullptr)
            {
                xml.addChildElement(descXml);
            }
        }

        juce::MemoryOutputStream stream;
        xml.writeTo(stream);
        return stream.getMemoryBlock();
    }

    /** Create a scan failed message */
    inline juce::MemoryBlock createScanFailedMessage(const juce::String& pluginPath,
                                                      const juce::String& errorMessage)
    {
        juce::XmlElement xml("ScanFailed");
        xml.setAttribute("path", pluginPath);
        xml.setAttribute("error", errorMessage);

        juce::MemoryOutputStream stream;
        xml.writeTo(stream);
        return stream.getMemoryBlock();
    }

    /** Create a heartbeat acknowledgment message */
    inline juce::MemoryBlock createHeartbeatAckMessage()
    {
        juce::XmlElement xml("HeartbeatAck");
        xml.setAttribute("time", static_cast<double>(juce::Time::getCurrentTime().toMilliseconds()));

        juce::MemoryOutputStream stream;
        xml.writeTo(stream);
        return stream.getMemoryBlock();
    }

    /** Create an error message */
    inline juce::MemoryBlock createErrorMessage(const juce::String& error)
    {
        juce::XmlElement xml("Error");
        xml.setAttribute("message", error);

        juce::MemoryOutputStream stream;
        xml.writeTo(stream);
        return stream.getMemoryBlock();
    }

    //==============================================================================
    // Message Parsing
    //==============================================================================

    /** Parse a message and return its XML element (caller owns the returned object) */
    inline std::unique_ptr<juce::XmlElement> parseMessage(const juce::MemoryBlock& data)
    {
        return juce::XmlDocument::parse(data.toString());
    }

    /** Get the message type from XML element name */
    inline juce::String getMessageType(const juce::XmlElement& xml)
    {
        return xml.getTagName();
    }

    /** Parse plugin descriptions from a ScanComplete message */
    inline juce::Array<juce::PluginDescription> parsePluginDescriptions(const juce::XmlElement& xml)
    {
        juce::Array<juce::PluginDescription> descriptions;

        for (auto* child : xml.getChildIterator())
        {
            juce::PluginDescription desc;
            if (desc.loadFromXml(*child))
            {
                descriptions.add(desc);
            }
        }

        return descriptions;
    }

    //==============================================================================
    // Utility Functions
    //==============================================================================

    /** Check if command line indicates this should run as scanner worker */
    inline bool isWorkerProcess(const juce::String& commandLine)
    {
        return commandLine.contains(kWorkerProcessArg);
    }

    /** Get the path to the current executable (for spawning worker) */
    inline juce::File getExecutablePath()
    {
        return juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    }

} // namespace PluginScannerProtocol
