/*
  ==============================================================================

    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

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

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "Audio/AudioEngine.h"
#include "Audio/AudioFileManager.h"
#include "Audio/AudioBufferManager.h"
#include "Audio/AudioProcessor.h"
#include "Commands/CommandIDs.h"
#include "Utils/Settings.h"
#include "Utils/AudioClipboard.h"
#include "Utils/UndoableEdits.h"
#include "UI/WaveformDisplay.h"
#include "UI/TransportControls.h"
#include "UI/Meters.h"
#include "UI/GainDialog.h"

//==============================================================================
/**
 * Selection info component that displays current selection details.
 * Shows both time-based and sample-based positions for precision.
 */
class SelectionInfoPanel : public juce::Component,
                          public juce::Timer
{
public:
    SelectionInfoPanel(WaveformDisplay& waveform, AudioBufferManager& bufferManager)
        : m_waveformDisplay(waveform),
          m_bufferManager(bufferManager)
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

        if (m_waveformDisplay.hasSelection() && m_bufferManager.hasAudioData())
        {
            auto bounds = getLocalBounds().reduced(5);

            // Get time-based selection
            double startTime = m_waveformDisplay.getSelectionStart();
            double endTime = m_waveformDisplay.getSelectionEnd();
            double duration = m_waveformDisplay.getSelectionDuration();

            // Format selection info based on current snap unit
            auto unitType = m_waveformDisplay.getSnapUnit();
            juce::String info = "Selection: ";

            // Format based on snap unit
            switch (unitType)
            {
                case AudioUnits::UnitType::Samples:
                {
                    int64_t startSample = m_bufferManager.timeToSample(startTime);
                    int64_t endSample = m_bufferManager.timeToSample(endTime);
                    int64_t durationSamples = endSample - startSample;
                    info += juce::String::formatted("%lld - %lld (%lld samples)",
                                                    startSample, endSample, durationSamples);
                    break;
                }
                case AudioUnits::UnitType::Milliseconds:
                {
                    info += juce::String::formatted("%.1f - %.1f ms (%.1f ms)",
                                                    startTime * 1000.0, endTime * 1000.0, duration * 1000.0);
                    break;
                }
                case AudioUnits::UnitType::Seconds:
                    info += juce::String::formatted("%s - %s (%s)",
                                                    m_waveformDisplay.getSelectionStartString().toRawUTF8(),
                                                    m_waveformDisplay.getSelectionEndString().toRawUTF8(),
                                                    m_waveformDisplay.getSelectionDurationString().toRawUTF8());
                    break;
                case AudioUnits::UnitType::Frames:
                {
                    double fps = m_waveformDisplay.getFrameRate();
                    int startFrame = AudioUnits::samplesToFrames(
                        m_bufferManager.timeToSample(startTime), fps, m_bufferManager.getSampleRate());
                    int endFrame = AudioUnits::samplesToFrames(
                        m_bufferManager.timeToSample(endTime), fps, m_bufferManager.getSampleRate());
                    info += juce::String::formatted("%d - %d frames (%d frames)",
                                                    startFrame, endFrame, endFrame - startFrame);
                    break;
                }
                default:
                    // Fallback to time-based display
                    info += juce::String::formatted("%s - %s (%s)",
                                                    m_waveformDisplay.getSelectionStartString().toRawUTF8(),
                                                    m_waveformDisplay.getSelectionEndString().toRawUTF8(),
                                                    m_waveformDisplay.getSelectionDurationString().toRawUTF8());
                    break;
            }

            g.drawText(info, bounds, juce::Justification::centredLeft, true);
        }
        else if (m_waveformDisplay.hasEditCursor() && m_bufferManager.hasAudioData())
        {
            // Show edit cursor position - format based on current snap unit
            auto bounds = getLocalBounds().reduced(5);
            double cursorTime = m_waveformDisplay.getEditCursorPosition();

            // Format based on current snap unit (same logic as selection info)
            auto unitType = m_waveformDisplay.getSnapUnit();
            juce::String info = "Edit Cursor: ";

            switch (unitType)
            {
                case AudioUnits::UnitType::Samples:
                {
                    int64_t cursorSample = m_bufferManager.timeToSample(cursorTime);
                    info += juce::String::formatted("%lld samples", cursorSample);
                    break;
                }
                case AudioUnits::UnitType::Milliseconds:
                {
                    info += juce::String::formatted("%.1f ms", cursorTime * 1000.0);
                    break;
                }
                case AudioUnits::UnitType::Seconds:
                {
                    // Format as time string
                    int hours = static_cast<int>(cursorTime / 3600.0);
                    int minutes = static_cast<int>((cursorTime - hours * 3600.0) / 60.0);
                    double seconds = cursorTime - hours * 3600.0 - minutes * 60.0;

                    if (hours > 0)
                        info += juce::String::formatted("%02d:%02d:%06.3f", hours, minutes, seconds);
                    else
                        info += juce::String::formatted("%02d:%06.3f", minutes, seconds);
                    break;
                }
                case AudioUnits::UnitType::Frames:
                {
                    double fps = m_waveformDisplay.getFrameRate();
                    int cursorFrame = AudioUnits::samplesToFrames(
                        m_bufferManager.timeToSample(cursorTime), fps, m_bufferManager.getSampleRate());
                    info += juce::String::formatted("%d frames", cursorFrame);
                    break;
                }
                default:
                {
                    // Fallback to time string
                    int hours = static_cast<int>(cursorTime / 3600.0);
                    int minutes = static_cast<int>((cursorTime - hours * 3600.0) / 60.0);
                    double seconds = cursorTime - hours * 3600.0 - minutes * 60.0;

                    if (hours > 0)
                        info += juce::String::formatted("%02d:%02d:%06.3f", hours, minutes, seconds);
                    else
                        info += juce::String::formatted("%02d:%06.3f", minutes, seconds);
                    break;
                }
            }

            g.setColour(juce::Colours::yellow);
            g.drawText(info, bounds, juce::Justification::centredLeft, true);
        }
        else if (m_waveformDisplay.isFileLoaded())
        {
            auto bounds = getLocalBounds().reduced(5);
            g.setColour(juce::Colours::grey);
            g.drawText("No selection - Click and drag to select, or click to place edit cursor", bounds,
                      juce::Justification::centredLeft, true);
        }
    }

    void timerCallback() override
    {
        repaint();
    }

private:
    WaveformDisplay& m_waveformDisplay;
    AudioBufferManager& m_bufferManager;
};

//==============================================================================
/**
 * Main application window component.
 * Handles UI, file operations, playback control, and keyboard shortcuts.
 */
