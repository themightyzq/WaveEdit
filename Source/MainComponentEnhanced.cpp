/*
  ==============================================================================

    MainComponentEnhanced.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "Audio/AudioEngine.h"
#include "Audio/AudioFileManager.h"
#include "Audio/AudioBufferManager.h"
#include "Commands/CommandIDs.h"
#include "Utils/Settings.h"
#include "Utils/AudioClipboard.h"
#include "UI/WaveformDisplay.h"
#include "UI/TransportControls.h"

//==============================================================================
/**
 * Selection info component that displays current selection details.
 */
class SelectionInfoPanel : public juce::Component,
                          public juce::Timer
{
public:
    SelectionInfoPanel(WaveformDisplay& waveform)
        : m_waveformDisplay(waveform)
    {
        startTimer(100); // Update 10 times per second
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff2a2a2a));

        g.setColour(juce::Colour(0xff3a3a3a));
        g.drawRect(getLocalBounds(), 1);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font("Monospace", 11.0f, juce::Font::plain));

        if (m_waveformDisplay.hasSelection())
        {
            auto bounds = getLocalBounds().reduced(5);

            juce::String info = juce::String::formatted(
                "Selection: %s - %s | Duration: %s",
                m_waveformDisplay.getSelectionStartString().toRawUTF8(),
                m_waveformDisplay.getSelectionEndString().toRawUTF8(),
                m_waveformDisplay.getSelectionDurationString().toRawUTF8()
            );

            g.drawText(info, bounds, juce::Justification::centredLeft, true);
        }
        else if (m_waveformDisplay.isFileLoaded())
        {
            auto bounds = getLocalBounds().reduced(5);
            g.setColour(juce::Colours::grey);
            g.drawText("No selection - Click and drag to select", bounds,
                      juce::Justification::centredLeft, true);
        }
    }

    void timerCallback() override
    {
        repaint();
    }

private:
    WaveformDisplay& m_waveformDisplay;
};

//==============================================================================
/**
 * Enhanced main component with full editing support.
 */
