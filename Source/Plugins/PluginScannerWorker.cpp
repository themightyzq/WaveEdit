/*
  ==============================================================================

    PluginScannerWorker.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "PluginScannerWorker.h"
#include <fstream>
#include <csignal>

#if JUCE_WINDOWS
  #include <windows.h>
  #include <io.h>  // For _open, _write, _close on Windows
  #define open _open
  #define write _write
  #define close _close
  #define O_WRONLY _O_WRONLY
  #define O_APPEND _O_APPEND
  #define O_CREAT _O_CREAT
#else
  #include <fcntl.h>
  #include <unistd.h>
#endif

// NOTE: CoreFoundation include removed - no longer needed since we only scan VST3
// (AudioUnit support has been removed from WaveEdit)

//==============================================================================
// Signal handler to catch crashes and exit cleanly (suppress crash dialogs)
// Note: On Windows, we rely on JUCE's built-in exception handling.
//       POSIX signal handling is only available on macOS/Linux.
//==============================================================================
#if ! JUCE_WINDOWS
static std::atomic<bool> g_crashHandled{false};
static juce::String g_currentPluginPath;  // Track which plugin is being scanned

static void crashSignalHandler(int signal)
{
    // Prevent re-entry if we crash in the handler
    if (g_crashHandled.exchange(true))
        _exit(99);

    // Log the crash - use write() which is async-signal-safe
    const char* signalName = "UNKNOWN";
    switch (signal)
    {
        case SIGSEGV: signalName = "SIGSEGV"; break;
        case SIGABRT: signalName = "SIGABRT"; break;
        case SIGBUS:  signalName = "SIGBUS"; break;
        case SIGFPE:  signalName = "SIGFPE"; break;
        case SIGILL:  signalName = "SIGILL"; break;
    }

    // Write crash info to log file (can't use workerLog - not signal safe)
    int fd = open("/tmp/waveedit_worker.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd >= 0)
    {
        char buf[512];
        snprintf(buf, sizeof(buf), "[WORKER %d] CRASH CAUGHT: %s - plugin scan failed (this is expected for some plugins)\n",
                 getpid(), signalName);
        write(fd, buf, strlen(buf));
        close(fd);
    }

    // Exit cleanly instead of crashing - this prevents macOS "quit unexpectedly" dialog
    _exit(100 + signal);
}

static void installCrashHandlers()
{
    // Install signal handlers for common crash signals
    struct sigaction sa;
    sa.sa_handler = crashSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  // Don't use SA_RESTART - we want to exit

    sigaction(SIGSEGV, &sa, nullptr);  // Segmentation fault
    sigaction(SIGABRT, &sa, nullptr);  // Abort (e.g., assertion failure)
    sigaction(SIGBUS, &sa, nullptr);   // Bus error
    sigaction(SIGFPE, &sa, nullptr);   // Floating point exception
    sigaction(SIGILL, &sa, nullptr);   // Illegal instruction
}
#else
// Windows: No-op - rely on JUCE's exception handling and Windows SEH
static void installCrashHandlers() {}
#endif

// Debug file logging for worker process (stderr doesn't work for subprocesses)
static void workerLog(const juce::String& message)
{
    // Use platform-appropriate temp directory
    #if JUCE_WINDOWS
    static std::ofstream logFile(juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("waveedit_worker.log").getFullPathName().toStdString(), std::ios::app);
    logFile << "[WORKER " << GetCurrentProcessId() << "] " << message.toStdString() << std::endl;
    #else
    static std::ofstream logFile("/tmp/waveedit_worker.log", std::ios::app);
    logFile << "[WORKER " << getpid() << "] " << message.toStdString() << std::endl;
    #endif
    logFile.flush();
}

//==============================================================================
PluginScannerWorker::PluginScannerWorker()
{
    // Initialize plugin formats - VST3 only
    // (AudioUnit support has been removed from WaveEdit)
    #if JUCE_PLUGINHOST_VST3
    m_formatManager.addFormat(new juce::VST3PluginFormat());
    #endif
}

PluginScannerWorker::~PluginScannerWorker()
{
}

int PluginScannerWorker::run(const juce::String& commandLine)
{
    workerLog("run() started - commandLine: " + commandLine);

    // Initialize connection to coordinator using the command line passed from main()
    // Note: We can't use JUCEApplication::getCommandLineParameters() here because
    // JUCE hasn't been fully initialized yet (we're running before JUCEApplicationBase::main())
    workerLog("Calling initialiseFromCommandLine...");
    if (!initialiseFromCommandLine(commandLine, PluginScannerProtocol::kWorkerProcessArg))
    {
        workerLog("ERROR: initialiseFromCommandLine failed!");
        std::cerr << "PluginScannerWorker: Failed to initialize from command line" << std::endl;
        return 1;
    }
    workerLog("initialiseFromCommandLine succeeded");

    // Send ready message to coordinator
    workerLog("Sending Ready message to coordinator...");
    sendMessageToCoordinator(PluginScannerProtocol::createReadyMessage());
    workerLog("Ready message sent");

    // Main message loop
    // Wait for messages from coordinator until shutdown or connection lost
    workerLog("Entering main loop...");
    while (!m_shouldShutdown.load() && !m_connectionLost.load())
    {
        // Simple sleep - VST3 plugins don't require run loop pumping like AudioUnits do
        // (AudioUnit support has been removed from WaveEdit)
        juce::Thread::sleep(50);

        // The handleMessageFromCoordinator callback handles incoming messages
        // We just need to keep the process alive and check for shutdown
    }

    workerLog("Main loop exited - shutdown=" + juce::String(m_shouldShutdown.load() ? "true" : "false") +
              " connectionLost=" + juce::String(m_connectionLost.load() ? "true" : "false"));
    return 0;
}

//==============================================================================
// ChildProcessWorker overrides
//==============================================================================

void PluginScannerWorker::handleMessageFromCoordinator(const juce::MemoryBlock& data)
{
    auto xml = PluginScannerProtocol::parseMessage(data);
    if (xml == nullptr)
    {
        sendMessageToCoordinator(
            PluginScannerProtocol::createErrorMessage("Failed to parse message"));
        return;
    }

    juce::String messageType = PluginScannerProtocol::getMessageType(*xml);

    if (messageType == "ScanPlugin")
    {
        handleScanPlugin(*xml);
    }
    else if (messageType == "Shutdown")
    {
        handleShutdown();
    }
    else if (messageType == "Heartbeat")
    {
        handleHeartbeat();
    }
    else
    {
        sendMessageToCoordinator(
            PluginScannerProtocol::createErrorMessage("Unknown message type: " + messageType));
    }
}

void PluginScannerWorker::handleConnectionLost()
{
    // Connection to coordinator lost - exit gracefully
    m_connectionLost.store(true);
}

//==============================================================================
// Message Handlers
//==============================================================================

void PluginScannerWorker::handleScanPlugin(const juce::XmlElement& xml)
{
    juce::String pluginPath = xml.getStringAttribute("path");
    juce::String formatName = xml.getStringAttribute("format");

    if (pluginPath.isEmpty())
    {
        sendMessageToCoordinator(
            PluginScannerProtocol::createScanFailedMessage(pluginPath, "Empty plugin path"));
        return;
    }

    // Notify coordinator that scan is starting
    sendMessageToCoordinator(PluginScannerProtocol::createScanStartedMessage(pluginPath));

    // Attempt to scan the plugin
    // NOTE: If this crashes, the worker process dies but the coordinator survives
    auto descriptions = scanPluginFile(pluginPath, formatName);

    if (descriptions.isEmpty())
    {
        // Scan completed but found no valid plugins
        sendMessageToCoordinator(
            PluginScannerProtocol::createScanFailedMessage(pluginPath, "No valid plugins found"));
    }
    else
    {
        // Success - send plugin descriptions back
        sendMessageToCoordinator(
            PluginScannerProtocol::createScanCompleteMessage(pluginPath, descriptions));
    }
}

void PluginScannerWorker::handleShutdown()
{
    m_shouldShutdown.store(true);
}

void PluginScannerWorker::handleHeartbeat()
{
    sendMessageToCoordinator(PluginScannerProtocol::createHeartbeatAckMessage());
}

//==============================================================================
// Plugin Scanning
//==============================================================================

juce::Array<juce::PluginDescription> PluginScannerWorker::scanPluginFile(
    const juce::String& pluginPath,
    const juce::String& formatName)
{
    juce::Array<juce::PluginDescription> descriptions;

    try
    {
        juce::File pluginFile(pluginPath);
        if (!pluginFile.exists())
        {
            return descriptions;
        }

        // Find the format to use
        juce::AudioPluginFormat* format = nullptr;

        for (int i = 0; i < m_formatManager.getNumFormats(); ++i)
        {
            auto* fmt = m_formatManager.getFormat(i);
            if (fmt == nullptr)
                continue;

            if (formatName.isEmpty() || fmt->getName() == formatName)
            {
                // Check if this format can handle this file
                if (fmt->fileMightContainThisPluginType(pluginPath))
                {
                    format = fmt;
                    break;
                }
            }
        }

        if (format == nullptr)
        {
            return descriptions;
        }

        // Scan the plugin with a timeout mechanism
        // Some plugins (especially those with AI/ML features or complex license validation)
        // can take a long time on first load. We use 60 seconds which matches PluginManager.
        // We use a separate thread with timeout to prevent blocking forever.

        static constexpr int kScanTimeoutMs = 60000;  // 60 second timeout per plugin (was 15)

        juce::OwnedArray<juce::PluginDescription> found;
        std::atomic<bool> scanComplete{false};
        std::atomic<bool> scanSuccess{false};

        // Create a thread to do the actual scan
        class ScanThread : public juce::Thread
        {
        public:
            ScanThread(juce::AudioPluginFormat* fmt, const juce::String& path,
                       juce::OwnedArray<juce::PluginDescription>& results,
                       std::atomic<bool>& complete, std::atomic<bool>& success)
                : Thread("PluginScanThread"), m_format(fmt), m_path(path),
                  m_results(results), m_complete(complete), m_success(success) {}

            void run() override
            {
                try
                {
                    // This is where crashes typically happen with badly behaved plugins
                    m_format->findAllTypesForFile(m_results, m_path);
                    m_success.store(true);
                }
                catch (...)
                {
                    m_success.store(false);
                }
                m_complete.store(true);
            }

        private:
            juce::AudioPluginFormat* m_format;
            juce::String m_path;
            juce::OwnedArray<juce::PluginDescription>& m_results;
            std::atomic<bool>& m_complete;
            std::atomic<bool>& m_success;
        };

        ScanThread scanThread(format, pluginPath, found, scanComplete, scanSuccess);
        scanThread.startThread();

        // Wait for scan to complete with timeout, while pumping run loop
        auto startTime = juce::Time::getMillisecondCounter();
        bool timedOut = false;

        while (!scanComplete.load())
        {
            auto elapsed = juce::Time::getMillisecondCounter() - startTime;
            if (elapsed > kScanTimeoutMs)
            {
                // Timeout - signal thread to stop
                std::cerr << "PluginScannerWorker: TIMEOUT scanning " << pluginPath.toStdString()
                          << " (>" << kScanTimeoutMs << "ms)" << std::endl;
                timedOut = true;
                scanThread.signalThreadShouldExit();
                break;
            }

            // Simple sleep - VST3 doesn't require run loop pumping like AudioUnits
            juce::Thread::sleep(50);
        }

        // CRITICAL: Must wait for thread to fully exit BEFORE accessing shared data
        // This prevents use-after-free if thread is still writing to 'found' array
        scanThread.waitForThreadToExit(2000);

        if (timedOut || !scanSuccess.load())
        {
            std::cerr << "PluginScannerWorker: Scan " << (timedOut ? "timed out" : "failed")
                      << " for " << pluginPath.toStdString() << std::endl;
            return descriptions;  // Return empty = scan failed
        }

        // Thread has fully exited - safe to access 'found' array
        for (auto* desc : found)
        {
            if (desc != nullptr)
            {
                descriptions.add(*desc);
            }
        }
    }
    catch (const std::exception& e)
    {
        // Plugin threw during scanning - log and return empty
        std::cerr << "PluginScannerWorker: Exception scanning " << pluginPath.toStdString()
                  << ": " << e.what() << std::endl;
    }
    catch (...)
    {
        // Some non-standard exception - log and return empty
        std::cerr << "PluginScannerWorker: Unknown exception scanning " << pluginPath.toStdString() << std::endl;
    }

    return descriptions;
}

//==============================================================================
// Entry Point
//==============================================================================

int runPluginScannerWorker(const juce::String& commandLine)
{
    // This runs when the app is started with --waveedit-plugin-scanner argument
    // We need to initialize JUCE basics but NOT create a GUI
    //
    // IMPORTANT: The command line must be passed in from main() because
    // JUCEApplication::getCommandLineParameters() is not available - JUCE
    // hasn't been fully initialized yet (we run BEFORE JUCEApplicationBase::main())

    // Install crash handlers FIRST - this prevents macOS "quit unexpectedly" dialogs
    // when a plugin crashes during scanning
    installCrashHandlers();

    workerLog("=== runPluginScannerWorker() entry point ===");
    workerLog("Crash handlers installed - plugin crashes will be caught silently");
    workerLog("Command line: " + commandLine);

    try
    {
        workerLog("Creating PluginScannerWorker instance...");
        PluginScannerWorker worker;
        workerLog("Worker created, calling run()...");
        int result = worker.run(commandLine);
        workerLog("run() returned: " + juce::String(result));
        return result;
    }
    catch (const std::exception& e)
    {
        workerLog("FATAL EXCEPTION: " + juce::String(e.what()));
        std::cerr << "PluginScannerWorker: Fatal exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        workerLog("FATAL UNKNOWN EXCEPTION");
        std::cerr << "PluginScannerWorker: Unknown fatal exception" << std::endl;
        return 1;
    }
}