class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public juce::ApplicationCommandTarget,
                      public juce::MenuBarModel,
                      public juce::Timer
{
public:
    MainComponent()
        : m_waveformDisplay(m_audioEngine.getFormatManager()),
          m_transportControls(m_audioEngine, m_waveformDisplay),
          m_selectionInfo(m_waveformDisplay, m_audioBufferManager),
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
        addAndMakeVisible(m_meters);

        // Add keyboard focus to handle shortcuts
        setWantsKeyboardFocus(true);

        // Set up command manager
        m_commandManager.registerAllCommandsForTarget(this);

        // Add keyboard mappings
        addKeyListener(m_commandManager.getKeyMappings());

        // Configure undo manager with transaction limits
        // Limit to 100 undo actions, with up to 100 transactions kept
        // This allows 100 individual undo steps for micro-level editing (e.g., gain adjustments)
        // minTransactions set to 90 to allow some headroom for complex multi-unit transactions
        m_undoManager.setMaxNumberOfStoredUnits(100, 90);

        // Start timer to update playback position
        startTimer(50); // Update every 50ms for smooth 60fps cursor

        // Clean up recent files on startup
        Settings::getInstance().cleanupRecentFiles();
    }

    ~MainComponent() override
    {
        m_audioEngine.closeAudioFile();
        stopTimer();
    }

    //==============================================================================
    // Edit Operations

    /**
     * Select all audio in the current file.
     */
    void selectAll()
    {
        if (!m_audioEngine.isFileLoaded())
        {
            return;
        }

        m_waveformDisplay.setSelection(0.0, m_audioBufferManager.getLengthInSeconds());
        juce::Logger::writeToLog("Selected all audio");
    }

    /**
     * Validates that the current selection is within valid bounds.
     * CRITICAL FIX (2025-10-07): Prevents negative sample positions and invalid ranges.
     *
     * @return true if selection is valid, false otherwise
     */
    bool validateSelection() const
    {
        if (!m_waveformDisplay.hasSelection())
        {
            return false;
        }

        auto selectionStart = m_waveformDisplay.getSelectionStart();
        auto selectionEnd = m_waveformDisplay.getSelectionEnd();

        // Check for negative times
        if (selectionStart < 0.0 || selectionEnd < 0.0)
        {
            juce::Logger::writeToLog(juce::String::formatted(
                "Invalid selection: negative time (start=%.6f, end=%.6f)",
                selectionStart, selectionEnd));
            return false;
        }

        // Check for times beyond duration
        double maxDuration = m_audioBufferManager.getLengthInSeconds();
        if (selectionStart > maxDuration || selectionEnd > maxDuration)
        {
            juce::Logger::writeToLog(juce::String::formatted(
                "Invalid selection: beyond duration (start=%.6f, end=%.6f, max=%.6f)",
                selectionStart, selectionEnd, maxDuration));
            return false;
        }

        // Check for inverted selection (should not happen, but be safe)
        if (selectionStart >= selectionEnd)
        {
            juce::Logger::writeToLog(juce::String::formatted(
                "Invalid selection: start >= end (start=%.6f, end=%.6f)",
                selectionStart, selectionEnd));
            return false;
        }

        return true;
    }

    /**
     * Copy the current selection to the clipboard.
     */
    void copySelection()
    {
        // CRITICAL FIX (2025-10-07): Validate selection before converting to samples
        if (!validateSelection() || !m_audioBufferManager.hasAudioData())
        {
            return;
        }

        auto selectionStart = m_waveformDisplay.getSelectionStart();
        auto selectionEnd = m_waveformDisplay.getSelectionEnd();

        // Convert time to samples (now safe - selection is validated)
        auto startSample = m_audioBufferManager.timeToSample(selectionStart);
        auto endSample = m_audioBufferManager.timeToSample(selectionEnd);

        // CRITICAL FIX (2025-10-08): getAudioRange expects COUNT, not end position!
        // Calculate number of samples in selection
        auto numSamples = endSample - startSample;

        // Get the audio data for the selection
        auto audioRange = m_audioBufferManager.getAudioRange(startSample, numSamples);

        if (audioRange.getNumSamples() > 0)
        {
            AudioClipboard::getInstance().copyAudio(audioRange, m_audioBufferManager.getSampleRate());

            juce::Logger::writeToLog(juce::String::formatted(
                "Copied %.2f seconds to clipboard", selectionEnd - selectionStart));
            repaint(); // Update status bar to show clipboard info
        }
    }

    /**
     * Cut the current selection to the clipboard.
     */
    void cutSelection()
    {
        // CRITICAL FIX (2025-10-07): Validate selection before operations
        if (!validateSelection() || !m_audioBufferManager.hasAudioData())
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
     * Paste from the clipboard at the current cursor position.
     */
    void pasteAtCursor()
    {
        if (!AudioClipboard::getInstance().hasAudio() || !m_audioBufferManager.hasAudioData())
        {
            return;
        }

        // CRITICAL FIX (BLOCKER #3): Begin new transaction for each edit operation
        m_undoManager.beginNewTransaction("Paste");

        // Use edit cursor position if available, otherwise use playback position
        double cursorPos;
        if (m_waveformDisplay.hasEditCursor())
        {
            cursorPos = m_waveformDisplay.getEditCursorPosition();
        }
        else
        {
            cursorPos = m_waveformDisplay.getPlaybackPosition();
        }

        auto insertSample = m_audioBufferManager.timeToSample(cursorPos);

        // Check for sample rate mismatch
        auto clipboardSampleRate = AudioClipboard::getInstance().getSampleRate();
        auto currentSampleRate = m_audioBufferManager.getSampleRate();

        if (std::abs(clipboardSampleRate - currentSampleRate) > 0.01)
        {
            auto result = juce::NativeMessageBox::showOkCancelBox(
                juce::MessageBoxIconType::WarningIcon,
                "Sample Rate Mismatch",
                juce::String::formatted(
                    "The clipboard audio has a different sample rate (%.0f Hz) "
                    "than the current file (%.0f Hz).\n\n"
                    "Paste anyway? (May affect pitch and speed)",
                    clipboardSampleRate, currentSampleRate),
                nullptr,
                nullptr);

            if (!result)
            {
                return;
            }
        }

        // Get clipboard audio
        auto clipboardAudio = AudioClipboard::getInstance().getAudio();

        if (clipboardAudio.getNumSamples() > 0)
        {
            // If there's a selection, replace it; otherwise insert at cursor
            if (m_waveformDisplay.hasSelection() && validateSelection())
            {
                auto selectionStart = m_waveformDisplay.getSelectionStart();
                auto selectionEnd = m_waveformDisplay.getSelectionEnd();
                // CRITICAL FIX (2025-10-07): Convert to samples (now safe - validated above)
                auto startSample = m_audioBufferManager.timeToSample(selectionStart);
                auto endSample = m_audioBufferManager.timeToSample(selectionEnd);

                // Create undoable replace action
                auto replaceAction = new ReplaceAction(
                    m_audioBufferManager,
                    m_audioEngine,
                    m_waveformDisplay,
                    startSample,
                    endSample - startSample,
                    clipboardAudio
                );

                // Perform the replace and add to undo manager
                m_undoManager.perform(replaceAction);

                juce::Logger::writeToLog(juce::String::formatted(
                    "Pasted %.2f seconds from clipboard, replacing selection (undoable)",
                    clipboardAudio.getNumSamples() / currentSampleRate));
            }
            else
            {
                // Create undoable insert action
                auto insertAction = new InsertAction(
                    m_audioBufferManager,
                    m_audioEngine,
                    m_waveformDisplay,
                    insertSample,
                    clipboardAudio
                );

                // Perform the insert and add to undo manager
                m_undoManager.perform(insertAction);

                juce::Logger::writeToLog(juce::String::formatted(
                    "Pasted %.2f seconds from clipboard (undoable)",
                    clipboardAudio.getNumSamples() / currentSampleRate));
            }

            // Mark as modified
            m_isModified = true;

            // Clear selection after paste
            m_waveformDisplay.clearSelection();

            // NOTE: Waveform display already updated by undo action's updatePlaybackAndDisplay()
            // Removed redundant reloadFromBuffer() call for performance

            repaint();
        }
    }

    /**
     * Delete the current selection.
     */
    void deleteSelection()
    {
        // CRITICAL FIX (2025-10-07): Validate selection before converting to samples
        if (!validateSelection() || !m_audioBufferManager.hasAudioData())
        {
            return;
        }

        // CRITICAL FIX (BLOCKER #3): Begin new transaction for each edit operation
        m_undoManager.beginNewTransaction("Delete");

        auto selectionStart = m_waveformDisplay.getSelectionStart();
        auto selectionEnd = m_waveformDisplay.getSelectionEnd();

        // Convert time to samples (now safe - selection is validated)
        auto startSample = m_audioBufferManager.timeToSample(selectionStart);
        auto endSample = m_audioBufferManager.timeToSample(selectionEnd);

        // Create undoable delete action
        auto deleteAction = new DeleteAction(
            m_audioBufferManager,
            m_audioEngine,
            m_waveformDisplay,
            startSample,
            endSample - startSample
        );

        // Perform the delete and add to undo manager
        m_undoManager.perform(deleteAction);

        // Mark as modified
        m_isModified = true;

        // Clear selection after delete
        m_waveformDisplay.clearSelection();

        // CRITICAL FIX (Phase 1.4 - Edit Cursor Preservation):
        // Set edit cursor at the deletion point for professional workflow
        // This enables cursor preservation during subsequent operations
        m_waveformDisplay.setEditCursor(selectionStart);

        // NOTE: Waveform display already updated by undo action's updatePlaybackAndDisplay()
        // Removed redundant reloadFromBuffer() call for performance

        juce::Logger::writeToLog(juce::String::formatted(
            "Deleted %.2f seconds (undoable), cursor set at %.2f",
            selectionEnd - selectionStart, selectionStart));

        repaint();
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

            // Clipboard info in the middle
            if (AudioClipboard::getInstance().hasAudio())
            {
                auto clipboardSection = statusBar.reduced(300, 0);
                clipboardSection.setX(statusBar.getCentreX() - 150);

                g.setColour(juce::Colours::lightgreen);
                auto clipboardBuffer = AudioClipboard::getInstance().getAudio();
                double clipboardDuration = clipboardBuffer.getNumSamples() / AudioClipboard::getInstance().getSampleRate();
                juce::String clipboardInfo = juce::String::formatted(
                    "Clipboard: %.2f s @ %.0f Hz",
                    clipboardDuration,
                    AudioClipboard::getInstance().getSampleRate());
                g.drawText(clipboardInfo, clipboardSection, juce::Justification::centred, true);
            }

            // Two-tier snap mode indicator
            auto snapSection = statusBar.removeFromRight(200);

            auto unitType = m_waveformDisplay.getSnapUnit();
            int increment = m_waveformDisplay.getSnapIncrement();
            bool zeroCrossing = m_waveformDisplay.isZeroCrossingEnabled();

            // Build snap text with unit and increment
            juce::String snapText;
            juce::Colour snapColor;

            if (increment == 0)
            {
                snapText = "[Snap: Off]";
                snapColor = juce::Colours::grey;
            }
            else
            {
                snapText = "[" + AudioUnits::formatIncrement(increment, unitType) + "]";
                snapColor = juce::Colours::lightblue;
            }

            // Add zero-crossing indicator if enabled
            if (zeroCrossing)
            {
                snapText += " [Zero X]";
                snapColor = juce::Colours::orange;
            }

            g.setColour(snapColor);
            g.setFont(12.0f);
            g.drawText(snapText, snapSection.reduced(5, 0), juce::Justification::centred, true);

            // State indicator on the right
            auto rightSection = statusBar.removeFromRight(100);
            g.setColour(juce::Colours::white);
            juce::String stateText;
            switch (m_audioEngine.getPlaybackState())
            {
                case PlaybackState::STOPPED: stateText = "Stopped"; break;
                case PlaybackState::PLAYING: stateText = "Playing"; break;
                case PlaybackState::PAUSED:  stateText = "Paused"; break;
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

        // Reserve space for meters on the right (120px width)
        m_meters.setBounds(bounds.removeFromRight(120));

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

            // Update level meters with audio levels
            m_meters.setPeakLevel(0, m_audioEngine.getPeakLevel(0));
            m_meters.setPeakLevel(1, m_audioEngine.getPeakLevel(1));
            m_meters.setRMSLevel(0, m_audioEngine.getRMSLevel(0));
            m_meters.setRMSLevel(1, m_audioEngine.getRMSLevel(1));
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
            CommandIDs::editSelectAll,
            CommandIDs::editCut,
            CommandIDs::editCopy,
            CommandIDs::editPaste,
            CommandIDs::editDelete,
            CommandIDs::playbackPlay,
            CommandIDs::playbackPause,
            CommandIDs::playbackStop,
            CommandIDs::playbackLoop,
            // View/Zoom commands
            CommandIDs::viewZoomIn,
            CommandIDs::viewZoomOut,
            CommandIDs::viewZoomFit,
            CommandIDs::viewZoomSelection,
            CommandIDs::viewZoomOneToOne,
            // Navigation commands
            CommandIDs::navigateLeft,
            CommandIDs::navigateRight,
            CommandIDs::navigateStart,
            CommandIDs::navigateEnd,
            CommandIDs::navigatePageLeft,
            CommandIDs::navigatePageRight,
            CommandIDs::navigateHomeVisible,
            CommandIDs::navigateEndVisible,
            CommandIDs::navigateCenterView,
            // Selection extension commands
            CommandIDs::selectExtendLeft,
            CommandIDs::selectExtendRight,
            CommandIDs::selectExtendStart,
            CommandIDs::selectExtendEnd,
            CommandIDs::selectExtendPageLeft,
            CommandIDs::selectExtendPageRight,
            // Snap commands
            CommandIDs::snapCycleMode,
            CommandIDs::snapToggleZeroCrossing,
            // Processing commands
            CommandIDs::processGain,
            CommandIDs::processIncreaseGain,
            CommandIDs::processDecreaseGain,
            CommandIDs::processNormalize,
            CommandIDs::processFadeIn,
            CommandIDs::processFadeOut
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

            case CommandIDs::editUndo:
                result.setInfo("Undo", "Undo the last operation", "Edit", 0);
                result.addDefaultKeypress('z', juce::ModifierKeys::commandModifier);
                result.setActive(m_undoManager.canUndo());
                break;

            case CommandIDs::editRedo:
                result.setInfo("Redo", "Redo the last undone operation", "Edit", 0);
                result.addDefaultKeypress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(m_undoManager.canRedo());
                break;

            case CommandIDs::editSelectAll:
                result.setInfo("Select All", "Select all audio", "Edit", 0);
                result.addDefaultKeypress('a', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::editCut:
                result.setInfo("Cut", "Cut selection to clipboard", "Edit", 0);
                result.addDefaultKeypress('x', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded() && m_waveformDisplay.hasSelection());
                break;

            case CommandIDs::editCopy:
                result.setInfo("Copy", "Copy selection to clipboard", "Edit", 0);
                result.addDefaultKeypress('c', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded() && m_waveformDisplay.hasSelection());
                break;

            case CommandIDs::editPaste:
                result.setInfo("Paste", "Paste from clipboard", "Edit", 0);
                result.addDefaultKeypress('v', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded() && AudioClipboard::getInstance().hasAudio());
                break;

            case CommandIDs::editDelete:
                result.setInfo("Delete", "Delete selection", "Edit", 0);
                result.addDefaultKeypress(juce::KeyPress::deleteKey, 0);
                result.setActive(m_audioEngine.isFileLoaded() && m_waveformDisplay.hasSelection());
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

            // View/Zoom commands
            case CommandIDs::viewZoomIn:
                result.setInfo("Zoom In", "Zoom in 2x", "View", 0);
                result.addDefaultKeypress('=', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::viewZoomOut:
                result.setInfo("Zoom Out", "Zoom out 2x", "View", 0);
                result.addDefaultKeypress('-', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::viewZoomFit:
                result.setInfo("Zoom to Fit", "Fit entire waveform to view", "View", 0);
                result.addDefaultKeypress('0', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::viewZoomSelection:
                result.setInfo("Zoom to Selection", "Zoom to current selection", "View", 0);
                result.addDefaultKeypress('e', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded() && m_waveformDisplay.hasSelection());
                break;

            case CommandIDs::viewZoomOneToOne:
                result.setInfo("Zoom 1:1", "Zoom to 1:1 sample resolution", "View", 0);
                result.addDefaultKeypress('1', juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            // Navigation commands
            case CommandIDs::navigateLeft:
                result.setInfo("Navigate Left", "Move cursor left by snap increment", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::leftKey, 0);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::navigateRight:
                result.setInfo("Navigate Right", "Move cursor right by snap increment", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::rightKey, 0);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::navigateStart:
                result.setInfo("Jump to Start", "Jump to start of file", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::leftKey, juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::navigateEnd:
                result.setInfo("Jump to End", "Jump to end of file", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::rightKey, juce::ModifierKeys::commandModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::navigatePageLeft:
                result.setInfo("Page Left", "Move cursor left by page increment", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::pageUpKey, 0);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::navigatePageRight:
                result.setInfo("Page Right", "Move cursor right by page increment", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::pageDownKey, 0);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::navigateHomeVisible:
                result.setInfo("Jump to Visible Start", "Jump to first visible sample", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::homeKey, 0);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::navigateEndVisible:
                result.setInfo("Jump to Visible End", "Jump to last visible sample", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::endKey, 0);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::navigateCenterView:
                result.setInfo("Center View", "Center view on cursor", "Navigation", 0);
                result.addDefaultKeypress('.', 0);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            // Selection extension commands
            case CommandIDs::selectExtendLeft:
                result.setInfo("Extend Selection Left", "Extend selection left by increment", "Selection", 0);
                result.addDefaultKeypress(juce::KeyPress::leftKey, juce::ModifierKeys::shiftModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::selectExtendRight:
                result.setInfo("Extend Selection Right", "Extend selection right by increment", "Selection", 0);
                result.addDefaultKeypress(juce::KeyPress::rightKey, juce::ModifierKeys::shiftModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::selectExtendStart:
                result.setInfo("Extend to Visible Start", "Extend selection to visible start", "Selection", 0);
                result.addDefaultKeypress(juce::KeyPress::homeKey, juce::ModifierKeys::shiftModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::selectExtendEnd:
                result.setInfo("Extend to Visible End", "Extend selection to visible end", "Selection", 0);
                result.addDefaultKeypress(juce::KeyPress::endKey, juce::ModifierKeys::shiftModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::selectExtendPageLeft:
                result.setInfo("Extend Selection Page Left", "Extend selection left by page increment", "Selection", 0);
                result.addDefaultKeypress(juce::KeyPress::pageUpKey, juce::ModifierKeys::shiftModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::selectExtendPageRight:
                result.setInfo("Extend Selection Page Right", "Extend selection right by page increment", "Selection", 0);
                result.addDefaultKeypress(juce::KeyPress::pageDownKey, juce::ModifierKeys::shiftModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            // Snap commands
            case CommandIDs::snapCycleMode:
                result.setInfo("Cycle Snap Mode", "Cycle through snap modes", "Snap", 0);
                result.addDefaultKeypress('g', 0);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::snapToggleZeroCrossing:
                result.setInfo("Toggle Zero Crossing Snap", "Quick toggle zero crossing snap", "Snap", 0);
                result.addDefaultKeypress('z', 0);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            // Processing operations
            case CommandIDs::processGain:
                result.setInfo("Gain...", "Apply precise gain adjustment", "Process", 0);
                result.addDefaultKeypress('g', juce::ModifierKeys::shiftModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::processIncreaseGain:
                result.setInfo("Increase Gain", "Increase gain by 1 dB", "Process", 0);
                result.addDefaultKeypress(juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::processDecreaseGain:
                result.setInfo("Decrease Gain", "Decrease gain by 1 dB", "Process", 0);
                result.addDefaultKeypress(juce::KeyPress::downKey, juce::ModifierKeys::shiftModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::processNormalize:
                result.setInfo("Normalize...", "Normalize audio to peak level", "Process", 0);
                result.addDefaultKeypress('n', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(m_audioEngine.isFileLoaded());
                break;

            case CommandIDs::processFadeIn:
                result.setInfo("Fade In", "Apply linear fade in to selection", "Process", 0);
                result.addDefaultKeypress('i', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(m_audioEngine.isFileLoaded() && m_waveformDisplay.hasSelection());
                break;

            case CommandIDs::processFadeOut:
                result.setInfo("Fade Out", "Apply linear fade out to selection", "Process", 0);
                result.addDefaultKeypress('o', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(m_audioEngine.isFileLoaded() && m_waveformDisplay.hasSelection());
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

            case CommandIDs::editUndo:
                if (m_undoManager.canUndo())
                {
                    juce::Logger::writeToLog(juce::String::formatted(
                        "Undo: %s (stack depth before: %d)",
                        m_undoManager.getUndoDescription().toRawUTF8(),
                        m_undoManager.getNumberOfUnitsTakenUpByStoredCommands()));

                    m_undoManager.undo();
                    m_isModified = true;

                    juce::Logger::writeToLog(juce::String::formatted(
                        "After undo - Can undo: %s, Can redo: %s",
                        m_undoManager.canUndo() ? "yes" : "no",
                        m_undoManager.canRedo() ? "yes" : "no"));

                    repaint();
                }
                return true;

            case CommandIDs::editRedo:
                if (m_undoManager.canRedo())
                {
                    juce::Logger::writeToLog(juce::String::formatted(
                        "Redo: %s (stack depth before: %d)",
                        m_undoManager.getRedoDescription().toRawUTF8(),
                        m_undoManager.getNumberOfUnitsTakenUpByStoredCommands()));

                    m_undoManager.redo();
                    m_isModified = true;

                    juce::Logger::writeToLog(juce::String::formatted(
                        "After redo - Can undo: %s, Can redo: %s",
                        m_undoManager.canUndo() ? "yes" : "no",
                        m_undoManager.canRedo() ? "yes" : "no"));

                    repaint();
                }
                return true;

            case CommandIDs::editSelectAll:
                selectAll();
                return true;

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

            // View/Zoom operations
            case CommandIDs::viewZoomIn:
                m_waveformDisplay.zoomIn();
                return true;

            case CommandIDs::viewZoomOut:
                m_waveformDisplay.zoomOut();
                return true;

            case CommandIDs::viewZoomFit:
                m_waveformDisplay.zoomToFit();
                return true;

            case CommandIDs::viewZoomSelection:
                m_waveformDisplay.zoomToSelection();
                return true;

            case CommandIDs::viewZoomOneToOne:
                m_waveformDisplay.zoomOneToOne();
                return true;

            // Navigation operations (simple movement)
            case CommandIDs::navigateLeft:
                m_waveformDisplay.navigateLeft(false);
                return true;

            case CommandIDs::navigateRight:
                m_waveformDisplay.navigateRight(false);
                return true;

            case CommandIDs::navigateStart:
                m_waveformDisplay.navigateToStart(false);
                return true;

            case CommandIDs::navigateEnd:
                m_waveformDisplay.navigateToEnd(false);
                return true;

            case CommandIDs::navigatePageLeft:
                m_waveformDisplay.navigatePageLeft();
                return true;

            case CommandIDs::navigatePageRight:
                m_waveformDisplay.navigatePageRight();
                return true;

            case CommandIDs::navigateHomeVisible:
                m_waveformDisplay.navigateToVisibleStart(false);
                return true;

            case CommandIDs::navigateEndVisible:
                m_waveformDisplay.navigateToVisibleEnd(false);
                return true;

            case CommandIDs::navigateCenterView:
                m_waveformDisplay.centerViewOnCursor();
                return true;

            // Selection extension operations
            case CommandIDs::selectExtendLeft:
                m_waveformDisplay.navigateLeft(true);
                return true;

            case CommandIDs::selectExtendRight:
                m_waveformDisplay.navigateRight(true);
                return true;

            case CommandIDs::selectExtendStart:
                m_waveformDisplay.navigateToVisibleStart(true);
                return true;

            case CommandIDs::selectExtendEnd:
                m_waveformDisplay.navigateToVisibleEnd(true);
                return true;

            case CommandIDs::selectExtendPageLeft:
                juce::Logger::writeToLog("selectExtendPageLeft command triggered");
                m_waveformDisplay.navigatePageLeft(true);
                return true;

            case CommandIDs::selectExtendPageRight:
                juce::Logger::writeToLog("selectExtendPageRight command triggered");
                m_waveformDisplay.navigatePageRight(true);
                return true;

            // Snap mode operations
            case CommandIDs::snapCycleMode:
                cycleSnapMode();
                return true;

            case CommandIDs::snapToggleZeroCrossing:
                toggleZeroCrossingSnap();
                return true;

            // Processing operations
            case CommandIDs::processGain:
                showGainDialog();
                return true;

            case CommandIDs::processIncreaseGain:
                applyGainAdjustment(+1.0f);
                return true;

            case CommandIDs::processDecreaseGain:
                applyGainAdjustment(-1.0f);
                return true;

            case CommandIDs::processNormalize:
                applyNormalize();
                return true;

            case CommandIDs::processFadeIn:
                applyFadeIn();
                return true;

            case CommandIDs::processFadeOut:
                applyFadeOut();
                return true;

            default:
                return false;
        }
    }

    //==============================================================================
    // MenuBarModel Implementation

    juce::StringArray getMenuBarNames() override
    {
        return { "File", "Edit", "Process", "Playback", "Help" };
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
        else if (menuIndex == 2) // Process menu
        {
            menu.addCommandItem(&m_commandManager, CommandIDs::processGain);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::processNormalize);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::processFadeIn);
            menu.addCommandItem(&m_commandManager, CommandIDs::processFadeOut);
        }
        else if (menuIndex == 3) // Playback menu
        {
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackPlay);
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackPause);
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackStop);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackLoop);
        }
        else if (menuIndex == 4) // Help menu
        {
            menu.addItem("About WaveEdit", [this] { showAbout(); });
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

            // Load file into AudioBufferManager for editing operations
            if (!m_audioBufferManager.loadFromFile(file, m_audioEngine.getFormatManager()))
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Buffer Error",
                    "Could not load file into editing buffer: " + file.getFileName(),
                    "OK");
            }

            // Add to recent files
            Settings::getInstance().addRecentFile(file);

            // Clear undo history when loading a new file
            m_undoManager.clearUndoHistory();

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

        // Save the edited audio buffer to the file
        bool saveSuccess = m_fileManager.overwriteFile(
            currentFile,
            m_audioBufferManager.getBuffer(),
            m_audioBufferManager.getSampleRate(),
            m_audioBufferManager.getBitDepth()
        );

        if (saveSuccess)
        {
            // Clear modified flag
            m_isModified = false;
            repaint();

            juce::Logger::writeToLog("File saved successfully: " + currentFile.getFullPathName());
        }
        else
        {
            // Show error dialog
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Save Failed",
                "Could not save file: " + currentFile.getFileName() + "\n\n" +
                "Error: " + m_fileManager.getLastError(),
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

                // Save the edited audio buffer to the new file
                bool saveSuccess = m_fileManager.saveAsWav(
                    file,
                    m_audioBufferManager.getBuffer(),
                    m_audioBufferManager.getSampleRate(),
                    m_audioBufferManager.getBitDepth()
                );

                if (saveSuccess)
                {
                    // Clear modified flag
                    m_isModified = false;

                    // Add to recent files
                    Settings::getInstance().addRecentFile(file);

                    repaint();

                    juce::Logger::writeToLog("File saved successfully as: " + file.getFullPathName());
                }
                else
                {
                    // Show error dialog
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Save As Failed",
                        "Could not save file as: " + file.getFileName() + "\n\n" +
                        "Error: " + m_fileManager.getLastError(),
                        "OK");
                }
            }
            // Clear the file chooser after use
            m_fileChooser.reset();
        });
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
        m_waveformDisplay.clear();
        m_undoManager.clearUndoHistory();
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
            // PRIORITY 5e/5f: Play from cursor position by default
            // Debug: Log current state
            juce::Logger::writeToLog("=== PLAYBACK START DEBUG ===");
            juce::Logger::writeToLog(juce::String::formatted("Has selection: %s",
                m_waveformDisplay.hasSelection() ? "YES" : "NO"));
            juce::Logger::writeToLog(juce::String::formatted("Has edit cursor: %s",
                m_waveformDisplay.hasEditCursor() ? "YES" : "NO"));

            if (m_waveformDisplay.hasSelection())
            {
                juce::Logger::writeToLog(juce::String::formatted("Selection: %.3f - %.3f s",
                    m_waveformDisplay.getSelectionStart(),
                    m_waveformDisplay.getSelectionEnd()));
            }

            if (m_waveformDisplay.hasEditCursor())
            {
                juce::Logger::writeToLog(juce::String::formatted("Edit cursor: %.3f s",
                    m_waveformDisplay.getEditCursorPosition()));
            }

            juce::Logger::writeToLog(juce::String::formatted("Current playback position: %.3f s",
                m_waveformDisplay.getPlaybackPosition()));

            // If there's a selection, start playback from the selection start
            if (m_waveformDisplay.hasSelection())
            {
                double startPos = m_waveformDisplay.getSelectionStart();
                juce::Logger::writeToLog(juce::String::formatted(
                    "→ Starting playback from selection start: %.3f s", startPos));
                m_audioEngine.setPosition(startPos);
            }
            else if (m_waveformDisplay.hasEditCursor())
            {
                // No selection: play from edit cursor position if set
                double startPos = m_waveformDisplay.getEditCursorPosition();
                juce::Logger::writeToLog(juce::String::formatted(
                    "→ Starting playback from edit cursor: %.3f s", startPos));
                m_audioEngine.setPosition(startPos);
            }
            else
            {
                // No selection or cursor: play from current playback position
                double startPos = m_waveformDisplay.getPlaybackPosition();
                juce::Logger::writeToLog(juce::String::formatted(
                    "→ Starting playback from playback position: %.3f s", startPos));
                m_audioEngine.setPosition(startPos);
            }

            juce::Logger::writeToLog("=== END DEBUG ===");

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

        // PRIORITY 5g: Fix loop mode - wire up to AudioEngine
        m_transportControls.toggleLoop();
        bool loopEnabled = m_transportControls.isLoopEnabled();
        m_audioEngine.setLooping(loopEnabled);

        juce::Logger::writeToLog(loopEnabled ? "Loop mode ON" : "Loop mode OFF");
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

    juce::ApplicationCommandManager& getCommandManager()
    {
        return m_commandManager;
    }

    //==============================================================================
    // Snap mode helpers

    void cycleSnapMode()
    {
        // Two-tier snap system: G key cycles through increments within current unit
        m_waveformDisplay.cycleSnapIncrement();

        // Force repaint to update snap mode indicator in status bar
        repaint();

        // Show status message with current unit and increment
        auto unitType = m_waveformDisplay.getSnapUnit();
        int increment = m_waveformDisplay.getSnapIncrement();
        juce::String message = "Snap: " + AudioUnits::formatIncrement(increment, unitType);
        juce::Logger::writeToLog(message);
    }

    void toggleZeroCrossingSnap()
    {
        // Two-tier snap system: Z key toggles zero-crossing (independent of unit snapping)
        m_waveformDisplay.toggleZeroCrossing();

        // Force repaint to update snap mode indicator in status bar
        repaint();

        // Show status message
        bool enabled = m_waveformDisplay.isZeroCrossingEnabled();
        juce::String message = enabled ? "Zero-crossing snap: ON" : "Zero-crossing snap: OFF";
        juce::Logger::writeToLog(message);
    }

    //==============================================================================
    // Gain adjustment helpers

    /**
     * Apply gain adjustment to entire file or selection.
     * Creates undo action for the gain adjustment.
     *
     * @param gainDB Gain adjustment in decibels (positive or negative)
     */
    void applyGainAdjustment(float gainDB)
    {
        if (!m_audioEngine.isFileLoaded())
        {
            return;
        }

        // Note: We no longer stop/restart playback here.
        // GainUndoAction::perform() uses reloadBufferPreservingPlayback() which safely
        // updates the audio buffer while preserving playback position if currently playing.
        // This allows real-time gain adjustments during playback without interruption.

        // Get current buffer
        auto& buffer = m_audioBufferManager.getMutableBuffer();
        if (buffer.getNumSamples() == 0)
        {
            return;
        }

        // Determine region to process
        int startSample = 0;
        int numSamples = buffer.getNumSamples();
        bool isSelection = false;

        if (m_waveformDisplay.hasSelection())
        {
            // Apply to selection only
            startSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionStart());
            int endSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionEnd());
            numSamples = endSample - startSample;
            isSelection = true;
        }

        // Store before state for undo (only the affected region for memory efficiency)
        juce::AudioBuffer<float> beforeBuffer;
        beforeBuffer.setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            beforeBuffer.copyFrom(ch, 0, buffer, ch, startSample, numSamples);
        }

        // CRITICAL FIX: Start a new transaction so each gain adjustment is a separate undo step
        // Without this, JUCE groups all actions into one transaction and undo undoes everything at once
        juce::String transactionName = juce::String::formatted(
            "Gain %+.1f dB (%s)",
            gainDB,
            isSelection ? "selection" : "entire file"
        );
        m_undoManager.beginNewTransaction(transactionName);

        // Create undo action (perform() will apply the gain)
        auto* undoAction = new GainUndoAction(
            m_audioBufferManager,
            m_waveformDisplay,
            m_audioEngine,
            beforeBuffer,
            startSample,
            numSamples,
            gainDB,
            isSelection
        );

        // perform() calls GainUndoAction::perform() which applies gain and updates display
        // while preserving playback state (uses reloadBufferPreservingPlayback internally)
        m_undoManager.perform(undoAction);

        // Mark as modified
        m_isModified = true;
    }

    /**
     * Show gain dialog and apply user-entered gain value.
     */
    void showGainDialog()
    {
        auto result = GainDialog::showDialog();

        if (result.has_value())
        {
            applyGainAdjustment(result.value());
        }
    }

    //==============================================================================
    // Normalization helpers

    /**
     * Apply normalization to entire file or selection.
     * Normalizes audio to 0dB peak level (or selection).
     * Creates undo action for the normalization.
     */
    void applyNormalize()
    {
        if (!m_audioEngine.isFileLoaded())
        {
            return;
        }

        // Get current buffer
        auto& buffer = m_audioBufferManager.getMutableBuffer();
        if (buffer.getNumSamples() == 0)
        {
            return;
        }

        // Determine region to process
        int startSample = 0;
        int numSamples = buffer.getNumSamples();
        bool isSelection = false;

        if (m_waveformDisplay.hasSelection())
        {
            // Normalize selection only
            startSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionStart());
            int endSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionEnd());
            numSamples = endSample - startSample;
            isSelection = true;
        }

        // Create a temporary buffer with just the region to normalize
        juce::AudioBuffer<float> regionBuffer;
        regionBuffer.setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            regionBuffer.copyFrom(ch, 0, buffer, ch, startSample, numSamples);
        }

        // Store before state for undo
        juce::AudioBuffer<float> beforeBuffer;
        beforeBuffer.makeCopyOf(regionBuffer, true);

        // Calculate peak level before normalization for status message
        float peakBefore = AudioProcessor::getPeakLevelDB(regionBuffer);

        // Start a new transaction
        juce::String transactionName = juce::String::formatted(
            "Normalize (%s)",
            isSelection ? "selection" : "entire file"
        );
        m_undoManager.beginNewTransaction(transactionName);

        // Create undo action (perform() will apply the normalization)
        auto* undoAction = new NormalizeUndoAction(
            m_audioBufferManager,
            m_waveformDisplay,
            m_audioEngine,
            beforeBuffer,
            startSample,
            numSamples,
            isSelection
        );

        // perform() calls NormalizeUndoAction::perform() which normalizes and updates display
        m_undoManager.perform(undoAction);

        // Mark as modified
        m_isModified = true;

        // Log the operation
        juce::String region = isSelection ? "selection" : "entire file";
        juce::String message = juce::String::formatted(
            "Normalized %s (peak: %.2f dB → 0.0 dB, gain: %+.2f dB)",
            region.toRawUTF8(), peakBefore, -peakBefore);
        juce::Logger::writeToLog(message);
    }

    /**
     * Undo action for gain adjustments.
     * Stores the before/after state and region information.
     */
    class GainUndoAction : public juce::UndoableAction
    {
    public:
        GainUndoAction(AudioBufferManager& bufferManager,
                      WaveformDisplay& waveform,
                      AudioEngine& audioEngine,
                      const juce::AudioBuffer<float>& beforeBuffer,
                      int startSample,
                      int numSamples,
                      float gainDB,
                      bool isSelection)
            : m_bufferManager(bufferManager),
              m_waveformDisplay(waveform),
              m_audioEngine(audioEngine),
              m_beforeBuffer(),
              m_startSample(startSample),
              m_numSamples(numSamples),
              m_gainDB(gainDB),
              m_isSelection(isSelection)
        {
            // Store only the affected region to save memory
            m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
            m_beforeBuffer.makeCopyOf(beforeBuffer, true);
        }

        bool perform() override
        {
            // Check playback state before applying gain
            bool wasPlaying = m_audioEngine.isPlaying();
            double positionBeforeEdit = m_audioEngine.getCurrentPosition();

            juce::Logger::writeToLog(juce::String::formatted(
                "GainUndoAction::perform - Before edit: playing=%s, position=%.3f",
                wasPlaying ? "YES" : "NO", positionBeforeEdit));

            // Apply gain to the current buffer
            auto& buffer = m_bufferManager.getMutableBuffer();
            AudioProcessor::applyGainToRange(buffer, m_gainDB, m_startSample, m_numSamples);

            // Reload buffer in AudioEngine - preserve playback if active
            bool reloadSuccess = m_audioEngine.reloadBufferPreservingPlayback(
                buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

            juce::Logger::writeToLog(juce::String::formatted(
                "GainUndoAction::perform - After reload: success=%s, playing=%s, position=%.3f",
                reloadSuccess ? "YES" : "NO",
                m_audioEngine.isPlaying() ? "YES" : "NO",
                m_audioEngine.getCurrentPosition()));

            // Update waveform display - preserve view and selection
            m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                              true, true); // preserveView=true, preserveEditCursor=true

            // Log the operation
            juce::String region = m_isSelection ? "selection" : "entire file";
            juce::String message = juce::String::formatted("Applied %+.1f dB gain to %s",
                                                            m_gainDB, region.toRawUTF8());
            juce::Logger::writeToLog(message);

            return true;
        }

        bool undo() override
        {
            // Restore the before state (only the affected region)
            auto& buffer = m_bufferManager.getMutableBuffer();

            // Copy the affected region from before buffer back to original position
            for (int ch = 0; ch < m_beforeBuffer.getNumChannels(); ++ch)
            {
                buffer.copyFrom(ch, m_startSample, m_beforeBuffer, ch, 0, m_numSamples);
            }

            // Reload buffer in AudioEngine - preserve playback if active
            m_audioEngine.reloadBufferPreservingPlayback(buffer, m_bufferManager.getSampleRate(),
                                                        buffer.getNumChannels());

            // Update waveform display - preserve view and selection
            m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                              true, true); // preserveView=true, preserveEditCursor=true

            return true;
        }

    private:
        AudioBufferManager& m_bufferManager;
        WaveformDisplay& m_waveformDisplay;
        AudioEngine& m_audioEngine;
        juce::AudioBuffer<float> m_beforeBuffer;
        int m_startSample;
        int m_numSamples;
        float m_gainDB;
        bool m_isSelection;
    };

    //==============================================================================
    // Fade In/Out helpers

    /**
     * Apply fade in to selection.
     * Creates undo action for the fade in operation.
     */
    void applyFadeIn()
    {
        if (!m_audioEngine.isFileLoaded())
        {
            return;
        }

        // Fade in requires a selection
        if (!m_waveformDisplay.hasSelection())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Fade In",
                "Please select a region to apply fade in.",
                "OK"
            );
            return;
        }

        // Get current buffer
        auto& buffer = m_audioBufferManager.getMutableBuffer();
        if (buffer.getNumSamples() == 0)
        {
            return;
        }

        // Get selection bounds
        int startSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionStart());
        int endSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionEnd());
        int numSamples = endSample - startSample;

        if (numSamples <= 0)
        {
            return;
        }

        // Create a temporary buffer with just the region to fade
        juce::AudioBuffer<float> regionBuffer;
        regionBuffer.setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            regionBuffer.copyFrom(ch, 0, buffer, ch, startSample, numSamples);
        }

        // Store before state for undo
        juce::AudioBuffer<float> beforeBuffer;
        beforeBuffer.makeCopyOf(regionBuffer, true);

        // Start a new transaction
        juce::String transactionName = "Fade In (selection)";
        m_undoManager.beginNewTransaction(transactionName);

        // Create undo action (perform() will apply the fade)
        auto* undoAction = new FadeInUndoAction(
            m_audioBufferManager,
            m_waveformDisplay,
            m_audioEngine,
            beforeBuffer,
            startSample,
            numSamples
        );

        // perform() calls FadeInUndoAction::perform() which applies fade and updates display
        m_undoManager.perform(undoAction);

        // Mark as modified
        m_isModified = true;

        // Log the operation
        juce::String message = juce::String::formatted(
            "Applied fade in to selection (%d samples, %.3f seconds)",
            numSamples, (double)numSamples / m_audioBufferManager.getSampleRate());
        juce::Logger::writeToLog(message);
    }

    /**
     * Apply fade out to selection.
     * Creates undo action for the fade out operation.
     */
    void applyFadeOut()
    {
        if (!m_audioEngine.isFileLoaded())
        {
            return;
        }

        // Fade out requires a selection
        if (!m_waveformDisplay.hasSelection())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Fade Out",
                "Please select a region to apply fade out.",
                "OK"
            );
            return;
        }

        // Get current buffer
        auto& buffer = m_audioBufferManager.getMutableBuffer();
        if (buffer.getNumSamples() == 0)
        {
            return;
        }

        // Get selection bounds
        int startSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionStart());
        int endSample = m_audioBufferManager.timeToSample(m_waveformDisplay.getSelectionEnd());
        int numSamples = endSample - startSample;

        if (numSamples <= 0)
        {
            return;
        }

        // Create a temporary buffer with just the region to fade
        juce::AudioBuffer<float> regionBuffer;
        regionBuffer.setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            regionBuffer.copyFrom(ch, 0, buffer, ch, startSample, numSamples);
        }

        // Store before state for undo
        juce::AudioBuffer<float> beforeBuffer;
        beforeBuffer.makeCopyOf(regionBuffer, true);

        // Start a new transaction
        juce::String transactionName = "Fade Out (selection)";
        m_undoManager.beginNewTransaction(transactionName);

        // Create undo action (perform() will apply the fade)
        auto* undoAction = new FadeOutUndoAction(
            m_audioBufferManager,
            m_waveformDisplay,
            m_audioEngine,
            beforeBuffer,
            startSample,
            numSamples
        );

        // perform() calls FadeOutUndoAction::perform() which applies fade and updates display
        m_undoManager.perform(undoAction);

        // Mark as modified
        m_isModified = true;

        // Log the operation
        juce::String message = juce::String::formatted(
            "Applied fade out to selection (%d samples, %.3f seconds)",
            numSamples, (double)numSamples / m_audioBufferManager.getSampleRate());
        juce::Logger::writeToLog(message);
    }

    /**
     * Undo action for normalization.
     * Stores the before state and region information.
     */
    class NormalizeUndoAction : public juce::UndoableAction
    {
    public:
        NormalizeUndoAction(AudioBufferManager& bufferManager,
                           WaveformDisplay& waveform,
                           AudioEngine& audioEngine,
                           const juce::AudioBuffer<float>& beforeBuffer,
                           int startSample,
                           int numSamples,
                           bool isSelection)
            : m_bufferManager(bufferManager),
              m_waveformDisplay(waveform),
              m_audioEngine(audioEngine),
              m_beforeBuffer(),
              m_startSample(startSample),
              m_numSamples(numSamples),
              m_isSelection(isSelection)
        {
            // Store only the affected region to save memory
            m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
            m_beforeBuffer.makeCopyOf(beforeBuffer, true);
        }

        bool perform() override
        {
            // Get the buffer and create a region buffer for normalization
            auto& buffer = m_bufferManager.getMutableBuffer();

            // Extract the region to normalize
            juce::AudioBuffer<float> regionBuffer;
            regionBuffer.setSize(buffer.getNumChannels(), m_numSamples);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                regionBuffer.copyFrom(ch, 0, buffer, ch, m_startSample, m_numSamples);
            }

            // Apply normalization to the region
            AudioProcessor::normalize(regionBuffer, 0.0f); // 0dB target

            // Copy normalized region back to main buffer
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                buffer.copyFrom(ch, m_startSample, regionBuffer, ch, 0, m_numSamples);
            }

            // Reload buffer in AudioEngine - preserve playback if active
            m_audioEngine.reloadBufferPreservingPlayback(
                buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

            // Update waveform display - preserve view and selection
            m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                              true, true); // preserveView=true, preserveEditCursor=true

            // Log the operation
            juce::String region = m_isSelection ? "selection" : "entire file";
            juce::String message = juce::String::formatted("Normalized %s to 0dB peak",
                                                            region.toRawUTF8());
            juce::Logger::writeToLog(message);

            return true;
        }

        bool undo() override
        {
            // Restore the before state (only the affected region)
            auto& buffer = m_bufferManager.getMutableBuffer();

            // Copy the affected region from before buffer back to original position
            for (int ch = 0; ch < m_beforeBuffer.getNumChannels(); ++ch)
            {
                buffer.copyFrom(ch, m_startSample, m_beforeBuffer, ch, 0, m_numSamples);
            }

            // Reload buffer in AudioEngine - preserve playback if active
            m_audioEngine.reloadBufferPreservingPlayback(buffer, m_bufferManager.getSampleRate(),
                                                        buffer.getNumChannels());

            // Update waveform display - preserve view and selection
            m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                              true, true); // preserveView=true, preserveEditCursor=true

            return true;
        }

    private:
        AudioBufferManager& m_bufferManager;
        WaveformDisplay& m_waveformDisplay;
        AudioEngine& m_audioEngine;
        juce::AudioBuffer<float> m_beforeBuffer;
        int m_startSample;
        int m_numSamples;
        bool m_isSelection;
    };

    /**
     * Undo action for fade in.
     * Stores the before state and region information.
     */
    class FadeInUndoAction : public juce::UndoableAction
    {
    public:
        FadeInUndoAction(AudioBufferManager& bufferManager,
                        WaveformDisplay& waveform,
                        AudioEngine& audioEngine,
                        const juce::AudioBuffer<float>& beforeBuffer,
                        int startSample,
                        int numSamples)
            : m_bufferManager(bufferManager),
              m_waveformDisplay(waveform),
              m_audioEngine(audioEngine),
              m_beforeBuffer(),
              m_startSample(startSample),
              m_numSamples(numSamples)
        {
            // Store only the affected region to save memory
            m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
            m_beforeBuffer.makeCopyOf(beforeBuffer, true);
        }

        bool perform() override
        {
            // Get the buffer and create a region buffer for fade in
            auto& buffer = m_bufferManager.getMutableBuffer();

            // Extract the region to fade
            juce::AudioBuffer<float> regionBuffer;
            regionBuffer.setSize(buffer.getNumChannels(), m_numSamples);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                regionBuffer.copyFrom(ch, 0, buffer, ch, m_startSample, m_numSamples);
            }

            // Apply fade in to the region
            AudioProcessor::fadeIn(regionBuffer, m_numSamples);

            // Copy faded region back to main buffer
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                buffer.copyFrom(ch, m_startSample, regionBuffer, ch, 0, m_numSamples);
            }

            // Reload buffer in AudioEngine - preserve playback if active
            m_audioEngine.reloadBufferPreservingPlayback(
                buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

            // Update waveform display - preserve view and selection
            m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                              true, true); // preserveView=true, preserveEditCursor=true

            // Log the operation
            juce::String message = "Applied fade in to selection";
            juce::Logger::writeToLog(message);

            return true;
        }

        bool undo() override
        {
            // Restore the before state (only the affected region)
            auto& buffer = m_bufferManager.getMutableBuffer();

            // Copy the affected region from before buffer back to original position
            for (int ch = 0; ch < m_beforeBuffer.getNumChannels(); ++ch)
            {
                buffer.copyFrom(ch, m_startSample, m_beforeBuffer, ch, 0, m_numSamples);
            }

            // Reload buffer in AudioEngine - preserve playback if active
            m_audioEngine.reloadBufferPreservingPlayback(buffer, m_bufferManager.getSampleRate(),
                                                        buffer.getNumChannels());

            // Update waveform display - preserve view and selection
            m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                              true, true); // preserveView=true, preserveEditCursor=true

            return true;
        }

    private:
        AudioBufferManager& m_bufferManager;
        WaveformDisplay& m_waveformDisplay;
        AudioEngine& m_audioEngine;
        juce::AudioBuffer<float> m_beforeBuffer;
        int m_startSample;
        int m_numSamples;
    };

    /**
     * Undo action for fade out.
     * Stores the before state and region information.
     */
    class FadeOutUndoAction : public juce::UndoableAction
    {
    public:
        FadeOutUndoAction(AudioBufferManager& bufferManager,
                         WaveformDisplay& waveform,
                         AudioEngine& audioEngine,
                         const juce::AudioBuffer<float>& beforeBuffer,
                         int startSample,
                         int numSamples)
            : m_bufferManager(bufferManager),
              m_waveformDisplay(waveform),
              m_audioEngine(audioEngine),
              m_beforeBuffer(),
              m_startSample(startSample),
              m_numSamples(numSamples)
        {
            // Store only the affected region to save memory
            m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
            m_beforeBuffer.makeCopyOf(beforeBuffer, true);
        }

        bool perform() override
        {
            // Get the buffer and create a region buffer for fade out
            auto& buffer = m_bufferManager.getMutableBuffer();

            // Extract the region to fade
            juce::AudioBuffer<float> regionBuffer;
            regionBuffer.setSize(buffer.getNumChannels(), m_numSamples);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                regionBuffer.copyFrom(ch, 0, buffer, ch, m_startSample, m_numSamples);
            }

            // Apply fade out to the region
            AudioProcessor::fadeOut(regionBuffer, m_numSamples);

            // Copy faded region back to main buffer
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                buffer.copyFrom(ch, m_startSample, regionBuffer, ch, 0, m_numSamples);
            }

            // Reload buffer in AudioEngine - preserve playback if active
            m_audioEngine.reloadBufferPreservingPlayback(
                buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

            // Update waveform display - preserve view and selection
            m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                              true, true); // preserveView=true, preserveEditCursor=true

            // Log the operation
            juce::String message = "Applied fade out to selection";
            juce::Logger::writeToLog(message);

            return true;
        }

        bool undo() override
        {
            // Restore the before state (only the affected region)
            auto& buffer = m_bufferManager.getMutableBuffer();

            // Copy the affected region from before buffer back to original position
            for (int ch = 0; ch < m_beforeBuffer.getNumChannels(); ++ch)
            {
                buffer.copyFrom(ch, m_startSample, m_beforeBuffer, ch, 0, m_numSamples);
            }

            // Reload buffer in AudioEngine - preserve playback if active
            m_audioEngine.reloadBufferPreservingPlayback(buffer, m_bufferManager.getSampleRate(),
                                                        buffer.getNumChannels());

            // Update waveform display - preserve view and selection
            m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                              true, true); // preserveView=true, preserveEditCursor=true

            return true;
        }

    private:
        AudioBufferManager& m_bufferManager;
        WaveformDisplay& m_waveformDisplay;
        AudioEngine& m_audioEngine;
        juce::AudioBuffer<float> m_beforeBuffer;
        int m_startSample;
        int m_numSamples;
    };

private:
    AudioEngine m_audioEngine;
    AudioFileManager m_fileManager;
    AudioBufferManager m_audioBufferManager;
    juce::ApplicationCommandManager m_commandManager;
    juce::UndoManager m_undoManager;
    std::unique_ptr<juce::FileChooser> m_fileChooser;
    WaveformDisplay m_waveformDisplay;
    TransportControls m_transportControls;
    SelectionInfoPanel m_selectionInfo;
    Meters m_meters;

    bool m_isModified;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

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
        // Create main window
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        // Clean up on exit
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        // User requested quit (Cmd+Q, Alt+F4, etc.)
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
        MainWindow(juce::String name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);

            // Create main component
            auto* mainComp = new MainComponent();
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
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(WaveEditApplication)
