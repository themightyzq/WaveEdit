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
#include "UI/ErrorDialog.h"
#include "UI/TabComponent.h"
#include "Utils/Document.h"
#include "Utils/DocumentManager.h"

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
                      public juce::Timer,
                      public DocumentManager::Listener
{
public:
    MainComponent()
        : m_tabComponent(m_documentManager)
    {
        setSize(1200, 750);

        // Listen to document manager events
        m_documentManager.addListener(this);

        // Setup tab component
        addAndMakeVisible(m_tabComponent);

        // Setup no-file label
        m_noFileLabel.setText("No file open", juce::dontSendNotification);
        m_noFileLabel.setFont(juce::Font(20.0f));
        m_noFileLabel.setJustificationType(juce::Justification::centred);
        m_noFileLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible(m_noFileLabel);

        // Create container for current document components
        m_currentDocumentContainer = std::make_unique<juce::Component>();
        addAndMakeVisible(*m_currentDocumentContainer);

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

        // Update component visibility based on whether we have documents
        updateComponentVisibility();
    }

    ~MainComponent() override
    {
        // Clear all undo histories before closing documents (prevents dangling references)
        for (int i = 0; i < m_documentManager.getNumDocuments(); ++i)
        {
            if (auto* doc = m_documentManager.getDocument(i))
            {
                doc->getUndoManager().clearUndoHistory();
                doc->getAudioEngine().stop();
            }
        }

        stopTimer();
        m_documentManager.removeListener(this);
    }

    //==============================================================================
    // DocumentManager::Listener methods

    void currentDocumentChanged(Document* newDocument) override
    {
        // Stop audio in previous document (not the new one!)
        if (m_previousDocument && m_previousDocument != newDocument)
        {
            m_previousDocument->getAudioEngine().stop();
        }

        // Update tracking
        m_previousDocument = newDocument;

        // Update window title to reflect new document
        updateWindowTitle();

        // Update UI to show new document
        updateComponentVisibility();

        // Repaint to update display
        repaint();
    }

    void documentAdded(Document* document, int index) override
    {
        // Update tab component to show new tab
        updateComponentVisibility();
    }

    void documentRemoved(Document* document, int index) override
    {
        // Update tab component to remove tab
        updateComponentVisibility();
    }

    //==============================================================================
    // UI Component Management

    /**
     * Updates visibility and arrangement of components based on current document state.
     */
    void updateComponentVisibility()
    {
        auto* doc = getCurrentDocument();
        bool hasDoc = (doc != nullptr);

        // Show/hide tab bar and no-file label
        m_tabComponent.setVisible(hasDoc);
        m_noFileLabel.setVisible(!hasDoc);
        m_currentDocumentContainer->setVisible(hasDoc);

        // Clear current container
        m_currentDocumentContainer->removeAllChildren();

        if (hasDoc)
        {
            // Add current document's components to container
            m_currentDocumentContainer->addAndMakeVisible(doc->getWaveformDisplay());
            m_currentDocumentContainer->addAndMakeVisible(doc->getTransportControls());
            // Note: SelectionInfoPanel and Meters are part of the document's components
        }

        // Trigger layout update
        resized();
    }

    //==============================================================================
    // Edit Operations

    /**
     * Select all audio in the current file.
     */
    void selectAll()
    {
        auto* doc = getCurrentDocument();
        if (!doc || !doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        doc->getWaveformDisplay().setSelection(0.0, doc->getBufferManager().getLengthInSeconds());
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
        auto* doc = getCurrentDocument();
        if (!doc || !doc->getWaveformDisplay().hasSelection())
        {
            return false;
        }

        auto selectionStart = doc->getWaveformDisplay().getSelectionStart();
        auto selectionEnd = doc->getWaveformDisplay().getSelectionEnd();

        // Check for negative times
        if (selectionStart < 0.0 || selectionEnd < 0.0)
        {
            juce::Logger::writeToLog(juce::String::formatted(
                "Invalid selection: negative time (start=%.6f, end=%.6f)",
                selectionStart, selectionEnd));
            return false;
        }

        // Check for times beyond duration
        double maxDuration = doc->getBufferManager().getLengthInSeconds();
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
        auto* doc = getCurrentDocument();
        if (!doc)
        {
            return;
        }

        // CRITICAL FIX (2025-10-07): Validate selection before converting to samples
        if (!validateSelection() || !doc->getBufferManager().hasAudioData())
        {
            return;
        }

        auto selectionStart = doc->getWaveformDisplay().getSelectionStart();
        auto selectionEnd = doc->getWaveformDisplay().getSelectionEnd();

        // Convert time to samples (now safe - selection is validated)
        auto startSample = doc->getBufferManager().timeToSample(selectionStart);
        auto endSample = doc->getBufferManager().timeToSample(selectionEnd);

        // CRITICAL FIX (2025-10-08): getAudioRange expects COUNT, not end position!
        // Calculate number of samples in selection
        auto numSamples = endSample - startSample;

        // Get the audio data for the selection
        auto audioRange = doc->getBufferManager().getAudioRange(startSample, numSamples);

        if (audioRange.getNumSamples() > 0)
        {
            AudioClipboard::getInstance().copyAudio(audioRange, doc->getBufferManager().getSampleRate());

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
        auto* doc = getCurrentDocument();
        if (!doc)
        {
            return;
        }

        // CRITICAL FIX (2025-10-07): Validate selection before operations
        if (!validateSelection() || !doc->getBufferManager().hasAudioData())
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
        auto* doc = getCurrentDocument();
        if (!doc || !AudioClipboard::getInstance().hasAudio() || !doc->getBufferManager().hasAudioData())
        {
            return;
        }

        // CRITICAL FIX (BLOCKER #3): Begin new transaction for each edit operation
        doc->getUndoManager().beginNewTransaction("Paste");

        // Use edit cursor position if available, otherwise use playback position
        double cursorPos;
        if (doc->getWaveformDisplay().hasEditCursor())
        {
            cursorPos = doc->getWaveformDisplay().getEditCursorPosition();
        }
        else
        {
            cursorPos = doc->getWaveformDisplay().getPlaybackPosition();
        }

        auto insertSample = doc->getBufferManager().timeToSample(cursorPos);

        // Check for sample rate mismatch
        auto clipboardSampleRate = AudioClipboard::getInstance().getSampleRate();
        auto currentSampleRate = doc->getBufferManager().getSampleRate();

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
            if (doc->getWaveformDisplay().hasSelection() && validateSelection())
            {
                auto selectionStart = doc->getWaveformDisplay().getSelectionStart();
                auto selectionEnd = doc->getWaveformDisplay().getSelectionEnd();
                // CRITICAL FIX (2025-10-07): Convert to samples (now safe - validated above)
                auto startSample = doc->getBufferManager().timeToSample(selectionStart);
                auto endSample = doc->getBufferManager().timeToSample(selectionEnd);

                // Create undoable replace action
                auto replaceAction = new ReplaceAction(
                    doc->getBufferManager(),
                    doc->getAudioEngine(),
                    doc->getWaveformDisplay(),
                    startSample,
                    endSample - startSample,
                    clipboardAudio
                );

                // Perform the replace and add to undo manager
                doc->getUndoManager().perform(replaceAction);

                juce::Logger::writeToLog(juce::String::formatted(
                    "Pasted %.2f seconds from clipboard, replacing selection (undoable)",
                    clipboardAudio.getNumSamples() / currentSampleRate));
            }
            else
            {
                // Create undoable insert action
                auto insertAction = new InsertAction(
                    doc->getBufferManager(),
                    doc->getAudioEngine(),
                    doc->getWaveformDisplay(),
                    insertSample,
                    clipboardAudio
                );

                // Perform the insert and add to undo manager
                doc->getUndoManager().perform(insertAction);

                juce::Logger::writeToLog(juce::String::formatted(
                    "Pasted %.2f seconds from clipboard (undoable)",
                    clipboardAudio.getNumSamples() / currentSampleRate));
            }

            // Mark as modified
            doc->setModified(true);

            // Clear selection after paste
            doc->getWaveformDisplay().clearSelection();

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
        auto* doc = getCurrentDocument();
        if (!doc) return;

        // CRITICAL FIX (2025-10-07): Validate selection before converting to samples
        if (!validateSelection() || !doc->getBufferManager().hasAudioData())
        {
            return;
        }

        // CRITICAL FIX (BLOCKER #3): Begin new transaction for each edit operation
        doc->getUndoManager().beginNewTransaction("Delete");

        auto selectionStart = doc->getWaveformDisplay().getSelectionStart();
        auto selectionEnd = doc->getWaveformDisplay().getSelectionEnd();

        // Convert time to samples (now safe - selection is validated)
        auto startSample = doc->getBufferManager().timeToSample(selectionStart);
        auto endSample = doc->getBufferManager().timeToSample(selectionEnd);

        // Create undoable delete action
        auto deleteAction = new DeleteAction(
            doc->getBufferManager(),
            doc->getAudioEngine(),
            doc->getWaveformDisplay(),
            startSample,
            endSample - startSample
        );

        // Perform the delete and add to undo manager
        doc->getUndoManager().perform(deleteAction);

        // Mark as modified
        doc->setModified(true);

        // Clear selection after delete
        doc->getWaveformDisplay().clearSelection();

        // CRITICAL FIX (Phase 1.4 - Edit Cursor Preservation):
        // Set edit cursor at the deletion point for professional workflow
        // This enables cursor preservation during subsequent operations
        doc->getWaveformDisplay().setEditCursor(selectionStart);

        // NOTE: Waveform display already updated by undo action's updatePlaybackAndDisplay()
        // Removed redundant reloadFromBuffer() call for performance

        juce::Logger::writeToLog(juce::String::formatted(
            "Deleted %.2f seconds (undoable), cursor set at %.2f",
            selectionEnd - selectionStart, selectionStart));

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto* doc = getCurrentDocument();

        // CRITICAL FIX: Always draw background and status bar, regardless of document state
        // The UI chrome must always be visible

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

        // Status bar text - adapt based on document state
        if (doc && doc->getAudioEngine().isFileLoaded())
        {
            g.setColour(juce::Colours::white);
            g.setFont(12.0f);

            auto leftSection = statusBar.reduced(10, 0);

            juce::String fileDisplayName = doc->getAudioEngine().getCurrentFile().getFileName();
            if (doc->isModified())
            {
                fileDisplayName += " *";
            }

            juce::String info = juce::String::formatted(
                "%s | %.1f kHz | %d ch | %d bit | %.2f / %.2f s",
                fileDisplayName.toRawUTF8(),
                doc->getAudioEngine().getSampleRate() / 1000.0,
                doc->getAudioEngine().getNumChannels(),
                doc->getAudioEngine().getBitDepth(),
                doc->getAudioEngine().getCurrentPosition(),
                doc->getAudioEngine().getTotalLength());

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

            auto unitType = doc->getWaveformDisplay().getSnapUnit();
            int increment = doc->getWaveformDisplay().getSnapIncrement();
            bool zeroCrossing = doc->getWaveformDisplay().isZeroCrossingEnabled();

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
            switch (doc->getAudioEngine().getPlaybackState())
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

        if (hasCurrentDocument())
        {
            // Tab bar at top (32px height)
            m_tabComponent.setBounds(bounds.removeFromTop(32));

            // Current document container takes remaining space
            m_currentDocumentContainer->setBounds(bounds);

            // Layout document components within container
            if (auto* doc = getCurrentDocument())
            {
                auto containerBounds = m_currentDocumentContainer->getLocalBounds();

                // Transport controls at top (80px height)
                doc->getTransportControls().setBounds(containerBounds.removeFromTop(80));

                // Waveform display takes remaining space
                // TODO: Add SelectionInfoPanel and Meters when Document class is updated
                doc->getWaveformDisplay().setBounds(containerBounds);
            }
        }
        else
        {
            // Center the no-file label
            m_noFileLabel.setBounds(bounds);
        }
    }

    void timerCallback() override
    {
        auto* doc = getCurrentDocument();
        if (!doc)
        {
            updateWindowTitle(); // Update window title even when no document
            repaint(); // Update status bar only
            return;
        }

        // Update window title (checks if modified state changed)
        updateWindowTitle();

        // Update waveform display with current playback position
        if (doc->getAudioEngine().isPlaying())
        {
            doc->getWaveformDisplay().setPlaybackPosition(doc->getAudioEngine().getCurrentPosition());
            repaint(); // Update status bar

            // TODO: Update level meters when Document class includes Meters
            // meters.setPeakLevel(0, doc->getAudioEngine().getPeakLevel(0));
            // meters.setPeakLevel(1, doc->getAudioEngine().getPeakLevel(1));
            // meters.setRMSLevel(0, doc->getAudioEngine().getRMSLevel(0));
            // meters.setRMSLevel(1, doc->getAudioEngine().getRMSLevel(1));
        }
        else
        {
            // TODO: Reset meters when Document class includes Meters
            // meters.setPeakLevel(0, 0.0f);
            // meters.setPeakLevel(1, 0.0f);
            // meters.setRMSLevel(0, 0.0f);
            // meters.setRMSLevel(1, 0.0f);
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
        auto* doc = getCurrentDocument();

        // CRITICAL FIX: Always set command info (text, shortcuts) so menus and shortcuts work
        // Use setActive() to disable document-dependent commands when no document exists

        switch (commandID)
        {
            case CommandIDs::fileOpen:
                result.setInfo("Open...", "Open an audio file", "File", 0);
                result.addDefaultKeypress('o', juce::ModifierKeys::commandModifier);
                // Always available (no document required)
                break;

            case CommandIDs::fileSave:
                result.setInfo("Save", "Save the current file", "File", 0);
                result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->isModified());
                break;

            case CommandIDs::fileSaveAs:
                result.setInfo("Save As...", "Save the current file with a new name", "File", 0);
                result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::fileClose:
                result.setInfo("Close", "Close the current file", "File", 0);
                result.addDefaultKeypress('w', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::fileExit:
                result.setInfo("Exit", "Exit the application", "File", 0);
                result.addDefaultKeypress('q', juce::ModifierKeys::commandModifier);
                // Always available (no document required)
                break;

            case CommandIDs::editUndo:
                result.setInfo("Undo", "Undo the last operation", "Edit", 0);
                result.addDefaultKeypress('z', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getUndoManager().canUndo());
                break;

            case CommandIDs::editRedo:
                result.setInfo("Redo", "Redo the last undone operation", "Edit", 0);
                result.addDefaultKeypress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getUndoManager().canRedo());
                break;

            case CommandIDs::editSelectAll:
                result.setInfo("Select All", "Select all audio", "Edit", 0);
                result.addDefaultKeypress('a', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::editCut:
                result.setInfo("Cut", "Cut selection to clipboard", "Edit", 0);
                result.addDefaultKeypress('x', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::editCopy:
                result.setInfo("Copy", "Copy selection to clipboard", "Edit", 0);
                result.addDefaultKeypress('c', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::editPaste:
                result.setInfo("Paste", "Paste from clipboard", "Edit", 0);
                result.addDefaultKeypress('v', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && AudioClipboard::getInstance().hasAudio());
                break;

            case CommandIDs::editDelete:
                result.setInfo("Delete", "Delete selection", "Edit", 0);
                result.addDefaultKeypress(juce::KeyPress::deleteKey, 0);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::playbackPlay:
                result.setInfo("Play/Stop", "Play or stop playback", "Playback", 0);
                result.addDefaultKeypress(juce::KeyPress::spaceKey, 0);
                result.addDefaultKeypress(juce::KeyPress::F12Key, 0);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::playbackPause:
                result.setInfo("Pause", "Pause or resume playback", "Playback", 0);
                result.addDefaultKeypress(juce::KeyPress::returnKey, 0);
                result.addDefaultKeypress(juce::KeyPress::F12Key, juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::playbackStop:
                result.setInfo("Stop", "Stop playback", "Playback", 0);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::playbackLoop:
                result.setInfo("Loop", "Toggle loop mode", "Playback", 0);
                result.addDefaultKeypress('q', 0);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            // View/Zoom commands
            case CommandIDs::viewZoomIn:
                result.setInfo("Zoom In", "Zoom in 2x", "View", 0);
                result.addDefaultKeypress('=', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::viewZoomOut:
                result.setInfo("Zoom Out", "Zoom out 2x", "View", 0);
                result.addDefaultKeypress('-', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::viewZoomFit:
                result.setInfo("Zoom to Fit", "Fit entire waveform to view", "View", 0);
                result.addDefaultKeypress('0', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::viewZoomSelection:
                result.setInfo("Zoom to Selection", "Zoom to current selection", "View", 0);
                result.addDefaultKeypress('e', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::viewZoomOneToOne:
                result.setInfo("Zoom 1:1", "Zoom to 1:1 sample resolution", "View", 0);
                result.addDefaultKeypress('1', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            // Navigation commands
            case CommandIDs::navigateLeft:
                result.setInfo("Navigate Left", "Move cursor left by snap increment", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::leftKey, 0);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigateRight:
                result.setInfo("Navigate Right", "Move cursor right by snap increment", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::rightKey, 0);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigateStart:
                result.setInfo("Jump to Start", "Jump to start of file", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::leftKey, juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigateEnd:
                result.setInfo("Jump to End", "Jump to end of file", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::rightKey, juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigatePageLeft:
                result.setInfo("Page Left", "Move cursor left by page increment", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::pageUpKey, 0);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigatePageRight:
                result.setInfo("Page Right", "Move cursor right by page increment", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::pageDownKey, 0);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigateHomeVisible:
                result.setInfo("Jump to Visible Start", "Jump to first visible sample", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::homeKey, 0);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigateEndVisible:
                result.setInfo("Jump to Visible End", "Jump to last visible sample", "Navigation", 0);
                result.addDefaultKeypress(juce::KeyPress::endKey, 0);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigateCenterView:
                result.setInfo("Center View", "Center view on cursor", "Navigation", 0);
                result.addDefaultKeypress('.', 0);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            // Selection extension commands
            case CommandIDs::selectExtendLeft:
                result.setInfo("Extend Selection Left", "Extend selection left by increment", "Selection", 0);
                result.addDefaultKeypress(juce::KeyPress::leftKey, juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::selectExtendRight:
                result.setInfo("Extend Selection Right", "Extend selection right by increment", "Selection", 0);
                result.addDefaultKeypress(juce::KeyPress::rightKey, juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::selectExtendStart:
                result.setInfo("Extend to Visible Start", "Extend selection to visible start", "Selection", 0);
                result.addDefaultKeypress(juce::KeyPress::homeKey, juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::selectExtendEnd:
                result.setInfo("Extend to Visible End", "Extend selection to visible end", "Selection", 0);
                result.addDefaultKeypress(juce::KeyPress::endKey, juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::selectExtendPageLeft:
                result.setInfo("Extend Selection Page Left", "Extend selection left by page increment", "Selection", 0);
                result.addDefaultKeypress(juce::KeyPress::pageUpKey, juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::selectExtendPageRight:
                result.setInfo("Extend Selection Page Right", "Extend selection right by page increment", "Selection", 0);
                result.addDefaultKeypress(juce::KeyPress::pageDownKey, juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            // Snap commands
            case CommandIDs::snapCycleMode:
                result.setInfo("Cycle Snap Mode", "Cycle through snap modes", "Snap", 0);
                result.addDefaultKeypress('g', 0);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::snapToggleZeroCrossing:
                result.setInfo("Toggle Zero Crossing Snap", "Quick toggle zero crossing snap", "Snap", 0);
                result.addDefaultKeypress('z', 0);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            // Processing operations
            case CommandIDs::processGain:
                result.setInfo("Gain...", "Apply precise gain adjustment", "Process", 0);
                result.addDefaultKeypress('g', juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::processIncreaseGain:
                result.setInfo("Increase Gain", "Increase gain by 1 dB", "Process", 0);
                result.addDefaultKeypress(juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::processDecreaseGain:
                result.setInfo("Decrease Gain", "Decrease gain by 1 dB", "Process", 0);
                result.addDefaultKeypress(juce::KeyPress::downKey, juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::processNormalize:
                result.setInfo("Normalize...", "Normalize audio to peak level", "Process", 0);
                result.addDefaultKeypress('n', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::processFadeIn:
                result.setInfo("Fade In", "Apply linear fade in to selection", "Process", 0);
                result.addDefaultKeypress('i', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::processFadeOut:
                result.setInfo("Fade Out", "Apply linear fade out to selection", "Process", 0);
                result.addDefaultKeypress('o', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            default:
                break;
        }
    }

    bool perform(const InvocationInfo& info) override
    {
        auto* doc = getCurrentDocument();

        // CRITICAL FIX: Don't early return - allow document-independent commands
        // Commands like File → Open and File → Exit must work without a document

        switch (info.commandID)
        {
            case CommandIDs::fileOpen:
                openFile();
                return true;

            case CommandIDs::fileExit:
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
                return true;

            case CommandIDs::fileSave:
                if (!doc) return false;
                saveFile();
                return true;

            case CommandIDs::fileSaveAs:
                if (!doc) return false;
                saveFileAs();
                return true;

            case CommandIDs::fileClose:
                if (!doc) return false;
                closeFile();
                return true;

            case CommandIDs::editUndo:
                if (!doc) return false;
                if (doc->getUndoManager().canUndo())
                {
                    juce::Logger::writeToLog(juce::String::formatted(
                        "Undo: %s (stack depth before: %d)",
                        doc->getUndoManager().getUndoDescription().toRawUTF8(),
                        doc->getUndoManager().getNumberOfUnitsTakenUpByStoredCommands()));

                    doc->getUndoManager().undo();
                    doc->setModified(true);

                    juce::Logger::writeToLog(juce::String::formatted(
                        "After undo - Can undo: %s, Can redo: %s",
                        doc->getUndoManager().canUndo() ? "yes" : "no",
                        doc->getUndoManager().canRedo() ? "yes" : "no"));

                    repaint();
                }
                return true;

            case CommandIDs::editRedo:
                if (!doc) return false;
                if (doc->getUndoManager().canRedo())
                {
                    juce::Logger::writeToLog(juce::String::formatted(
                        "Redo: %s (stack depth before: %d)",
                        doc->getUndoManager().getRedoDescription().toRawUTF8(),
                        doc->getUndoManager().getNumberOfUnitsTakenUpByStoredCommands()));

                    doc->getUndoManager().redo();
                    doc->setModified(true);

                    juce::Logger::writeToLog(juce::String::formatted(
                        "After redo - Can undo: %s, Can redo: %s",
                        doc->getUndoManager().canUndo() ? "yes" : "no",
                        doc->getUndoManager().canRedo() ? "yes" : "no"));

                    repaint();
                }
                return true;

            case CommandIDs::editSelectAll:
                if (!doc) return false;
                selectAll();
                return true;

            case CommandIDs::editCut:
                if (!doc) return false;
                cutSelection();
                return true;

            case CommandIDs::editCopy:
                if (!doc) return false;
                copySelection();
                return true;

            case CommandIDs::editPaste:
                if (!doc) return false;
                pasteAtCursor();
                return true;

            case CommandIDs::editDelete:
                if (!doc) return false;
                deleteSelection();
                return true;

            case CommandIDs::playbackPlay:
                if (!doc) return false;
                togglePlayback();
                return true;

            case CommandIDs::playbackPause:
                if (!doc) return false;
                pausePlayback();
                return true;

            case CommandIDs::playbackStop:
                if (!doc) return false;
                stopPlayback();
                return true;

            case CommandIDs::playbackLoop:
                if (!doc) return false;
                toggleLoop();
                return true;

            // View/Zoom operations
            case CommandIDs::viewZoomIn:
                if (!doc) return false;
                doc->getWaveformDisplay().zoomIn();
                return true;

            case CommandIDs::viewZoomOut:
                if (!doc) return false;
                doc->getWaveformDisplay().zoomOut();
                return true;

            case CommandIDs::viewZoomFit:
                if (!doc) return false;
                doc->getWaveformDisplay().zoomToFit();
                return true;

            case CommandIDs::viewZoomSelection:
                if (!doc) return false;
                doc->getWaveformDisplay().zoomToSelection();
                return true;

            case CommandIDs::viewZoomOneToOne:
                if (!doc) return false;
                doc->getWaveformDisplay().zoomOneToOne();
                return true;

            // Navigation operations (simple movement)
            case CommandIDs::navigateLeft:
                if (!doc) return false;
                doc->getWaveformDisplay().navigateLeft(false);
                return true;

            case CommandIDs::navigateRight:
                if (!doc) return false;
                doc->getWaveformDisplay().navigateRight(false);
                return true;

            case CommandIDs::navigateStart:
                if (!doc) return false;
                doc->getWaveformDisplay().navigateToStart(false);
                return true;

            case CommandIDs::navigateEnd:
                if (!doc) return false;
                doc->getWaveformDisplay().navigateToEnd(false);
                return true;

            case CommandIDs::navigatePageLeft:
                if (!doc) return false;
                doc->getWaveformDisplay().navigatePageLeft();
                return true;

            case CommandIDs::navigatePageRight:
                if (!doc) return false;
                doc->getWaveformDisplay().navigatePageRight();
                return true;

            case CommandIDs::navigateHomeVisible:
                if (!doc) return false;
                doc->getWaveformDisplay().navigateToVisibleStart(false);
                return true;

            case CommandIDs::navigateEndVisible:
                if (!doc) return false;
                doc->getWaveformDisplay().navigateToVisibleEnd(false);
                return true;

            case CommandIDs::navigateCenterView:
                if (!doc) return false;
                doc->getWaveformDisplay().centerViewOnCursor();
                return true;

            // Selection extension operations
            case CommandIDs::selectExtendLeft:
                if (!doc) return false;
                doc->getWaveformDisplay().navigateLeft(true);
                return true;

            case CommandIDs::selectExtendRight:
                if (!doc) return false;
                doc->getWaveformDisplay().navigateRight(true);
                return true;

            case CommandIDs::selectExtendStart:
                if (!doc) return false;
                doc->getWaveformDisplay().navigateToVisibleStart(true);
                return true;

            case CommandIDs::selectExtendEnd:
                if (!doc) return false;
                doc->getWaveformDisplay().navigateToVisibleEnd(true);
                return true;

            case CommandIDs::selectExtendPageLeft:
                if (!doc) return false;
                juce::Logger::writeToLog("selectExtendPageLeft command triggered");
                doc->getWaveformDisplay().navigatePageLeft(true);
                return true;

            case CommandIDs::selectExtendPageRight:
                if (!doc) return false;
                juce::Logger::writeToLog("selectExtendPageRight command triggered");
                doc->getWaveformDisplay().navigatePageRight(true);
                return true;

            // Snap mode operations
            case CommandIDs::snapCycleMode:
                if (!doc) return false;
                cycleSnapMode();
                return true;

            case CommandIDs::snapToggleZeroCrossing:
                if (!doc) return false;
                toggleZeroCrossingSnap();
                return true;

            // Processing operations
            case CommandIDs::processGain:
                if (!doc) return false;
                showGainDialog();
                return true;

            case CommandIDs::processIncreaseGain:
                if (!doc) return false;
                applyGainAdjustment(+1.0f);
                return true;

            case CommandIDs::processDecreaseGain:
                if (!doc) return false;
                applyGainAdjustment(-1.0f);
                return true;

            case CommandIDs::processNormalize:
                if (!doc) return false;
                applyNormalize();
                return true;

            case CommandIDs::processFadeIn:
                if (!doc) return false;
                applyFadeIn();
                return true;

            case CommandIDs::processFadeOut:
                if (!doc) return false;
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

        // Open each dropped file in its own tab
        for (const auto& filePath : files)
        {
            juce::File file(filePath);
            if (file.existsAsFile() && file.hasFileExtension(".wav"))
            {
                auto* newDoc = m_documentManager.openDocument(file);
                if (newDoc)
                {
                    juce::Logger::writeToLog("Opened dropped file: " + file.getFileName());
                }
            }
        }
    }

    //==============================================================================
    // File Operations

    void openFile()
    {
        // Don't create new file chooser if one is already active
        if (m_fileChooser != nullptr)
        {
            juce::Logger::writeToLog("File chooser already active");
            return;
        }

        // Create file chooser - now supports multiple file selection
        m_fileChooser = std::make_unique<juce::FileChooser>(
            "Open Audio File(s)",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.wav",
            true);

        auto folderChooserFlags = juce::FileBrowserComponent::openMode |
                                  juce::FileBrowserComponent::canSelectFiles |
                                  juce::FileBrowserComponent::canSelectMultipleItems;

        m_fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& chooser)
        {
            // CRITICAL FIX: Always reset file chooser, even if no files selected or error occurs
            // This prevents "File chooser already active" error

            auto files = chooser.getResults();

            if (!files.isEmpty())
            {
                // Open each selected file in its own document tab
                for (const auto& file : files)
                {
                    if (file != juce::File())
                    {
                        // Use DocumentManager to open files (supports multiple tabs)
                        auto* newDoc = m_documentManager.openDocument(file);
                        if (newDoc)
                        {
                            juce::Logger::writeToLog("Opened file: " + file.getFileName());
                        }
                        else
                        {
                            // Show user-friendly error dialog
                            ErrorDialog::showFileError(
                                "open",
                                file.getFullPathName(),
                                "Unsupported format or corrupted data"
                            );
                        }
                    }
                }
            }

            // CRITICAL: Always clear the file chooser after use, regardless of outcome
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
            ErrorDialog::showFileError(
                "open",
                file.getFullPathName(),
                m_fileManager.getLastError()
            );
            return;
        }

        // Check file permissions
        if (!file.hasReadAccess())
        {
            ErrorDialog::showFileError(
                "open",
                file.getFullPathName(),
                "No read permission for this file"
            );
            return;
        }

        // Use DocumentManager to open the file
        auto* doc = m_documentManager.openDocument(file);
        if (doc != nullptr)
        {
            // Add to recent files
            Settings::getInstance().addRecentFile(file);

            // Document is now active, UI will update via listener
            juce::Logger::writeToLog("Opened file: " + file.getFileName());
        }
        else
        {
            ErrorDialog::showFileError(
                "open",
                file.getFullPathName(),
                "Could not load audio data from this file"
            );
        }
    }

    void saveFile()
    {
        auto* doc = getCurrentDocument();
        if (!doc || !doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        auto currentFile = doc->getAudioEngine().getCurrentFile();

        // Check if file exists and is writable
        if (!currentFile.existsAsFile())
        {
            // File doesn't exist, do Save As instead
            saveFileAs();
            return;
        }

        if (!currentFile.hasWriteAccess())
        {
            juce::String message = "No write permission for this file.";
            message += "\n\nUse 'Save As' to save to a different location.";
            ErrorDialog::show("Permission Error", message, ErrorDialog::Severity::Error);
            return;
        }

        // Save the edited audio buffer to the file
        bool saveSuccess = m_fileManager.overwriteFile(
            currentFile,
            doc->getBufferManager().getBuffer(),
            doc->getBufferManager().getSampleRate(),
            doc->getBufferManager().getBitDepth()
        );

        if (saveSuccess)
        {
            // Clear modified flag
            doc->setModified(false);
            repaint();

            juce::Logger::writeToLog("File saved successfully: " + currentFile.getFullPathName());
        }
        else
        {
            // Show error dialog
            ErrorDialog::showWithDetails(
                "Save Failed",
                "Could not save file: " + currentFile.getFileName(),
                m_fileManager.getLastError(),
                ErrorDialog::Severity::Error
            );
        }
    }

    void saveFileAs()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getAudioEngine().isFileLoaded())
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
            doc->getAudioEngine().getCurrentFile().getParentDirectory(),
            "*.wav",
            true);

        auto folderChooserFlags = juce::FileBrowserComponent::saveMode |
                                  juce::FileBrowserComponent::canSelectFiles |
                                  juce::FileBrowserComponent::warnAboutOverwriting;

        m_fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& chooser)
        {
            auto* doc = getCurrentDocument();
            if (!doc) return;

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
                    doc->getBufferManager().getBuffer(),
                    doc->getBufferManager().getSampleRate(),
                    doc->getBufferManager().getBitDepth()
                );

                if (saveSuccess)
                {
                    // Clear modified flag
                    doc->setModified(false);

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
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        // Check if document has unsaved changes
        if (doc->isModified())
        {
            auto result = juce::AlertWindow::showYesNoCancelBox(
                juce::AlertWindow::WarningIcon,
                "Unsaved Changes",
                "Do you want to save changes to \"" + doc->getFilename() + "\" before closing?",
                "Save",
                "Don't Save",
                "Cancel",
                nullptr,
                nullptr);

            if (result == 0) // Cancel
            {
                return;
            }
            else if (result == 1) // Save
            {
                saveFile();  // Save before closing
                if (doc->isModified())  // If save failed or was cancelled
                {
                    return;  // Don't close
                }
            }
            // result == 2 means "Don't Save" - proceed with close
        }

        // Clear undo history to prevent dangling references
        doc->getUndoManager().clearUndoHistory();

        doc->getAudioEngine().closeAudioFile();
        doc->getWaveformDisplay().clear();
        doc->setModified(false);
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
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        if (doc->getAudioEngine().isPlaying())
        {
            doc->getAudioEngine().stop();
        }
        else
        {
            // PRIORITY 5e/5f: Play from cursor position by default
            // Debug: Log current state
            juce::Logger::writeToLog("=== PLAYBACK START DEBUG ===");
            juce::Logger::writeToLog(juce::String::formatted("Has selection: %s",
                doc->getWaveformDisplay().hasSelection() ? "YES" : "NO"));
            juce::Logger::writeToLog(juce::String::formatted("Has edit cursor: %s",
                doc->getWaveformDisplay().hasEditCursor() ? "YES" : "NO"));

            if (doc->getWaveformDisplay().hasSelection())
            {
                juce::Logger::writeToLog(juce::String::formatted("Selection: %.3f - %.3f s",
                    doc->getWaveformDisplay().getSelectionStart(),
                    doc->getWaveformDisplay().getSelectionEnd()));
            }

            if (doc->getWaveformDisplay().hasEditCursor())
            {
                juce::Logger::writeToLog(juce::String::formatted("Edit cursor: %.3f s",
                    doc->getWaveformDisplay().getEditCursorPosition()));
            }

            juce::Logger::writeToLog(juce::String::formatted("Current playback position: %.3f s",
                doc->getWaveformDisplay().getPlaybackPosition()));

            // If there's a selection, start playback from the selection start
            if (doc->getWaveformDisplay().hasSelection())
            {
                double startPos = doc->getWaveformDisplay().getSelectionStart();
                juce::Logger::writeToLog(juce::String::formatted(
                    "→ Starting playback from selection start: %.3f s", startPos));
                doc->getAudioEngine().setPosition(startPos);
            }
            else if (doc->getWaveformDisplay().hasEditCursor())
            {
                // No selection: play from edit cursor position if set
                double startPos = doc->getWaveformDisplay().getEditCursorPosition();
                juce::Logger::writeToLog(juce::String::formatted(
                    "→ Starting playback from edit cursor: %.3f s", startPos));
                doc->getAudioEngine().setPosition(startPos);
            }
            else
            {
                // No selection or cursor: play from current playback position
                double startPos = doc->getWaveformDisplay().getPlaybackPosition();
                juce::Logger::writeToLog(juce::String::formatted(
                    "→ Starting playback from playback position: %.3f s", startPos));
                doc->getAudioEngine().setPosition(startPos);
            }

            juce::Logger::writeToLog("=== END DEBUG ===");

            doc->getAudioEngine().play();
        }

        repaint();
    }

    void stopPlayback()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        doc->getAudioEngine().stop();
        repaint();
    }

    void pausePlayback()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        auto state = doc->getAudioEngine().getPlaybackState();

        if (state == PlaybackState::PLAYING)
        {
            doc->getAudioEngine().pause();
        }
        else if (state == PlaybackState::PAUSED)
        {
            doc->getAudioEngine().play();
        }

        repaint();
    }

    void toggleLoop()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        // PRIORITY 5g: Fix loop mode - wire up to AudioEngine
        doc->getTransportControls().toggleLoop();
        bool loopEnabled = doc->getTransportControls().isLoopEnabled();
        doc->getAudioEngine().setLooping(loopEnabled);

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
        auto* doc = getCurrentDocument();
        if (!doc) return;

        // Two-tier snap system: G key cycles through increments within current unit
        doc->getWaveformDisplay().cycleSnapIncrement();

        // Force repaint to update snap mode indicator in status bar
        repaint();

        // Show status message with current unit and increment
        auto unitType = doc->getWaveformDisplay().getSnapUnit();
        int increment = doc->getWaveformDisplay().getSnapIncrement();
        juce::String message = "Snap: " + AudioUnits::formatIncrement(increment, unitType);
        juce::Logger::writeToLog(message);
    }

    void toggleZeroCrossingSnap()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        // Two-tier snap system: Z key toggles zero-crossing (independent of unit snapping)
        doc->getWaveformDisplay().toggleZeroCrossing();

        // Force repaint to update snap mode indicator in status bar
        repaint();

        // Show status message
        bool enabled = doc->getWaveformDisplay().isZeroCrossingEnabled();
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
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        // Note: We no longer stop/restart playback here.
        // GainUndoAction::perform() uses reloadBufferPreservingPlayback() which safely
        // updates the audio buffer while preserving playback position if currently playing.
        // This allows real-time gain adjustments during playback without interruption.

        // Get current buffer
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        if (buffer.getNumSamples() == 0)
        {
            return;
        }

        // Determine region to process
        int startSample = 0;
        int numSamples = buffer.getNumSamples();
        bool isSelection = false;

        if (doc->getWaveformDisplay().hasSelection())
        {
            // Apply to selection only
            startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
            int endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
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
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action (perform() will apply the gain)
        auto* undoAction = new GainUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer,
            startSample,
            numSamples,
            gainDB,
            isSelection
        );

        // perform() calls GainUndoAction::perform() which applies gain and updates display
        // while preserving playback state (uses reloadBufferPreservingPlayback internally)
        doc->getUndoManager().perform(undoAction);

        // Mark as modified
        doc->setModified(true);
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
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        // Get current buffer
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        if (buffer.getNumSamples() == 0)
        {
            return;
        }

        // Determine region to process
        int startSample = 0;
        int numSamples = buffer.getNumSamples();
        bool isSelection = false;

        if (doc->getWaveformDisplay().hasSelection())
        {
            // Normalize selection only
            startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
            int endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
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
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action (perform() will apply the normalization)
        auto* undoAction = new NormalizeUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer,
            startSample,
            numSamples,
            isSelection
        );

        // perform() calls NormalizeUndoAction::perform() which normalizes and updates display
        doc->getUndoManager().perform(undoAction);

        // Mark as modified
        doc->setModified(true);

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
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        // Fade in requires a selection
        if (!doc->getWaveformDisplay().hasSelection())
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
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        if (buffer.getNumSamples() == 0)
        {
            return;
        }

        // Get selection bounds
        int startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
        int endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
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
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action (perform() will apply the fade)
        auto* undoAction = new FadeInUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer,
            startSample,
            numSamples
        );

        // perform() calls FadeInUndoAction::perform() which applies fade and updates display
        doc->getUndoManager().perform(undoAction);

        // Mark as modified
        doc->setModified(true);

        // Log the operation
        juce::String message = juce::String::formatted(
            "Applied fade in to selection (%d samples, %.3f seconds)",
            numSamples, (double)numSamples / doc->getBufferManager().getSampleRate());
        juce::Logger::writeToLog(message);
    }

    /**
     * Apply fade out to selection.
     * Creates undo action for the fade out operation.
     */
    void applyFadeOut()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        // Fade out requires a selection
        if (!doc->getWaveformDisplay().hasSelection())
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
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        if (buffer.getNumSamples() == 0)
        {
            return;
        }

        // Get selection bounds
        int startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
        int endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
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
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action (perform() will apply the fade)
        auto* undoAction = new FadeOutUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer,
            startSample,
            numSamples
        );

        // perform() calls FadeOutUndoAction::perform() which applies fade and updates display
        doc->getUndoManager().perform(undoAction);

        // Mark as modified
        doc->setModified(true);

        // Log the operation
        juce::String message = juce::String::formatted(
            "Applied fade out to selection (%d samples, %.3f seconds)",
            numSamples, (double)numSamples / doc->getBufferManager().getSampleRate());
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
    // Multi-document management
    DocumentManager m_documentManager;
    TabComponent m_tabComponent;
    Document* m_previousDocument = nullptr;  // Track previous document for cleanup during switching

    // Shared resources (keep these)
    AudioFileManager m_fileManager;
    juce::ApplicationCommandManager m_commandManager;
    std::unique_ptr<juce::FileChooser> m_fileChooser;

    // "No file open" label
    juce::Label m_noFileLabel;

    // Current document display containers (for showing active document's components)
    std::unique_ptr<juce::Component> m_currentDocumentContainer;

    // Helper methods for safe document access
    Document* getCurrentDocument() const
    {
        return m_documentManager.getCurrentDocument();
    }

    bool hasCurrentDocument() const
    {
        return getCurrentDocument() != nullptr;
    }

    /**
     * Updates the window title to reflect current document and modified state.
     * Format: "filename.wav * - WaveEdit" (when modified)
     *         "filename.wav - WaveEdit" (when not modified)
     *         "WaveEdit" (when no document)
     */
    void updateWindowTitle()
    {
        // Get the top-level window (DocumentWindow)
        auto* window = findParentComponentOfClass<juce::DocumentWindow>();
        if (!window)
            return;

        auto* doc = getCurrentDocument();

        // Build window title
        juce::String title;
        if (doc && doc->getAudioEngine().isFileLoaded())
        {
            // Get filename
            juce::String filename = doc->getAudioEngine().getCurrentFile().getFileName();
            if (filename.isEmpty())
                filename = "Untitled";

            // Add modified indicator
            if (doc->isModified())
                title = filename + " * - WaveEdit";
            else
                title = filename + " - WaveEdit";
        }
        else
        {
            // No document loaded
            title = "WaveEdit";
        }

        // Only update if title changed (avoid unnecessary redraws)
        if (window->getName() != title)
        {
            window->setName(title);
        }
    }

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
