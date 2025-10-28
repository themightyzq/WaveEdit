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
#include "UI/SettingsPanel.h"
#include "UI/FilePropertiesDialog.h"
#include "UI/GoToPositionDialog.h"
#include "UI/EditRegionBoundariesDialog.h"
#include "UI/StripSilenceDialog.h"
#include "UI/BatchExportDialog.h"
#include "UI/RegionListPanel.h"
#include "UI/TabComponent.h"
#include "Utils/Document.h"
#include "Utils/DocumentManager.h"
#include "Utils/RegionExporter.h"

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
                      public DocumentManager::Listener,
                      public RegionListPanel::Listener
{
public:
    MainComponent(juce::AudioDeviceManager& deviceManager)
        : m_audioDeviceManager(deviceManager),
          m_tabComponent(m_documentManager)
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

        // Load UI preferences from settings
        int timeFormatInt = Settings::getInstance().getSetting("display.timeFormat", 2);  // Default to Seconds (2)
        m_timeFormat = static_cast<AudioUnits::TimeFormat>(timeFormatInt);

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

        // Close region list panel if open
        if (m_regionListWindow)
        {
            delete m_regionListWindow;  // Window owns the panel, will delete it too
            m_regionListWindow = nullptr;
            m_regionListPanel = nullptr;  // Window deleted the panel, just null our pointer
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
        // Wire up region callbacks for this document
        setupRegionCallbacks(document);

        // Update tab component to show new tab
        updateComponentVisibility();
    }

    void documentRemoved(Document* document, int index) override
    {
        // Update tab component to remove tab
        updateComponentVisibility();
    }

    //==============================================================================
    // RegionListPanel::Listener methods

    void regionListPanelJumpToRegion(int regionIndex) override
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            if (auto* region = doc->getRegionManager().getRegion(regionIndex))
            {
                // Jump to region and select its range
                int64_t startSample = region->getStartSample();
                int64_t endSample = region->getEndSample();

                // Convert to time for selection
                double sampleRate = doc->getBufferManager().getSampleRate();
                double startTime = static_cast<double>(startSample) / sampleRate;
                double endTime = static_cast<double>(endSample) / sampleRate;

                // Set selection
                doc->getWaveformDisplay().setSelection(startTime, endTime);

                // Center view on region start
                doc->getWaveformDisplay().setVisibleRange(startTime, endTime);
                doc->getWaveformDisplay().zoomToSelection();
            }
        }
    }

    void regionListPanelRegionDeleted(int regionIndex) override
    {
        // Region already deleted from RegionManager
        // Just refresh the waveform display
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            doc->getWaveformDisplay().repaint();
        }
    }

    void regionListPanelRegionRenamed(int regionIndex, const juce::String& newName) override
    {
        // Region already renamed in RegionManager
        // Refresh both the waveform display and the region display
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            doc->getWaveformDisplay().repaint();
            doc->getRegionDisplay().repaint();  // This ensures region names update in the waveform
        }
    }

    void regionListPanelRegionSelected(int regionIndex) override
    {
        // Single-click: Select the region's range and zoom out to show full waveform
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            if (auto* region = doc->getRegionManager().getRegion(regionIndex))
            {
                // Get region boundaries
                int64_t startSample = region->getStartSample();
                int64_t endSample = region->getEndSample();

                // Convert to time for selection
                double sampleRate = doc->getBufferManager().getSampleRate();
                double startTime = static_cast<double>(startSample) / sampleRate;
                double endTime = static_cast<double>(endSample) / sampleRate;

                // Zoom out to show full waveform first
                doc->getWaveformDisplay().zoomToFit();

                // Then set selection to the region
                doc->getWaveformDisplay().setSelection(startTime, endTime);
                doc->getWaveformDisplay().repaint();
            }
        }
    }

    void regionListPanelBatchRename(const std::vector<int>& regionIndices) override
    {
        // Called when user wants to start batch rename workflow (e.g., from Cmd+Shift+N shortcut)
        // This method now simply expands the batch rename section in the RegionListPanel
        if (!m_regionListPanel)
            return;

        // Expand the batch rename section (if collapsed)
        m_regionListPanel->expandBatchRenameSection(true);

        // Note: The actual rename logic happens in regionListPanelBatchRenameApply()
        // when the user clicks "Apply" button in the batch rename section
    }

    void regionListPanelBatchRenameApply(const std::vector<int>& regionIndices,
                                          const std::vector<juce::String>& newNames) override
    {
        // Called when user clicks "Apply" in the batch rename section
        auto* doc = m_documentManager.getCurrentDocument();
        if (!doc || regionIndices.empty() || newNames.empty())
            return;

        // Ensure indices and names match
        if (regionIndices.size() != newNames.size())
        {
            jassertfalse;  // Programming error - should never happen
            return;
        }

        // Collect old names for undo
        std::vector<juce::String> oldNames;
        oldNames.reserve(regionIndices.size());

        for (int index : regionIndices)
        {
            if (auto* region = doc->getRegionManager().getRegion(index))
            {
                oldNames.push_back(region->getName());
            }
            else
            {
                // Region no longer exists - shouldn't happen
                jassertfalse;
                return;
            }
        }

        // Create undo action with old and new names
        auto* undoAction = new BatchRenameRegionUndoAction(
            doc->getRegionManager(),
            &doc->getRegionDisplay(),
            regionIndices,
            oldNames,
            newNames
        );

        // Add to undo manager and perform
        doc->getUndoManager().perform(undoAction);

        // Debug: Log undo/redo state
        juce::Logger::writeToLog("Batch rename action added to undo manager. Can undo: " +
                                   juce::String(doc->getUndoManager().canUndo() ? "YES" : "NO"));

        // Refresh displays
        doc->getWaveformDisplay().repaint();
        doc->getRegionDisplay().repaint();

        // Refresh region list panel
        if (m_regionListPanel)
        {
            m_regionListPanel->refresh();
        }
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
            m_currentDocumentContainer->addAndMakeVisible(doc->getRegionDisplay());
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

        // Create undoable delete action (with region manager and display for undo support)
        auto deleteAction = new DeleteAction(
            doc->getBufferManager(),
            doc->getAudioEngine(),
            doc->getWaveformDisplay(),
            startSample,
            endSample - startSample,
            &doc->getRegionManager(),  // Pass region manager for undo support
            &doc->getRegionDisplay()   // Pass region display to update visuals after undo
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

            // Format time according to user preference
            juce::String currentTime = AudioUnits::formatTime(
                doc->getAudioEngine().getCurrentPosition(),
                doc->getAudioEngine().getSampleRate(),
                m_timeFormat);
            juce::String totalTime = AudioUnits::formatTime(
                doc->getAudioEngine().getTotalLength(),
                doc->getAudioEngine().getSampleRate(),
                m_timeFormat);

            juce::String info = juce::String::formatted(
                "%s | %.1f kHz | %d ch | %d bit | %s / %s",
                fileDisplayName.toRawUTF8(),
                doc->getAudioEngine().getSampleRate() / 1000.0,
                doc->getAudioEngine().getNumChannels(),
                doc->getAudioEngine().getBitDepth(),
                currentTime.toRawUTF8(),
                totalTime.toRawUTF8());

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

            // Zoom level display (before snap mode)
            auto zoomSection = statusBar.removeFromRight(120);
            double zoomPercentage = doc->getWaveformDisplay().getZoomPercentage();
            juce::String zoomText;
            if (zoomPercentage >= 10000.0)
            {
                zoomText = juce::String::formatted("Zoom: %.0fk%%", zoomPercentage / 1000.0);
            }
            else if (zoomPercentage >= 1000.0)
            {
                zoomText = juce::String::formatted("Zoom: %.1fk%%", zoomPercentage / 1000.0);
            }
            else
            {
                zoomText = juce::String::formatted("Zoom: %.0f%%", zoomPercentage);
            }

            g.setColour(juce::Colours::lightcyan);
            g.setFont(12.0f);
            g.drawText(zoomText, zoomSection.reduced(5, 0), juce::Justification::centred, true);

            // Time format indicator (clickable) - fixed position to left of snap settings
            auto formatSection = statusBar.removeFromRight(120);

            // Store bounds for mouse click detection
            m_formatIndicatorBounds = formatSection;

            juce::String formatName = AudioUnits::timeFormatToString(m_timeFormat);
            juce::String formatText = "[" + formatName + " ▼]";

            g.setColour(juce::Colours::lightgreen);
            g.setFont(12.0f);
            g.drawText(formatText, formatSection.reduced(5, 0), juce::Justification::centred, true);

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

                // Region display below transport (32px height)
                doc->getRegionDisplay().setBounds(containerBounds.removeFromTop(32));

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

    void mouseDown(const juce::MouseEvent& event) override
    {
        // Check if click is on time format indicator
        if (m_formatIndicatorBounds.contains(event.getPosition()))
        {
            showTimeFormatMenu();
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

        // Auto-save check (Phase 3.5 - Priority #6)
        // Check auto-save every ~60 seconds (1200 ticks at 50ms each)
        m_autoSaveTimerTicks++;
        if (m_autoSaveTimerTicks >= m_autoSaveCheckInterval)
        {
            m_autoSaveTimerTicks = 0;  // Reset counter
            performAutoSave();
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
            CommandIDs::fileProperties,  // Alt+Enter - Show file properties
            CommandIDs::filePreferences, // Cmd+, - Preferences/Settings
            // Tab commands
            CommandIDs::tabClose,
            CommandIDs::tabCloseAll,
            CommandIDs::tabNext,
            CommandIDs::tabPrevious,
            CommandIDs::tabSelect1,
            CommandIDs::tabSelect2,
            CommandIDs::tabSelect3,
            CommandIDs::tabSelect4,
            CommandIDs::tabSelect5,
            CommandIDs::tabSelect6,
            CommandIDs::tabSelect7,
            CommandIDs::tabSelect8,
            CommandIDs::tabSelect9,
            CommandIDs::fileExit,
            CommandIDs::editUndo,
            CommandIDs::editRedo,
            CommandIDs::editSelectAll,
            CommandIDs::editCut,
            CommandIDs::editCopy,
            CommandIDs::editPaste,
            CommandIDs::editDelete,
            CommandIDs::editSilence,
            CommandIDs::editTrim,
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
            CommandIDs::viewCycleTimeFormat,  // Shift+G - Cycle time format
            CommandIDs::viewAutoScroll,       // Cmd+Shift+F - Auto-scroll during playback
            CommandIDs::viewZoomToRegion,     // Z or Cmd+Option+Z - Zoom to selected region
            CommandIDs::viewAutoPreviewRegions,  // Toggle auto-play regions on select
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
            CommandIDs::navigateGoToPosition,
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
            CommandIDs::processFadeOut,
            CommandIDs::processDCOffset,
            // Region commands (Phase 3 Tier 2)
            CommandIDs::regionAdd,
            CommandIDs::regionDelete,
            CommandIDs::regionNext,
            CommandIDs::regionPrevious,
            CommandIDs::regionSelectInverse,
            CommandIDs::regionSelectAll,
            CommandIDs::regionStripSilence,
            CommandIDs::regionExportAll,
            CommandIDs::regionShowList,
            CommandIDs::regionSnapToZeroCrossing,  // Phase 3.3 - Toggle zero-crossing snap
            // Region nudge commands (Phase 3.3 - Feature #6)
            CommandIDs::regionNudgeStartLeft,
            CommandIDs::regionNudgeStartRight,
            CommandIDs::regionNudgeEndLeft,
            CommandIDs::regionNudgeEndRight,
            CommandIDs::regionBatchRename,  // Phase 3.4 - Batch rename selected regions
            // Region editing commands (Phase 3.4)
            CommandIDs::regionMerge,
            CommandIDs::regionSplit,
            CommandIDs::regionCopy,
            CommandIDs::regionPaste
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
                // No keyboard shortcut - Cmd+W handled by tabClose for consistency
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::fileProperties:
                result.setInfo("Properties...", "Show file properties", "File", 0);
                result.addDefaultKeypress(juce::KeyPress::returnKey, juce::ModifierKeys::altModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::fileExit:
                result.setInfo("Exit", "Exit the application", "File", 0);
                result.addDefaultKeypress('q', juce::ModifierKeys::commandModifier);
                // Always available (no document required)
                break;

            case CommandIDs::filePreferences:
                result.setInfo("Preferences...", "Open preferences dialog", "File", 0);
                result.addDefaultKeypress(',', juce::ModifierKeys::commandModifier);
                // Always available (no document required)
                break;

            // Tab operations
            case CommandIDs::tabClose:
                result.setInfo("Close Tab", "Close current tab", "File", 0);
                result.addDefaultKeypress('w', juce::ModifierKeys::commandModifier);
                result.setActive(hasCurrentDocument());
                break;

            case CommandIDs::tabCloseAll:
                result.setInfo("Close All Tabs", "Close all open tabs", "File", 0);
                result.addDefaultKeypress('w', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(hasCurrentDocument());
                break;

            case CommandIDs::tabNext:
                result.setInfo("Next Tab", "Switch to next tab", "File", 0);
                result.addDefaultKeypress(juce::KeyPress::tabKey, juce::ModifierKeys::commandModifier);
                result.setActive(m_documentManager.getNumDocuments() > 1);
                break;

            case CommandIDs::tabPrevious:
                result.setInfo("Previous Tab", "Switch to previous tab", "File", 0);
                result.addDefaultKeypress(juce::KeyPress::tabKey, juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(m_documentManager.getNumDocuments() > 1);
                break;

            case CommandIDs::tabSelect1:
                result.setInfo("Jump to Tab 1", "Switch to tab 1", "File", 0);
                result.addDefaultKeypress('1', juce::ModifierKeys::commandModifier);
                result.setActive(m_documentManager.getNumDocuments() >= 1);
                break;

            case CommandIDs::tabSelect2:
                result.setInfo("Jump to Tab 2", "Switch to tab 2", "File", 0);
                result.addDefaultKeypress('2', juce::ModifierKeys::commandModifier);
                result.setActive(m_documentManager.getNumDocuments() >= 2);
                break;

            case CommandIDs::tabSelect3:
                result.setInfo("Jump to Tab 3", "Switch to tab 3", "File", 0);
                result.addDefaultKeypress('3', juce::ModifierKeys::commandModifier);
                result.setActive(m_documentManager.getNumDocuments() >= 3);
                break;

            case CommandIDs::tabSelect4:
                result.setInfo("Jump to Tab 4", "Switch to tab 4", "File", 0);
                result.addDefaultKeypress('4', juce::ModifierKeys::commandModifier);
                result.setActive(m_documentManager.getNumDocuments() >= 4);
                break;

            case CommandIDs::tabSelect5:
                result.setInfo("Jump to Tab 5", "Switch to tab 5", "File", 0);
                result.addDefaultKeypress('5', juce::ModifierKeys::commandModifier);
                result.setActive(m_documentManager.getNumDocuments() >= 5);
                break;

            case CommandIDs::tabSelect6:
                result.setInfo("Jump to Tab 6", "Switch to tab 6", "File", 0);
                result.addDefaultKeypress('6', juce::ModifierKeys::commandModifier);
                result.setActive(m_documentManager.getNumDocuments() >= 6);
                break;

            case CommandIDs::tabSelect7:
                result.setInfo("Jump to Tab 7", "Switch to tab 7", "File", 0);
                result.addDefaultKeypress('7', juce::ModifierKeys::commandModifier);
                result.setActive(m_documentManager.getNumDocuments() >= 7);
                break;

            case CommandIDs::tabSelect8:
                result.setInfo("Jump to Tab 8", "Switch to tab 8", "File", 0);
                result.addDefaultKeypress('8', juce::ModifierKeys::commandModifier);
                result.setActive(m_documentManager.getNumDocuments() >= 8);
                break;

            case CommandIDs::tabSelect9:
                result.setInfo("Jump to Tab 9", "Switch to tab 9", "File", 0);
                result.addDefaultKeypress('9', juce::ModifierKeys::commandModifier);
                result.setActive(m_documentManager.getNumDocuments() >= 9);
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

            case CommandIDs::editSilence:
                result.setInfo("Silence", "Fill selection with silence", "Edit", 0);
                result.addDefaultKeypress('l', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::editTrim:
                result.setInfo("Trim", "Delete everything outside selection", "Edit", 0);
                result.addDefaultKeypress('t', juce::ModifierKeys::commandModifier);
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
                result.addDefaultKeypress('0', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::viewZoomSelection:
                result.setInfo("Zoom to Selection", "Zoom to current selection", "View", 0);
                result.addDefaultKeypress('e', juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::viewZoomOneToOne:
                result.setInfo("Zoom 1:1", "Zoom to 1:1 sample resolution", "View", 0);
                result.addDefaultKeypress('0', juce::ModifierKeys::commandModifier);  // Cmd+0 (not Cmd+1, which is used for tab switching)
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::viewCycleTimeFormat:
                result.setInfo("Cycle Time Format", "Cycle through time display formats (Samples/Ms/Sec/Frames)", "View", 0);
                result.addDefaultKeypress('g', juce::ModifierKeys::shiftModifier);  // Shift+G for Format
                result.setActive(true);  // Always active
                break;

            case CommandIDs::viewAutoScroll:
                result.setInfo("Follow Playback", "Auto-scroll to follow playback cursor", "View", 0);
                result.addDefaultKeypress('f', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);  // Cmd+Shift+F
                result.setTicked(doc && doc->getWaveformDisplay().isFollowPlayback());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::viewZoomToRegion:
                result.setInfo("Zoom to Region", "Zoom to fit selected region with margins", "View", 0);
                result.addDefaultKeypress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier);  // Cmd+Option+Z
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getRegionManager().getSelectedRegionIndex() >= 0);
                break;

            case CommandIDs::viewAutoPreviewRegions:
                result.setInfo("Auto-Preview Regions", "Automatically play regions when clicked", "View", 0);
                result.addDefaultKeypress('P', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier);  // Cmd+Alt+P
                result.setTicked(Settings::getInstance().getAutoPreviewRegions());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getRegionManager().getNumRegions() > 0);
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

            case CommandIDs::navigateGoToPosition:
                result.setInfo("Go To Position...", "Jump to exact position", "Navigation", 0);
                result.addDefaultKeypress('g', juce::ModifierKeys::commandModifier);
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
                result.setInfo("Toggle Snap", "Toggle snap on/off (maintains last increment)", "Snap", 0);
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
                result.addDefaultKeypress('g', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);  // Cmd+Shift+G (changed from Shift+G to avoid conflict with viewCycleTimeFormat)
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
                result.addDefaultKeypress('f', juce::ModifierKeys::commandModifier);  // Changed from Cmd+Shift+I to Cmd+F (no conflict)
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::processFadeOut:
                result.setInfo("Fade Out", "Apply linear fade out to selection", "Process", 0);
                result.addDefaultKeypress('o', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::processDCOffset:
                result.setInfo("Remove DC Offset", "Remove DC offset from entire file", "Process", 0);
                result.addDefaultKeypress('d', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            // Region commands (Phase 3 Tier 2)
            case CommandIDs::regionAdd:
                result.setInfo("Add Region", "Create region from current selection", "Region", 0);
                result.addDefaultKeypress('r', 0);  // Just 'R' key (no modifiers)
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::regionDelete:
                result.setInfo("Delete Region", "Delete selected region", "Region", 0);
                result.addDefaultKeypress(juce::KeyPress::deleteKey, juce::ModifierKeys::commandModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::regionNext:
                result.setInfo("Next Region", "Jump to next region", "Region", 0);
                result.addDefaultKeypress(']', 0);  // Just ']' key (no modifiers)
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::regionPrevious:
                result.setInfo("Previous Region", "Jump to previous region", "Region", 0);
                result.addDefaultKeypress('[', 0);  // Just '[' key (no modifiers)
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::regionSelectInverse:
                result.setInfo("Select Inverse of Regions", "Select everything NOT in regions", "Region", 0);
                result.addDefaultKeypress('i', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::regionSelectAll:
                result.setInfo("Select All Regions", "Select union of all regions", "Region", 0);
                result.addDefaultKeypress('a', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::regionStripSilence:
                result.setInfo("Auto Region", "Auto-create regions from non-silent sections", "Region", 0);
                result.addDefaultKeypress('r', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::regionExportAll:
                result.setInfo("Export Regions As Files", "Export each region as a separate audio file", "Region", 0);
                result.addDefaultKeypress('e', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getRegionManager().getNumRegions() > 0);
                break;

            case CommandIDs::regionBatchRename:
                result.setInfo("Batch Rename Regions", "Rename multiple selected regions at once", "Region", 0);
                result.addDefaultKeypress('n', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getRegionManager().getNumRegions() >= 2);
                break;

            // Region editing commands (Phase 3.4)
            case CommandIDs::regionMerge:
                result.setInfo("Merge Regions", "Merge selected regions", "Region", 0);
                result.addDefaultKeypress('m', juce::ModifierKeys::shiftModifier);
                result.setActive(doc && canMergeRegions(doc));
                break;

            case CommandIDs::regionSplit:
                result.setInfo("Split Region at Cursor", "Split region at cursor position", "Region", 0);
                result.addDefaultKeypress('t', juce::ModifierKeys::commandModifier);
                result.setActive(doc && canSplitRegion(doc));
                break;

            case CommandIDs::regionCopy:
                result.setInfo("Copy Region", "Copy selected region definition to clipboard", "Region", 0);
                result.addDefaultKeypress('c', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getRegionManager().getSelectedRegionIndex() >= 0);
                break;

            case CommandIDs::regionPaste:
                result.setInfo("Paste Regions at Cursor", "Paste regions at cursor position", "Region", 0);
                result.addDefaultKeypress('v', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && m_hasRegionClipboard);
                break;

            case CommandIDs::regionShowList:
                result.setInfo("Show Region List", "Display list of all regions", "Region", 0);
                result.addDefaultKeypress('m', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::regionSnapToZeroCrossing:
                result.setInfo("Snap to Zero Crossings", "Snap region boundaries to zero crossings", "Region", 0);
                result.setTicked(Settings::getInstance().getSnapRegionsToZeroCrossings());
                result.setActive(true);  // Always available (checkbox menu item)
                break;

            // Region nudge commands (Phase 3.3 - Feature #6)
            case CommandIDs::regionNudgeStartLeft:
                result.setInfo("Nudge Region Start Left", "Move region start boundary left by snap increment", "Region", 0);
                result.addDefaultKeypress(juce::KeyPress::leftKey, juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getRegionManager().getSelectedRegionIndex() >= 0);
                break;

            case CommandIDs::regionNudgeStartRight:
                result.setInfo("Nudge Region Start Right", "Move region start boundary right by snap increment", "Region", 0);
                result.addDefaultKeypress(juce::KeyPress::rightKey, juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getRegionManager().getSelectedRegionIndex() >= 0);
                break;

            case CommandIDs::regionNudgeEndLeft:
                result.setInfo("Nudge Region End Left", "Move region end boundary left by snap increment", "Region", 0);
                result.addDefaultKeypress(juce::KeyPress::leftKey, juce::ModifierKeys::shiftModifier | juce::ModifierKeys::altModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getRegionManager().getSelectedRegionIndex() >= 0);
                break;

            case CommandIDs::regionNudgeEndRight:
                result.setInfo("Nudge Region End Right", "Move region end boundary right by snap increment", "Region", 0);
                result.addDefaultKeypress(juce::KeyPress::rightKey, juce::ModifierKeys::shiftModifier | juce::ModifierKeys::altModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getRegionManager().getSelectedRegionIndex() >= 0);
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

            case CommandIDs::filePreferences:
                SettingsPanel::showDialog(this, m_audioDeviceManager);
                return true;

            // Tab operations
            case CommandIDs::tabClose:
                if (!doc) return false;
                m_documentManager.closeDocument(doc);
                return true;

            case CommandIDs::tabCloseAll:
                m_documentManager.closeAllDocuments();
                return true;

            case CommandIDs::tabNext:
                m_documentManager.selectNextDocument();
                return true;

            case CommandIDs::tabPrevious:
                m_documentManager.selectPreviousDocument();
                return true;

            case CommandIDs::tabSelect1:
                m_documentManager.setCurrentDocumentIndex(0);
                return true;

            case CommandIDs::tabSelect2:
                m_documentManager.setCurrentDocumentIndex(1);
                return true;

            case CommandIDs::tabSelect3:
                m_documentManager.setCurrentDocumentIndex(2);
                return true;

            case CommandIDs::tabSelect4:
                m_documentManager.setCurrentDocumentIndex(3);
                return true;

            case CommandIDs::tabSelect5:
                m_documentManager.setCurrentDocumentIndex(4);
                return true;

            case CommandIDs::tabSelect6:
                m_documentManager.setCurrentDocumentIndex(5);
                return true;

            case CommandIDs::tabSelect7:
                m_documentManager.setCurrentDocumentIndex(6);
                return true;

            case CommandIDs::tabSelect8:
                m_documentManager.setCurrentDocumentIndex(7);
                return true;

            case CommandIDs::tabSelect9:
                m_documentManager.setCurrentDocumentIndex(8);
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

            case CommandIDs::fileProperties:
                if (!doc) return false;
                FilePropertiesDialog::showDialog(this, *doc);
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

                    // Refresh RegionListPanel if open
                    if (m_regionListPanel)
                        m_regionListPanel->refresh();

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

                    // Refresh RegionListPanel if open
                    if (m_regionListPanel)
                        m_regionListPanel->refresh();

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

            case CommandIDs::viewCycleTimeFormat:
                // Cycle to next time format
                m_timeFormat = AudioUnits::getNextTimeFormat(m_timeFormat);
                // Save to settings
                Settings::getInstance().setSetting("display.timeFormat", static_cast<int>(m_timeFormat));
                Settings::getInstance().save();
                // Force UI update
                repaint();
                return true;

            case CommandIDs::viewAutoScroll:
                if (!doc) return false;
                // Toggle follow-playback mode
                doc->getWaveformDisplay().setFollowPlayback(!doc->getWaveformDisplay().isFollowPlayback());
                return true;

            case CommandIDs::viewZoomToRegion:
                if (!doc) return false;
                doc->getWaveformDisplay().zoomToRegion();
                return true;

            case CommandIDs::viewAutoPreviewRegions:
                // Toggle auto-preview setting (global, not per-document)
                Settings::getInstance().setAutoPreviewRegions(!Settings::getInstance().getAutoPreviewRegions());
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

            case CommandIDs::navigateGoToPosition:
            {
                if (!doc) return false;
                // Show Go To Position dialog
                auto& engine = doc->getAudioEngine();
                GoToPositionDialog::showDialog(
                    this,
                    m_timeFormat,
                    engine.getSampleRate(),
                    30.0,  // Default FPS (TODO: make this user-configurable in settings)
                    static_cast<int64_t>(engine.getTotalLength() * engine.getSampleRate()),
                    [this](int64_t positionInSamples)
                    {
                        // Callback when user confirms position
                        auto* doc = m_documentManager.getCurrentDocument();
                        if (doc)
                        {
                            double positionInSeconds = AudioUnits::samplesToSeconds(
                                positionInSamples,
                                doc->getAudioEngine().getSampleRate());
                            doc->getWaveformDisplay().setEditCursor(positionInSeconds);
                            doc->getWaveformDisplay().clearSelection();  // Clear selection, just move cursor
                            doc->getWaveformDisplay().centerViewOnCursor();  // Scroll to position
                        }
                    });
                return true;
            }

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

            // Region operations (Phase 3 Tier 2)
            case CommandIDs::regionAdd:
                if (!doc) return false;
                addRegionFromSelection();
                return true;

            case CommandIDs::regionDelete:
                if (!doc) return false;
                deleteSelectedRegion();
                return true;

            case CommandIDs::regionNext:
                if (!doc) return false;
                jumpToNextRegion();
                return true;

            case CommandIDs::regionPrevious:
                if (!doc) return false;
                jumpToPreviousRegion();
                return true;

            case CommandIDs::regionSelectInverse:
                if (!doc) return false;
                selectInverseOfRegions();
                return true;

            case CommandIDs::regionSelectAll:
                if (!doc) return false;
                selectAllRegions();
                return true;

            case CommandIDs::regionStripSilence:
                if (!doc) return false;
                showStripSilenceDialog();
                return true;

            case CommandIDs::regionExportAll:
                if (!doc) return false;
                showBatchExportDialog();
                return true;

            case CommandIDs::regionBatchRename:
            {
                if (!doc) return false;

                // Open Region List Panel with batch rename section expanded
                // (Same logic as regionShowList, plus batch rename section expansion)

                // Create the panel if it doesn't exist
                if (!m_regionListPanel)
                {
                    m_regionListPanel = new RegionListPanel(
                        &doc->getRegionManager(),
                        doc->getBufferManager().getSampleRate()
                    );
                    m_regionListPanel->setListener(this);
                    m_regionListPanel->setCommandManager(&m_commandManager);
                }
                else
                {
                    // Update with current document's data
                    m_regionListPanel->setSampleRate(doc->getBufferManager().getSampleRate());
                }

                // Show in a window
                if (!m_regionListWindow || !m_regionListWindow->isVisible())
                {
                    m_regionListWindow = m_regionListPanel->showInWindow(false); // non-modal
                }
                else
                {
                    // Bring existing window to front
                    m_regionListWindow->toFront(true);
                }

                // Expand batch rename section
                m_regionListPanel->expandBatchRenameSection(true);

                return true;
            }

            // Region editing commands (Phase 3.4)
            case CommandIDs::regionMerge:
                if (!doc) return false;
                mergeSelectedRegions();
                return true;

            case CommandIDs::regionSplit:
                if (!doc) return false;
                splitRegionAtCursor();
                return true;

            case CommandIDs::regionCopy:
                if (!doc) return false;
                copyRegionsToClipboard();
                return true;

            case CommandIDs::regionPaste:
                if (!doc) return false;
                pasteRegionsFromClipboard();
                return true;

            case CommandIDs::regionShowList:
            {
                if (auto* doc = m_documentManager.getCurrentDocument())
                {
                    // Create the panel if it doesn't exist
                    if (!m_regionListPanel)
                    {
                        m_regionListPanel = new RegionListPanel(
                            &doc->getRegionManager(),
                            doc->getBufferManager().getSampleRate()
                        );
                        m_regionListPanel->setListener(this);
                        m_regionListPanel->setCommandManager(&m_commandManager);
                    }
                    else
                    {
                        // Update with current document's data
                        m_regionListPanel->setSampleRate(doc->getBufferManager().getSampleRate());
                    }

                    // Show in a window
                    if (!m_regionListWindow || !m_regionListWindow->isVisible())
                    {
                        m_regionListWindow = m_regionListPanel->showInWindow(false); // non-modal
                    }
                    else
                    {
                        // Bring existing window to front
                        m_regionListWindow->toFront(true);
                    }
                }
                return true;
            }

            case CommandIDs::regionSnapToZeroCrossing:
            {
                // Toggle the preference setting
                bool currentValue = Settings::getInstance().getSnapRegionsToZeroCrossings();
                Settings::getInstance().setSnapRegionsToZeroCrossings(!currentValue);

                // Update menu checkmark by invalidating command info
                m_commandManager.commandStatusChanged();

                return true;
            }

            // Region nudge commands (Phase 3.3 - Feature #6)
            case CommandIDs::regionNudgeStartLeft:
                if (!doc) return false;
                nudgeRegionBoundary(true, true);  // nudgeStart=true, moveLeft=true
                return true;

            case CommandIDs::regionNudgeStartRight:
                if (!doc) return false;
                nudgeRegionBoundary(true, false);  // nudgeStart=true, moveLeft=false
                return true;

            case CommandIDs::regionNudgeEndLeft:
                if (!doc) return false;
                nudgeRegionBoundary(false, true);  // nudgeStart=false, moveLeft=true
                return true;

            case CommandIDs::regionNudgeEndRight:
                if (!doc) return false;
                nudgeRegionBoundary(false, false);  // nudgeStart=false, moveLeft=false
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

            case CommandIDs::processDCOffset:
                if (!doc) return false;
                applyDCOffsetRemoval();
                return true;

            case CommandIDs::editSilence:
                if (!doc) return false;
                silenceSelection();
                return true;

            case CommandIDs::editTrim:
                if (!doc) return false;
                trimToSelection();
                return true;

            default:
                return false;
        }
    }

    //==============================================================================
    // MenuBarModel Implementation

    juce::StringArray getMenuBarNames() override
    {
        return { "File", "Edit", "View", "Region", "Process", "Playback", "Help" };
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
            menu.addCommandItem(&m_commandManager, CommandIDs::fileProperties);
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

            menu.addCommandItem(&m_commandManager, CommandIDs::filePreferences);
            menu.addSeparator();
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

            menu.addCommandItem(&m_commandManager, CommandIDs::editSilence);
            menu.addCommandItem(&m_commandManager, CommandIDs::editTrim);

            menu.addSeparator();

            menu.addCommandItem(&m_commandManager, CommandIDs::editSelectAll);
        }
        else if (menuIndex == 2) // View menu
        {
            // Zoom commands
            menu.addCommandItem(&m_commandManager, CommandIDs::viewZoomIn);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewZoomOut);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewZoomFit);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewZoomSelection);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewZoomOneToOne);

            menu.addSeparator();

            // Display options
            menu.addCommandItem(&m_commandManager, CommandIDs::viewCycleTimeFormat);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewAutoScroll);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewZoomToRegion);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewAutoPreviewRegions);
        }
        else if (menuIndex == 3) // Region menu
        {
            menu.addCommandItem(&m_commandManager, CommandIDs::regionAdd);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionDelete);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::regionNext);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionPrevious);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::regionSelectInverse);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionSelectAll);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::regionStripSilence);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionExportAll);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionBatchRename);
            menu.addSeparator();
            // Region editing (Phase 3.4)
            menu.addCommandItem(&m_commandManager, CommandIDs::regionMerge);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionSplit);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::regionCopy);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionPaste);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::regionShowList);
        }
        else if (menuIndex == 4) // Process menu
        {
            menu.addCommandItem(&m_commandManager, CommandIDs::processGain);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::processNormalize);
            menu.addCommandItem(&m_commandManager, CommandIDs::processDCOffset);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::processFadeIn);
            menu.addCommandItem(&m_commandManager, CommandIDs::processFadeOut);
        }
        else if (menuIndex == 5) // Playback menu
        {
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackPlay);
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackPause);
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackStop);
            menu.addSeparator();
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackLoop);
        }
        else if (menuIndex == 6) // Help menu
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

        // G key toggles snap on/off (maintains last used increment)
        doc->getWaveformDisplay().toggleSnap();

        // Force repaint to update snap mode indicator in status bar
        repaint();

        // Show status message with current state
        bool enabled = doc->getWaveformDisplay().isSnapEnabled();
        if (enabled)
        {
            auto unitType = doc->getWaveformDisplay().getSnapUnit();
            int increment = doc->getWaveformDisplay().getSnapIncrement();
            juce::String message = "Snap: ON (" + AudioUnits::formatIncrement(increment, unitType) + ")";
            juce::Logger::writeToLog(message);
        }
        else
        {
            juce::Logger::writeToLog("Snap: OFF");
        }
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
    // Region helpers (Phase 3 Tier 2)

    void addRegionFromSelection()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getWaveformDisplay().hasSelection())
        {
            juce::Logger::writeToLog("Cannot create region: No selection");
            return;
        }

        // Get selection in samples
        double startTime = doc->getWaveformDisplay().getSelectionStart();
        double endTime = doc->getWaveformDisplay().getSelectionEnd();
        int64_t startSample = doc->getBufferManager().timeToSample(startTime);
        int64_t endSample = doc->getBufferManager().timeToSample(endTime);

        // Apply zero-crossing snap if enabled (Phase 3.3)
        if (Settings::getInstance().getSnapRegionsToZeroCrossings())
        {
            const auto& buffer = doc->getBufferManager().getBuffer();
            if (buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0)
            {
                int channel = 0;  // Use first channel for snap detection
                int searchRadius = 1000;  // 1000 samples (~22ms at 44.1kHz)

                int64_t originalStart = startSample;
                int64_t originalEnd = endSample;

                startSample = AudioUnits::snapToZeroCrossing(startSample, buffer, channel, searchRadius);
                endSample = AudioUnits::snapToZeroCrossing(endSample, buffer, channel, searchRadius);

                juce::Logger::writeToLog(juce::String::formatted(
                    "Zero-crossing snap: start %lld -> %lld, end %lld -> %lld",
                    originalStart, startSample, originalEnd, endSample));
            }
        }

        // Create region with auto-generated numerical name (001, 002, etc.)
        int regionNum = doc->getRegionManager().getNumRegions() + 1;
        juce::String regionName = juce::String(regionNum).paddedLeft('0', 3);  // Zero-padded to 3 digits
        Region newRegion(regionName, startSample, endSample);

        // Start a new transaction
        juce::String transactionName = "Add Region: " + regionName;
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action (perform() will add the region)
        auto* undoAction = new AddRegionUndoAction(
            doc->getRegionManager(),
            doc->getRegionDisplay(),
            doc->getFile(),
            newRegion
        );

        // perform() calls AddRegionUndoAction::perform() which adds the region
        doc->getUndoManager().perform(undoAction);

        // Repaint to show new region
        repaint();

        juce::Logger::writeToLog("Added region: " + regionName);
    }

    void deleteSelectedRegion()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        // Get selected region index
        int regionIndex = doc->getRegionManager().getSelectedRegionIndex();
        if (regionIndex < 0)
        {
            juce::Logger::writeToLog("Cannot delete region: No region selected");
            return;
        }

        // Get region before deleting (for logging)
        const Region* region = doc->getRegionManager().getRegion(regionIndex);
        if (!region)
        {
            juce::Logger::writeToLog("Cannot delete region: Invalid region index");
            return;
        }

        juce::String regionName = region->getName();

        // Start a new transaction
        juce::String transactionName = "Delete Region: " + regionName;
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action (perform() will delete the region)
        auto* undoAction = new DeleteRegionUndoAction(
            doc->getRegionManager(),
            doc->getRegionDisplay(),
            doc->getFile(),
            regionIndex
        );

        // perform() calls DeleteRegionUndoAction::perform() which deletes the region
        doc->getUndoManager().perform(undoAction);

        // Repaint to update display
        repaint();

        juce::Logger::writeToLog("Deleted region: " + regionName);
    }

    void jumpToNextRegion()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        int numRegions = doc->getRegionManager().getNumRegions();
        if (numRegions == 0)
        {
            juce::Logger::writeToLog("No regions to navigate");
            return;
        }

        // Create sorted list of region indices by start sample (timeline order)
        juce::Array<int> sortedIndices;
        for (int i = 0; i < numRegions; ++i)
            sortedIndices.add(i);

        // Sort by timeline order (start sample position) using std::sort
        std::sort(sortedIndices.begin(), sortedIndices.end(),
            [&](int a, int b)
            {
                const Region* regionA = doc->getRegionManager().getRegion(a);
                const Region* regionB = doc->getRegionManager().getRegion(b);
                if (!regionA || !regionB) return false;
                return regionA->getStartSample() < regionB->getStartSample();
            });

        // Get current selected region index
        int currentIndex = doc->getRegionManager().getSelectedRegionIndex();

        // Find position in sorted list
        int nextIndex;
        if (currentIndex < 0)
        {
            // No region selected, start from first in timeline
            nextIndex = sortedIndices[0];
        }
        else
        {
            // Find current position in sorted list
            int currentPos = sortedIndices.indexOf(currentIndex);
            if (currentPos < 0)
                currentPos = 0; // Fallback if not found

            // Move to next in timeline order with wraparound
            int nextPos = (currentPos + 1) % sortedIndices.size();
            nextIndex = sortedIndices[nextPos];
        }

        const Region* nextRegion = doc->getRegionManager().getRegion(nextIndex);
        if (nextRegion)
        {
            // Set selection to region bounds
            double startTime = doc->getBufferManager().sampleToTime(nextRegion->getStartSample());
            double endTime = doc->getBufferManager().sampleToTime(nextRegion->getEndSample());
            doc->getWaveformDisplay().setSelection(startTime, endTime);

            // Select region in manager
            doc->getRegionManager().setSelectedRegionIndex(nextIndex);

            repaint();
            juce::Logger::writeToLog("Jumped to next region (timeline order): " + nextRegion->getName());
        }
    }

    void jumpToPreviousRegion()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        int numRegions = doc->getRegionManager().getNumRegions();
        if (numRegions == 0)
        {
            juce::Logger::writeToLog("No regions to navigate");
            return;
        }

        // Create sorted list of region indices by start sample (timeline order)
        juce::Array<int> sortedIndices;
        for (int i = 0; i < numRegions; ++i)
            sortedIndices.add(i);

        // Sort by timeline order (start sample position) using std::sort
        std::sort(sortedIndices.begin(), sortedIndices.end(),
            [&](int a, int b)
            {
                const Region* regionA = doc->getRegionManager().getRegion(a);
                const Region* regionB = doc->getRegionManager().getRegion(b);
                if (!regionA || !regionB) return false;
                return regionA->getStartSample() < regionB->getStartSample();
            });

        // Get current selected region index
        int currentIndex = doc->getRegionManager().getSelectedRegionIndex();

        // Find position in sorted list
        int prevIndex;
        if (currentIndex < 0)
        {
            // No region selected, start from last in timeline
            prevIndex = sortedIndices[sortedIndices.size() - 1];
        }
        else
        {
            // Find current position in sorted list
            int currentPos = sortedIndices.indexOf(currentIndex);
            if (currentPos < 0)
                currentPos = 0; // Fallback if not found

            // Move to previous in timeline order with wraparound
            int prevPos = (currentPos - 1 + sortedIndices.size()) % sortedIndices.size();
            prevIndex = sortedIndices[prevPos];
        }

        const Region* prevRegion = doc->getRegionManager().getRegion(prevIndex);
        if (prevRegion)
        {
            // Set selection to region bounds
            double startTime = doc->getBufferManager().sampleToTime(prevRegion->getStartSample());
            double endTime = doc->getBufferManager().sampleToTime(prevRegion->getEndSample());
            doc->getWaveformDisplay().setSelection(startTime, endTime);

            // Select region in manager
            doc->getRegionManager().setSelectedRegionIndex(prevIndex);

            repaint();
            juce::Logger::writeToLog("Jumped to previous region (timeline order): " + prevRegion->getName());
        }
    }

    void selectInverseOfRegions()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        // Get inverse ranges (everything NOT in regions)
        int64_t totalSamples = doc->getBufferManager().getNumSamples();
        auto inverseRanges = doc->getRegionManager().getInverseRanges(totalSamples);

        if (inverseRanges.isEmpty())
        {
            juce::Logger::writeToLog("No inverse selection: All audio covered by regions");
            return;
        }

        // For MVP: Select the first inverse range
        // TODO Phase 4: Support multi-range selection
        auto firstRange = inverseRanges[0];
        double startTime = doc->getBufferManager().sampleToTime(firstRange.first);
        double endTime = doc->getBufferManager().sampleToTime(firstRange.second);

        doc->getWaveformDisplay().setSelection(startTime, endTime);
        repaint();

        juce::Logger::writeToLog(juce::String::formatted(
            "Selected inverse of regions (%d gap%s found, showing first)",
            inverseRanges.size(),
            inverseRanges.size() == 1 ? "" : "s"));
    }

    void selectAllRegions()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (doc->getRegionManager().getNumRegions() == 0)
        {
            juce::Logger::writeToLog("No regions to select");
            return;
        }

        // Find union of all regions (earliest start to latest end)
        int64_t earliestStart = std::numeric_limits<int64_t>::max();
        int64_t latestEnd = 0;

        for (int i = 0; i < doc->getRegionManager().getNumRegions(); ++i)
        {
            const Region* region = doc->getRegionManager().getRegion(i);
            if (region)
            {
                earliestStart = std::min(earliestStart, region->getStartSample());
                latestEnd = std::max(latestEnd, region->getEndSample());
            }
        }

        double startTime = doc->getBufferManager().sampleToTime(earliestStart);
        double endTime = doc->getBufferManager().sampleToTime(latestEnd);

        doc->getWaveformDisplay().setSelection(startTime, endTime);
        repaint();

        juce::Logger::writeToLog("Selected union of all regions");
    }

    /**
     * Show Auto Region dialog for auto-creating regions from non-silent audio.
     */
    void showStripSilenceDialog()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        // Get audio buffer, sample rate, and current file from current document
        const auto& buffer = doc->getBufferManager().getBuffer();
        double sampleRate = doc->getBufferManager().getSampleRate();
        juce::File currentFile = doc->getAudioEngine().getCurrentFile();

        // CRITICAL: Capture old regions BEFORE showing dialog
        // This enables undo support since the dialog modifies regions directly
        juce::Array<Region> oldRegions;
        const auto& allRegions = doc->getRegionManager().getAllRegions();
        for (const auto& region : allRegions)
        {
            oldRegions.add(region);
        }

        // Create dialog
        auto* dialog = new StripSilenceDialog(doc->getRegionManager(), buffer, sampleRate);

        // Set up Apply callback with retrospective undo support
        dialog->onApply = [this, doc, currentFile, oldRegions = oldRegions](int numRegionsCreated) mutable
        {
            // Get current region display from document
            auto& regionDisplay = doc->getRegionDisplay();

            // NOTE: The dialog already applied Auto Region in applyStripSilence(false)
            // So the regions are already changed. We create a retrospective undo action.

            // Capture new regions (after Auto Region)
            juce::Array<Region> newRegions;
            const auto& currentRegions = doc->getRegionManager().getAllRegions();
            for (const auto& region : currentRegions)
            {
                newRegions.add(region);
            }

            // Create retrospective undo action
            // This action doesn't need to call perform() since changes are already applied
            auto* undoAction = new RetrospectiveStripSilenceUndoAction(
                doc->getRegionManager(),
                regionDisplay,
                currentFile,
                oldRegions,
                newRegions
            );

            // Add to undo manager (don't call perform() since changes already applied)
            doc->getUndoManager().perform(undoAction, "Auto Region");

            // Save regions to JSON file
            doc->getRegionManager().saveToFile(currentFile);

            // Request repaint to show created regions
            regionDisplay.repaint();

            juce::Logger::writeToLog("Auto Region: Created " + juce::String(numRegionsCreated) + " regions with undo support");
        };

        // Set up Cancel callback
        dialog->onCancel = []()
        {
            juce::Logger::writeToLog("Auto Region: Cancelled by user");
        };

        // Show dialog modally
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(dialog);
        options.dialogTitle = "Auto Region";
        options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.useBottomRightCornerResizer = false;

        // Center over main window
        auto mainBounds = getScreenBounds();
        auto dialogBounds = juce::Rectangle<int>(0, 0, 650, 580);  // Size for waveform preview
        dialogBounds.setCentre(mainBounds.getCentre());
        options.content->setBounds(dialogBounds);

        // Launch dialog
        options.launchAsync();
    }

    //==============================================================================
    // Region editing helper methods (Phase 3.4)

    /**
     * Checks if the selected region can be merged with the next region.
     */
    bool canMergeRegions(Document* doc) const
    {
        if (!doc) return false;

        auto& regionManager = doc->getRegionManager();
        int numSelected = regionManager.getNumSelectedRegions();

        // Phase 3.5: Multi-selection merge support
        if (numSelected >= 2)
        {
            // Multiple regions selected: Can merge them all
            return true;
        }
        else if (numSelected == 1)
        {
            // Single region selected: Can merge if there's a next region (legacy behavior)
            int selectedIndex = regionManager.getPrimarySelectionIndex();
            return (selectedIndex >= 0 && selectedIndex < regionManager.getNumRegions() - 1);
        }

        // No regions selected: Cannot merge
        return false;
    }

    /**
     * Checks if the region under cursor can be split.
     */
    bool canSplitRegion(Document* doc) const
    {
        if (!doc) return false;

        auto& regionManager = doc->getRegionManager();
        auto& waveformDisplay = doc->getWaveformDisplay();
        auto& bufferManager = doc->getBufferManager();

        int64_t cursorSample = bufferManager.timeToSample(waveformDisplay.getEditCursorPosition());
        int regionIndex = regionManager.findRegionAtSample(cursorSample);

        if (regionIndex < 0)
            return false;

        const Region* region = regionManager.getRegion(regionIndex);
        if (!region)
            return false;

        // Can split if cursor is within region and both resulting regions have >= 1 sample
        return cursorSample > region->getStartSample() &&
               cursorSample < region->getEndSample();
    }

    /**
     * Merges the selected region with the next adjacent region.
     */
    void mergeSelectedRegions()
    {
        auto* doc = getCurrentDocument();
        if (!doc || !canMergeRegions(doc))
            return;

        auto& regionManager = doc->getRegionManager();

        // Save original regions and indices for undo
        juce::Array<int> originalIndices;
        juce::Array<Region> originalRegions;
        auto selectedIndices = regionManager.getSelectedRegionIndices();

        for (int idx : selectedIndices)
        {
            const Region* region = regionManager.getRegion(idx);
            if (region != nullptr)
            {
                originalIndices.add(idx);
                originalRegions.add(*region);
            }
        }

        // Create and perform undo action
        auto* undoAction = new MultiMergeRegionsUndoAction(
            regionManager,
            doc->getRegionDisplay(),
            doc->getFile(),
            originalIndices,
            originalRegions
        );

        doc->getUndoManager().perform(undoAction);

        // Repaint displays
        doc->getWaveformDisplay().repaint();

        juce::String mergedNames;
        for (int i = 0; i < originalRegions.size(); ++i)
        {
            if (i > 0) mergedNames += " + ";
            mergedNames += originalRegions[i].getName();
        }
        juce::Logger::writeToLog("Merged " + juce::String(originalRegions.size()) + " regions: " + mergedNames);
    }

    /**
     * Splits the region at the cursor position.
     */
    void splitRegionAtCursor()
    {
        auto* doc = getCurrentDocument();
        if (!doc || !canSplitRegion(doc))
            return;

        auto& regionManager = doc->getRegionManager();
        auto& waveformDisplay = doc->getWaveformDisplay();
        auto& bufferManager = doc->getBufferManager();

        int64_t splitSample = bufferManager.timeToSample(waveformDisplay.getEditCursorPosition());
        int regionIndex = regionManager.findRegionAtSample(splitSample);

        // Save original region for undo
        Region originalRegion = *regionManager.getRegion(regionIndex);

        // Create undo action
        auto* undoAction = new SplitRegionUndoAction(
            regionManager,
            &doc->getRegionDisplay(),
            regionIndex,
            splitSample,
            originalRegion
        );

        doc->getUndoManager().perform(undoAction);

        juce::Logger::writeToLog("Split region: " + originalRegion.getName());
    }

    /**
     * Copies the selected region definition to clipboard.
     */
    void copyRegionsToClipboard()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        auto& regionManager = doc->getRegionManager();

        // Copy only the selected region to clipboard
        m_regionClipboard.clear();

        int selectedIndex = regionManager.getSelectedRegionIndex();
        if (selectedIndex >= 0)
        {
            if (const auto* region = regionManager.getRegion(selectedIndex))
            {
                m_regionClipboard.push_back(*region);
                juce::Logger::writeToLog("Copied region '" + region->getName() + "' to clipboard");
            }
        }

        m_hasRegionClipboard = !m_regionClipboard.empty();
    }

    /**
     * Pastes regions from clipboard at cursor position.
     */
    void pasteRegionsFromClipboard()
    {
        auto* doc = getCurrentDocument();
        if (!doc || !m_hasRegionClipboard) return;

        auto& regionManager = doc->getRegionManager();
        auto& waveformDisplay = doc->getWaveformDisplay();
        auto& bufferManager = doc->getBufferManager();

        // Get cursor position as paste offset
        int64_t cursorSample = bufferManager.timeToSample(waveformDisplay.getEditCursorPosition());

        // Calculate offset (align first region's start to cursor)
        if (m_regionClipboard.empty()) return;

        int64_t firstRegionStart = m_regionClipboard[0].getStartSample();
        int64_t offset = cursorSample - firstRegionStart;

        // Paste all regions with offset
        int numPasted = 0;
        for (const auto& region : m_regionClipboard)
        {
            int64_t newStart = region.getStartSample() + offset;
            int64_t newEnd = region.getEndSample() + offset;

            // Check if fits within audio duration (both negative and exceeding boundaries)
            int64_t maxSample = bufferManager.getNumSamples();
            if (newStart < 0 || newEnd > maxSample)
            {
                juce::Logger::writeToLog(juce::String::formatted(
                    "Stopped pasting: Region '%s' would be outside file bounds (start=%lld, end=%lld, max=%lld)",
                    region.getName().toRawUTF8(), newStart, newEnd, maxSample));
                break;  // Stop pasting if region would be out of bounds
            }

            Region newRegion(region.getName(), newStart, newEnd);
            newRegion.setColor(region.getColor());
            regionManager.addRegion(newRegion);
            numPasted++;
        }

        doc->getRegionDisplay().repaint();

        juce::Logger::writeToLog(juce::String::formatted(
            "Pasted %d region%s at sample %lld",
            numPasted,
            numPasted == 1 ? "" : "s",
            cursorSample));
    }

    /**
     * Shows the batch export dialog for exporting regions as separate files.
     * Called from message thread only.
     */
    void showBatchExportDialog()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        // Validate preconditions
        if (!doc->getAudioEngine().isFileLoaded())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "No Audio File",
                "Please load an audio file before exporting regions.",
                "OK"
            );
            return;
        }

        // Check if we have regions to export
        if (doc->getRegionManager().getNumRegions() == 0)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "No Regions to Export",
                "There are no regions defined in this file.\n\n"
                "Create regions first using:\n"
                "  • R - Create region from selection\n"
                "  • Cmd+Shift+R - Auto-create regions (Strip Silence)",
                "OK"
            );
            return;
        }

        // Show the batch export dialog
        auto exportSettings = BatchExportDialog::showDialog(
            doc->getFile(),
            doc->getRegionManager()
        );

        if (!exportSettings.has_value())
        {
            // User cancelled
            return;
        }

        // Prepare export settings for RegionExporter
        RegionExporter::ExportSettings settings;
        settings.outputDirectory = exportSettings->outputDirectory;
        settings.includeRegionName = exportSettings->includeRegionName;
        settings.includeIndex = exportSettings->includeIndex;
        settings.bitDepth = 24; // Default to 24-bit for professional quality

        // Create progress dialog with progress value
        double progressValue = 0.0;
        auto progressDialog = std::make_unique<juce::AlertWindow>(
            "Exporting Regions",
            "Exporting regions to separate files...",
            juce::AlertWindow::NoIcon
        );
        progressDialog->addProgressBarComponent(progressValue);
        progressDialog->enterModalState();

        // Export regions with progress callback
        int totalRegions = doc->getRegionManager().getNumRegions();
        int exportedCount = RegionExporter::exportRegions(
            doc->getBufferManager().getBuffer(),
            doc->getAudioEngine().getSampleRate(),
            doc->getRegionManager(),
            doc->getFile(),
            settings,
            [dlg = progressDialog.get(), totalRegions, &progressValue](int current, int total, const juce::String& regionName)
            {
                // Update progress value (referenced by AlertWindow)
                progressValue = static_cast<double>(current + 1) / static_cast<double>(total);

                // Update message
                dlg->setMessage("Exporting: " + regionName +
                               " (" + juce::String(current + 1) + "/" +
                               juce::String(totalRegions) + ")");

                return true; // Continue export
            }
        );

        // Close progress dialog
        progressDialog->exitModalState(0);
        progressDialog.reset();

        // Show result message
        if (exportedCount == totalRegions)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Export Complete",
                "Successfully exported " + juce::String(exportedCount) +
                " region" + (exportedCount > 1 ? "s" : "") + " to:\n\n" +
                settings.outputDirectory.getFullPathName(),
                "OK"
            );
        }
        else if (exportedCount > 0)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Partial Export",
                "Exported " + juce::String(exportedCount) + " of " +
                juce::String(totalRegions) + " regions.\n\n" +
                "Check the console log for details about failed exports.",
                "OK"
            );
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Export Failed",
                "Failed to export any regions.\n\n"
                "Check the console log for error details.",
                "OK"
            );
        }
    }

    /**
     * Nudge region boundary by current snap increment.
     * Phase 3.3 - Feature #6: Keyboard precision control for region boundaries.
     *
     * @param nudgeStart true to nudge start boundary, false to nudge end boundary
     * @param moveLeft true to move left (negative), false to move right (positive)
     */
    void nudgeRegionBoundary(bool nudgeStart, bool moveLeft)
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        // Get selected region
        int regionIndex = doc->getRegionManager().getSelectedRegionIndex();
        if (regionIndex < 0)
        {
            juce::Logger::writeToLog("Cannot nudge region: No region selected");
            return;
        }

        Region* region = doc->getRegionManager().getRegion(regionIndex);
        if (!region)
        {
            juce::Logger::writeToLog("Cannot nudge region: Invalid region index");
            return;
        }

        // Get current snap increment in samples
        int64_t increment = doc->getWaveformDisplay().getSnapIncrementInSamples();
        if (moveLeft)
            increment = -increment;

        // Calculate new boundary position
        int64_t oldPosition = nudgeStart ? region->getStartSample() : region->getEndSample();
        int64_t newPosition = oldPosition + increment;

        // Validate boundaries
        int64_t totalSamples = doc->getBufferManager().getNumSamples();

        if (nudgeStart)
        {
            // Start boundary constraints:
            // - Can't go below 0
            // - Can't exceed end boundary (must maintain at least 1 sample region)
            int64_t endSample = region->getEndSample();
            newPosition = juce::jlimit((int64_t)0, endSample - 1, newPosition);
        }
        else
        {
            // End boundary constraints:
            // - Can't go below start boundary (must maintain at least 1 sample region)
            // - Can't exceed total duration
            int64_t startSample = region->getStartSample();
            newPosition = juce::jlimit(startSample + 1, totalSamples, newPosition);
        }

        // Check if position actually changed
        if (newPosition == oldPosition)
        {
            juce::Logger::writeToLog("Cannot nudge region: Boundary already at limit");
            return;
        }

        // Start a new transaction
        juce::String transactionName = juce::String::formatted(
            "Nudge Region %s %s: %s",
            nudgeStart ? "Start" : "End",
            moveLeft ? "Left" : "Right",
            region->getName().toRawUTF8()
        );
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action (perform() will apply the nudge)
        auto* undoAction = new NudgeRegionUndoAction(
            doc->getRegionManager(),
            &doc->getRegionDisplay(),
            regionIndex,
            nudgeStart,
            oldPosition,
            newPosition
        );

        // perform() calls NudgeRegionUndoAction::perform() which updates the boundary
        doc->getUndoManager().perform(undoAction);

        // Log the nudge
        double oldTime = doc->getBufferManager().sampleToTime(oldPosition);
        double newTime = doc->getBufferManager().sampleToTime(newPosition);
        juce::Logger::writeToLog(juce::String::formatted(
            "Nudged region '%s' %s: %.3fs -> %.3fs (delta: %lld samples)",
            region->getName().toRawUTF8(),
            nudgeStart ? "start" : "end",
            oldTime,
            newTime,
            increment
        ));

        // Repaint to show updated region
        repaint();
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
     * Apply silence to selection.
     * Fills the selected region with digital silence (zeros).
     * Creates undo action for the silence operation.
     */
    void silenceSelection()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        // Silence requires a selection
        if (!doc->getWaveformDisplay().hasSelection())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Silence",
                "Please select a region to silence.",
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

        // Create a temporary buffer with just the region to silence
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
        juce::String transactionName = "Silence (selection)";
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action (perform() will apply the silence)
        auto* undoAction = new SilenceUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer,
            startSample,
            numSamples
        );

        // perform() calls SilenceUndoAction::perform() which silences and updates display
        doc->getUndoManager().perform(undoAction);

        // Mark as modified
        doc->setModified(true);

        // Log the operation
        juce::String message = juce::String::formatted(
            "Silenced selection (%d samples, %.3f seconds)",
            numSamples, (double)numSamples / doc->getBufferManager().getSampleRate());
        juce::Logger::writeToLog(message);
    }

    /**
     * Trim to selection.
     * Deletes everything OUTSIDE the selection, keeping only the selected region.
     * Creates undo action for the trim operation.
     */
    void trimToSelection()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        // Trim requires a selection
        if (!doc->getWaveformDisplay().hasSelection())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Trim",
                "Please select a region to keep. Everything outside will be deleted.",
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
        double selectionStart = doc->getWaveformDisplay().getSelectionStart();
        double selectionEnd = doc->getWaveformDisplay().getSelectionEnd();
        int startSample = doc->getBufferManager().timeToSample(selectionStart);
        int endSample = doc->getBufferManager().timeToSample(selectionEnd);
        int numSamples = endSample - startSample;

        if (numSamples <= 0)
        {
            return;
        }

        // Store entire buffer before trimming for undo
        juce::AudioBuffer<float> beforeBuffer;
        beforeBuffer.makeCopyOf(buffer, true);

        // Start a new transaction
        juce::String transactionName = "Trim (selection)";
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action (perform() will apply the trim)
        auto* undoAction = new TrimUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer,
            startSample,
            numSamples
        );

        // perform() calls TrimUndoAction::perform() which trims and updates display
        doc->getUndoManager().perform(undoAction);

        // Mark as modified
        doc->setModified(true);

        // Log the operation
        juce::String message = juce::String::formatted(
            "Trimmed to selection (%d samples kept, %.3f seconds)",
            numSamples, (double)numSamples / doc->getBufferManager().getSampleRate());
        juce::Logger::writeToLog(message);
    }

    /**
     * Apply DC offset removal to entire file.
     * DC offset causes waveform asymmetry and can affect dynamics processing.
     * Creates undo action for the DC offset removal operation.
     */
    void applyDCOffsetRemoval()
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

        // Store entire buffer before processing for undo
        juce::AudioBuffer<float> beforeBuffer;
        beforeBuffer.makeCopyOf(buffer, true);

        // Start a new transaction
        juce::String transactionName = "Remove DC Offset (entire file)";
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action (perform() will apply the DC offset removal)
        auto* undoAction = new DCOffsetRemovalUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer
        );

        // perform() calls DCOffsetRemovalUndoAction::perform() which removes DC offset and updates display
        doc->getUndoManager().perform(undoAction);

        // Mark as modified
        doc->setModified(true);

        // Log the operation
        juce::String message = "Removed DC offset from entire file";
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

    /**
     * Undo action for silence.
     * Stores the before state and region information.
     */
    class SilenceUndoAction : public juce::UndoableAction
    {
    public:
        SilenceUndoAction(AudioBufferManager& bufferManager,
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
            // Silence the region using AudioBufferManager
            bool success = m_bufferManager.silenceRange(m_startSample, m_numSamples);

            if (!success)
            {
                juce::Logger::writeToLog("SilenceUndoAction::perform - Failed to silence range");
                return false;
            }

            // Get the updated buffer
            auto& buffer = m_bufferManager.getMutableBuffer();

            // Reload buffer in AudioEngine - preserve playback if active
            m_audioEngine.reloadBufferPreservingPlayback(
                buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

            // Update waveform display - preserve view and selection
            m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                              true, true); // preserveView=true, preserveEditCursor=true

            // Log the operation
            juce::Logger::writeToLog("Applied silence to selection");

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
     * Undo action for trim.
     * Stores the entire buffer before trimming since trim changes the file length.
     */
    class TrimUndoAction : public juce::UndoableAction
    {
    public:
        TrimUndoAction(AudioBufferManager& bufferManager,
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
            // Store entire buffer since trim changes the file length
            m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
            m_beforeBuffer.makeCopyOf(beforeBuffer, true);
        }

        bool perform() override
        {
            // Trim to the range using AudioBufferManager
            bool success = m_bufferManager.trimToRange(m_startSample, m_numSamples);

            if (!success)
            {
                juce::Logger::writeToLog("TrimUndoAction::perform - Failed to trim range");
                return false;
            }

            // Get the updated buffer
            auto& buffer = m_bufferManager.getMutableBuffer();

            // Reload buffer in AudioEngine - preserve playback if active
            m_audioEngine.reloadBufferPreservingPlayback(
                buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

            // Update waveform display - clear selection since file length changed
            m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                              false, false); // preserveView=false, preserveEditCursor=false
            m_waveformDisplay.clearSelection();
            m_waveformDisplay.setEditCursor(0.0);

            // Log the operation
            juce::Logger::writeToLog("Trimmed to selection");

            return true;
        }

        bool undo() override
        {
            // Restore the entire buffer (before trim)
            auto& buffer = m_bufferManager.getMutableBuffer();
            buffer.setSize(m_beforeBuffer.getNumChannels(), m_beforeBuffer.getNumSamples());
            buffer.makeCopyOf(m_beforeBuffer, true);

            // Reload buffer in AudioEngine - preserve playback if active
            m_audioEngine.reloadBufferPreservingPlayback(buffer, m_bufferManager.getSampleRate(),
                                                        buffer.getNumChannels());

            // Update waveform display - clear selection since file length changed
            m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                              false, false); // preserveView=false, preserveEditCursor=false

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
     * Undo action for DC offset removal.
     * Stores the entire buffer before processing.
     */
    class DCOffsetRemovalUndoAction : public juce::UndoableAction
    {
    public:
        DCOffsetRemovalUndoAction(AudioBufferManager& bufferManager,
                                 WaveformDisplay& waveform,
                                 AudioEngine& audioEngine,
                                 const juce::AudioBuffer<float>& beforeBuffer)
            : m_bufferManager(bufferManager),
              m_waveformDisplay(waveform),
              m_audioEngine(audioEngine),
              m_beforeBuffer()
        {
            // Store entire buffer before DC offset removal
            m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
            m_beforeBuffer.makeCopyOf(beforeBuffer, true);
        }

        bool perform() override
        {
            // Apply DC offset removal to the entire buffer
            auto& buffer = m_bufferManager.getMutableBuffer();
            bool success = AudioProcessor::removeDCOffset(buffer);

            if (!success)
            {
                juce::Logger::writeToLog("DCOffsetRemovalUndoAction::perform - Failed to remove DC offset");
                return false;
            }

            // Reload buffer in AudioEngine - preserve playback if active
            m_audioEngine.reloadBufferPreservingPlayback(
                buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

            // Update waveform display - preserve view and selection
            m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                              true, true); // preserveView=true, preserveEditCursor=true

            // Log the operation
            juce::Logger::writeToLog("Removed DC offset from entire file");

            return true;
        }

        bool undo() override
        {
            // Restore the entire buffer (before DC offset removal)
            auto& buffer = m_bufferManager.getMutableBuffer();
            buffer.makeCopyOf(m_beforeBuffer, true);

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
    };

    //==============================================================================
    // Region Undo Actions (Phase 3 Tier 2)

    /**
     * Undoable action for adding a region.
     * Stores the region data to enable undo/redo.
     */
    class AddRegionUndoAction : public juce::UndoableAction
    {
    public:
        AddRegionUndoAction(RegionManager& regionManager,
                           RegionDisplay& regionDisplay,
                           const juce::File& audioFile,
                           const Region& region)
            : m_regionManager(regionManager),
              m_regionDisplay(regionDisplay),
              m_audioFile(audioFile),
              m_region(region),
              m_regionIndex(-1)
        {
        }

        bool perform() override
        {
            // Add region to manager
            m_regionManager.addRegion(m_region);
            m_regionIndex = m_regionManager.getNumRegions() - 1;

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            juce::Logger::writeToLog("Added region: " + m_region.getName());
            return true;
        }

        bool undo() override
        {
            // Remove the region we added
            if (m_regionIndex >= 0 && m_regionIndex < m_regionManager.getNumRegions())
            {
                m_regionManager.removeRegion(m_regionIndex);

                // Save to sidecar JSON file
                m_regionManager.saveToFile(m_audioFile);

                // Update display
                m_regionDisplay.repaint();

                juce::Logger::writeToLog("Undid region addition: " + m_region.getName());
            }
            return true;
        }

        int getSizeInUnits() override { return sizeof(*this) + sizeof(Region); }

    private:
        RegionManager& m_regionManager;
        RegionDisplay& m_regionDisplay;
        juce::File m_audioFile;
        Region m_region;
        int m_regionIndex;  // Index where region was added
    };

    /**
     * Undoable action for deleting a region.
     * Stores the deleted region and its index to enable restoration.
     */
    class DeleteRegionUndoAction : public juce::UndoableAction
    {
    public:
        DeleteRegionUndoAction(RegionManager& regionManager,
                              RegionDisplay& regionDisplay,
                              const juce::File& audioFile,
                              int regionIndex)
            : m_regionManager(regionManager),
              m_regionDisplay(regionDisplay),
              m_audioFile(audioFile),
              m_regionIndex(regionIndex),
              m_deletedRegion("", 0, 0)  // Placeholder, will be filled in perform()
        {
        }

        bool perform() override
        {
            // Store the region before deleting it
            const Region* region = m_regionManager.getRegion(m_regionIndex);
            if (region == nullptr)
            {
                juce::Logger::writeToLog("DeleteRegionUndoAction::perform - Invalid region index");
                return false;
            }

            // Copy the region data
            m_deletedRegion = *region;

            // Delete the region
            m_regionManager.removeRegion(m_regionIndex);

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            juce::Logger::writeToLog("Deleted region: " + m_deletedRegion.getName());
            return true;
        }

        bool undo() override
        {
            // Re-insert the region at its original index
            m_regionManager.insertRegionAt(m_regionIndex, m_deletedRegion);

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            juce::Logger::writeToLog("Undid region deletion: " + m_deletedRegion.getName());
            return true;
        }

        int getSizeInUnits() override { return sizeof(*this) + sizeof(Region); }

    private:
        RegionManager& m_regionManager;
        RegionDisplay& m_regionDisplay;
        juce::File m_audioFile;
        int m_regionIndex;
        Region m_deletedRegion;
    };

    /**
     * Undoable action for renaming a region.
     * Stores old and new names to enable undo/redo.
     */
    class RenameRegionUndoAction : public juce::UndoableAction
    {
    public:
        RenameRegionUndoAction(RegionManager& regionManager,
                              RegionDisplay& regionDisplay,
                              const juce::File& audioFile,
                              int regionIndex,
                              const juce::String& oldName,
                              const juce::String& newName)
            : m_regionManager(regionManager),
              m_regionDisplay(regionDisplay),
              m_audioFile(audioFile),
              m_regionIndex(regionIndex),
              m_oldName(oldName),
              m_newName(newName)
        {
        }

        bool perform() override
        {
            // Rename the region
            Region* region = m_regionManager.getRegion(m_regionIndex);
            if (region == nullptr)
            {
                juce::Logger::writeToLog("RenameRegionUndoAction::perform - Invalid region index");
                return false;
            }

            region->setName(m_newName);

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            juce::Logger::writeToLog("Renamed region from '" + m_oldName + "' to '" + m_newName + "'");
            return true;
        }

        bool undo() override
        {
            // Restore the old name
            Region* region = m_regionManager.getRegion(m_regionIndex);
            if (region != nullptr)
            {
                region->setName(m_oldName);

                // Save to sidecar JSON file
                m_regionManager.saveToFile(m_audioFile);

                // Update display
                m_regionDisplay.repaint();

                juce::Logger::writeToLog("Undid region rename: restored '" + m_oldName + "'");
            }
            return true;
        }

        int getSizeInUnits() override { return sizeof(*this) + m_oldName.length() + m_newName.length(); }

    private:
        RegionManager& m_regionManager;
        RegionDisplay& m_regionDisplay;
        juce::File m_audioFile;
        int m_regionIndex;
        juce::String m_oldName;
        juce::String m_newName;
    };

    /**
     * Undoable action for changing a region's color.
     * Stores old and new colors to enable undo/redo.
     */
    class ChangeRegionColorUndoAction : public juce::UndoableAction
    {
    public:
        ChangeRegionColorUndoAction(RegionManager& regionManager,
                                   RegionDisplay& regionDisplay,
                                   const juce::File& audioFile,
                                   int regionIndex,
                                   const juce::Colour& oldColor,
                                   const juce::Colour& newColor)
            : m_regionManager(regionManager),
              m_regionDisplay(regionDisplay),
              m_audioFile(audioFile),
              m_regionIndex(regionIndex),
              m_oldColor(oldColor),
              m_newColor(newColor)
        {
        }

        bool perform() override
        {
            // Change the region color
            Region* region = m_regionManager.getRegion(m_regionIndex);
            if (region == nullptr)
            {
                juce::Logger::writeToLog("ChangeRegionColorUndoAction::perform - Invalid region index");
                return false;
            }

            region->setColor(m_newColor);

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            juce::Logger::writeToLog("Changed region color");
            return true;
        }

        bool undo() override
        {
            // Restore the old color
            Region* region = m_regionManager.getRegion(m_regionIndex);
            if (region != nullptr)
            {
                region->setColor(m_oldColor);

                // Save to sidecar JSON file
                m_regionManager.saveToFile(m_audioFile);

                // Update display
                m_regionDisplay.repaint();

                juce::Logger::writeToLog("Undid region color change");
            }
            return true;
        }

        int getSizeInUnits() override { return sizeof(*this) + sizeof(juce::Colour) * 2; }

    private:
        RegionManager& m_regionManager;
        RegionDisplay& m_regionDisplay;
        juce::File m_audioFile;
        int m_regionIndex;
        juce::Colour m_oldColor;
        juce::Colour m_newColor;
    };

    /**
     * Undoable action for resizing a region's boundaries.
     * Stores old and new start/end samples to enable undo/redo.
     */
    class ResizeRegionUndoAction : public juce::UndoableAction
    {
    public:
        ResizeRegionUndoAction(RegionManager& regionManager,
                              RegionDisplay& regionDisplay,
                              const juce::File& audioFile,
                              int regionIndex,
                              int64_t oldStart,
                              int64_t oldEnd,
                              int64_t newStart,
                              int64_t newEnd)
            : m_regionManager(regionManager),
              m_regionDisplay(regionDisplay),
              m_audioFile(audioFile),
              m_regionIndex(regionIndex),
              m_oldStart(oldStart),
              m_oldEnd(oldEnd),
              m_newStart(newStart),
              m_newEnd(newEnd)
        {
        }

        bool perform() override
        {
            // Resize the region
            Region* region = m_regionManager.getRegion(m_regionIndex);
            if (region == nullptr)
            {
                juce::Logger::writeToLog("ResizeRegionUndoAction::perform - Invalid region index");
                return false;
            }

            region->setStartSample(m_newStart);
            region->setEndSample(m_newEnd);

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            juce::Logger::writeToLog(juce::String::formatted(
                "Resized region: %lld-%lld → %lld-%lld",
                m_oldStart, m_oldEnd, m_newStart, m_newEnd));
            return true;
        }

        bool undo() override
        {
            // Restore the old boundaries
            Region* region = m_regionManager.getRegion(m_regionIndex);
            if (region != nullptr)
            {
                region->setStartSample(m_oldStart);
                region->setEndSample(m_oldEnd);

                // Save to sidecar JSON file
                m_regionManager.saveToFile(m_audioFile);

                // Update display
                m_regionDisplay.repaint();

                juce::Logger::writeToLog("Undid region resize");
            }
            return true;
        }

        int getSizeInUnits() override { return sizeof(*this) + sizeof(int64_t) * 4; }

    private:
        RegionManager& m_regionManager;
        RegionDisplay& m_regionDisplay;
        juce::File m_audioFile;
        int m_regionIndex;
        int64_t m_oldStart;
        int64_t m_oldEnd;
        int64_t m_newStart;
        int64_t m_newEnd;
    };

    /**
     * Undoable action for merging multiple selected regions.
     * Stores the original regions and their indices to enable restoration.
     */
    class MultiMergeRegionsUndoAction : public juce::UndoableAction
    {
    public:
        MultiMergeRegionsUndoAction(RegionManager& regionManager,
                                    RegionDisplay& regionDisplay,
                                    const juce::File& audioFile,
                                    const juce::Array<int>& originalIndices,
                                    const juce::Array<Region>& originalRegions)
            : m_regionManager(regionManager),
              m_regionDisplay(regionDisplay),
              m_audioFile(audioFile),
              m_originalIndices(originalIndices),
              m_originalRegions(originalRegions),
              m_mergedRegionIndex(-1)
        {
            // Store the merged region that will be created
            // We need to create it now for undo purposes
        }

        bool perform() override
        {
            // Merge using RegionManager's multi-selection merge
            if (!m_regionManager.mergeSelectedRegions())
            {
                juce::Logger::writeToLog("MultiMergeRegionsUndoAction::perform() - Merge failed");
                return false;
            }

            // Store the index of the merged region (should be where first region was)
            m_mergedRegionIndex = m_originalIndices[0];

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            juce::Logger::writeToLog("Merged " + juce::String(m_originalRegions.size()) + " regions");
            return true;
        }

        bool undo() override
        {
            // Remove the merged region
            if (m_mergedRegionIndex >= 0 && m_mergedRegionIndex < m_regionManager.getNumRegions())
            {
                m_regionManager.removeRegion(m_mergedRegionIndex);

                // Restore original regions at their original indices
                for (int i = 0; i < m_originalIndices.size(); ++i)
                {
                    int originalIndex = m_originalIndices[i];
                    const Region& region = m_originalRegions[i];

                    // Insert at original position
                    m_regionManager.insertRegionAt(originalIndex, region);
                }

                // Save to sidecar JSON file
                m_regionManager.saveToFile(m_audioFile);

                // Update display
                m_regionDisplay.repaint();

                juce::Logger::writeToLog("Undid merge of " + juce::String(m_originalRegions.size()) + " regions");
            }
            return true;
        }

        int getSizeInUnits() override
        {
            return sizeof(*this) + m_originalRegions.size() * sizeof(Region);
        }

    private:
        RegionManager& m_regionManager;
        RegionDisplay& m_regionDisplay;
        juce::File m_audioFile;
        juce::Array<int> m_originalIndices;  // Original indices of regions being merged
        juce::Array<Region> m_originalRegions;  // Original regions before merge
        int m_mergedRegionIndex;  // Index where merged region was placed
    };

    /**
     * Undoable action for Auto Region auto-region creation.
     * Stores the old region state and the new regions created by Auto Region.
     */
    class StripSilenceUndoAction : public juce::UndoableAction
    {
    public:
        StripSilenceUndoAction(RegionManager& regionManager,
                              RegionDisplay& regionDisplay,
                              const juce::File& audioFile,
                              const juce::AudioBuffer<float>& buffer,
                              double sampleRate,
                              float thresholdDB,
                              float minRegionLengthMs,
                              float minSilenceLengthMs,
                              float preRollMs,
                              float postRollMs)
            : m_regionManager(regionManager),
              m_regionDisplay(regionDisplay),
              m_audioFile(audioFile),
              m_buffer(buffer),
              m_sampleRate(sampleRate),
              m_thresholdDB(thresholdDB),
              m_minRegionLengthMs(minRegionLengthMs),
              m_minSilenceLengthMs(minSilenceLengthMs),
              m_preRollMs(preRollMs),
              m_postRollMs(postRollMs)
        {
            // Store old regions before Auto Region is applied
            const auto& allRegions = m_regionManager.getAllRegions();
            for (const auto& region : allRegions)
            {
                m_oldRegions.add(region);
            }
        }

        bool perform() override
        {
            // Apply Auto Region algorithm (clears old regions and creates new ones)
            m_regionManager.autoCreateRegions(m_buffer, m_sampleRate,
                                             m_thresholdDB, m_minRegionLengthMs,
                                             m_minSilenceLengthMs,
                                             m_preRollMs, m_postRollMs);

            // Store the newly created regions for redo
            m_newRegions.clear();
            const auto& allRegions = m_regionManager.getAllRegions();
            for (const auto& region : allRegions)
            {
                m_newRegions.add(region);
            }

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            juce::Logger::writeToLog("Auto Region: Created " + juce::String(m_newRegions.size()) + " regions");
            return true;
        }

        bool undo() override
        {
            // Clear current regions
            m_regionManager.removeAllRegions();

            // Restore old regions (before Auto Region was applied)
            for (const auto& region : m_oldRegions)
            {
                m_regionManager.addRegion(region);
            }

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            juce::Logger::writeToLog("Undid Auto Region: Restored " + juce::String(m_oldRegions.size()) + " original regions");
            return true;
        }

        int getSizeInUnits() override
        {
            // Only count the regions - m_buffer is stored by reference (not copied)
            return sizeof(*this) +
                   m_oldRegions.size() * sizeof(Region) +
                   m_newRegions.size() * sizeof(Region);
        }

    private:
        RegionManager& m_regionManager;
        RegionDisplay& m_regionDisplay;
        juce::File m_audioFile;
        const juce::AudioBuffer<float>& m_buffer;
        double m_sampleRate;
        float m_thresholdDB;
        float m_minRegionLengthMs;
        float m_minSilenceLengthMs;
        float m_preRollMs;
        float m_postRollMs;

        juce::Array<Region> m_oldRegions;  // Regions before Auto Region
        juce::Array<Region> m_newRegions;  // Regions created by Auto Region
    };

    /**
     * Retrospective undoable action for Auto Region.
     * Used when the dialog has already applied changes.
     * Stores both old and new region states for undo/redo.
     */
    class RetrospectiveStripSilenceUndoAction : public juce::UndoableAction
    {
    public:
        RetrospectiveStripSilenceUndoAction(RegionManager& regionManager,
                                            RegionDisplay& regionDisplay,
                                            const juce::File& audioFile,
                                            const juce::Array<Region>& oldRegions,
                                            const juce::Array<Region>& newRegions)
            : m_regionManager(regionManager),
              m_regionDisplay(regionDisplay),
              m_audioFile(audioFile),
              m_oldRegions(oldRegions),
              m_newRegions(newRegions)
        {
        }

        bool perform() override
        {
            // Changes are already applied by the dialog.
            // This method is only called on redo, so restore new regions.
            m_regionManager.removeAllRegions();

            for (const auto& region : m_newRegions)
            {
                m_regionManager.addRegion(region);
            }

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            juce::Logger::writeToLog("Redo Auto Region: Restored " + juce::String(m_newRegions.size()) + " regions");
            return true;
        }

        bool undo() override
        {
            // Clear current regions
            m_regionManager.removeAllRegions();

            // Restore old regions (before Auto Region was applied)
            for (const auto& region : m_oldRegions)
            {
                m_regionManager.addRegion(region);
            }

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            juce::Logger::writeToLog("Undo Auto Region: Restored " + juce::String(m_oldRegions.size()) + " original regions");
            return true;
        }

        int getSizeInUnits() override
        {
            return sizeof(*this) +
                   m_oldRegions.size() * sizeof(Region) +
                   m_newRegions.size() * sizeof(Region);
        }

    private:
        RegionManager& m_regionManager;
        RegionDisplay& m_regionDisplay;
        juce::File m_audioFile;
        juce::Array<Region> m_oldRegions;  // Regions before Auto Region
        juce::Array<Region> m_newRegions;  // Regions created by Auto Region
    };

private:
    // Audio device manager (shared across all documents)
    juce::AudioDeviceManager& m_audioDeviceManager;

    // Multi-document management
    DocumentManager m_documentManager;
    TabComponent m_tabComponent;
    Document* m_previousDocument = nullptr;  // Track previous document for cleanup during switching

    // Shared resources (keep these)
    AudioFileManager m_fileManager;
    juce::ApplicationCommandManager m_commandManager;
    std::unique_ptr<juce::FileChooser> m_fileChooser;

    // UI preferences (Phase 3.5)
    AudioUnits::TimeFormat m_timeFormat = AudioUnits::TimeFormat::Seconds;  // Default to seconds
    juce::Rectangle<int> m_formatIndicatorBounds;  // Bounds of clickable format indicator in status bar

    // Auto-save tracking (Phase 3.5 - Priority #6)
    int m_autoSaveTimerTicks = 0;  // Counter for auto-save timer (incremented every 50ms)
    const int m_autoSaveCheckInterval = 1200;  // Check auto-save every 1200 ticks (60 seconds)
    juce::ThreadPool m_autoSaveThreadPool {1};  // Background thread pool for auto-save (1 thread)

    // "No file open" label
    juce::Label m_noFileLabel;

    // Current document display containers (for showing active document's components)
    std::unique_ptr<juce::Component> m_currentDocumentContainer;

    // Region List Panel (Phase 3.4)
    RegionListPanel* m_regionListPanel = nullptr;  // Window owns this, we just track it
    juce::DocumentWindow* m_regionListWindow = nullptr;

    // Region clipboard (Phase 3.4) - for copy/paste region definitions
    std::vector<Region> m_regionClipboard;
    bool m_hasRegionClipboard = false;

    // Helper methods for safe document access

    /**
     * Wires up region callbacks for a document.
     * This connects RegionDisplay user interactions to RegionManager state updates.
     */
    void setupRegionCallbacks(Document* doc)
    {
        if (!doc)
            return;

        auto& regionDisplay = doc->getRegionDisplay();

        // onRegionClicked: Select region and update waveform selection
        regionDisplay.onRegionClicked = [this, doc](int regionIndex)
        {
            if (!doc) return;

            const Region* region = doc->getRegionManager().getRegion(regionIndex);
            if (!region)
            {
                juce::Logger::writeToLog("Cannot select region: Invalid region index");
                return;
            }

            // CRITICAL FIX: Do NOT modify selection state here!
            // RegionDisplay has already handled multi-selection correctly in mouseDown()
            // This callback should ONLY update the waveform display, not override selection

            // Update waveform selection to match the clicked region bounds
            double startTime = doc->getBufferManager().sampleToTime(region->getStartSample());
            double endTime = doc->getBufferManager().sampleToTime(region->getEndSample());
            doc->getWaveformDisplay().setSelection(startTime, endTime);

            // Repaint to show visual feedback
            doc->getRegionDisplay().repaint();

            // Synchronize with RegionListPanel if open
            if (m_regionListPanel)
            {
                m_regionListPanel->selectRegion(regionIndex);
            }

            // Auto-preview: Automatically play region if enabled
            if (Settings::getInstance().getAutoPreviewRegions())
            {
                // Stop any current playback first
                if (doc->getAudioEngine().isPlaying())
                {
                    doc->getAudioEngine().stop();
                }

                // Start playing from region start
                // Note: Playback will continue to end of file unless stopped manually
                // Future enhancement: Add auto-stop at region end using timer
                doc->getAudioEngine().setPosition(startTime);
                doc->getAudioEngine().play();

                juce::Logger::writeToLog("Auto-previewing region: " + region->getName() +
                                         " (" + juce::String(startTime, 3) + "s - " +
                                         juce::String(endTime, 3) + "s)");
            }

            juce::Logger::writeToLog("Selected region: " + region->getName() +
                                    " (" + juce::String(startTime, 3) + "s - " +
                                    juce::String(endTime, 3) + "s)");
        };

        // onRegionDoubleClicked: Zoom to fit the double-clicked region
        regionDisplay.onRegionDoubleClicked = [doc](int regionIndex)
        {
            if (!doc) return;

            // Zoom to the double-clicked region
            doc->getWaveformDisplay().zoomToRegion(regionIndex);

            juce::Logger::writeToLog("Zoomed to region " + juce::String(regionIndex));
        };

        // onRegionRenamed: Update region name with undo support
        regionDisplay.onRegionRenamed = [doc](int regionIndex, const juce::String& newName)
        {
            if (!doc) return;

            Region* region = doc->getRegionManager().getRegion(regionIndex);
            if (!region)
            {
                juce::Logger::writeToLog("Cannot rename region: Invalid region index");
                return;
            }

            juce::String oldName = region->getName();

            // Start a new transaction
            juce::String transactionName = "Rename Region: " + oldName + " → " + newName;
            doc->getUndoManager().beginNewTransaction(transactionName);

            // Create undo action
            auto* undoAction = new RenameRegionUndoAction(
                doc->getRegionManager(),
                doc->getRegionDisplay(),
                doc->getFile(),
                regionIndex,
                oldName,
                newName
            );

            // perform() calls RenameRegionUndoAction::perform() which renames the region
            doc->getUndoManager().perform(undoAction);

            juce::Logger::writeToLog("Renamed region from '" + oldName + "' to '" + newName + "'");
        };

        // onRegionColorChanged: Update region color with undo support
        regionDisplay.onRegionColorChanged = [doc](int regionIndex, const juce::Colour& newColor)
        {
            if (!doc) return;

            Region* region = doc->getRegionManager().getRegion(regionIndex);
            if (!region)
            {
                juce::Logger::writeToLog("Cannot change region color: Invalid region index");
                return;
            }

            juce::Colour oldColor = region->getColor();

            // Start a new transaction
            juce::String transactionName = "Change Region Color";
            doc->getUndoManager().beginNewTransaction(transactionName);

            // Create undo action
            auto* undoAction = new ChangeRegionColorUndoAction(
                doc->getRegionManager(),
                doc->getRegionDisplay(),
                doc->getFile(),
                regionIndex,
                oldColor,
                newColor
            );

            // perform() calls ChangeRegionColorUndoAction::perform() which changes the color
            doc->getUndoManager().perform(undoAction);

            juce::Logger::writeToLog("Changed region color");
        };

        // onRegionDeleted: Remove region from manager with undo support
        regionDisplay.onRegionDeleted = [doc](int regionIndex)
        {
            if (!doc) return;

            Region* region = doc->getRegionManager().getRegion(regionIndex);
            if (!region)
            {
                juce::Logger::writeToLog("Cannot delete region: Invalid region index");
                return;
            }

            juce::String regionName = region->getName();

            // Start a new transaction
            juce::String transactionName = "Delete Region: " + regionName;
            doc->getUndoManager().beginNewTransaction(transactionName);

            // Create undo action
            auto* undoAction = new DeleteRegionUndoAction(
                doc->getRegionManager(),
                doc->getRegionDisplay(),
                doc->getFile(),
                regionIndex
            );

            // perform() calls DeleteRegionUndoAction::perform() which deletes the region
            doc->getUndoManager().perform(undoAction);

            juce::Logger::writeToLog("Deleted region: " + regionName);
        };

        // onRegionResized: Update region boundaries with undo support
        regionDisplay.onRegionResized = [doc](int regionIndex, int64_t oldStart, int64_t oldEnd, int64_t newStart, int64_t newEnd)
        {
            if (!doc) return;

            Region* region = doc->getRegionManager().getRegion(regionIndex);
            if (!region)
            {
                juce::Logger::writeToLog("Cannot resize region: Invalid region index");
                return;
            }

            // Skip if boundaries haven't changed (using parameters, not region)
            if (oldStart == newStart && oldEnd == newEnd)
                return;

            // Start a new transaction
            juce::String transactionName = "Resize Region: " + region->getName();
            doc->getUndoManager().beginNewTransaction(transactionName);

            // Create undo action with original boundaries from parameters
            auto* undoAction = new ResizeRegionUndoAction(
                doc->getRegionManager(),
                doc->getRegionDisplay(),
                doc->getFile(),
                regionIndex,
                oldStart,
                oldEnd,
                newStart,
                newEnd
            );

            // perform() calls ResizeRegionUndoAction::perform() which resizes the region
            doc->getUndoManager().perform(undoAction);

            juce::Logger::writeToLog(juce::String::formatted(
                "Resized region '%s': %lld-%lld → %lld-%lld samples",
                region->getName().toRawUTF8(),
                oldStart, oldEnd, newStart, newEnd));
        };

        // onRegionResizing: Real-time visual feedback during region resize drag
        regionDisplay.onRegionResizing = [doc]()
        {
            if (!doc) return;

            // Repaint WaveformDisplay to update region overlays in real-time
            doc->getWaveformDisplay().repaint();
        };

        // onRegionEditBoundaries: Show dialog to edit region boundaries numerically
        regionDisplay.onRegionEditBoundaries = [this, doc](int regionIndex)
        {
            if (!doc) return;

            Region* region = doc->getRegionManager().getRegion(regionIndex);
            if (!region)
            {
                juce::Logger::writeToLog("Cannot edit region boundaries: Invalid region index");
                return;
            }

            // Get audio context
            double sampleRate = doc->getBufferManager().getSampleRate();
            double fps = 30.0;  // Default FPS (TODO: make this user-configurable in settings)
            int64_t totalSamples = doc->getBufferManager().getNumSamples();
            AudioUnits::TimeFormat currentFormat = m_timeFormat;

            // Show dialog
            EditRegionBoundariesDialog::showDialog(
                this,  // parent component
                *region,
                currentFormat,
                sampleRate,
                fps,
                totalSamples,
                [doc, regionIndex](int64_t newStart, int64_t newEnd)
                {
                    // Callback when user clicks OK
                    Region* region = doc->getRegionManager().getRegion(regionIndex);
                    if (!region) return;

                    // Get old boundaries for undo
                    int64_t oldStart = region->getStartSample();
                    int64_t oldEnd = region->getEndSample();

                    // Skip if boundaries haven't changed
                    if (oldStart == newStart && oldEnd == newEnd)
                    {
                        juce::Logger::writeToLog("No changes to region boundaries");
                        return;
                    }

                    // Start a new transaction
                    juce::String transactionName = "Edit Region Boundaries: " + region->getName();
                    doc->getUndoManager().beginNewTransaction(transactionName);

                    // Create undo action
                    auto* undoAction = new ResizeRegionUndoAction(
                        doc->getRegionManager(),
                        doc->getRegionDisplay(),
                        doc->getFile(),
                        regionIndex,
                        oldStart,
                        oldEnd,
                        newStart,
                        newEnd
                    );

                    // Perform action
                    doc->getUndoManager().perform(undoAction);

                    // Mark document as modified
                    doc->setModified(true);

                    // Repaint to show updated region
                    doc->getRegionDisplay().repaint();
                    doc->getWaveformDisplay().repaint();

                    juce::Logger::writeToLog(juce::String::formatted(
                        "Edited region '%s' boundaries: %lld-%lld → %lld-%lld samples",
                        region->getName().toRawUTF8(),
                        oldStart, oldEnd, newStart, newEnd));
                }
            );
        };
    }

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

    /**
     * Shows a popup menu to select time format.
     * Displays the 4 available formats and updates m_timeFormat when user selects one.
     */
    void showTimeFormatMenu()
    {
        juce::PopupMenu menu;

        // Add all 4 time formats
        menu.addItem(1, "Samples", true, m_timeFormat == AudioUnits::TimeFormat::Samples);
        menu.addItem(2, "Milliseconds", true, m_timeFormat == AudioUnits::TimeFormat::Milliseconds);
        menu.addItem(3, "Seconds", true, m_timeFormat == AudioUnits::TimeFormat::Seconds);
        menu.addItem(4, "Frames", true, m_timeFormat == AudioUnits::TimeFormat::Frames);

        // Show menu at mouse position
        menu.showMenuAsync(juce::PopupMenu::Options(),
            [this](int result)
            {
                if (result > 0)
                {
                    // Update time format based on selection
                    switch (result)
                    {
                        case 1: m_timeFormat = AudioUnits::TimeFormat::Samples; break;
                        case 2: m_timeFormat = AudioUnits::TimeFormat::Milliseconds; break;
                        case 3: m_timeFormat = AudioUnits::TimeFormat::Seconds; break;
                        case 4: m_timeFormat = AudioUnits::TimeFormat::Frames; break;
                    }

                    // Save to settings
                    Settings::getInstance().setSetting("display.timeFormat", static_cast<int>(m_timeFormat));
                    Settings::getInstance().save();

                    // Force UI update
                    repaint();
                }
            });
    }

    /**
     * Auto-save job that runs on background thread.
     * Performs file I/O without blocking the message thread.
     */
    struct AutoSaveJob : public juce::ThreadPoolJob
    {
        juce::AudioBuffer<float> bufferCopy;
        juce::File targetFile;
        juce::File originalFile;
        double sampleRate;
        int bitDepth;

        AutoSaveJob(const juce::AudioBuffer<float>& buffer,
                    const juce::File& target,
                    const juce::File& original,
                    double rate,
                    int depth)
            : juce::ThreadPoolJob("AutoSave"),
              targetFile(target),
              originalFile(original),
              sampleRate(rate),
              bitDepth(depth)
        {
            // Make a copy of the buffer for thread safety
            bufferCopy.makeCopyOf(buffer);
        }

        JobStatus runJob() override
        {
            try
            {
                // Create output stream
                auto outputStream = targetFile.createOutputStream();
                if (!outputStream)
                {
                    logFailure("Could not create output stream");
                    return jobHasFinished;
                }

                // Create WAV writer
                juce::WavAudioFormat wavFormat;
                std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
                    outputStream.get(),
                    sampleRate,
                    bufferCopy.getNumChannels(),
                    bitDepth,
                    {},
                    0));

                if (!writer)
                {
                    logFailure("Could not create audio writer");
                    return jobHasFinished;
                }

                // Write buffer to file
                outputStream.release();  // Writer takes ownership
                bool success = writer->writeFromAudioSampleBuffer(bufferCopy, 0, bufferCopy.getNumSamples());
                writer.reset();  // Flush and close

                if (success)
                {
                    // Log success on message thread
                    juce::MessageManager::callAsync([file = targetFile]()
                    {
                        juce::Logger::writeToLog("Auto-saved: " + file.getFullPathName());
                    });
                }
                else
                {
                    logFailure("Write operation failed");
                }
            }
            catch (const std::exception& e)
            {
                logFailure(juce::String("Exception: ") + e.what());
            }
            catch (...)
            {
                logFailure("Unknown exception");
            }

            return jobHasFinished;
        }

    private:
        void logFailure(const juce::String& reason)
        {
            juce::MessageManager::callAsync([file = originalFile, reason]()
            {
                juce::Logger::writeToLog("Auto-save failed for " + file.getFullPathName() + ": " + reason);
            });
        }
    };

    /**
     * Performs auto-save of modified documents to temp location.
     * Called periodically by timerCallback (every ~60 seconds).
     * Thread-safe: Copies buffers and runs file I/O on background thread.
     */
    void performAutoSave()
    {
        // Check if auto-save is enabled (thread-safe read from Settings singleton)
        bool autoSaveEnabled = Settings::getInstance().getSetting("autoSave.enabled", true);
        if (!autoSaveEnabled)
            return;

        // Get auto-save directory
        juce::File autoSaveDir = Settings::getInstance().getSettingsDirectory().getChildFile("autosave");
        if (!autoSaveDir.exists())
            autoSaveDir.createDirectory();

        // Check each document for auto-save
        for (int i = 0; i < m_documentManager.getNumDocuments(); ++i)
        {
            auto* doc = m_documentManager.getDocument(i);
            if (!doc || !doc->isModified())
                continue;  // Skip unmodified documents

            // Check if document has a file (skip untitled documents for now)
            if (!doc->getAudioEngine().isFileLoaded())
                continue;

            // Create auto-save filename: autosave_[originalname]_[timestamp].wav
            juce::File originalFile = doc->getAudioEngine().getCurrentFile();
            juce::String timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
            juce::String autoSaveFilename = "autosave_" + originalFile.getFileNameWithoutExtension() + "_" + timestamp + ".wav";
            juce::File autoSaveFile = autoSaveDir.getChildFile(autoSaveFilename);

            // Get audio data (on message thread - safe)
            const auto& buffer = doc->getBufferManager().getBuffer();
            double sampleRate = doc->getAudioEngine().getSampleRate();
            int bitDepth = doc->getAudioEngine().getBitDepth();

            // Create auto-save job (makes buffer copy for thread safety)
            auto* job = new AutoSaveJob(buffer, autoSaveFile, originalFile, sampleRate, bitDepth);

            // Add job to thread pool (will run on background thread)
            m_autoSaveThreadPool.addJob(job, true);  // deleteJobWhenFinished = true
        }

        // Clean up old auto-save files (fast operation, OK on message thread)
        cleanupOldAutoSaves(autoSaveDir);
    }

    /**
     * Cleans up old auto-save files, keeping only the most recent 3 per original file.
     */
    void cleanupOldAutoSaves(const juce::File& autoSaveDir)
    {
        if (!autoSaveDir.exists())
            return;

        // Get all auto-save files
        juce::Array<juce::File> autoSaveFiles;
        autoSaveDir.findChildFiles(autoSaveFiles, juce::File::findFiles, false, "autosave_*.wav");

        // Group files by original name
        std::map<juce::String, juce::Array<juce::File>> filesByOriginal;
        for (auto& file : autoSaveFiles)
        {
            // Extract original name from autosave_[originalname]_[timestamp].wav
            juce::String filename = file.getFileNameWithoutExtension();
            juce::StringArray parts = juce::StringArray::fromTokens(filename, "_", "");
            if (parts.size() >= 2)
            {
                juce::String originalName = parts[1];
                filesByOriginal[originalName].add(file);
            }
        }

        // For each original file, keep only the newest 3 auto-saves
        for (auto& pair : filesByOriginal)
        {
            auto& files = pair.second;
            if (files.size() <= 3)
                continue;

            // Sort by modification time (newest first)
            // JUCE Array::sort needs a comparator object, not a lambda
            struct FileComparator
            {
                static int compareElements(const juce::File& first, const juce::File& second)
                {
                    auto timeA = first.getLastModificationTime();
                    auto timeB = second.getLastModificationTime();
                    if (timeA > timeB) return -1;
                    if (timeA < timeB) return 1;
                    return 0;
                }
            };
            FileComparator comp;
            files.sort(comp, false);

            // Delete all but the newest 3
            for (int i = 3; i < files.size(); ++i)
            {
                files[i].deleteFile();
                juce::Logger::writeToLog("Deleted old auto-save: " + files[i].getFullPathName());
            }
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
    juce::AudioDeviceManager m_audioDeviceManager;
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(WaveEditApplication)
