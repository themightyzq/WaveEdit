/*
  ==============================================================================

    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/


#include "MainComponent.h"


//==============================================================================
/**
 * Main application class.
 * Handles application lifecycle and main window creation.
 */
class WaveEditApplication : public juce::JUCEApplication
{
public:
    WaveEditApplication() {}

    const juce::String getApplicationName() override
    {
        return "WaveEdit";
    }

    const juce::String getApplicationVersion() override
    {
        return "0.1.0";
    }

    bool moreThanOneInstanceAllowed() override
    {
        // Allow multiple instances for editing different files
        return true;
    }

    void initialise(const juce::String& /*commandLine*/) override
    {
        // Install a file logger so juce::Logger::writeToLog() ends up in a
        // documented, user-discoverable place. JUCE picks the platform-correct
        // location: ~/Library/Logs/WaveEdit/WaveEdit.log on macOS,
        // %APPDATA%\WaveEdit\WaveEdit.log on Windows,
        // ~/.config/WaveEdit/WaveEdit.log on Linux.
        m_fileLogger.reset(juce::FileLogger::createDefaultAppLogger(
            "WaveEdit",
            "WaveEdit.log",
            "WaveEdit " + getApplicationVersion() + " session log"));
        juce::Logger::setCurrentLogger(m_fileLogger.get());
        juce::Logger::writeToLog("WaveEdit " + getApplicationVersion() + " starting up");

        // Set custom LookAndFeel with WCAG-compliant focus indicators
        m_lookAndFeel = std::make_unique<waveedit::WaveEditLookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel(m_lookAndFeel.get());

        // Initialize audio device manager
        juce::String audioError = m_audioDeviceManager.initialise(
            2,      // number of input channels
            2,      // number of output channels
            nullptr, // saved state
            true);   // select default device

        if (audioError.isNotEmpty())
        {
            juce::Logger::writeToLog("Audio initialization error: " + audioError);
        }

        // Create main window
        mainWindow.reset(new MainWindow(getApplicationName(), m_audioDeviceManager));
    }

    void shutdown() override
    {
        // Clean up on exit
        mainWindow = nullptr;

        // Detach the file logger before destroying it.
        juce::Logger::writeToLog("WaveEdit shutting down cleanly");
        juce::Logger::setCurrentLogger(nullptr);
        m_fileLogger.reset();
    }

    void systemRequestedQuit() override
    {
        // Check for unsaved changes in any document
        if (auto* mainComp = dynamic_cast<MainComponent*>(mainWindow->getContentComponent()))
        {
            if (mainComp->hasUnsavedChanges())
            {
                // Show warning and get user choice
                int result = juce::NativeMessageBox::showYesNoCancelBox(
                    juce::MessageBoxIconType::WarningIcon,
                    "Unsaved Changes",
                    "You have unsaved changes. Do you want to save before quitting?",
                    nullptr,
                    juce::ModalCallbackFunction::create([this, mainComp](int choice)
                    {
                        if (choice == 1)  // Yes - Save and quit
                        {
                            if (mainComp->saveAllModifiedDocuments())
                            {
                                quit();
                            }
                            // If save failed, don't quit
                        }
                        else if (choice == 2)  // No - Quit without saving
                        {
                            quit();
                        }
                        // Cancel (0) - Do nothing
                    })
                );
                return;  // Don't quit immediately - let callback handle it
            }
        }

        // No unsaved changes, quit immediately
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override
    {
        // Another instance was started (if allowed)
        // In future, this could open the file in a new window
    }

    /**
     * Main application window.
     */
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name, juce::AudioDeviceManager& deviceManager)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);

            // Create main component with audio device manager
            auto* mainComp = new MainComponent(deviceManager);
            setContentOwned(mainComp, true);

            // Set up menu bar
           #if JUCE_MAC
            setMenuBar(mainComp);
           #else
            setMenuBar(mainComp, 30);
           #endif

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
           #else
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
           #endif

            setVisible(true);

            // Create tooltip window for toolbar buttons and other UI elements
            m_tooltipWindow = std::make_unique<juce::TooltipWindow>(this, 500);
        }

        ~MainWindow() override
        {
            // Clear menu bar before deleting content
            setMenuBar(nullptr);
        }

        void closeButtonPressed() override
        {
            // Handle close button - in future, check for unsaved changes
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        std::unique_ptr<juce::TooltipWindow> m_tooltipWindow;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<waveedit::WaveEditLookAndFeel> m_lookAndFeel;
    juce::AudioDeviceManager m_audioDeviceManager;
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<juce::FileLogger> m_fileLogger;
};

//==============================================================================
// Custom main() to support out-of-process plugin scanning.
// When launched with --waveedit-plugin-scanner, we run as a scanner subprocess
// instead of the main GUI application. This is critical for crash isolation.
//
// IMPORTANT: The scanner worker runs BEFORE JUCE is initialized, so we must
// pass the command line directly - it cannot use JUCEApplication methods.
//
// When defining custom main(), we must also set up the createInstance pointer
// that JUCE uses to create the application instance. The START_JUCE_APPLICATION
// macro normally does this, but since we're providing our own main(), we need
// to do it manually.