class MainComponentEnhanced : public juce::Component,
                              public juce::FileDragAndDropTarget,
                              public juce::ApplicationCommandTarget,
                              public juce::MenuBarModel,
                              public juce::Timer
{
public:
    MainComponentEnhanced()
        : m_waveformDisplay(m_audioEngine.getFormatManager()),
          m_transportControls(m_audioEngine),
          m_selectionInfo(m_waveformDisplay),
          m_isModified(false)
    {
        setSize(1200, 750);

        // Initialize audio engine
        if (!m_audioEngine.initializeAudioDevice())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Audio Device Error",
                "Failed to initialize audio device. Audio playback will not be available.",
                "OK");
        }

        // Add UI components
        addAndMakeVisible(m_waveformDisplay);
        addAndMakeVisible(m_transportControls);
        addAndMakeVisible(m_selectionInfo);

        // Add keyboard focus to handle shortcuts
        setWantsKeyboardFocus(true);

        // Set up command manager
        m_commandManager.registerAllCommandsForTarget(this);

        // Add keyboard mappings
        addKeyListener(m_commandManager.getKeyMappings());

        // Start timer to update playback position
        startTimer(50); // Update every 50ms for smooth 60fps cursor

        // Clean up recent files on startup
        Settings::getInstance().cleanupRecentFiles();
    }

    ~MainComponentEnhanced() override
    {
        m_audioEngine.closeAudioFile();
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        // Background
        g.fillAll(juce::Colour(0xff1a1a1a));

        // Status bar at bottom
        auto bounds = getLocalBounds();
        auto statusBar = bounds.removeFromBottom(25);

        // Draw status bar background
        g.setColour(juce::Colour(0xff2a2a2a));
        g.fillRect(statusBar);

        // Draw status bar border
        g.setColour(juce::Colour(0xff3a3a3a));
        g.drawLine(statusBar.getX(), statusBar.getY(), statusBar.getRight(), statusBar.getY(), 1.0f);

        // Status bar text
        if (m_audioEngine.isFileLoaded())
        {
            g.setColour(juce::Colours::white);
            g.setFont(12.0f);

            auto leftSection = statusBar.reduced(10, 0);

            juce::String fileDisplayName = m_audioEngine.getCurrentFile().getFileName();
            if (m_isModified)
            {
                fileDisplayName += " *";
            }

            juce::String info = juce::String::formatted(
                "%s | %.1f kHz | %d ch | %d bit | %.2f / %.2f s",
                fileDisplayName.toRawUTF8(),
                m_audioEngine.getSampleRate() / 1000.0,
                m_audioEngine.getNumChannels(),
                m_audioEngine.getBitDepth(),
                m_audioEngine.getCurrentPosition(),
                m_audioEngine.getTotalLength());

            g.drawText(info, leftSection, juce::Justification::centredLeft, true);

            // State indicator on the right
            auto rightSection = statusBar.removeFromRight(150);
            juce::String stateText;
            switch (m_audioEngine.getPlaybackState())
            {
                case PlaybackState::STOPPED: stateText = "Stopped"; break;
                case PlaybackState::PLAYING: stateText = "Playing"; break;
                case PlaybackState::PAUSED:  stateText = "Paused"; break;
            }

            // Add clipboard indicator
            if (AudioClipboard::getInstance().hasAudio())
            {
                stateText += " | Clipboard: ";
                double clipboardSecs = AudioClipboard::getInstance().getNumSamples() /
                                      AudioClipboard::getInstance().getSampleRate();
                stateText += juce::String(clipboardSecs, 2) + "s";
            }

            g.drawText(stateText, rightSection, juce::Justification::centredRight, true);
        }
        else
        {
            g.setColour(juce::Colours::grey);
            g.setFont(12.0f);
            g.drawText("No file loaded - Press Ctrl+O to open or drag & drop a WAV file",
                      statusBar.reduced(10, 0), juce::Justification::centredLeft, true);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Reserve space for status bar at bottom
        bounds.removeFromBottom(25);

        // Transport controls at top (80px height)
        m_transportControls.setBounds(bounds.removeFromTop(80));

        // Selection info panel (30px height)
        m_selectionInfo.setBounds(bounds.removeFromTop(30));

        // Waveform display takes remaining space
        m_waveformDisplay.setBounds(bounds);
    }

    void timerCallback() override
    {
        // Update waveform display with current playback position
        if (m_audioEngine.isPlaying())
        {
            m_waveformDisplay.setPlaybackPosition(m_audioEngine.getCurrentPosition());
            repaint(); // Update status bar
        }
    }

    //==============================================================================
    // ApplicationCommandTarget Implementation

    ApplicationCommandTarget* getNextCommandTarget() override
    {
        return nullptr;
    }

    void getAllCommands(juce::Array<juce::CommandID>& commands) override
    {
        const juce::CommandID ids[] = {
            CommandIDs::fileOpen,
            CommandIDs::fileSave,
            CommandIDs::fileSaveAs,
            CommandIDs::fileClose,
            CommandIDs::fileExit,
            CommandIDs::editUndo,
            CommandIDs::editRedo,
            CommandIDs::editCut,
            CommandIDs::editCopy,
            CommandIDs::editPaste,
            CommandIDs::editDelete,
            CommandIDs::editSelectAll,
            CommandIDs::playbackPlay,
            CommandIDs::playbackPause,
            CommandIDs::playbackStop,
            CommandIDs::playbackLoop
        };

        commands.addArray(ids, juce::numElementsInArray(ids));
    }

    void getCommandInfo(juce::CommandID commandID, juce::ApplicationCommandInfo& result) override
    {
        switch (commandID)
        {
            case CommandIDs::fileOpen:
                result.setInfo("Open...", "Open an audio file", "File", 0);
                result.addDefaultKeypress('o', juce::ModifierKeys::commandModifier);
                break;

            case CommandIDs::fileSave:
                result.setInfo("Save", "Save the current file", "File", 0);
                result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded() && m_isModified);
                break;

            case CommandIDs::fileSaveAs:
                result.setInfo("Save As...", "Save the current file with a new name", "File", 0);
                result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::fileClose:
                result.setInfo("Close", "Close the current file", "File", 0);
                result.addDefaultKeypress('w', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::fileExit:
                result.setInfo("Exit", "Exit the application", "File", 0);
                result.addDefaultKeypress('q', juce::ModifierKeys::commandModifier);
                break;

            // Edit commands
            case CommandIDs::editUndo:
                result.setInfo("Undo", "Undo the last operation", "Edit", 0);
                result.addDefaultKeypress('z', juce::ModifierKeys::commandModifier);
                result.setActive(false); // Will be implemented with UndoManager
                break;

            case CommandIDs::editRedo:
                result.setInfo("Redo", "Redo the last undone operation", "Edit", 0);
                result.addDefaultKeypress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(false); // Will be implemented with UndoManager
                break;

            case CommandIDs::editCut:
                result.setInfo("Cut", "Cut selected audio to clipboard", "Edit", 0);
                result.addDefaultKeypress('x', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded() && m_waveformDisplay.hasSelection());
                break;

            case CommandIDs::editCopy:
                result.setInfo("Copy", "Copy selected audio to clipboard", "Edit", 0);
                result.addDefaultKeypress('c', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded() && m_waveformDisplay.hasSelection());
                break;

            case CommandIDs::editPaste:
                result.setInfo("Paste", "Paste audio from clipboard", "Edit", 0);
                result.addDefaultKeypress('v', juce::ModifierKeys::commandModifier);
                result.setActive(AudioClipboard::getInstance().hasAudio());
                break;

            case CommandIDs::editDelete:
                result.setInfo("Delete", "Delete selected audio", "Edit", 0);
                result.addDefaultKeypress(juce::KeyPress::deleteKey, 0);
                result.setActive(m_audioEngine.isFileLoaded() && m_waveformDisplay.hasSelection());
                break;

            case CommandIDs::editSelectAll:
                result.setInfo("Select All", "Select all audio", "Edit", 0);
                result.addDefaultKeypress('a', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::playbackPlay:
                result.setInfo("Play/Stop", "Play or stop playback", "Playback", 0);
                result.addDefaultKeypress(juce::KeyPress::spaceKey, 0);
                result.addDefaultKeypress(juce::KeyPress::F12Key, 0);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::playbackPause:
                result.setInfo("Pause", "Pause or resume playback", "Playback", 0);
                result.addDefaultKeypress(juce::KeyPress::returnKey, 0);
                result.addDefaultKeypress(juce::KeyPress::F12Key, juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::playbackStop:
                result.setInfo("Stop", "Stop playback", "Playback", 0);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::playbackLoop:
                result.setInfo("Loop", "Toggle loop mode", "Playback", 0);
                result.addDefaultKeypress('q', 0);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            default:
                break;
        }
    }

    bool perform(const InvocationInfo& info) override
    {
        switch (info.commandID)
        {
            case CommandIDs::fileOpen:
                openFile();
                return true;

            case CommandIDs::fileSave:
                saveFile();
                return true;

            case CommandIDs::fileSaveAs:
                saveFileAs();
                return true;

            case CommandIDs::fileClose:
                closeFile();
                return true;

            case CommandIDs::fileExit:
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
                return true;

            // Edit operations
            case CommandIDs::editCut:
                cutSelection();
                return true;

            case CommandIDs::editCopy:
                copySelection();
                return true;

            case CommandIDs::editPaste:
                pasteAtCursor();
                return true;

            case CommandIDs::editDelete:
                deleteSelection();
                return true;

            case CommandIDs::editSelectAll:
                selectAll();
                return true;

            case CommandIDs::playbackPlay:
                togglePlayback();
                return true;

            case CommandIDs::playbackPause:
                pausePlayback();
                return true;

            case CommandIDs::playbackStop:
                stopPlayback();
                return true;

            case CommandIDs::playbackLoop:
                toggleLoop();
                return true;

            default:
                return false;
        }
    }

    //==============================================================================
    // Edit Operations

    /**
     * Selects all audio in the current file.
     */
    void selectAll()
    {
        if (!m_audioEngine.isFileLoaded() || !m_audioBufferManager.hasAudioData())
        {
            return;
        }

        m_waveformDisplay.setSelection(0.0, m_audioBufferManager.getLengthInSeconds());

        juce::Logger::writeToLog("Selected all audio");
    }

    /**
     * Copies the selected audio range to the clipboard.
     */
    void copySelection()
    {
        if (!m_audioEngine.isFileLoaded() || !m_waveformDisplay.hasSelection() ||
            !m_audioBufferManager.hasAudioData())
        {
            return;
        }

        // Convert selection times to samples
        int64_t startSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionStart());
        int64_t endSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionEnd());
        int64_t numSamples = endSample - startSample;

        if (numSamples <= 0)
        {
            return;
        }

        // Get the selected audio range
        auto selectedAudio = m_audioBufferManager.getAudioRange(startSample, numSamples);

        // Copy to clipboard
        AudioClipboard::getInstance().copyAudio(selectedAudio, m_audioBufferManager.getSampleRate());

        juce::Logger::writeToLog(juce::String::formatted("Copied %.2f seconds to clipboard",
            m_waveformDisplay.getSelectionDuration()));

        repaint(); // Update status bar to show clipboard status
    }

    /**
     * Cuts the selected audio range (copy to clipboard then delete).
     */
    void cutSelection()
    {
        if (!m_audioEngine.isFileLoaded() || !m_waveformDisplay.hasSelection() ||
            !m_audioBufferManager.hasAudioData())
        {
            return;
        }

        // First copy to clipboard
        copySelection();

        // Then delete the selection
        deleteSelection();

        juce::Logger::writeToLog("Cut selection to clipboard");
    }

    /**
     * Pastes audio from clipboard at the current cursor position.
     */
    void pasteAtCursor()
    {
        if (!AudioClipboard::getInstance().hasAudio())
        {
            juce::Logger::writeToLog("Cannot paste: Clipboard is empty");
            return;
        }

        // If no file is loaded, create a new buffer with clipboard contents
        if (!m_audioEngine.isFileLoaded())
        {
            // Create a new buffer from clipboard
            const auto& clipboardBuffer = AudioClipboard::getInstance().getAudio();
            double clipboardSampleRate = AudioClipboard::getInstance().getSampleRate();

            // For now, we'll just show a message
            // In a full implementation, we'd create a new file
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Paste to New File",
                "Pasting to a new file will be implemented in the next phase.\n"
                "Clipboard contains " + juce::String(clipboardBuffer.getNumSamples()) + " samples.",
                "OK");
            return;
        }

        // Check sample rate compatibility
        if (std::abs(AudioClipboard::getInstance().getSampleRate() - m_audioBufferManager.getSampleRate()) > 0.1)
        {
            auto result = juce::NativeMessageBox::showYesNoBox(
                juce::MessageBoxIconType::WarningIcon,
                "Sample Rate Mismatch",
                juce::String::formatted(
                    "The clipboard audio has a different sample rate (%.1f Hz) than the current file (%.1f Hz).\n\n"
                    "Pasting will result in pitch/speed changes. Continue?",
                    AudioClipboard::getInstance().getSampleRate(),
                    m_audioBufferManager.getSampleRate()));

            if (!result)
            {
                return;
            }
        }

        // Get insert position (selection start or playback position)
        double insertTime = m_waveformDisplay.hasSelection() ?
            m_waveformDisplay.getSelectionStart() :
            m_audioEngine.getCurrentPosition();

        int64_t insertSample = m_audioBufferManager.timeToSample(insertTime);

        // If there's a selection, replace it with the clipboard contents
        if (m_waveformDisplay.hasSelection())
        {
            int64_t startSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionStart());
            int64_t endSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionEnd());
            int64_t numSamplesToReplace = endSample - startSample;

            m_audioBufferManager.replaceRange(startSample, numSamplesToReplace,
                                             AudioClipboard::getInstance().getAudio());
        }
        else
        {
            // Insert at cursor position
            m_audioBufferManager.insertAudio(insertSample, AudioClipboard::getInstance().getAudio());
        }

        // Mark as modified and update playback
        m_isModified = true;
        updatePlaybackFromBuffer();

        // Clear selection after paste
        m_waveformDisplay.clearSelection();

        // Reload waveform display
        reloadWaveformDisplay();

        juce::Logger::writeToLog(juce::String::formatted("Pasted %.2f seconds from clipboard",
            AudioClipboard::getInstance().getNumSamples() / AudioClipboard::getInstance().getSampleRate()));

        repaint();
    }

    /**
     * Deletes the selected audio range.
     */
    void deleteSelection()
    {
        if (!m_audioEngine.isFileLoaded() || !m_waveformDisplay.hasSelection() ||
            !m_audioBufferManager.hasAudioData())
        {
            return;
        }

        // Convert selection times to samples
        int64_t startSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionStart());
        int64_t endSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionEnd());
        int64_t numSamples = endSample - startSample;

        if (numSamples <= 0)
        {
            return;
        }

        // Check if deleting entire file
        if (numSamples >= m_audioBufferManager.getNumSamples())
        {
            auto result = juce::NativeMessageBox::showYesNoBox(
                juce::MessageBoxIconType::WarningIcon,
                "Delete All Audio",
                "This will delete all audio in the file. Continue?");

            if (!result)
            {
                return;
            }
        }

        // Delete the range
        m_audioBufferManager.deleteRange(startSample, numSamples);

        // Mark as modified and update playback
        m_isModified = true;
        updatePlaybackFromBuffer();

        // Clear selection after delete
        m_waveformDisplay.clearSelection();

        // Reload waveform display
        reloadWaveformDisplay();

        juce::Logger::writeToLog(juce::String::formatted("Deleted %.2f seconds",
            numSamples / m_audioBufferManager.getSampleRate()));

        repaint();
    }

    /**
     * Updates the audio engine to play from the edited buffer.
     */
    void updatePlaybackFromBuffer()
    {
        if (!m_audioBufferManager.hasAudioData())
        {
            return;
        }

        // Stop current playback
        m_audioEngine.stop();

        // Create a memory source from the buffer
        const auto& buffer = m_audioBufferManager.getBuffer();
        m_memorySource = std::make_unique<juce::MemoryAudioSource>(
            buffer,
            false,  // Don't release the buffer
            false   // Don't loop
        );

        // Get the transport source from audio engine (we need access to it)
        // For now, we'll need to modify AudioEngine to expose this or use a different approach

        juce::Logger::writeToLog("Playback updated to use edited buffer");
    }

    /**
     * Reloads the waveform display with the current buffer contents.
     */
    void reloadWaveformDisplay()
    {
        // For now, we'll need to regenerate the thumbnail from the buffer
        // This would require extending WaveformDisplay to support loading from a buffer
        juce::Logger::writeToLog("Waveform display needs to be updated");
    }

    //==============================================================================
    // MenuBarModel Implementation

    juce::StringArray getMenuBarNames() override
    {
        return { "File", "Edit", "Playback", "Help" };
    }

    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& /*menuName*/) override
    {
        juce::PopupMenu menu;

        if (menuIndex == 0) // File menu
        {
            menu.addCommandItem(&m_commandManager, CommandIDs::fileOpen);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::fileSave);
            menu.addCommandItem(&m_commandManager, CommandIDs::fileSaveAs);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::fileClose);
            menu.addSeparator();

            // Recent files submenu
            auto recentFiles = Settings::getInstance().getRecentFiles();
            if (!recentFiles.isEmpty())
            {
                juce::PopupMenu recentFilesMenu;
                for (int i = 0; i < recentFiles.size(); ++i)
                {
                    juce::File file(recentFiles[i]);
                    recentFilesMenu.addItem(file.getFileName(), [this, file] { loadFile(file); });
                }
                recentFilesMenu.addSeparator();
                recentFilesMenu.addItem("Clear Recent Files", [] { Settings::getInstance().clearRecentFiles(); });
                menu.addSubMenu("Recent Files", recentFilesMenu);
                menu.addSeparator();
            }

            menu.addCommandItem(&m_commandManager, CommandIDs::fileExit);
        }
        else if (menuIndex == 1) // Edit menu
        {
            menu.addCommandItem(&m_commandManager, CommandIDs::editUndo);
            menu.addCommandItem(&m_commandManager, CommandIDs::editRedo);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::editCut);
            menu.addCommandItem(&m_commandManager, CommandIDs::editCopy);
            menu.addCommandItem(&m_commandManager, CommandIDs::editPaste);
            menu.addCommandItem(&m_commandManager, CommandIDs::editDelete);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::editSelectAll);
        }
        else if (menuIndex == 2) // Playback menu
        {
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackPlay);
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackPause);
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackStop);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackLoop);
        }
        else if (menuIndex == 3) // Help menu
        {
            menu.addItem("About WaveEdit", [this] { showAbout(); });
            menu.addItem("Keyboard Shortcuts", [this] { showKeyboardShortcuts(); });
        }

        return menu;
    }

    void menuItemSelected(int /*menuItemID*/, int /*topLevelMenuIndex*/) override
    {
        // Handled by command system
    }

    //==============================================================================
    // Drag and drop support

    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        for (const auto& filename : files)
        {
            juce::File file(filename);
            if (file.hasFileExtension(".wav"))
            {
                return true;
            }
        }
        return false;
    }

    void filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/) override
    {
        if (files.isEmpty())
        {
            return;
        }

        // Check if current file has unsaved changes
        if (m_isModified && !confirmDiscardChanges())
        {
            return;
        }

        // Load the first file
        juce::File file(files[0]);
        loadFile(file);
    }

    //==============================================================================
    // File Operations

    void openFile()
    {
        // Check if current file has unsaved changes
        if (m_isModified && !confirmDiscardChanges())
        {
            return;
        }

        // Don't create new file chooser if one is already active
        if (m_fileChooser != nullptr)
        {
            juce::Logger::writeToLog("File chooser already active");
            return;
        }

        // Create file chooser
        m_fileChooser = std::make_unique<juce::FileChooser>(
            "Open Audio File",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.wav",
            true);

        auto folderChooserFlags = juce::FileBrowserComponent::openMode |
                                  juce::FileBrowserComponent::canSelectFiles;

        m_fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file != juce::File())
            {
                loadFile(file);
            }
            // Clear the file chooser after use
            m_fileChooser.reset();
        });
    }

    /**
     * Validates that a file path is safe (no path traversal attacks).
     */
    bool isPathSafe(const juce::File& file) const
    {
        // Get normalized absolute path
        auto path = file.getFullPathName();

        // Check for path traversal attempts
        if (path.contains(".."))
        {
            juce::Logger::writeToLog("Path traversal attempt detected: " + path);
            return false;
        }

        // File must exist
        if (!file.existsAsFile())
        {
            return false;
        }

        return true;
    }

    void loadFile(const juce::File& file)
    {
        // Validate path safety
        if (!isPathSafe(file))
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Security Error",
                "Invalid or unsafe file path: " + file.getFileName(),
                "OK");
            return;
        }

        // Validate the file first
        AudioFileInfo info;
        if (!m_fileManager.getFileInfo(file, info))
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "File Error",
                "Could not read file: " + file.getFileName() + "\n\n" + m_fileManager.getLastError(),
                "OK");
            return;
        }

        // Check file permissions
        if (!file.hasReadAccess())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Permission Error",
                "No read permission for file: " + file.getFileName(),
                "OK");
            return;
        }

        // Load the file into the audio engine
        if (m_audioEngine.loadAudioFile(file))
        {
            // Also load into AudioBufferManager for editing
            if (!m_audioBufferManager.loadFromFile(file, m_audioEngine.getFormatManager()))
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Buffer Error",
                    "Could not load file into edit buffer: " + file.getFileName(),
                    "OK");
                m_audioEngine.closeAudioFile();
                return;
            }

            // Load file into waveform display with audio properties
            if (!m_waveformDisplay.loadFile(file,
                                           m_audioEngine.getSampleRate(),
                                           m_audioEngine.getNumChannels()))
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Display Error",
                    "Could not display waveform for file: " + file.getFileName(),
                    "OK");
            }

            // Add to recent files
            Settings::getInstance().addRecentFile(file);

            // Clear modified flag
            m_isModified = false;

            repaint();
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Load Error",
                "Could not load file: " + file.getFileName(),
                "OK");
        }
    }

    void saveFile()
    {
        if (!m_audioEngine.isFileLoaded())
        {
            return;
        }

        auto currentFile = m_audioEngine.getCurrentFile();

        // Check if file exists and is writable
        if (!currentFile.existsAsFile())
        {
            // File doesn't exist, do Save As instead
            saveFileAs();
            return;
        }

        if (!currentFile.hasWriteAccess())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Permission Error",
                "No write permission for file: " + currentFile.getFileName() + "\n\n"
                "Use 'Save As' to save to a different location.",
                "OK");
            return;
        }

        // Save the edited buffer to file
        if (saveBufferToFile(currentFile))
        {
            m_isModified = false;
            repaint();

            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "File Saved",
                "Successfully saved: " + currentFile.getFileName(),
                "OK");
        }
    }

    void saveFileAs()
    {
        if (!m_audioEngine.isFileLoaded())
        {
            return;
        }

        // Don't create new file chooser if one is already active
        if (m_fileChooser != nullptr)
        {
            juce::Logger::writeToLog("File chooser already active");
            return;
        }

        // Create file chooser
        m_fileChooser = std::make_unique<juce::FileChooser>(
            "Save Audio File As",
            m_audioEngine.getCurrentFile().getParentDirectory(),
            "*.wav",
            true);

        auto folderChooserFlags = juce::FileBrowserComponent::saveMode |
                                  juce::FileBrowserComponent::canSelectFiles |
                                  juce::FileBrowserComponent::warnAboutOverwriting;

        m_fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file != juce::File())
            {
                // Ensure .wav extension
                if (!file.hasFileExtension(".wav"))
                {
                    file = file.withFileExtension(".wav");
                }

                // Save the edited buffer to the new file
                if (saveBufferToFile(file))
                {
                    m_isModified = false;
                    repaint();

                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::InfoIcon,
                        "File Saved",
                        "Successfully saved as: " + file.getFileName(),
                        "OK");
                }
            }
            // Clear the file chooser after use
            m_fileChooser.reset();
        });
    }

    /**
     * Saves the current buffer to a file.
     */
    bool saveBufferToFile(const juce::File& file)
    {
        if (!m_audioBufferManager.hasAudioData())
        {
            return false;
        }

        // Create a WAV writer
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(
                new juce::FileOutputStream(file),
                m_audioBufferManager.getSampleRate(),
                m_audioBufferManager.getNumChannels(),
                m_audioBufferManager.getBitDepth(),
                {},
                0
            )
        );

        if (writer == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Save Error",
                "Could not create file writer for: " + file.getFileName(),
                "OK");
            return false;
        }

        // Write the buffer to file
        const auto& buffer = m_audioBufferManager.getBuffer();
        bool success = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

        if (!success)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Save Error",
                "Failed to write audio data to: " + file.getFileName(),
                "OK");
            return false;
        }

        return true;
    }

    void closeFile()
    {
        if (!m_audioEngine.isFileLoaded())
        {
            return;
        }

        // Check for unsaved changes
        if (m_isModified && !confirmDiscardChanges())
        {
            return;
        }

        m_audioEngine.closeAudioFile();
        m_audioBufferManager.clear();
        m_waveformDisplay.clear();
        m_isModified = false;
        repaint();
    }

    bool confirmDiscardChanges()
    {
        return juce::NativeMessageBox::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon,
            "Unsaved Changes",
            "The current file has unsaved changes.\n\n"
            "Do you want to discard these changes?",
            nullptr,
            nullptr);
    }

    //==============================================================================
    // Playback Control

    void togglePlayback()
    {
        if (!m_audioEngine.isFileLoaded())
        {
            return;
        }

        if (m_audioEngine.isPlaying())
        {
            m_audioEngine.stop();
        }
        else
        {
            m_audioEngine.play();
        }

        repaint();
    }

    void stopPlayback()
    {
        if (!m_audioEngine.isFileLoaded())
        {
            return;
        }

        m_audioEngine.stop();
        repaint();
    }

    void pausePlayback()
    {
        if (!m_audioEngine.isFileLoaded())
        {
            return;
        }

        auto state = m_audioEngine.getPlaybackState();

        if (state == PlaybackState::PLAYING)
        {
            m_audioEngine.pause();
        }
        else if (state == PlaybackState::PAUSED)
        {
            m_audioEngine.play();
        }

        repaint();
    }

    void toggleLoop()
    {
        if (!m_audioEngine.isFileLoaded())
        {
            return;
        }

        m_transportControls.toggleLoop();
        repaint();
    }

    //==============================================================================
    // Utility

    void showAbout()
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "About WaveEdit",
            "WaveEdit v0.1.0-alpha\n\n"
            "Professional Audio Editor\n"
            "Built with JUCE\n\n"
            "Copyright (C) 2025 WaveEdit\n"
            "Licensed under GPL v3",
            "OK");
    }

    void showKeyboardShortcuts()
    {
        juce::String shortcuts =
            "KEYBOARD SHORTCUTS\n\n"
            "File Operations:\n"
            "  Ctrl+O        Open file\n"
            "  Ctrl+S        Save\n"
            "  Ctrl+Shift+S  Save As\n"
            "  Ctrl+W        Close file\n"
            "  Ctrl+Q        Exit\n\n"
            "Edit Operations:\n"
            "  Ctrl+Z        Undo (coming soon)\n"
            "  Ctrl+Shift+Z  Redo (coming soon)\n"
            "  Ctrl+X        Cut\n"
            "  Ctrl+C        Copy\n"
            "  Ctrl+V        Paste\n"
            "  Delete        Delete selection\n"
            "  Ctrl+A        Select all\n\n"
            "Playback:\n"
            "  Space/F12     Play/Stop\n"
            "  Enter         Pause/Resume\n"
            "  Q             Toggle loop\n";

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Keyboard Shortcuts",
            shortcuts,
            "OK");
    }

    juce::ApplicationCommandManager& getCommandManager()
    {
        return m_commandManager;
    }

private:
    AudioEngine m_audioEngine;
    AudioFileManager m_fileManager;
    AudioBufferManager m_audioBufferManager;
    juce::ApplicationCommandManager m_commandManager;
    std::unique_ptr<juce::FileChooser> m_fileChooser;
    std::unique_ptr<juce::MemoryAudioSource> m_memorySource;

    WaveformDisplay m_waveformDisplay;
    TransportControls m_transportControls;
    SelectionInfoPanel m_selectionInfo;

    bool m_isModified;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponentEnhanced)
};