// Forward declaration of the application factory function
static juce::JUCEApplicationBase* juce_CreateApplication() { return new WaveEditApplication(); }

// CLI flag handling shared across platforms. Returns true if a flag was
// handled and main() should exit without launching the GUI.
static bool handleCommandLineFlags(const juce::String& commandLine, int& exitCode)
{
    auto trimmed = commandLine.trim();
    if (trimmed.startsWithIgnoreCase("--help") || trimmed.startsWithIgnoreCase("-h")
        || trimmed == "/?" || trimmed == "help")
    {
        std::printf(
            "WaveEdit - Professional Audio Editor\n"
            "\n"
            "Usage: WaveEdit [options] [file.wav]\n"
            "\n"
            "Options:\n"
            "  -h, --help       Show this help message and exit\n"
            "  -v, --version    Print the version and exit\n"
            "\n"
            "Without arguments, WaveEdit launches its GUI. Pass a path to a\n"
            "WAV/FLAC/OGG/MP3 file to open it on launch.\n"
            "\n"
            "See https://github.com/themightyzq/WaveEdit for more.\n");
        exitCode = 0;
        return true;
    }
    if (trimmed.startsWithIgnoreCase("--version") || trimmed.startsWithIgnoreCase("-v"))
    {
        std::printf("WaveEdit 0.1.0\n");
        exitCode = 0;
        return true;
    }
    return false;
}

#if JUCE_MAC
extern "C" int main(int argc, char* argv[])
{
    // Build command line string for potential worker use
    juce::String commandLine;
    for (int i = 1; i < argc; ++i)
    {
        if (i > 1)
            commandLine += " ";
        commandLine += juce::String(argv[i]);
    }

    // Check if we're being launched as a plugin scanner worker
    if (commandLine.contains(PluginScannerProtocol::kWorkerProcessArg))
    {
        // Run as scanner worker - no GUI, just IPC
        // Pass command line since JUCE isn't initialized yet
        return runPluginScannerWorker(commandLine);
    }

    // Handle --help / --version before any GUI initialization.
    int cliExit = 0;
    if (handleCommandLineFlags(commandLine, cliExit))
        return cliExit;

    // Set up the application factory - required when using custom main()
    juce::JUCEApplicationBase::createInstance = &juce_CreateApplication;

    // Normal application launch
    return juce::JUCEApplicationBase::main(argc, const_cast<const char**>(argv));
}

#elif JUCE_WINDOWS
// Use JUCE's recommended WinMain signature with proper Windows types
// Note: JUCE_BEGIN_IGNORE_WARNINGS_MSVC/END suppress warning 28251 about WinMain annotation
JUCE_BEGIN_IGNORE_WARNINGS_MSVC (28251)
extern "C" int __stdcall WinMain(struct HINSTANCE__*, struct HINSTANCE__*, char* cmdLine, int)
JUCE_END_IGNORE_WARNINGS_MSVC
{
    juce::String commandLine(cmdLine);

    // Check if we're being launched as a plugin scanner worker
    if (commandLine.contains(PluginScannerProtocol::kWorkerProcessArg))
    {
        // Run as scanner worker - no GUI, just IPC
        // Pass command line since JUCE isn't initialized yet
        return runPluginScannerWorker(commandLine);
    }

    // Handle --help / --version before any GUI initialization.
    int cliExit = 0;
    if (handleCommandLineFlags(commandLine, cliExit))
        return cliExit;

    // Set up the application factory - required when using custom main()
    juce::JUCEApplicationBase::createInstance = &juce_CreateApplication;

    // Normal application launch - on Windows, JUCEApplicationBase::main() takes no args
    // (JUCE internally handles command line via GetCommandLine() on Windows)
    return juce::JUCEApplicationBase::main();
}

#elif JUCE_LINUX
extern "C" int main(int argc, char* argv[])
{
    // Build command line string for potential worker use
    juce::String commandLine;
    for (int i = 1; i < argc; ++i)
    {
        if (i > 1)
            commandLine += " ";
        commandLine += juce::String(argv[i]);
    }

    // Check if we're being launched as a plugin scanner worker
    if (commandLine.contains(PluginScannerProtocol::kWorkerProcessArg))
    {
        // Run as scanner worker - no GUI, just IPC
        // Pass command line since JUCE isn't initialized yet
        return runPluginScannerWorker(commandLine);
    }

    // Handle --help / --version before any GUI initialization.
    int cliExit = 0;
    if (handleCommandLineFlags(commandLine, cliExit))
        return cliExit;

    // Set up the application factory - required when using custom main()
    juce::JUCEApplicationBase::createInstance = &juce_CreateApplication;

    // Normal application launch
    return juce::JUCEApplicationBase::main(argc, const_cast<const char**>(argv));
}
#endif
