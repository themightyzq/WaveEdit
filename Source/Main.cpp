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
#include "UI/NewFileDialog.h"
#include "UI/NormalizeDialog.h"
#include "UI/FadeInDialog.h"
#include "UI/FadeOutDialog.h"
#include "UI/DCOffsetDialog.h"
#include "UI/ParametricEQDialog.h"
#include "UI/GraphicalEQEditor.h"
#include "UI/RecordingDialog.h"
#include "UI/ErrorDialog.h"
#include "UI/SettingsPanel.h"
#include "UI/FilePropertiesDialog.h"
#include "UI/BWFEditorDialog.h"
#include "UI/iXMLEditorDialog.h"
#include "UI/GoToPositionDialog.h"
#include "UI/EditRegionBoundariesDialog.h"
#include "UI/StripSilenceDialog.h"
#include "UI/BatchExportDialog.h"
#include "UI/SaveAsOptionsPanel.h"
#include "UI/RegionListPanel.h"
#include "UI/MarkerListPanel.h"
#include "UI/SpectrumAnalyzer.h"
#include "UI/TabComponent.h"
#include "UI/KeyboardCheatSheetDialog.h"
#include "UI/PluginManagerDialog.h"
#include "UI/OfflinePluginDialog.h"
#include "UI/PluginChainPanel.h"
#include "UI/PluginChainWindow.h"
#include "UI/PluginEditorWindow.h"
#include "Plugins/PluginManager.h"
#include "Plugins/PluginScannerProtocol.h"
#include "Plugins/PluginScannerWorker.h"
#include "Plugins/PluginPathsPanel.h"
#include "Plugins/PluginScanDialogs.h"
#include "Plugins/PluginChainRenderer.h"
#include "UI/ProgressDialog.h"
#include "Utils/Document.h"
#include "Utils/DocumentManager.h"
#include "Utils/RegionExporter.h"
#include "Utils/KeymapManager.h"
#include "Utils/ToolbarManager.h"
#include "UI/CustomizableToolbar.h"
#include "UI/ToolbarCustomizationDialog.h"

//==============================================================================
// Progress Dialog Threshold
// Operations affecting more than this many samples will show a progress dialog.
// 500,000 samples = ~11 seconds at 44.1kHz, ~10.4 seconds at 48kHz
constexpr int64_t kProgressDialogThreshold = 500000;

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
 * Custom DocumentWindow that supports a close button callback.
 * Used for spectrum analyzer and other floating windows that need to
 * synchronize their visibility state with menu checkmarks.
 *
 * Thread Safety: The close callback is executed asynchronously on the
 * message thread using MessageManager::callAsync() to ensure safe
 * interaction with audio engine and UI state.
 *
 * @param name Window title
 * @param backgroundColour Window background color
 * @param requiredButtons Button configuration (DocumentWindow::allButtons, etc.)
 * @param onCloseCallback Optional callback invoked when close button pressed (moved for efficiency)
 */
class CallbackDocumentWindow : public juce::DocumentWindow
{
public:
    CallbackDocumentWindow(const juce::String& name,
                          juce::Colour backgroundColour,
                          int requiredButtons,
                          std::function<void()> onCloseCallback = nullptr)
        : juce::DocumentWindow(name, backgroundColour, requiredButtons),
          m_onCloseCallback(std::move(onCloseCallback))  // Move instead of copy
    {
    }

    void closeButtonPressed() override
    {
        // Hide the window instead of deleting it
        setVisible(false);

        // Invoke the callback if provided (already on message thread, but ensure safety)
        if (m_onCloseCallback)
        {
            juce::MessageManager::callAsync(m_onCloseCallback);
        }
    }

private:
    std::function<void()> m_onCloseCallback;
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
                      public RegionListPanel::Listener,
                      public MarkerListPanel::Listener
{
public:
    MainComponent(juce::AudioDeviceManager& deviceManager)
        : m_audioDeviceManager(deviceManager),
          m_tabComponent(m_documentManager),
          m_keymapManager(m_commandManager)  // Initialize KeymapManager with command manager reference
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

        #if JUCE_MAC
        // Register menu bar with macOS native menu system
        // CRITICAL: This enables Cmd+, and other macOS system shortcuts to work
        juce::MenuBarModel::setMacMainMenu(this);
        #endif

        // Start timer to update playback position
        startTimer(50); // Update every 50ms for smooth 60fps cursor

        // Clean up recent files on startup
        Settings::getInstance().cleanupRecentFiles();

        // Load UI preferences from settings
        int timeFormatInt = Settings::getInstance().getSetting("display.timeFormat", 2);  // Default to Seconds (2)
        m_timeFormat = static_cast<AudioUnits::TimeFormat>(timeFormatInt);

        // Load keyboard shortcut template (Phase 3)
        // Get active template name from settings (defaults to "Default")
        juce::String activeTemplate = Settings::getInstance().getSetting("keyboard.activeTemplate", "Default").toString();
        if (!m_keymapManager.loadTemplate(activeTemplate))
        {
            // If loading fails, fall back to Default template
            juce::Logger::writeToLog("Failed to load keyboard template '" + activeTemplate + "', falling back to Default");
            m_keymapManager.loadTemplate("Default");
        }

        // Initialize customizable toolbar
        m_toolbar = std::make_unique<CustomizableToolbar>(m_commandManager, m_toolbarManager);
        addAndMakeVisible(*m_toolbar);

        // Update component visibility based on whether we have documents
        updateComponentVisibility();

        // Plugin scan logic:
        // - Plugin scanning is DISABLED at startup for fast, reliable startup
        // - User can manually trigger scan from Plugins menu when ready
        // - Some Apple system AudioUnits hang during scanning, so on-demand scanning is safer
        // - Cached plugins from previous scans are still loaded automatically

        // NOTE: Removed showCrashedPluginWarningIfNeeded() - it caused infinite modal dialog loops
        // when combined with Timer::callAfterDelay. Users can check blacklist from Plugins menu.

        auto cachedPlugins = PluginManager::getInstance().getAvailablePlugins();
        if (cachedPlugins.isEmpty())
        {
            // First run (no cached plugins) - ask user if they want to scan
            DBG("No cached plugins found - showing first-run dialog");

            // Use async dialog to avoid blocking the message thread during startup
            // Declare SafePointer before lambda to ensure MSVC compatibility
            juce::Component::SafePointer<MainComponent> safeThis(this);
            juce::Timer::callAfterDelay(500, [safeThis]()
            {
                if (safeThis == nullptr)
                    return;

                juce::NativeMessageBox::showYesNoBox(
                    juce::MessageBoxIconType::QuestionIcon,
                    "Scan for VST3 Plugins?",
                    "WaveEdit can scan your system for VST3 plugins.\n\n"
                    "This allows you to apply effects to your audio files.\n\n"
                    "Would you like to scan for plugins now?\n\n"
                    "(You can also do this later from Plugins > Rescan Plugins)",
                    nullptr,
                    juce::ModalCallbackFunction::create([safeThis](int result)
                    {
                        if (safeThis != nullptr && result == 1)  // Yes
                        {
                            safeThis->startPluginScan(false);
                        }
                    })
                );
            });
        }
        else
        {
            DBG("Found " + juce::String(cachedPlugins.size()) + " cached plugins");
        }
    }

    // NOTE: showCrashedPluginWarningIfNeeded() was removed entirely.
    // It caused infinite modal dialog loops via Timer::callAfterDelay + NativeMessageBox.
    // Users can check the blacklist from the Plugins menu if needed.

    /**
     * Start background VST3 plugin scanning.
     * Updates status bar with progress.
     * @param forceRescan If true, clears cached plugins and rescans from scratch
     */
    void startPluginScan(bool forceRescan = false)
    {
        auto& pluginManager = PluginManager::getInstance();

        // Skip if already scanning
        if (pluginManager.isScanInProgress())
            return;

        m_pluginScanInProgress = true;
        m_pluginScanProgress = 0.0f;
        m_pluginScanCurrentPlugin = "Initializing...";
        repaint();

        // Use appropriate scan method based on forceRescan flag
        auto progressCallback = [this](float progress, const juce::String& currentPlugin)
        {
            juce::MessageManager::callAsync([this, progress, currentPlugin]()
            {
                m_pluginScanProgress = progress;
                m_pluginScanCurrentPlugin = currentPlugin;
                repaint();  // Refresh status bar
            });
        };

        auto completionCallback = [this](bool success, int numPluginsFound)
        {
            juce::MessageManager::callAsync([this, success, numPluginsFound]()
            {
                m_pluginScanInProgress = false;
                m_pluginScanProgress = 1.0f;

                // Show completion status in status bar briefly
                if (success)
                {
                    m_pluginScanCurrentPlugin = "Complete: " + juce::String(numPluginsFound) + " plugins found";
                    DBG("Plugin scan complete: found " + juce::String(numPluginsFound) + " plugins");
                }
                else
                {
                    m_pluginScanCurrentPlugin = "Scan cancelled or failed";
                    DBG("Plugin scan failed or was cancelled");
                }

                repaint();

                // NOTE: Removed showCrashedPluginWarningIfNeeded() - caused infinite dialog loops

                // Clear the completion message after a few seconds
                // Use SafePointer to prevent use-after-free if component is destroyed
                // Declare SafePointer before lambda to ensure MSVC compatibility
                juce::Component::SafePointer<MainComponent> safeComp(this);
                juce::Timer::callAfterDelay(3000, [safeComp]()
                {
                    if (safeComp != nullptr && !safeComp->m_pluginScanInProgress)
                    {
                        safeComp->m_pluginScanCurrentPlugin = "";
                        safeComp->repaint();
                    }
                });
            });
        };

        if (forceRescan)
        {
            pluginManager.forceRescan(progressCallback, completionCallback);
        }
        else
        {
            pluginManager.startScanAsync(progressCallback, completionCallback);
        }
    }

    ~MainComponent() override
    {
        #if JUCE_MAC
        // Unregister menu bar from macOS native menu system
        juce::MenuBarModel::setMacMainMenu(nullptr);
        #endif

        // Clean up spectrum analyzer window if open
        if (m_spectrumAnalyzerWindow)
        {
            // Disconnect from any audio engine
            for (int i = 0; i < m_documentManager.getNumDocuments(); ++i)
            {
                if (auto* doc = m_documentManager.getDocument(i))
                {
                    doc->getAudioEngine().setSpectrumAnalyzer(nullptr);
                }
            }

            delete m_spectrumAnalyzerWindow;  // Window owns the analyzer, will delete it too
            m_spectrumAnalyzerWindow = nullptr;
            m_spectrumAnalyzer = nullptr;  // Window deleted the analyzer, just null our pointer
        }

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

        // Update spectrum analyzer connection if it's open
        if (m_spectrumAnalyzer && m_spectrumAnalyzerWindow && m_spectrumAnalyzerWindow->isVisible())
        {
            // Disconnect from old document
            if (m_previousDocument && m_previousDocument != newDocument)
            {
                m_previousDocument->getAudioEngine().setSpectrumAnalyzer(nullptr);
            }

            // Connect to new document
            if (newDocument)
            {
                newDocument->getAudioEngine().setSpectrumAnalyzer(m_spectrumAnalyzer);
            }
        }

        // Update tracking
        m_previousDocument = newDocument;

        // Update customizable toolbar with new document context
        if (m_toolbar)
        {
            m_toolbar->setDocument(newDocument);
        }

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

        // Wire up marker callbacks for this document (Phase 3.4)
        setupMarkerCallbacks(document);

        // Update tab component to show new tab
        updateComponentVisibility();
    }

    void documentRemoved(Document* document, int index) override
    {
        // Close any Plugin Chain windows for this document before it's destroyed
        if (document != nullptr)
        {
            auto* chain = &document->getAudioEngine().getPluginChain();
            for (int i = m_pluginChainListeners.size() - 1; i >= 0; --i)
            {
                if (m_pluginChainListeners[i] != nullptr && m_pluginChainListeners[i]->isForChain(chain))
                {
                    m_pluginChainListeners[i]->documentClosed();
                    m_pluginChainListeners.remove(i);
                }
            }
        }

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
    // MarkerListPanel::Listener methods

    void markerListPanelJumpToMarker(int markerIndex) override
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            const auto* marker = doc->getMarkerManager().getMarker(markerIndex);
            if (marker)
            {
                // Set playback position to marker
                doc->getAudioEngine().setPosition(marker->getPosition());

                // Select the marker
                doc->getMarkerManager().setSelectedMarkerIndex(markerIndex);

                // Repaint to show visual feedback
                doc->getMarkerDisplay().repaint();
                repaint();
            }
        }
    }

    void markerListPanelMarkerDeleted(int markerIndex) override
    {
        // Marker was deleted from panel - refresh main display
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            doc->getMarkerDisplay().repaint();
        }
    }

    void markerListPanelMarkerRenamed(int markerIndex, const juce::String& newName) override
    {
        // Marker was renamed - update display
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            doc->setModified(true);
            doc->getMarkerDisplay().repaint();
        }
    }

    void markerListPanelMarkerSelected(int markerIndex) override
    {
        // Marker was selected in panel - sync with marker display
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            doc->getMarkerManager().setSelectedMarkerIndex(markerIndex);
            doc->getMarkerDisplay().repaint();
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
            m_currentDocumentContainer->addAndMakeVisible(doc->getMarkerDisplay());  // Phase 3.4
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

        // Plugin scan progress indicator (displayed on the right side when scanning)
        if (m_pluginScanInProgress)
        {
            auto scanSection = statusBar.removeFromRight(300);

            // Progress bar background
            auto progressBarBounds = scanSection.reduced(8, 6);
            g.setColour(juce::Colour(0xff1a1a1a));
            g.fillRoundedRectangle(progressBarBounds.toFloat(), 3.0f);

            // Progress bar fill
            auto fillBounds = progressBarBounds;
            fillBounds.setWidth(static_cast<int>(progressBarBounds.getWidth() * m_pluginScanProgress));
            g.setColour(juce::Colour(0xff4a9eff));  // Blue progress color
            g.fillRoundedRectangle(fillBounds.toFloat(), 3.0f);

            // Progress bar border
            g.setColour(juce::Colour(0xff4a4a4a));
            g.drawRoundedRectangle(progressBarBounds.toFloat(), 3.0f, 1.0f);

            // Scan text overlay
            g.setColour(juce::Colours::white);
            g.setFont(10.0f);
            juce::String scanText = "Scanning: " + m_pluginScanCurrentPlugin;
            if (scanText.length() > 40)
                scanText = scanText.substring(0, 37) + "...";
            g.drawText(scanText, progressBarBounds.reduced(4, 0), juce::Justification::centred, true);
        }
        else if (m_pluginScanCurrentPlugin.isNotEmpty())
        {
            // Show completion message briefly after scan finishes
            auto scanSection = statusBar.removeFromRight(250);
            g.setColour(juce::Colours::lightgreen);
            g.setFont(11.0f);
            g.drawText(m_pluginScanCurrentPlugin, scanSection.reduced(8, 0), juce::Justification::centredRight, true);
        }

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

            // Customizable toolbar below tabs (36px height, replaces 80px transport)
            // Note: m_toolbar may be null during initial construction when setSize triggers resized()
            if (m_toolbar)
                m_toolbar->setBounds(bounds.removeFromTop(m_toolbar->getPreferredHeight()));

            // Current document container takes remaining space
            m_currentDocumentContainer->setBounds(bounds);

            // Layout document components within container
            if (auto* doc = getCurrentDocument())
            {
                auto containerBounds = m_currentDocumentContainer->getLocalBounds();

                // NOTE: Transport controls are now in the toolbar, not here
                // The old 80px TransportControls is still part of Document but not displayed
                // We hide it to avoid confusion (transport is now in toolbar)
                doc->getTransportControls().setVisible(false);

                // Region display at top of container (32px height)
                doc->getRegionDisplay().setBounds(containerBounds.removeFromTop(32));

                // Marker display below regions (32px height) - Phase 3.4
                doc->getMarkerDisplay().setBounds(containerBounds.removeFromTop(32));

                // Waveform display takes remaining space
                // Height gained: 80px (old transport) - 36px (new toolbar) = 44px more for waveform!
                doc->getWaveformDisplay().setBounds(containerBounds);
            }
        }
        else
        {
            // Hide toolbar when no document is open
            // Note: m_toolbar may be null during initial construction
            if (m_toolbar)
                m_toolbar->setBounds(0, 0, 0, 0);

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
            // CRITICAL: getCurrentPosition() already returns FILE coordinates
            // For OFFLINE_BUFFER mode, it adds the preview offset internally
            // For REALTIME_DSP mode, it returns the file position directly
            // No additional offset needed here!
            double position = doc->getAudioEngine().getCurrentPosition();

            doc->getWaveformDisplay().setPlaybackPosition(position);
            repaint(); // Update status bar

            // Future: Update level meters when integrated into Document class
        }
        else
        {
            // Future: Reset meters when integrated into Document class
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
            CommandIDs::fileNew,
            CommandIDs::fileOpen,
            CommandIDs::fileSave,
            CommandIDs::fileSaveAs,
            CommandIDs::fileClose,
            CommandIDs::fileProperties,  // Alt+Enter - Show file properties
            CommandIDs::fileEditBWFMetadata, // Edit BWF metadata
            CommandIDs::fileEditiXMLMetadata, // Edit iXML/SoundMiner metadata
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
            CommandIDs::playbackLoopRegion,  // Cmd+Shift+L - Loop selected region
            CommandIDs::playbackRecord,      // Cmd+R - Start recording
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
            CommandIDs::viewToggleRegions,    // Cmd+Shift+H - Toggle region visibility
            CommandIDs::viewSpectrumAnalyzer, // Cmd+Alt+S - Show/hide Spectrum Analyzer
            // Spectrum Analyzer configuration commands
            CommandIDs::viewSpectrumFFTSize512,
            CommandIDs::viewSpectrumFFTSize1024,
            CommandIDs::viewSpectrumFFTSize2048,
            CommandIDs::viewSpectrumFFTSize4096,
            CommandIDs::viewSpectrumFFTSize8192,
            CommandIDs::viewSpectrumWindowHann,
            CommandIDs::viewSpectrumWindowHamming,
            CommandIDs::viewSpectrumWindowBlackman,
            CommandIDs::viewSpectrumWindowRectangular,
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
            CommandIDs::processParametricEQ,
            CommandIDs::processGraphicalEQ,
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
            CommandIDs::regionPaste,
            // Marker commands (Phase 3.4)
            CommandIDs::markerAdd,
            CommandIDs::markerDelete,
            CommandIDs::markerNext,
            CommandIDs::markerPrevious,
            CommandIDs::markerShowList,
            // Plugin commands (VST3/AU plugin chain)
            CommandIDs::pluginShowChain,
            CommandIDs::pluginApplyChain,
            CommandIDs::pluginOffline,
            CommandIDs::pluginBypassAll,
            CommandIDs::pluginRescan,
            CommandIDs::pluginShowSettings,
            CommandIDs::pluginClearCache,
            // Help commands
            CommandIDs::helpAbout,
            CommandIDs::helpShortcuts,
            // Toolbar commands
            CommandIDs::toolbarCustomize,
            CommandIDs::toolbarReset
        };

        commands.addArray(ids, juce::numElementsInArray(ids));
    }

    void getCommandInfo(juce::CommandID commandID, juce::ApplicationCommandInfo& result) override
    {
        auto* doc = getCurrentDocument();

        // Get keyboard shortcut from active template (Phase 3)
        auto keyPress = m_keymapManager.getKeyPress(commandID);

        // CRITICAL FIX: Always set command info (text, shortcuts) so menus and shortcuts work
        // Use setActive() to disable document-dependent commands when no document exists

        switch (commandID)
        {
            case CommandIDs::fileNew:
                result.setInfo("New...", "Create a new audio file", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                // Always available (no document required)
                break;

            case CommandIDs::fileOpen:
                result.setInfo("Open...", "Open an audio file", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                // Always available (no document required)
                break;

            case CommandIDs::fileSave:
                result.setInfo("Save", "Save the current file", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->isModified());
                break;

            case CommandIDs::fileSaveAs:
                result.setInfo("Save As...", "Save the current file with a new name", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::fileClose:
                result.setInfo("Close", "Close the current file", "File", 0);
                // No keyboard shortcut - Cmd+W handled by tabClose for consistency
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::fileProperties:
                result.setInfo("Properties...", "Show file properties", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::fileEditBWFMetadata:
                result.setInfo("Edit BWF Metadata...", "Edit broadcast wave format metadata", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::fileEditiXMLMetadata:
                result.setInfo("Edit iXML Metadata...", "Edit SoundMiner/iXML metadata", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::fileExit:
                result.setInfo("Exit", "Exit the application", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                // Always available (no document required)
                break;

            case CommandIDs::filePreferences:
                result.setInfo("Preferences...", "Open preferences dialog", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                // Always available (no document required)
                break;

            // Tab operations
            case CommandIDs::tabClose:
                result.setInfo("Close Tab", "Close current tab", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(hasCurrentDocument());
                break;

            case CommandIDs::tabCloseAll:
                result.setInfo("Close All Tabs", "Close all open tabs", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(hasCurrentDocument());
                break;

            case CommandIDs::tabNext:
                result.setInfo("Next Tab", "Switch to next tab", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Ctrl+Tab (moved from Cmd+Tab to avoid macOS App Switcher conflict)
                result.setActive(m_documentManager.getNumDocuments() > 1);
                break;

            case CommandIDs::tabPrevious:
                result.setInfo("Previous Tab", "Switch to previous tab", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Ctrl+Shift+Tab (moved from Cmd+Shift+Tab to avoid macOS App Switcher conflict)
                result.setActive(m_documentManager.getNumDocuments() > 1);
                break;

            case CommandIDs::tabSelect1:
                result.setInfo("Jump to Tab 1", "Switch to tab 1", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(m_documentManager.getNumDocuments() >= 1);
                break;

            case CommandIDs::tabSelect2:
                result.setInfo("Jump to Tab 2", "Switch to tab 2", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(m_documentManager.getNumDocuments() >= 2);
                break;

            case CommandIDs::tabSelect3:
                result.setInfo("Jump to Tab 3", "Switch to tab 3", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(m_documentManager.getNumDocuments() >= 3);
                break;

            case CommandIDs::tabSelect4:
                result.setInfo("Jump to Tab 4", "Switch to tab 4", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(m_documentManager.getNumDocuments() >= 4);
                break;

            case CommandIDs::tabSelect5:
                result.setInfo("Jump to Tab 5", "Switch to tab 5", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(m_documentManager.getNumDocuments() >= 5);
                break;

            case CommandIDs::tabSelect6:
                result.setInfo("Jump to Tab 6", "Switch to tab 6", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(m_documentManager.getNumDocuments() >= 6);
                break;

            case CommandIDs::tabSelect7:
                result.setInfo("Jump to Tab 7", "Switch to tab 7", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(m_documentManager.getNumDocuments() >= 7);
                break;

            case CommandIDs::tabSelect8:
                result.setInfo("Jump to Tab 8", "Switch to tab 8", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(m_documentManager.getNumDocuments() >= 8);
                break;

            case CommandIDs::tabSelect9:
                result.setInfo("Jump to Tab 9", "Switch to tab 9", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(m_documentManager.getNumDocuments() >= 9);
                break;

            case CommandIDs::editUndo:
                result.setInfo("Undo", "Undo the last operation", "Edit", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getUndoManager().canUndo());
                break;

            case CommandIDs::editRedo:
                result.setInfo("Redo", "Redo the last undone operation", "Edit", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getUndoManager().canRedo());
                break;

            case CommandIDs::editSelectAll:
                result.setInfo("Select All", "Select all audio", "Edit", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::editCut:
                result.setInfo("Cut", "Cut selection to clipboard", "Edit", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::editCopy:
                result.setInfo("Copy", "Copy selection to clipboard", "Edit", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::editPaste:
                result.setInfo("Paste", "Paste from clipboard", "Edit", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && AudioClipboard::getInstance().hasAudio());
                break;

            case CommandIDs::editDelete:
                result.setInfo("Delete", "Delete selection", "Edit", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::editSilence:
                result.setInfo("Silence", "Fill selection with silence", "Edit", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::editTrim:
                result.setInfo("Trim", "Delete everything outside selection", "Edit", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::playbackPlay:
                result.setInfo("Play/Stop", "Play or stop playback from cursor", "Playback", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Shift+Space (alternate)
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::playbackPause:
                result.setInfo("Pause", "Pause or resume playback", "Playback", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::playbackStop:
                result.setInfo("Stop", "Stop playback", "Playback", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Escape = explicit stop (industry standard)
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::playbackLoop:
                result.setInfo("Loop", "Toggle loop mode", "Playback", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::playbackLoopRegion:
                result.setInfo("Loop Region", "Loop the selected region", "Playback", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+Shift+L
                result.setActive(doc && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::playbackRecord:
                result.setInfo("Record", "Record audio from input device", "Playback", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+R
                result.setActive(true);  // Recording is always available
                break;

            // View/Zoom commands
            case CommandIDs::viewZoomIn:
                result.setInfo("Zoom In", "Zoom in 2x", "View", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::viewZoomOut:
                result.setInfo("Zoom Out", "Zoom out 2x", "View", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::viewZoomFit:
                result.setInfo("Zoom to Fit", "Fit entire waveform to view", "View", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::viewZoomSelection:
                result.setInfo("Zoom to Selection", "Zoom to current selection", "View", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::viewZoomOneToOne:
                result.setInfo("Zoom 1:1", "Zoom to 1:1 sample resolution", "View", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+0 (not Cmd+1, which is used for tab switching)
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::viewCycleTimeFormat:
                result.setInfo("Cycle Time Format", "Cycle through time display formats (Samples/Ms/Sec/Frames)", "View", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+Shift+T (moved from Shift+G to avoid conflict with processGain)
                result.setActive(true);  // Always active
                break;

            case CommandIDs::viewAutoScroll:
                result.setInfo("Follow Playback", "Auto-scroll to follow playback cursor", "View", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+Shift+F
                result.setTicked(doc && doc->getWaveformDisplay().isFollowPlayback());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::viewZoomToRegion:
                result.setInfo("Zoom to Region", "Zoom to fit selected region with margins", "View", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+Option+Z
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getRegionManager().getSelectedRegionIndex() >= 0);
                break;

            case CommandIDs::viewAutoPreviewRegions:
                result.setInfo("Auto-Preview Regions", "Automatically play regions when clicked", "View", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+Alt+P
                result.setTicked(Settings::getInstance().getAutoPreviewRegions());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getRegionManager().getNumRegions() > 0);
                break;

            case CommandIDs::viewToggleRegions:
                result.setInfo("Show/Hide Regions", "Toggle region visibility in waveform display", "View", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+Shift+H
                result.setTicked(Settings::getInstance().getRegionsVisible());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::viewSpectrumAnalyzer:
                result.setInfo("Spectrum Analyzer", "Show/hide real-time spectrum analyzer", "View", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+Alt+S
                result.setTicked(m_spectrumAnalyzerWindow && m_spectrumAnalyzerWindow->isVisible());
                result.setActive(true);  // Always available
                break;

            // Spectrum Analyzer FFT Size Commands
            case CommandIDs::viewSpectrumFFTSize512:
                result.setInfo("FFT Size: 512", "Set FFT size to 512 samples (faster, lower resolution)", "View", 0);
                result.setTicked(m_spectrumAnalyzer && m_spectrumAnalyzer->getFFTSize() == SpectrumAnalyzer::FFTSize::SIZE_512);
                result.setActive(m_spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumFFTSize1024:
                result.setInfo("FFT Size: 1024", "Set FFT size to 1024 samples", "View", 0);
                result.setTicked(m_spectrumAnalyzer && m_spectrumAnalyzer->getFFTSize() == SpectrumAnalyzer::FFTSize::SIZE_1024);
                result.setActive(m_spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumFFTSize2048:
                result.setInfo("FFT Size: 2048", "Set FFT size to 2048 samples (default, balanced)", "View", 0);
                result.setTicked(m_spectrumAnalyzer && m_spectrumAnalyzer->getFFTSize() == SpectrumAnalyzer::FFTSize::SIZE_2048);
                result.setActive(m_spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumFFTSize4096:
                result.setInfo("FFT Size: 4096", "Set FFT size to 4096 samples (higher resolution)", "View", 0);
                result.setTicked(m_spectrumAnalyzer && m_spectrumAnalyzer->getFFTSize() == SpectrumAnalyzer::FFTSize::SIZE_4096);
                result.setActive(m_spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumFFTSize8192:
                result.setInfo("FFT Size: 8192", "Set FFT size to 8192 samples (highest resolution, slower)", "View", 0);
                result.setTicked(m_spectrumAnalyzer && m_spectrumAnalyzer->getFFTSize() == SpectrumAnalyzer::FFTSize::SIZE_8192);
                result.setActive(m_spectrumAnalyzer != nullptr);
                break;

            // Spectrum Analyzer Window Function Commands
            case CommandIDs::viewSpectrumWindowHann:
                result.setInfo("Window: Hann", "Use Hann window function (default, good general purpose)", "View", 0);
                result.setTicked(m_spectrumAnalyzer && m_spectrumAnalyzer->getWindowFunction() == SpectrumAnalyzer::WindowFunction::HANN);
                result.setActive(m_spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumWindowHamming:
                result.setInfo("Window: Hamming", "Use Hamming window function (slightly narrower main lobe)", "View", 0);
                result.setTicked(m_spectrumAnalyzer && m_spectrumAnalyzer->getWindowFunction() == SpectrumAnalyzer::WindowFunction::HAMMING);
                result.setActive(m_spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumWindowBlackman:
                result.setInfo("Window: Blackman", "Use Blackman window function (better sidelobe suppression)", "View", 0);
                result.setTicked(m_spectrumAnalyzer && m_spectrumAnalyzer->getWindowFunction() == SpectrumAnalyzer::WindowFunction::BLACKMAN);
                result.setActive(m_spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumWindowRectangular:
                result.setInfo("Window: Rectangular", "Use rectangular window (no windowing, best frequency resolution)", "View", 0);
                result.setTicked(m_spectrumAnalyzer && m_spectrumAnalyzer->getWindowFunction() == SpectrumAnalyzer::WindowFunction::RECTANGULAR);
                result.setActive(m_spectrumAnalyzer != nullptr);
                break;

            // Navigation commands
            case CommandIDs::navigateLeft:
                result.setInfo("Navigate Left", "Move cursor left by snap increment", "Navigation", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigateRight:
                result.setInfo("Navigate Right", "Move cursor right by snap increment", "Navigation", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigateStart:
                result.setInfo("Jump to Start", "Jump to start of file", "Navigation", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigateEnd:
                result.setInfo("Jump to End", "Jump to end of file", "Navigation", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigatePageLeft:
                result.setInfo("Page Left", "Move cursor left by page increment", "Navigation", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigatePageRight:
                result.setInfo("Page Right", "Move cursor right by page increment", "Navigation", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigateHomeVisible:
                result.setInfo("Jump to Visible Start", "Jump to first visible sample", "Navigation", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigateEndVisible:
                result.setInfo("Jump to Visible End", "Jump to last visible sample", "Navigation", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigateCenterView:
                result.setInfo("Center View", "Center view on cursor", "Navigation", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::navigateGoToPosition:
                result.setInfo("Go To Position...", "Jump to exact position", "Navigation", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            // Selection extension commands
            case CommandIDs::selectExtendLeft:
                result.setInfo("Extend Selection Left", "Extend selection left by increment", "Selection", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::selectExtendRight:
                result.setInfo("Extend Selection Right", "Extend selection right by increment", "Selection", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::selectExtendStart:
                result.setInfo("Extend to Visible Start", "Extend selection to visible start", "Selection", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::selectExtendEnd:
                result.setInfo("Extend to Visible End", "Extend selection to visible end", "Selection", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::selectExtendPageLeft:
                result.setInfo("Extend Selection Page Left", "Extend selection left by page increment", "Selection", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::selectExtendPageRight:
                result.setInfo("Extend Selection Page Right", "Extend selection right by page increment", "Selection", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            // Snap commands
            case CommandIDs::snapCycleMode:
                result.setInfo("Toggle Snap", "Toggle snap on/off (maintains last increment)", "Snap", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::snapToggleZeroCrossing:
                result.setInfo("Toggle Zero Crossing Snap", "Quick toggle zero crossing snap", "Snap", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            // Processing operations
            case CommandIDs::processGain:
                result.setInfo("Gain...", "Apply precise gain adjustment", "Process", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Shift+G (viewCycleTimeFormat moved to Cmd+Shift+T to resolve conflict)
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());  // Works with or without selection
                break;

            case CommandIDs::processIncreaseGain:
                result.setInfo("Increase Gain", "Increase gain by 1 dB", "Process", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::processDecreaseGain:
                result.setInfo("Decrease Gain", "Decrease gain by 1 dB", "Process", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::processParametricEQ:
                result.setInfo("Parametric EQ...", "3-band parametric EQ", "Process", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::processGraphicalEQ:
                result.setInfo("Graphical EQ...", "Graphical 3-band parametric EQ editor", "Process", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::processNormalize:
                result.setInfo("Normalize...", "Normalize audio to peak level", "Process", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::processFadeIn:
                result.setInfo("Fade In", "Apply linear fade in to selection", "Process", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Changed from Cmd+Shift+I to Cmd+F (no conflict)
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::processFadeOut:
                result.setInfo("Fade Out", "Apply linear fade out to selection", "Process", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::processDCOffset:
                result.setInfo("Remove DC Offset", "Remove DC offset from entire file", "Process", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            // Region commands (Phase 3 Tier 2)
            case CommandIDs::regionAdd:
                result.setInfo("Add Region", "Create region from current selection", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Just 'R' key (no modifiers)
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getWaveformDisplay().hasSelection());
                break;

            case CommandIDs::regionDelete:
                result.setInfo("Delete Region", "Delete selected region", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::regionNext:
                result.setInfo("Next Region", "Jump to next region", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Just ']' key (no modifiers)
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::regionPrevious:
                result.setInfo("Previous Region", "Jump to previous region", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Just '[' key (no modifiers)
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::regionSelectInverse:
                result.setInfo("Select Inverse of Regions", "Select everything NOT in regions", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::regionSelectAll:
                result.setInfo("Select All Regions", "Select union of all regions", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::regionStripSilence:
                result.setInfo("Auto Region", "Auto-create regions from non-silent sections", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::regionExportAll:
                result.setInfo("Export Regions As Files", "Export each region as a separate audio file", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() && doc->getRegionManager().getNumRegions() > 0);
                break;

            case CommandIDs::regionBatchRename:
                result.setInfo("Batch Rename Regions", "Rename multiple selected regions at once", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+Shift+B (moved from Cmd+Shift+N to avoid conflict with processNormalize)
                result.setActive(doc && doc->getRegionManager().getNumRegions() >= 2);
                break;

            // Region editing commands (Phase 3.4)
            case CommandIDs::regionMerge:
                result.setInfo("Merge Regions", "Merge selected regions", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+J (J = Join, Pro Tools standard)
                result.setActive(doc && canMergeRegions(doc));
                break;

            case CommandIDs::regionSplit:
                result.setInfo("Split Region at Cursor", "Split region at cursor position", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+K (K = "kut/split", moved from Cmd+R to avoid conflict with playbackRecord)
                result.setActive(doc && canSplitRegion(doc));
                break;

            case CommandIDs::regionCopy:
                result.setInfo("Copy Region", "Copy selected region definition to clipboard", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getRegionManager().getSelectedRegionIndex() >= 0);
                break;

            case CommandIDs::regionPaste:
                result.setInfo("Paste Regions at Cursor", "Paste regions at cursor position", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && m_hasRegionClipboard);
                break;

            case CommandIDs::regionShowList:
                result.setInfo("Show Region List", "Display list of all regions", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
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
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getRegionManager().getSelectedRegionIndex() >= 0);
                break;

            case CommandIDs::regionNudgeStartRight:
                result.setInfo("Nudge Region Start Right", "Move region start boundary right by snap increment", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getRegionManager().getSelectedRegionIndex() >= 0);
                break;

            case CommandIDs::regionNudgeEndLeft:
                result.setInfo("Nudge Region End Left", "Move region end boundary left by snap increment", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getRegionManager().getSelectedRegionIndex() >= 0);
                break;

            case CommandIDs::regionNudgeEndRight:
                result.setInfo("Nudge Region End Right", "Move region end boundary right by snap increment", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getRegionManager().getSelectedRegionIndex() >= 0);
                break;

            // Marker commands (Phase 3.4)
            case CommandIDs::markerAdd:
                result.setInfo("Add Marker", "Add marker at cursor position", "Marker", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Just 'M' key
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::markerDelete:
                result.setInfo("Delete Marker", "Delete selected marker", "Marker", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getMarkerManager().getSelectedMarkerIndex() >= 0);
                break;

            case CommandIDs::markerNext:
                result.setInfo("Next Marker", "Jump to next marker", "Marker", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getMarkerManager().getNumMarkers() > 0);
                break;

            case CommandIDs::markerPrevious:
                result.setInfo("Previous Marker", "Jump to previous marker", "Marker", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getMarkerManager().getNumMarkers() > 0);
                break;

            case CommandIDs::markerShowList:
                result.setInfo("Show Marker List", "Show/hide marker list panel", "Marker", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+Shift+K (moved from Cmd+M to avoid macOS Minimize Window conflict)
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            // Help commands
            case CommandIDs::helpAbout:
                result.setInfo("About WaveEdit", "Show application information", "Help", 0);
                // No keyboard shortcut (standard macOS About accessed via menu)
                result.setActive(true);
                break;

            case CommandIDs::helpShortcuts:
                result.setInfo("Keyboard Shortcuts", "Show keyboard shortcut reference", "Help", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+/ = help (common standard)
                result.setActive(true);
                break;

            //==============================================================================
            // Plugin commands (VST3/AU plugin chain)
            case CommandIDs::pluginShowChain:
                result.setInfo("Show Plugin Chain", "Show the plugin chain panel", "Plugins", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::pluginApplyChain:
                result.setInfo("Apply Plugin Chain", "Apply plugin chain to selection (offline)", "Plugins", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                !doc->getAudioEngine().getPluginChain().isEmpty());
                break;

            case CommandIDs::pluginOffline:
                result.setInfo("Offline Plugin...", "Apply a single plugin to selection", "Plugins", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                else
                    result.addDefaultKeypress('o', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                !PluginManager::getInstance().getAvailablePlugins().isEmpty());
                break;

            case CommandIDs::pluginBypassAll:
                result.setInfo("Bypass All Plugins", "Bypass/enable all plugins in chain", "Plugins", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && !doc->getAudioEngine().getPluginChain().isEmpty());
                result.setTicked(doc && doc->getAudioEngine().getPluginChain().areAllBypassed());
                break;

            case CommandIDs::pluginRescan:
                result.setInfo("Rescan Plugins", "Rescan for VST3/AU plugins", "Plugins", 0);
                result.setActive(!PluginManager::getInstance().isScanInProgress());
                break;

            case CommandIDs::pluginShowSettings:
                result.setInfo("Plugin Search Paths...", "Configure VST3 plugin search directories", "Plugins", 0);
                break;

            case CommandIDs::pluginClearCache:
                result.setInfo("Clear Cache & Rescan", "Delete plugin cache and perform full rescan", "Plugins", 0);
                result.setActive(!PluginManager::getInstance().isScanInProgress());
                break;

            //==============================================================================
            // Toolbar commands
            case CommandIDs::toolbarCustomize:
                result.setInfo("Customize Toolbar...", "Customize toolbar layout and buttons", "View", 0);
                result.setActive(true);  // Always available
                break;

            case CommandIDs::toolbarReset:
                result.setInfo("Reset Toolbar", "Reset toolbar to default layout", "View", 0);
                result.setActive(true);  // Always available
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
            case CommandIDs::fileNew:
            {
                auto settings = NewFileDialog::showDialog();
                if (settings.has_value())
                {
                    // Create new document
                    auto* newDoc = m_documentManager.createDocument();
                    if (newDoc != nullptr)
                    {
                        // Calculate number of samples
                        int64_t numSamples = static_cast<int64_t>(settings->durationSeconds * settings->sampleRate);

                        // Create empty audio buffer with silence
                        juce::AudioBuffer<float> emptyBuffer(settings->numChannels, static_cast<int>(numSamples));
                        emptyBuffer.clear();

                        // Load into buffer manager
                        auto& buffer = newDoc->getBufferManager().getMutableBuffer();
                        buffer.setSize(settings->numChannels, static_cast<int>(numSamples));
                        buffer.clear();

                        // Load into audio engine
                        newDoc->getAudioEngine().loadFromBuffer(emptyBuffer, settings->sampleRate, settings->numChannels);

                        // Load waveform display
                        newDoc->getWaveformDisplay().reloadFromBuffer(emptyBuffer, settings->sampleRate, false, false);

                        // Setup region display
                        newDoc->getRegionDisplay().setSampleRate(settings->sampleRate);
                        newDoc->getRegionDisplay().setTotalDuration(settings->durationSeconds);
                        newDoc->getRegionDisplay().setVisibleRange(0.0, settings->durationSeconds);
                        newDoc->getRegionDisplay().setAudioBuffer(&buffer);

                        // Setup marker display
                        newDoc->getMarkerDisplay().setSampleRate(settings->sampleRate);
                        newDoc->getMarkerDisplay().setTotalDuration(settings->durationSeconds);

                        // Set document as modified (new file needs to be saved)
                        newDoc->setModified(true);

                        // Switch to the new document
                        m_documentManager.setCurrentDocument(newDoc);

                        juce::Logger::writeToLog("Created new audio file: " +
                                                juce::String(numSamples) + " samples, " +
                                                juce::String(settings->sampleRate) + " Hz, " +
                                                juce::String(settings->numChannels) + " channels");
                    }
                }
                return true;
            }

            case CommandIDs::fileOpen:
                openFile();
                return true;

            case CommandIDs::fileExit:
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
                return true;

            case CommandIDs::filePreferences:
                SettingsPanel::showDialog(this, m_audioDeviceManager, m_commandManager, m_keymapManager);
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

            case CommandIDs::fileEditBWFMetadata:
                if (!doc) return false;
                BWFEditorDialog::showDialog(this, doc->getBWFMetadata(), [this, doc]()
                {
                    // Mark document as modified when BWF metadata changes
                    doc->setModified(true);
                    juce::Logger::writeToLog("BWF metadata updated - document marked as modified");
                });
                return true;

            case CommandIDs::fileEditiXMLMetadata:
                if (!doc) return false;
                iXMLEditorDialog::showDialog(this, doc->getiXMLMetadata(),
                                            doc->getFilename(), [this, doc]()
                {
                    // Mark document as modified when iXML metadata changes
                    doc->setModified(true);
                    juce::Logger::writeToLog("iXML metadata updated - document marked as modified");
                });
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

                    // Refresh MarkerListPanel if open
                    if (m_markerListPanel)
                        m_markerListPanel->refresh();

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

                    // Refresh MarkerListPanel if open
                    if (m_markerListPanel)
                        m_markerListPanel->refresh();

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

            case CommandIDs::playbackLoopRegion:
            {
                if (!doc) return false;

                // Get selected region
                auto& regionMgr = doc->getRegionManager();
                int selectedIndex = regionMgr.getSelectedRegionIndex();

                if (selectedIndex < 0 || selectedIndex >= regionMgr.getNumRegions())
                {
                    juce::Logger::writeToLog("No region selected for loop playback");
                    return false;
                }

                auto* region = regionMgr.getRegion(selectedIndex);
                if (!region)
                {
                    juce::Logger::writeToLog("Invalid region for loop playback");
                    return false;
                }

                // Set selection to region boundaries (current loop implementation uses selection)
                auto& audioEngine = doc->getAudioEngine();
                double sampleRate = audioEngine.getSampleRate();

                double startTime = region->getStartSample() / sampleRate;
                double endTime = region->getEndSample() / sampleRate;

                // Set selection to match region boundaries for loop playback
                doc->getWaveformDisplay().setSelection(region->getStartSample(), region->getEndSample());

                // Enable loop mode
                audioEngine.setLooping(true);

                juce::Logger::writeToLog("Loop region: " + region->getName() +
                                         " (" + juce::String(startTime, 3) + "s - " +
                                         juce::String(endTime, 3) + "s)");

                // Start playback from region start
                audioEngine.setPosition(startTime);
                audioEngine.play();

                return true;
            }

            case CommandIDs::playbackRecord:
            {
                // Check if a file is currently open - if so, ask user where to put the recording
                auto* currentDoc = m_documentManager.getCurrentDocument();
                bool appendToExisting = false;

                if (currentDoc != nullptr)
                {
                    // Show choice dialog: Insert at cursor or create new file
                    int choice = juce::AlertWindow::showYesNoCancelBox(
                        juce::AlertWindow::QuestionIcon,
                        "Recording Destination",
                        "A file is currently open. Where would you like to place the recording?\n\n"
                        "• YES: Insert at cursor position (punch-in)\n"
                        "• NO: Create new file with recording\n"
                        "• CANCEL: Don't record",
                        "Insert at Cursor",
                        "Create New File",
                        "Cancel"
                    );

                    if (choice == 0)  // Cancel
                    {
                        return true;  // User canceled
                    }

                    appendToExisting = (choice == 1);  // 1 = Yes (insert at cursor), 2 = No (new file)
                }

                // Open recording dialog with proper RAII listener management
                // NOTE: Listener is managed by dialog's LaunchOptions and will be deleted
                // when the dialog window closes
                class RecordingListener : public RecordingDialog::Listener
                {
                public:
                    RecordingListener(DocumentManager* docMgr, Document* targetDoc, bool append)
                        : m_documentManager(docMgr), m_targetDocument(targetDoc), m_appendMode(append) {}

                    void recordingCompleted(const juce::AudioBuffer<float>& audioBuffer,
                                           double sampleRate,
                                           int numChannels) override
                    {
                        if (m_appendMode && m_targetDocument != nullptr)
                        {
                            // Append to existing document
                            appendToDocument(m_targetDocument, audioBuffer, sampleRate, numChannels);
                        }
                        else
                        {
                            // Create new document with recorded audio
                            createNewDocument(audioBuffer, sampleRate, numChannels);
                        }
                    }

                private:
                    void appendToDocument(Document* targetDoc, const juce::AudioBuffer<float>& audioBuffer,
                                         double sampleRate, int numChannels)
                    {
                        // Get cursor position (playback head) - this is where we'll insert
                        double cursorPositionSeconds = targetDoc->getWaveformDisplay().getPlaybackPosition();

                        // Get current buffer
                        auto& currentBuffer = targetDoc->getBufferManager().getMutableBuffer();
                        double currentSampleRate = targetDoc->getAudioEngine().getSampleRate();
                        int insertPositionSamples = static_cast<int>(cursorPositionSeconds * currentSampleRate);

                        // Clamp insert position to valid range
                        insertPositionSamples = juce::jlimit(0, currentBuffer.getNumSamples(), insertPositionSamples);

                        int currentSamples = currentBuffer.getNumSamples();
                        int newSamples = audioBuffer.getNumSamples();
                        int totalSamples = currentSamples + newSamples;

                        // Create combined buffer with inserted audio
                        juce::AudioBuffer<float> combinedBuffer(
                            juce::jmax(currentBuffer.getNumChannels(), audioBuffer.getNumChannels()),
                            totalSamples
                        );

                        // Copy audio before insertion point
                        for (int ch = 0; ch < currentBuffer.getNumChannels(); ++ch)
                        {
                            combinedBuffer.copyFrom(ch, 0, currentBuffer, ch, 0, insertPositionSamples);
                        }

                        // Insert new recording at cursor position
                        for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
                        {
                            combinedBuffer.copyFrom(ch, insertPositionSamples, audioBuffer, ch, 0, newSamples);
                        }

                        // Copy audio after insertion point
                        int remainingSamples = currentSamples - insertPositionSamples;
                        if (remainingSamples > 0)
                        {
                            for (int ch = 0; ch < currentBuffer.getNumChannels(); ++ch)
                            {
                                combinedBuffer.copyFrom(ch, insertPositionSamples + newSamples,
                                                       currentBuffer, ch, insertPositionSamples, remainingSamples);
                            }
                        }

                        // Update document with combined buffer
                        currentBuffer.makeCopyOf(combinedBuffer, true);
                        targetDoc->getAudioEngine().loadFromBuffer(combinedBuffer, sampleRate,
                                                                   combinedBuffer.getNumChannels());
                        targetDoc->getWaveformDisplay().reloadFromBuffer(combinedBuffer, sampleRate, false, false);
                        targetDoc->getRegionDisplay().setTotalDuration(totalSamples / sampleRate);
                        targetDoc->getMarkerDisplay().setTotalDuration(totalSamples / sampleRate);
                        targetDoc->setModified(true);

                        juce::Logger::writeToLog("Recording inserted at cursor position (" +
                                               juce::String(cursorPositionSeconds, 3) + "s): " +
                                               juce::String(newSamples) + " samples added");
                    }

                    void createNewDocument(const juce::AudioBuffer<float>& audioBuffer,
                                          double sampleRate, int numChannels)
                    {
                        auto* newDoc = m_documentManager->createDocument();
                        if (newDoc != nullptr)
                        {
                            // Load the recorded audio into the document
                            auto& buffer = newDoc->getBufferManager().getMutableBuffer();
                            buffer.setSize(audioBuffer.getNumChannels(), audioBuffer.getNumSamples());
                            buffer.makeCopyOf(audioBuffer, true);

                            // Load into the audio engine
                            newDoc->getAudioEngine().loadFromBuffer(audioBuffer, sampleRate, numChannels);

                            // Load waveform display directly from buffer
                            newDoc->getWaveformDisplay().reloadFromBuffer(audioBuffer, sampleRate, false, false);

                            // Setup region display
                            newDoc->getRegionDisplay().setSampleRate(sampleRate);
                            newDoc->getRegionDisplay().setTotalDuration(audioBuffer.getNumSamples() / sampleRate);
                            newDoc->getRegionDisplay().setVisibleRange(0.0, audioBuffer.getNumSamples() / sampleRate);
                            newDoc->getRegionDisplay().setAudioBuffer(&buffer);

                            // Setup marker display
                            newDoc->getMarkerDisplay().setSampleRate(sampleRate);
                            newDoc->getMarkerDisplay().setTotalDuration(audioBuffer.getNumSamples() / sampleRate);

                            // Set document as modified (new recording needs to be saved)
                            newDoc->setModified(true);

                            juce::Logger::writeToLog("Recording completed: " +
                                                   juce::String(audioBuffer.getNumSamples()) + " samples, " +
                                                   juce::String(sampleRate) + " Hz, " +
                                                   juce::String(numChannels) + " channels");
                        }
                        else
                        {
                            juce::Logger::writeToLog("ERROR: Failed to create new document for recording");
                        }
                    }

                    DocumentManager* m_documentManager;
                    Document* m_targetDocument;
                    bool m_appendMode;

                    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingListener)
                };

                // Dialog content component takes ownership via LaunchOptions::content.setOwned()
                // Dialog window is responsible for cleanup when closed
                RecordingDialog::showDialog(this, m_audioDeviceManager,
                                           new RecordingListener(&m_documentManager, currentDoc, appendToExisting));
                return true;
            }

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

            case CommandIDs::viewToggleRegions:
            {
                // Toggle region visibility setting (global, not per-document)
                bool currentlyVisible = Settings::getInstance().getRegionsVisible();
                Settings::getInstance().setRegionsVisible(!currentlyVisible);

                // Repaint all documents to update region visibility
                for (int i = 0; i < m_documentManager.getNumDocuments(); ++i)
                {
                    auto* document = m_documentManager.getDocument(i);
                    if (document)
                    {
                        document->getRegionDisplay().setVisible(!currentlyVisible);
                        document->getWaveformDisplay().repaint();
                    }
                }

                return true;
            }

            case CommandIDs::viewSpectrumAnalyzer:
            {
                // Create the spectrum analyzer if it doesn't exist
                if (!m_spectrumAnalyzer)
                {
                    auto* analyzer = new SpectrumAnalyzer();
                    m_spectrumAnalyzer = analyzer;  // Track pointer for later use

                    // Create window for the spectrum analyzer with close button callback
                    m_spectrumAnalyzerWindow = new CallbackDocumentWindow("Spectrum Analyzer",
                                                                          juce::Colour(0xff2a2a2a),
                                                                          juce::DocumentWindow::allButtons,
                                                                          [this]()
                    {
                        // Callback when close button is clicked - hide window and disconnect from audio engine
                        if (m_spectrumAnalyzer)
                        {
                            if (auto* doc = getCurrentDocument())
                            {
                                doc->getAudioEngine().setSpectrumAnalyzer(nullptr);
                            }
                        }
                        // Trigger menu state update
                        m_commandManager.commandStatusChanged();
                    });

                    // Window now owns the analyzer - will delete it when window is deleted
                    m_spectrumAnalyzerWindow->setContentOwned(analyzer, false);
                    m_spectrumAnalyzerWindow->setResizable(true, true);
                    m_spectrumAnalyzerWindow->setSize(600, 400);
                    m_spectrumAnalyzerWindow->setAlwaysOnTop(true);
                    m_spectrumAnalyzerWindow->centreWithSize(600, 400);
                    m_spectrumAnalyzerWindow->setUsingNativeTitleBar(true);

                    // Connect to current document's audio engine if available
                    if (auto* doc = getCurrentDocument())
                    {
                        doc->getAudioEngine().setSpectrumAnalyzer(m_spectrumAnalyzer);
                    }
                }

                // Toggle window visibility
                if (m_spectrumAnalyzerWindow)
                {
                    bool isVisible = m_spectrumAnalyzerWindow->isVisible();
                    m_spectrumAnalyzerWindow->setVisible(!isVisible);

                    if (!isVisible && m_spectrumAnalyzer)
                    {
                        // Connect to current document when showing
                        if (auto* doc = getCurrentDocument())
                        {
                            doc->getAudioEngine().setSpectrumAnalyzer(m_spectrumAnalyzer);
                        }
                    }
                    else if (isVisible && m_spectrumAnalyzer)
                    {
                        // Disconnect when hiding
                        if (auto* doc = getCurrentDocument())
                        {
                            doc->getAudioEngine().setSpectrumAnalyzer(nullptr);
                        }
                    }
                }

                return true;
            }

            // Spectrum Analyzer FFT Size Commands
            case CommandIDs::viewSpectrumFFTSize512:
                if (m_spectrumAnalyzer)
                {
                    m_spectrumAnalyzer->setFFTSize(SpectrumAnalyzer::FFTSize::SIZE_512);
                    m_commandManager.commandStatusChanged();
                }
                return true;

            case CommandIDs::viewSpectrumFFTSize1024:
                if (m_spectrumAnalyzer)
                {
                    m_spectrumAnalyzer->setFFTSize(SpectrumAnalyzer::FFTSize::SIZE_1024);
                    m_commandManager.commandStatusChanged();
                }
                return true;

            case CommandIDs::viewSpectrumFFTSize2048:
                if (m_spectrumAnalyzer)
                {
                    m_spectrumAnalyzer->setFFTSize(SpectrumAnalyzer::FFTSize::SIZE_2048);
                    m_commandManager.commandStatusChanged();
                }
                return true;

            case CommandIDs::viewSpectrumFFTSize4096:
                if (m_spectrumAnalyzer)
                {
                    m_spectrumAnalyzer->setFFTSize(SpectrumAnalyzer::FFTSize::SIZE_4096);
                    m_commandManager.commandStatusChanged();
                }
                return true;

            case CommandIDs::viewSpectrumFFTSize8192:
                if (m_spectrumAnalyzer)
                {
                    m_spectrumAnalyzer->setFFTSize(SpectrumAnalyzer::FFTSize::SIZE_8192);
                    m_commandManager.commandStatusChanged();
                }
                return true;

            // Spectrum Analyzer Window Function Commands
            case CommandIDs::viewSpectrumWindowHann:
                if (m_spectrumAnalyzer)
                {
                    m_spectrumAnalyzer->setWindowFunction(SpectrumAnalyzer::WindowFunction::HANN);
                    m_commandManager.commandStatusChanged();
                }
                return true;

            case CommandIDs::viewSpectrumWindowHamming:
                if (m_spectrumAnalyzer)
                {
                    m_spectrumAnalyzer->setWindowFunction(SpectrumAnalyzer::WindowFunction::HAMMING);
                    m_commandManager.commandStatusChanged();
                }
                return true;

            case CommandIDs::viewSpectrumWindowBlackman:
                if (m_spectrumAnalyzer)
                {
                    m_spectrumAnalyzer->setWindowFunction(SpectrumAnalyzer::WindowFunction::BLACKMAN);
                    m_commandManager.commandStatusChanged();
                }
                return true;

            case CommandIDs::viewSpectrumWindowRectangular:
                if (m_spectrumAnalyzer)
                {
                    m_spectrumAnalyzer->setWindowFunction(SpectrumAnalyzer::WindowFunction::RECTANGULAR);
                    m_commandManager.commandStatusChanged();
                }
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
                    30.0,  // Default FPS (future: user-configurable via settings)
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

            // Marker operations (Phase 3.4)
            case CommandIDs::markerAdd:
                if (!doc) return false;
                addMarkerAtCursor();
                return true;

            case CommandIDs::markerDelete:
                if (!doc) return false;
                deleteSelectedMarker();
                return true;

            case CommandIDs::markerNext:
                if (!doc) return false;
                jumpToNextMarker();
                return true;

            case CommandIDs::markerPrevious:
                if (!doc) return false;
                jumpToPreviousMarker();
                return true;

            case CommandIDs::markerShowList:
            {
                if (auto* doc = m_documentManager.getCurrentDocument())
                {
                    // Create the panel if it doesn't exist
                    if (!m_markerListPanel)
                    {
                        m_markerListPanel = new MarkerListPanel(
                            &doc->getMarkerManager(),
                            doc->getBufferManager().getSampleRate()
                        );
                        m_markerListPanel->setListener(this);
                        m_markerListPanel->setCommandManager(&m_commandManager);

                        // Show in window
                        m_markerListWindow = m_markerListPanel->showInWindow(false);
                    }
                    else
                    {
                        // Toggle window visibility
                        if (m_markerListWindow)
                        {
                            bool isVisible = m_markerListWindow->isVisible();
                            m_markerListWindow->setVisible(!isVisible);

                            if (!isVisible)
                            {
                                // Refresh panel when showing
                                m_markerListPanel->refresh();
                            }
                        }
                    }
                }
                return true;
            }

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

            case CommandIDs::processParametricEQ:
                if (!doc) return false;
                showParametricEQDialog();
                return true;

            case CommandIDs::processGraphicalEQ:
                if (!doc) return false;
                showGraphicalEQDialog();
                return true;

            case CommandIDs::processNormalize:
                if (!doc) return false;
                showNormalizeDialog();
                return true;

            case CommandIDs::processFadeIn:
                if (!doc) return false;
                showFadeInDialog();
                return true;

            case CommandIDs::processFadeOut:
                if (!doc) return false;
                showFadeOutDialog();
                return true;

            case CommandIDs::processDCOffset:
                if (!doc) return false;
                showDCOffsetDialog();
                return true;

            case CommandIDs::editSilence:
                if (!doc) return false;
                silenceSelection();
                return true;

            case CommandIDs::editTrim:
                if (!doc) return false;
                trimToSelection();
                return true;

            // Help commands
            case CommandIDs::helpAbout:
                showAboutDialog();
                return true;

            case CommandIDs::helpShortcuts:
                showKeyboardShortcutsDialog();
                return true;

            //==============================================================================
            // Plugin commands (VST3/AU plugin chain)
            case CommandIDs::pluginShowChain:
                showPluginChainPanel();
                return true;

            case CommandIDs::pluginApplyChain:
                if (!doc) return false;
                applyPluginChainToSelection();
                return true;

            case CommandIDs::pluginOffline:
                if (!doc) return false;
                showOfflinePluginDialog();
                return true;

            case CommandIDs::pluginBypassAll:
                if (!doc) return false;
                {
                    auto& chain = doc->getAudioEngine().getPluginChain();
                    chain.setAllBypassed(!chain.areAllBypassed());
                }
                return true;

            case CommandIDs::pluginRescan:
                // Use startPluginScan() to get progress bar in status bar
                startPluginScan(true);  // true = force rescan
                return true;

            case CommandIDs::pluginShowSettings:
                PluginPathsPanel::showDialog();
                return true;

            case CommandIDs::pluginClearCache:
            {
                // Ask user for confirmation before clearing cache
                auto result = juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::QuestionIcon,
                    "Clear Plugin Cache",
                    "This will delete all cached plugin data and perform a fresh scan.\n\n"
                    "This is useful if:\n"
                    "- You've installed or removed plugins\n"
                    "- A plugin is not showing up\n"
                    "- You want to fix scan-related issues\n\n"
                    "Continue?",
                    "Clear & Rescan",
                    "Cancel"
                );

                if (result)
                {
                    bool success = PluginManager::getInstance().clearCache();

                    if (!success)
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::WarningIcon,
                            "Cache Clear Warning",
                            "Some cache files could not be deleted.\n\n"
                            "This may happen if:\n"
                            "- Files are in use by another application\n"
                            "- Antivirus is blocking file deletion\n\n"
                            "The rescan will continue, but you may need to manually delete "
                            "the WaveEdit folder in your config directory for a complete reset."
                        );
                    }

                    startPluginScan(true);  // Force rescan after clearing
                }
                return true;
            }

            //==============================================================================
            // Toolbar commands
            case CommandIDs::toolbarCustomize:
            {
                // Show the toolbar customization dialog
                if (ToolbarCustomizationDialog::showDialog(m_toolbarManager, m_commandManager))
                {
                    // Layout was changed - reload toolbar
                    m_toolbar->loadLayout(m_toolbarManager.getCurrentLayout());
                    m_toolbar->resized();
                    resized();  // May need to re-layout if toolbar height changed
                }
                return true;
            }

            case CommandIDs::toolbarReset:
            {
                // Reset toolbar to default layout
                m_toolbarManager.loadLayout("Default");
                m_toolbar->loadLayout(m_toolbarManager.getCurrentLayout());
                m_toolbar->resized();
                resized();
                return true;
            }

            default:
                return false;
        }
    }

    //==============================================================================
    // MenuBarModel Implementation

    juce::StringArray getMenuBarNames() override
    {
        return { "File", "Edit", "View", "Region", "Marker", "Process", "Plugins", "Playback", "Help" };
    }

    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& /*menuName*/) override
    {
        juce::PopupMenu menu;

        if (menuIndex == 0) // File menu
        {
            // --- Document ---
            menu.addSectionHeader("Document");
            menu.addCommandItem(&m_commandManager, CommandIDs::fileNew);
            menu.addCommandItem(&m_commandManager, CommandIDs::fileOpen);
            menu.addCommandItem(&m_commandManager, CommandIDs::fileSave);
            menu.addCommandItem(&m_commandManager, CommandIDs::fileSaveAs);
            menu.addCommandItem(&m_commandManager, CommandIDs::fileClose);

            // --- Metadata ---
            menu.addSectionHeader("Metadata");
            menu.addCommandItem(&m_commandManager, CommandIDs::fileProperties);
            menu.addCommandItem(&m_commandManager, CommandIDs::fileEditBWFMetadata);
            menu.addCommandItem(&m_commandManager, CommandIDs::fileEditiXMLMetadata);

            // Recent files submenu
            auto recentFiles = Settings::getInstance().getRecentFiles();
            if (!recentFiles.isEmpty())
            {
                menu.addSectionHeader("Recent");
                juce::PopupMenu recentFilesMenu;
                for (int i = 0; i < recentFiles.size(); ++i)
                {
                    juce::File file(recentFiles[i]);
                    recentFilesMenu.addItem(file.getFileName(), [this, file] { loadFile(file); });
                }
                recentFilesMenu.addSeparator();
                recentFilesMenu.addItem("Clear Recent Files", [] { Settings::getInstance().clearRecentFiles(); });
                menu.addSubMenu("Recent Files", recentFilesMenu);
            }

            // --- Application ---
            menu.addSectionHeader("Application");
            menu.addCommandItem(&m_commandManager, CommandIDs::filePreferences);
            menu.addCommandItem(&m_commandManager, CommandIDs::fileExit);
        }
        else if (menuIndex == 1) // Edit menu
        {
            // --- History ---
            menu.addSectionHeader("History");
            menu.addCommandItem(&m_commandManager, CommandIDs::editUndo);
            menu.addCommandItem(&m_commandManager, CommandIDs::editRedo);

            // --- Clipboard ---
            menu.addSectionHeader("Clipboard");
            menu.addCommandItem(&m_commandManager, CommandIDs::editCut);
            menu.addCommandItem(&m_commandManager, CommandIDs::editCopy);
            menu.addCommandItem(&m_commandManager, CommandIDs::editPaste);
            menu.addCommandItem(&m_commandManager, CommandIDs::editDelete);

            // --- Audio Editing ---
            menu.addSectionHeader("Audio Editing");
            menu.addCommandItem(&m_commandManager, CommandIDs::editSilence);
            menu.addCommandItem(&m_commandManager, CommandIDs::editTrim);

            // --- Selection ---
            menu.addSectionHeader("Selection");
            menu.addCommandItem(&m_commandManager, CommandIDs::editSelectAll);
        }
        else if (menuIndex == 2) // View menu
        {
            // --- Zoom ---
            menu.addSectionHeader("Zoom");
            menu.addCommandItem(&m_commandManager, CommandIDs::viewZoomIn);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewZoomOut);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewZoomFit);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewZoomSelection);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewZoomOneToOne);

            // --- Display Options ---
            menu.addSectionHeader("Display");
            menu.addCommandItem(&m_commandManager, CommandIDs::viewCycleTimeFormat);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewAutoScroll);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewZoomToRegion);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewAutoPreviewRegions);
            menu.addCommandItem(&m_commandManager, CommandIDs::viewToggleRegions);

            // --- Spectrum Analyzer ---
            menu.addSectionHeader("Spectrum Analyzer");
            menu.addCommandItem(&m_commandManager, CommandIDs::viewSpectrumAnalyzer);

            // Spectrum Analyzer Configuration Submenus
            juce::PopupMenu fftSizeMenu;
            fftSizeMenu.addCommandItem(&m_commandManager, CommandIDs::viewSpectrumFFTSize512);
            fftSizeMenu.addCommandItem(&m_commandManager, CommandIDs::viewSpectrumFFTSize1024);
            fftSizeMenu.addCommandItem(&m_commandManager, CommandIDs::viewSpectrumFFTSize2048);
            fftSizeMenu.addCommandItem(&m_commandManager, CommandIDs::viewSpectrumFFTSize4096);
            fftSizeMenu.addCommandItem(&m_commandManager, CommandIDs::viewSpectrumFFTSize8192);
            menu.addSubMenu("Spectrum FFT Size", fftSizeMenu, m_spectrumAnalyzer != nullptr);

            juce::PopupMenu windowFunctionMenu;
            windowFunctionMenu.addCommandItem(&m_commandManager, CommandIDs::viewSpectrumWindowHann);
            windowFunctionMenu.addCommandItem(&m_commandManager, CommandIDs::viewSpectrumWindowHamming);
            windowFunctionMenu.addCommandItem(&m_commandManager, CommandIDs::viewSpectrumWindowBlackman);
            windowFunctionMenu.addCommandItem(&m_commandManager, CommandIDs::viewSpectrumWindowRectangular);
            menu.addSubMenu("Spectrum Window Function", windowFunctionMenu, m_spectrumAnalyzer != nullptr);

            // --- Toolbar ---
            menu.addSectionHeader("Toolbar");
            menu.addCommandItem(&m_commandManager, CommandIDs::toolbarCustomize);
            menu.addCommandItem(&m_commandManager, CommandIDs::toolbarReset);
        }
        else if (menuIndex == 3) // Region menu
        {
            // --- Create/Delete ---
            menu.addSectionHeader("Create/Delete");
            menu.addCommandItem(&m_commandManager, CommandIDs::regionAdd);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionDelete);

            // --- Navigation ---
            menu.addSectionHeader("Navigation");
            menu.addCommandItem(&m_commandManager, CommandIDs::regionNext);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionPrevious);

            // --- Selection ---
            menu.addSectionHeader("Selection");
            menu.addCommandItem(&m_commandManager, CommandIDs::regionSelectInverse);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionSelectAll);

            // --- Editing ---
            menu.addSectionHeader("Editing");
            menu.addCommandItem(&m_commandManager, CommandIDs::regionMerge);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionSplit);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionCopy);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionPaste);

            // --- Batch Operations ---
            menu.addSectionHeader("Batch Operations");
            menu.addCommandItem(&m_commandManager, CommandIDs::regionStripSilence);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionExportAll);
            menu.addCommandItem(&m_commandManager, CommandIDs::regionBatchRename);

            // --- View ---
            menu.addSectionHeader("View");
            menu.addCommandItem(&m_commandManager, CommandIDs::regionShowList);
        }
        else if (menuIndex == 4) // Marker menu (Phase 3.4)
        {
            // --- Create/Delete ---
            menu.addSectionHeader("Create/Delete");
            menu.addCommandItem(&m_commandManager, CommandIDs::markerAdd);
            menu.addCommandItem(&m_commandManager, CommandIDs::markerDelete);

            // --- Navigation ---
            menu.addSectionHeader("Navigation");
            menu.addCommandItem(&m_commandManager, CommandIDs::markerNext);
            menu.addCommandItem(&m_commandManager, CommandIDs::markerPrevious);

            // --- View ---
            menu.addSectionHeader("View");
            menu.addCommandItem(&m_commandManager, CommandIDs::markerShowList);
        }
        else if (menuIndex == 5) // Process menu
        {
            // --- Volume ---
            menu.addSectionHeader("Volume");
            menu.addCommandItem(&m_commandManager, CommandIDs::processGain);
            menu.addCommandItem(&m_commandManager, CommandIDs::processNormalize);

            // --- Equalization ---
            menu.addSectionHeader("Equalization");
            menu.addCommandItem(&m_commandManager, CommandIDs::processParametricEQ);
            menu.addCommandItem(&m_commandManager, CommandIDs::processGraphicalEQ);

            // --- Repair ---
            menu.addSectionHeader("Repair");
            menu.addCommandItem(&m_commandManager, CommandIDs::processDCOffset);

            // --- Fades ---
            menu.addSectionHeader("Fades");
            menu.addCommandItem(&m_commandManager, CommandIDs::processFadeIn);
            menu.addCommandItem(&m_commandManager, CommandIDs::processFadeOut);
        }
        else if (menuIndex == 6) // Plugins menu (VST3/AU plugin chain)
        {
            // --- Plugin Chain ---
            menu.addSectionHeader("Plugin Chain");
            menu.addCommandItem(&m_commandManager, CommandIDs::pluginShowChain);
            menu.addCommandItem(&m_commandManager, CommandIDs::pluginApplyChain);
            menu.addCommandItem(&m_commandManager, CommandIDs::pluginBypassAll);

            // --- Offline Processing ---
            menu.addSectionHeader("Offline Processing");
            menu.addCommandItem(&m_commandManager, CommandIDs::pluginOffline);

            // --- Plugin Management ---
            menu.addSectionHeader("Plugin Management");
            menu.addCommandItem(&m_commandManager, CommandIDs::pluginRescan);
            menu.addCommandItem(&m_commandManager, CommandIDs::pluginShowSettings);
            menu.addCommandItem(&m_commandManager, CommandIDs::pluginClearCache);
        }
        else if (menuIndex == 7) // Playback menu
        {
            // --- Transport ---
            menu.addSectionHeader("Transport");
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackPlay);
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackPause);
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackStop);

            // --- Recording ---
            menu.addSectionHeader("Recording");
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackRecord);

            // --- Looping ---
            menu.addSectionHeader("Looping");
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackLoop);
            menu.addCommandItem(&m_commandManager, CommandIDs::playbackLoopRegion);
        }
        else if (menuIndex == 8) // Help menu
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
                // Remember the directory for next time
                Settings::getInstance().setLastFileDirectory(file.getParentDirectory());

                auto* newDoc = m_documentManager.openDocument(file);
                if (newDoc)
                {
                    // Add to recent files
                    Settings::getInstance().addRecentFile(file);

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
        // Support WAV, FLAC, MP3, and OGG formats
        m_fileChooser = std::make_unique<juce::FileChooser>(
            "Open Audio File(s)",
            Settings::getInstance().getLastFileDirectory(),
            "*.wav;*.flac;*.mp3;*.ogg",
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
                        // Remember the directory for next time
                        Settings::getInstance().setLastFileDirectory(file.getParentDirectory());

                        // Use DocumentManager to open files (supports multiple tabs)
                        auto* newDoc = m_documentManager.openDocument(file);
                        if (newDoc)
                        {
                            // Add to recent files
                            Settings::getInstance().addRecentFile(file);

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

        // Remember the directory for next time
        Settings::getInstance().setLastFileDirectory(file.getParentDirectory());

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

        // Save using Document::saveFile() which includes BWF and iXML metadata
        bool saveSuccess = doc->saveFile(currentFile, doc->getBufferManager().getBitDepth());

        if (saveSuccess)
        {
            repaint();
            juce::Logger::writeToLog("File saved successfully with metadata: " + currentFile.getFullPathName());
        }
        else
        {
            // Show error dialog
            ErrorDialog::showWithDetails(
                "Save Failed",
                "Could not save file: " + currentFile.getFileName(),
                "Failed to write file with metadata",
                ErrorDialog::Severity::Error
            );
        }
    }

    void saveFileAs()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        // Get current file, or provide default for unsaved documents
        juce::File currentFile = doc->getAudioEngine().getCurrentFile();
        if (!currentFile.existsAsFile())
        {
            // Provide sensible default: Documents/Untitled.wav
            currentFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                               .getChildFile("Untitled.wav");
        }

        // Show Save As dialog to get format and encoding settings
        double sourceSampleRate = doc->getAudioEngine().getSampleRate();
        int sourceChannels = doc->getBufferManager().getBuffer().getNumChannels();

        auto result = SaveAsOptionsPanel::showDialog(sourceSampleRate, sourceChannels, currentFile);

        // If user cancelled, exit
        if (!result.has_value())
            return;

        auto settings = result.value();

        juce::Logger::writeToLog("Saving as " + settings.format.toUpperCase() +
                                 " - Bit depth: " + juce::String(settings.bitDepth) +
                                 ", Quality: " + juce::String(settings.quality) +
                                 ", Sample rate: " + juce::String(settings.targetSampleRate > 0.0 ? settings.targetSampleRate : sourceSampleRate, 0) + " Hz");

        // Save using Document::saveFile() with all settings
        bool saveSuccess = doc->saveFile(settings.targetFile, settings.bitDepth, settings.quality, settings.targetSampleRate);

        if (saveSuccess)
        {
            // Remember the directory for next time
            Settings::getInstance().setLastFileDirectory(settings.targetFile.getParentDirectory());

            // Add to recent files
            Settings::getInstance().addRecentFile(settings.targetFile);

            repaint();

            juce::Logger::writeToLog("File saved successfully with metadata: " + settings.targetFile.getFullPathName());
        }
        else
        {
            // Show error dialog
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Save As Failed",
                "Could not save file as: " + settings.targetFile.getFileName() + "\n\n" +
                "Failed to write file. Check console for details.",
                "OK");
        }
    }

    bool hasUnsavedChanges() const
    {
        // Check if any document has unsaved changes
        for (int i = 0; i < m_documentManager.getNumDocuments(); ++i)
        {
            if (auto* doc = m_documentManager.getDocument(i))
            {
                if (doc->isModified())
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool saveAllModifiedDocuments()
    {
        // Save all modified documents
        for (int i = 0; i < m_documentManager.getNumDocuments(); ++i)
        {
            if (auto* doc = m_documentManager.getDocument(i))
            {
                if (doc->isModified())
                {
                    // Switch to this document's tab
                    m_documentManager.setCurrentDocumentIndex(i);

                    // Save it
                    auto currentFile = doc->getAudioEngine().getCurrentFile();
                    if (!currentFile.existsAsFile())
                    {
                        // File doesn't exist, would need Save As - abort for now
                        juce::Logger::writeToLog("Cannot auto-save untitled document - skipping");
                        continue;
                    }

                    bool success = doc->saveFile(currentFile, doc->getBufferManager().getBitDepth());
                    if (!success)
                    {
                        juce::Logger::writeToLog("Failed to save: " + doc->getFilename());
                        return false;  // Abort on first failure
                    }
                }
            }
        }
        return true;  // All saves successful
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

        // Close the document (which removes the tab and cleans up)
        m_documentManager.closeDocument(doc);
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
            // CRITICAL: Always clear stale loop points before starting new playback
            // This prevents loop points from previous sessions affecting current playback
            doc->getAudioEngine().clearLoopPoints();
            doc->getAudioEngine().setLooping(false);

            // Prioritize selection playback (professional audio editor behavior)
            // If selection exists, play selection once, then stop
            if (doc->getWaveformDisplay().hasSelection())
            {
                double selStart = doc->getWaveformDisplay().getSelectionStart();
                double selEnd = doc->getWaveformDisplay().getSelectionEnd();

                // Set playback to start at selection start
                doc->getAudioEngine().setPosition(selStart);

                // Set loop points to constrain playback to selection
                // looping=false means stop at selEnd (don't loop back)
                doc->getAudioEngine().setLoopPoints(selStart, selEnd);
                doc->getAudioEngine().setLooping(false);
            }
            else if (doc->getWaveformDisplay().hasEditCursor())
            {
                // No selection: play from edit cursor to end of file
                double startPos = doc->getWaveformDisplay().getEditCursorPosition();
                doc->getAudioEngine().setPosition(startPos);
                // Loop points already cleared above
            }
            else
            {
                // No selection or cursor: play from beginning
                doc->getAudioEngine().setPosition(0.0);
                // Loop points already cleared above
            }

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
        // Future: Support multi-range selection for advanced editing workflows
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

        // Copy advanced naming options (Phase 4 enhancements)
        settings.customTemplate = exportSettings->customTemplate;
        settings.prefix = exportSettings->prefix;
        settings.suffix = exportSettings->suffix;
        settings.usePaddedIndex = exportSettings->usePaddedIndex;
        settings.suffixBeforeIndex = exportSettings->suffixBeforeIndex;

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
    // Marker helpers (Phase 3.4)

    /**
     * Add a marker at the current cursor position.
     * Creates an undo action for the marker addition.
     */
    void addMarkerAtCursor()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        if (!doc->getAudioEngine().isFileLoaded())
        {
            juce::Logger::writeToLog("Cannot add marker: No file loaded");
            return;
        }

        // Get cursor position from tracked click position (most reliable)
        // This is set by WaveformClickTracker every time user clicks on waveform
        double cursorTime = 0.0;
        if (m_hasLastClickPosition)
        {
            // Use tracked click position (always accurate, never cleared)
            cursorTime = m_lastClickTimeInSeconds;
        }
        else if (doc->getWaveformDisplay().hasEditCursor())
        {
            // Fallback 1: Use edit cursor if available
            cursorTime = doc->getWaveformDisplay().getEditCursorPosition();
        }
        else
        {
            // Fallback 2: Use playback position as last resort
            cursorTime = doc->getAudioEngine().getCurrentPosition();
        }

        // Convert time to samples for marker storage
        int64_t cursorSample = doc->getBufferManager().timeToSample(cursorTime);

        // Generate default marker name (M1, M2, etc.)
        int markerCount = doc->getMarkerManager().getNumMarkers() + 1;
        juce::String markerName = juce::String("M") + juce::String(markerCount);

        // Create marker with default color (yellow)
        Marker marker(markerName, cursorSample, juce::Colours::yellow);

        // Create undo action
        doc->getUndoManager().beginNewTransaction("Add Marker");
        auto* undoAction = new AddMarkerUndoAction(
            doc->getMarkerManager(),
            &doc->getMarkerDisplay(),
            marker
        );
        doc->getUndoManager().perform(undoAction);

        juce::Logger::writeToLog(juce::String::formatted(
            "Added marker '%s' at %.3fs (sample %lld)",
            markerName.toRawUTF8(),
            cursorTime,
            cursorSample
        ));

        // Repaint to show new marker
        repaint();
    }

    /**
     * Delete the currently selected marker.
     * Creates an undo action for the marker deletion.
     */
    void deleteSelectedMarker()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        int selectedIndex = doc->getMarkerManager().getSelectedMarkerIndex();
        if (selectedIndex < 0)
        {
            juce::Logger::writeToLog("Cannot delete marker: No marker selected");
            return;
        }

        const Marker* marker = doc->getMarkerManager().getMarker(selectedIndex);
        if (!marker)
        {
            juce::Logger::writeToLog("Cannot delete marker: Invalid marker index");
            return;
        }

        // Save marker info for logging
        juce::String markerName = marker->getName();
        int64_t markerPosition = marker->getPosition();

        // Create undo action
        doc->getUndoManager().beginNewTransaction("Delete Marker");
        auto* undoAction = new DeleteMarkerUndoAction(
            doc->getMarkerManager(),
            &doc->getMarkerDisplay(),
            selectedIndex,
            *marker
        );
        doc->getUndoManager().perform(undoAction);

        juce::Logger::writeToLog(juce::String::formatted(
            "Deleted marker '%s' at sample %lld",
            markerName.toRawUTF8(),
            markerPosition
        ));

        // Repaint to remove marker
        repaint();
    }

    /**
     * Jump playback position to the next marker after current position.
     */
    void jumpToNextMarker()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        int64_t currentSample = doc->getAudioEngine().getCurrentPosition();
        int nextIndex = doc->getMarkerManager().getNextMarkerIndex(currentSample);

        if (nextIndex < 0)
        {
            juce::Logger::writeToLog("No marker found after current position");
            return;
        }

        const Marker* marker = doc->getMarkerManager().getMarker(nextIndex);
        if (!marker)
        {
            juce::Logger::writeToLog("Invalid marker index");
            return;
        }

        // Set playback position to marker
        doc->getAudioEngine().setPosition(marker->getPosition());

        // Select the marker
        doc->getMarkerManager().setSelectedMarkerIndex(nextIndex);

        juce::Logger::writeToLog(juce::String::formatted(
            "Jumped to marker '%s' at sample %lld",
            marker->getName().toRawUTF8(),
            marker->getPosition()
        ));

        // Repaint to show selection
        repaint();
    }

    /**
     * Jump playback position to the previous marker before current position.
     */
    void jumpToPreviousMarker()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        int64_t currentSample = doc->getAudioEngine().getCurrentPosition();
        int prevIndex = doc->getMarkerManager().getPreviousMarkerIndex(currentSample);

        if (prevIndex < 0)
        {
            juce::Logger::writeToLog("No marker found before current position");
            return;
        }

        const Marker* marker = doc->getMarkerManager().getMarker(prevIndex);
        if (!marker)
        {
            juce::Logger::writeToLog("Invalid marker index");
            return;
        }

        // Set playback position to marker
        doc->getAudioEngine().setPosition(marker->getPosition());

        // Select the marker
        doc->getMarkerManager().setSelectedMarkerIndex(prevIndex);

        juce::Logger::writeToLog(juce::String::formatted(
            "Jumped to marker '%s' at sample %lld",
            marker->getName().toRawUTF8(),
            marker->getPosition()
        ));

        // Repaint to show selection
        repaint();
    }

    //==============================================================================
    // Gain adjustment helpers

    /**
     * Apply gain adjustment to entire file or selection.
     * Creates undo action for the gain adjustment.
     *
     * @param gainDB Gain adjustment in decibels (positive or negative)
     * @param startSampleParam Optional start sample (for dialog-based apply with explicit bounds)
     * @param endSampleParam Optional end sample (for dialog-based apply with explicit bounds)
     */
    void applyGainAdjustment(float gainDB, int64_t startSampleParam = -1, int64_t endSampleParam = -1)
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

        // CRITICAL FIX: Use explicit bounds if provided (from dialog preview)
        // This ensures we apply to the SAME region that was previewed
        if (startSampleParam >= 0 && endSampleParam >= 0)
        {
            // Explicit bounds provided (from dialog) - use these
            startSample = static_cast<int>(startSampleParam);
            numSamples = static_cast<int>(endSampleParam - startSampleParam);
            isSelection = (startSampleParam != 0 || endSampleParam != buffer.getNumSamples());
        }
        else if (doc->getWaveformDisplay().hasSelection())
        {
            // No explicit bounds - check current selection (for keyboard shortcuts)
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
     * NEW: Passes AudioEngine to enable real-time preview.
     * Supports selection-based preview - if user has selection, only that region is previewed.
     */
    void showGainDialog()
    {
        Document* doc = m_documentManager.getCurrentDocument();
        if (!doc || !doc->getAudioEngine().isFileLoaded())
        {
            // No document or file loaded - show dialog without preview
            auto result = GainDialog::showDialog(nullptr, nullptr, 0, 0);

            if (result.has_value())
            {
                applyGainAdjustment(result.value());
            }
            return;
        }

        // Get selection bounds (or entire file if no selection)
        auto& waveform = doc->getWaveformDisplay();
        auto& engine = doc->getAudioEngine();
        bool hasSelection = waveform.hasSelection();

        // Convert time-based selection (seconds) to sample-based (samples)
        double sampleRate = engine.getSampleRate();
        int64_t startSample = hasSelection ?
            static_cast<int64_t>(waveform.getSelectionStart() * sampleRate) : 0;
        int64_t endSample = hasSelection ?
            static_cast<int64_t>(waveform.getSelectionEnd() * sampleRate) :
            static_cast<int64_t>(engine.getTotalLength() * sampleRate);

        // Show dialog with preview support and selection bounds
        auto result = GainDialog::showDialog(&doc->getAudioEngine(), &doc->getBufferManager(), startSample, endSample);

        if (result.has_value())
        {
            // CRITICAL: Pass the SAME bounds that were previewed to ensure apply matches preview
            applyGainAdjustment(result.value(), startSample, endSample);
        }
    }

    /**
     * Show normalize dialog and apply normalization to selection (or entire file).
     * Allows user to set target peak level and preview before applying.
     */
    void showNormalizeDialog()
    {
        Document* doc = m_documentManager.getCurrentDocument();
        if (!doc || !doc->getAudioEngine().isFileLoaded())
            return;

        // Get selection bounds (or entire file if no selection)
        auto& waveform = doc->getWaveformDisplay();
        auto& engine = doc->getAudioEngine();
        bool hasSelection = waveform.hasSelection();

        // Convert time-based selection (seconds) to sample-based (samples)
        double sampleRate = engine.getSampleRate();
        int64_t startSample = hasSelection ?
            static_cast<int64_t>(waveform.getSelectionStart() * sampleRate) : 0;
        int64_t endSample = hasSelection ?
            static_cast<int64_t>(waveform.getSelectionEnd() * sampleRate) :
            static_cast<int64_t>(engine.getTotalLength() * sampleRate);

        // DEBUG: Log the bounds being passed to the dialog
        juce::Logger::writeToLog("showNormalizeDialog - Creating dialog with bounds:");
        juce::Logger::writeToLog("  Has selection: " + juce::String(hasSelection ? "YES" : "NO"));
        if (hasSelection) {
            juce::Logger::writeToLog("  Selection start time: " + juce::String(waveform.getSelectionStart()) + " seconds");
            juce::Logger::writeToLog("  Selection end time: " + juce::String(waveform.getSelectionEnd()) + " seconds");
        }
        juce::Logger::writeToLog("  Start sample: " + juce::String(startSample));
        juce::Logger::writeToLog("  End sample: " + juce::String(endSample));
        juce::Logger::writeToLog("  Sample rate: " + juce::String(sampleRate));

        // Create and configure dialog
        NormalizeDialog dialog(&doc->getAudioEngine(), &doc->getBufferManager(), startSample, endSample);

        // Set up callbacks
        dialog.onApply([this, doc, &dialog](float targetDB) {
            // Get selection or entire file
            int startSample = 0;
            int numSamples = doc->getBufferManager().getBuffer().getNumSamples();
            bool isSelection = false;

            if (doc->getWaveformDisplay().hasSelection())
            {
                startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
                int endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
                numSamples = endSample - startSample;
                isSelection = true;
            }

            // Store before state for undo (MUST happen before any processing)
            auto& buffer = doc->getBufferManager().getMutableBuffer();
            auto beforeBuffer = std::make_shared<juce::AudioBuffer<float>>();
            beforeBuffer->setSize(buffer.getNumChannels(), numSamples);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                beforeBuffer->copyFrom(ch, 0, buffer, ch, startSample, numSamples);
            }

            // Get mode and calculate required gain
            NormalizeDialog::NormalizeMode mode = dialog.getMode();
            float currentLevel = (mode == NormalizeDialog::NormalizeMode::RMS) ?
                dialog.getCurrentRMSDB() : dialog.getCurrentPeakDB();
            float requiredGainDB = targetDB - currentLevel;

            // Build transaction name
            juce::String modeStr = (mode == NormalizeDialog::NormalizeMode::RMS) ? "RMS" : "Peak";
            juce::String transactionName = juce::String::formatted(
                "Normalize %s to %.1f dB (%s)",
                modeStr.toRawUTF8(),
                targetDB,
                isSelection ? "selection" : "entire file"
            );

            // Close the dialog first (before potentially showing progress dialog)
            if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState(1);

            // Check if we need progress dialog for large operations
            if (numSamples >= kProgressDialogThreshold)
            {
                // ASYNCHRONOUS PATH: Large operation - show progress dialog
                // Create a working copy of the selection region for processing
                auto regionBuffer = std::make_shared<juce::AudioBuffer<float>>();
                regionBuffer->setSize(buffer.getNumChannels(), numSamples);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    regionBuffer->copyFrom(ch, 0, buffer, ch, startSample, numSamples);
                }

                ProgressDialog::runWithProgress(
                    transactionName,
                    [regionBuffer, requiredGainDB, numSamples](ProgressCallback progress) -> bool {
                        // Apply gain to the region copy (not the main buffer)
                        // Use startSample=0 since regionBuffer is already extracted from startSample
                        return AudioProcessor::applyGainWithProgress(*regionBuffer, requiredGainDB, 0, numSamples, progress);
                    },
                    [doc, beforeBuffer, regionBuffer, startSample, numSamples, transactionName, requiredGainDB, isSelection](bool success) {
                        if (success)
                        {
                            // Copy processed region back to main buffer at correct position
                            auto& buf = doc->getBufferManager().getMutableBuffer();
                            for (int ch = 0; ch < regionBuffer->getNumChannels(); ++ch)
                            {
                                buf.copyFrom(ch, startSample, *regionBuffer, ch, 0, numSamples);
                            }

                            // Register undo action (operation already applied)
                            doc->getUndoManager().beginNewTransaction(transactionName);
                            auto* undoAction = new GainUndoAction(
                                doc->getBufferManager(),
                                doc->getWaveformDisplay(),
                                doc->getAudioEngine(),
                                *beforeBuffer,
                                startSample,
                                numSamples,
                                requiredGainDB,  // float gainDB
                                isSelection      // bool isSelection
                            );
                            // Mark as already performed so undo() will restore, but perform() is no-op
                            undoAction->markAsAlreadyPerformed();
                            doc->getUndoManager().perform(undoAction);
                            doc->setModified(true);

                            // Update waveform display
                            doc->getAudioEngine().reloadBufferPreservingPlayback(
                                buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                            doc->getWaveformDisplay().reloadFromBuffer(
                                buf, doc->getAudioEngine().getSampleRate(), true, true);
                        }
                        else
                        {
                            // Cancelled: Restore buffer from snapshot
                            auto& buf = doc->getBufferManager().getMutableBuffer();
                            for (int ch = 0; ch < beforeBuffer->getNumChannels(); ++ch)
                            {
                                buf.copyFrom(ch, startSample, *beforeBuffer, ch, 0, numSamples);
                            }
                            // Update display to show restored state
                            doc->getAudioEngine().reloadBufferPreservingPlayback(
                                buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                            doc->getWaveformDisplay().reloadFromBuffer(
                                buf, doc->getAudioEngine().getSampleRate(), true, true);
                        }
                    }
                );
            }
            else
            {
                // SYNCHRONOUS PATH: Small operation - existing behavior
                doc->getUndoManager().beginNewTransaction(transactionName);

                auto* undoAction = new GainUndoAction(
                    doc->getBufferManager(),
                    doc->getWaveformDisplay(),
                    doc->getAudioEngine(),
                    *beforeBuffer,
                    startSample,
                    numSamples,
                    requiredGainDB,  // float gainDB
                    isSelection      // bool isSelection
                );

                doc->getUndoManager().perform(undoAction);
                doc->setModified(true);
            }
        });

        dialog.onCancel([&dialog]() {
            if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState(0);
        });

        // Show dialog modally
        juce::DialogWindow::LaunchOptions options;
        options.content.setNonOwned(&dialog);  // Use setNonOwned for stack-allocated dialog
        options.componentToCentreAround = this;
        options.dialogTitle = "Normalize";
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;

        options.runModal();
    }

    /**
     * Show fade in dialog and apply fade to selection.
     * Requires selection (won't work on entire file).
     */
    void showFadeInDialog()
    {
        Document* doc = m_documentManager.getCurrentDocument();
        if (!doc || !doc->getAudioEngine().isFileLoaded())
            return;

        // Fade in requires a selection
        if (!doc->getWaveformDisplay().hasSelection())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Fade In",
                "Please select a region of audio to fade in.",
                "OK");
            return;
        }

        // Get selection bounds
        auto& waveform = doc->getWaveformDisplay();
        auto& engine = doc->getAudioEngine();
        double sampleRate = engine.getSampleRate();
        int64_t startSample = static_cast<int64_t>(waveform.getSelectionStart() * sampleRate);
        int64_t endSample = static_cast<int64_t>(waveform.getSelectionEnd() * sampleRate);

        // Create and configure dialog
        FadeInDialog dialog(&doc->getAudioEngine(), &doc->getBufferManager(), startSample, endSample);

        // Set up callbacks
        dialog.onApply([this, doc, &dialog]() {
            // Get selection
            int startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
            int endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
            int numSamples = endSample - startSample;

            // Store before state for undo (MUST happen before any processing)
            auto& buffer = doc->getBufferManager().getMutableBuffer();
            auto beforeBuffer = std::make_shared<juce::AudioBuffer<float>>();
            beforeBuffer->setSize(buffer.getNumChannels(), numSamples);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                beforeBuffer->copyFrom(ch, 0, buffer, ch, startSample, numSamples);
            }

            // Get selected curve type from dialog
            FadeCurveType curveType = dialog.getSelectedCurveType();

            // Close the dialog first (before potentially showing progress dialog)
            if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState(1);

            // Check if we need progress dialog for large operations
            if (numSamples >= kProgressDialogThreshold)
            {
                // ASYNCHRONOUS PATH: Large operation - show progress dialog
                // Create a working copy of the selection region for processing
                auto regionBuffer = std::make_shared<juce::AudioBuffer<float>>();
                regionBuffer->setSize(buffer.getNumChannels(), numSamples);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    regionBuffer->copyFrom(ch, 0, buffer, ch, startSample, numSamples);
                }

                ProgressDialog::runWithProgress(
                    "Fade In",
                    [regionBuffer, numSamples, curveType](ProgressCallback progress) -> bool {
                        // Apply fade to the region copy (not the main buffer)
                        return AudioProcessor::fadeInWithProgress(*regionBuffer, numSamples, curveType, progress);
                    },
                    [doc, beforeBuffer, regionBuffer, startSample, numSamples, curveType](bool success) {
                        if (success)
                        {
                            // Copy processed region back to main buffer at correct position
                            auto& buf = doc->getBufferManager().getMutableBuffer();
                            for (int ch = 0; ch < regionBuffer->getNumChannels(); ++ch)
                            {
                                buf.copyFrom(ch, startSample, *regionBuffer, ch, 0, numSamples);
                            }

                            // Register undo action (operation already applied)
                            doc->getUndoManager().beginNewTransaction("Fade In");
                            auto* undoAction = new FadeInUndoAction(
                                doc->getBufferManager(),
                                doc->getWaveformDisplay(),
                                doc->getAudioEngine(),
                                *beforeBuffer,
                                startSample,
                                numSamples,
                                curveType
                            );
                            undoAction->markAsAlreadyPerformed();
                            doc->getUndoManager().perform(undoAction);
                            doc->setModified(true);

                            // Update waveform display
                            doc->getAudioEngine().reloadBufferPreservingPlayback(
                                buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                            doc->getWaveformDisplay().reloadFromBuffer(
                                buf, doc->getAudioEngine().getSampleRate(), true, true);
                        }
                        else
                        {
                            // Cancelled: Restore buffer from snapshot
                            auto& buf = doc->getBufferManager().getMutableBuffer();
                            for (int ch = 0; ch < beforeBuffer->getNumChannels(); ++ch)
                            {
                                buf.copyFrom(ch, startSample, *beforeBuffer, ch, 0, numSamples);
                            }
                            doc->getAudioEngine().reloadBufferPreservingPlayback(
                                buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                            doc->getWaveformDisplay().reloadFromBuffer(
                                buf, doc->getAudioEngine().getSampleRate(), true, true);
                        }
                    }
                );
            }
            else
            {
                // SYNCHRONOUS PATH: Small operation - existing behavior
                doc->getUndoManager().beginNewTransaction("Fade In");

                auto* undoAction = new FadeInUndoAction(
                    doc->getBufferManager(),
                    doc->getWaveformDisplay(),
                    doc->getAudioEngine(),
                    *beforeBuffer,
                    startSample,
                    numSamples,
                    curveType
                );

                doc->getUndoManager().perform(undoAction);
                doc->setModified(true);
            }
        });

        dialog.onCancel([&dialog]() {
            if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState(0);
        });

        // Show dialog modally
        juce::DialogWindow::LaunchOptions options;
        options.content.setNonOwned(&dialog);  // Use setNonOwned for stack-allocated dialog
        options.componentToCentreAround = this;
        options.dialogTitle = "Fade In";
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;

        options.runModal();
    }

    /**
     * Show fade out dialog and apply fade to selection.
     * Requires selection (won't work on entire file).
     */
    void showFadeOutDialog()
    {
        Document* doc = m_documentManager.getCurrentDocument();
        if (!doc || !doc->getAudioEngine().isFileLoaded())
            return;

        // Fade out requires a selection
        if (!doc->getWaveformDisplay().hasSelection())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Fade Out",
                "Please select a region of audio to fade out.",
                "OK");
            return;
        }

        // Get selection bounds
        auto& waveform = doc->getWaveformDisplay();
        auto& engine = doc->getAudioEngine();
        double sampleRate = engine.getSampleRate();
        int64_t startSample = static_cast<int64_t>(waveform.getSelectionStart() * sampleRate);
        int64_t endSample = static_cast<int64_t>(waveform.getSelectionEnd() * sampleRate);

        // Create and configure dialog
        FadeOutDialog dialog(&doc->getAudioEngine(), &doc->getBufferManager(), startSample, endSample);

        // Set up callbacks
        dialog.onApply([this, doc, &dialog]() {
            // Get selection
            int startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
            int endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
            int numSamples = endSample - startSample;

            // Store before state for undo (MUST happen before any processing)
            auto& buffer = doc->getBufferManager().getMutableBuffer();
            auto beforeBuffer = std::make_shared<juce::AudioBuffer<float>>();
            beforeBuffer->setSize(buffer.getNumChannels(), numSamples);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                beforeBuffer->copyFrom(ch, 0, buffer, ch, startSample, numSamples);
            }

            // Get selected curve type from dialog
            FadeCurveType curveType = dialog.getSelectedCurveType();

            // Close the dialog first (before potentially showing progress dialog)
            if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState(1);

            // Check if we need progress dialog for large operations
            if (numSamples >= kProgressDialogThreshold)
            {
                // ASYNCHRONOUS PATH: Large operation - show progress dialog
                // Create a working copy of the selection region for processing
                auto regionBuffer = std::make_shared<juce::AudioBuffer<float>>();
                regionBuffer->setSize(buffer.getNumChannels(), numSamples);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    regionBuffer->copyFrom(ch, 0, buffer, ch, startSample, numSamples);
                }

                ProgressDialog::runWithProgress(
                    "Fade Out",
                    [regionBuffer, numSamples, curveType](ProgressCallback progress) -> bool {
                        // Apply fade to the region copy (not the main buffer)
                        return AudioProcessor::fadeOutWithProgress(*regionBuffer, numSamples, curveType, progress);
                    },
                    [doc, beforeBuffer, regionBuffer, startSample, numSamples, curveType](bool success) {
                        if (success)
                        {
                            // Copy processed region back to main buffer at correct position
                            auto& buf = doc->getBufferManager().getMutableBuffer();
                            for (int ch = 0; ch < regionBuffer->getNumChannels(); ++ch)
                            {
                                buf.copyFrom(ch, startSample, *regionBuffer, ch, 0, numSamples);
                            }

                            // Register undo action (operation already applied)
                            doc->getUndoManager().beginNewTransaction("Fade Out");
                            auto* undoAction = new FadeOutUndoAction(
                                doc->getBufferManager(),
                                doc->getWaveformDisplay(),
                                doc->getAudioEngine(),
                                *beforeBuffer,
                                startSample,
                                numSamples,
                                curveType
                            );
                            undoAction->markAsAlreadyPerformed();
                            doc->getUndoManager().perform(undoAction);
                            doc->setModified(true);

                            // Update waveform display
                            doc->getAudioEngine().reloadBufferPreservingPlayback(
                                buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                            doc->getWaveformDisplay().reloadFromBuffer(
                                buf, doc->getAudioEngine().getSampleRate(), true, true);
                        }
                        else
                        {
                            // Cancelled: Restore buffer from snapshot
                            auto& buf = doc->getBufferManager().getMutableBuffer();
                            for (int ch = 0; ch < beforeBuffer->getNumChannels(); ++ch)
                            {
                                buf.copyFrom(ch, startSample, *beforeBuffer, ch, 0, numSamples);
                            }
                            doc->getAudioEngine().reloadBufferPreservingPlayback(
                                buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                            doc->getWaveformDisplay().reloadFromBuffer(
                                buf, doc->getAudioEngine().getSampleRate(), true, true);
                        }
                    }
                );
            }
            else
            {
                // SYNCHRONOUS PATH: Small operation - existing behavior
                doc->getUndoManager().beginNewTransaction("Fade Out");

                auto* undoAction = new FadeOutUndoAction(
                    doc->getBufferManager(),
                    doc->getWaveformDisplay(),
                    doc->getAudioEngine(),
                    *beforeBuffer,
                    startSample,
                    numSamples,
                    curveType
                );

                doc->getUndoManager().perform(undoAction);
                doc->setModified(true);
            }
        });

        dialog.onCancel([&dialog]() {
            if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState(0);
        });

        // Show dialog modally
        juce::DialogWindow::LaunchOptions options;
        options.content.setNonOwned(&dialog);  // Use setNonOwned for stack-allocated dialog
        options.componentToCentreAround = this;
        options.dialogTitle = "Fade Out";
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;

        options.runModal();
    }

    /**
     * Show DC offset dialog and remove DC offset from selection.
     * Requires selection (won't work on entire file).
     */
    void showDCOffsetDialog()
    {
        Document* doc = m_documentManager.getCurrentDocument();
        if (!doc || !doc->getAudioEngine().isFileLoaded())
            return;

        // Check for selection (DC offset dialog requires selection)
        auto& waveform = doc->getWaveformDisplay();
        if (!waveform.hasSelection())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "No Selection",
                "Please select a region before removing DC offset.",
                "OK"
            );
            return;
        }

        // Convert selection to sample coordinates
        auto& engine = doc->getAudioEngine();
        double sampleRate = engine.getSampleRate();
        int64_t startSample = static_cast<int64_t>(waveform.getSelectionStart() * sampleRate);
        int64_t endSample = static_cast<int64_t>(waveform.getSelectionEnd() * sampleRate);

        // Create dialog
        DCOffsetDialog dialog(&doc->getAudioEngine(), &doc->getBufferManager(), startSample, endSample);

        // Set up apply callback
        dialog.onApply([this, doc, &dialog]() {
            // Get selection bounds
            int startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
            int endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
            int numSamples = endSample - startSample;

            // Store before state for undo (MUST happen before any processing)
            auto& buffer = doc->getBufferManager().getMutableBuffer();
            auto beforeBuffer = std::make_shared<juce::AudioBuffer<float>>();
            beforeBuffer->setSize(buffer.getNumChannels(), numSamples);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                beforeBuffer->copyFrom(ch, 0, buffer, ch, startSample, numSamples);
            }

            // Close the dialog first (before potentially showing progress dialog)
            if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState(1);

            // Check if we need progress dialog for large operations
            if (numSamples >= kProgressDialogThreshold)
            {
                // ASYNCHRONOUS PATH: Large operation - show progress dialog
                // Create a working copy of the selection region for processing
                auto regionBuffer = std::make_shared<juce::AudioBuffer<float>>();
                regionBuffer->setSize(buffer.getNumChannels(), numSamples);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    regionBuffer->copyFrom(ch, 0, buffer, ch, startSample, numSamples);
                }

                ProgressDialog::runWithProgress(
                    "Remove DC Offset",
                    [regionBuffer](ProgressCallback progress) -> bool {
                        // Apply DC offset removal to the region copy (not the main buffer)
                        return AudioProcessor::removeDCOffsetWithProgress(*regionBuffer, progress);
                    },
                    [doc, beforeBuffer, regionBuffer, startSample, numSamples](bool success) {
                        if (success)
                        {
                            // Copy processed region back to main buffer at correct position
                            auto& buf = doc->getBufferManager().getMutableBuffer();
                            for (int ch = 0; ch < regionBuffer->getNumChannels(); ++ch)
                            {
                                buf.copyFrom(ch, startSample, *regionBuffer, ch, 0, numSamples);
                            }

                            // Register undo action (operation already applied)
                            doc->getUndoManager().beginNewTransaction("Remove DC Offset (selection)");
                            auto* undoAction = new DCOffsetRemovalUndoAction(
                                doc->getBufferManager(),
                                doc->getWaveformDisplay(),
                                doc->getAudioEngine(),
                                *beforeBuffer,
                                startSample,
                                numSamples
                            );
                            undoAction->markAsAlreadyPerformed();
                            doc->getUndoManager().perform(undoAction);
                            doc->setModified(true);

                            // Update waveform display
                            doc->getAudioEngine().reloadBufferPreservingPlayback(
                                buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                            doc->getWaveformDisplay().reloadFromBuffer(
                                buf, doc->getAudioEngine().getSampleRate(), true, true);
                        }
                        else
                        {
                            // Cancelled: Restore buffer from snapshot
                            auto& buf = doc->getBufferManager().getMutableBuffer();
                            for (int ch = 0; ch < beforeBuffer->getNumChannels(); ++ch)
                            {
                                buf.copyFrom(ch, startSample, *beforeBuffer, ch, 0, numSamples);
                            }
                            doc->getAudioEngine().reloadBufferPreservingPlayback(
                                buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                            doc->getWaveformDisplay().reloadFromBuffer(
                                buf, doc->getAudioEngine().getSampleRate(), true, true);
                        }
                    }
                );
            }
            else
            {
                // SYNCHRONOUS PATH: Small operation - existing behavior
                doc->getUndoManager().beginNewTransaction("Remove DC Offset (selection)");

                auto* undoAction = new DCOffsetRemovalUndoAction(
                    doc->getBufferManager(),
                    doc->getWaveformDisplay(),
                    doc->getAudioEngine(),
                    *beforeBuffer,
                    startSample,
                    numSamples
                );

                doc->getUndoManager().perform(undoAction);
                doc->setModified(true);
            }
        });

        dialog.onCancel([&dialog]() {
            if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState(0);
        });

        // Show dialog modally
        juce::DialogWindow::LaunchOptions options;
        options.content.setNonOwned(&dialog);  // Use setNonOwned for stack-allocated dialog
        options.componentToCentreAround = this;
        options.dialogTitle = "Remove DC Offset";
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;

        options.runModal();
    }

    /**
     * Show parametric EQ dialog and apply EQ to selection (or entire file if no selection).
     */
    void showParametricEQDialog()
    {
        Document* doc = m_documentManager.getCurrentDocument();
        if (!doc || !doc->getAudioEngine().isFileLoaded())
            return;

        // Get selection bounds (or entire file if no selection)
        auto& waveform = doc->getWaveformDisplay();
        auto& engine = doc->getAudioEngine();
        bool hasSelection = waveform.hasSelection();

        // Convert time-based selection (seconds) to sample-based (samples)
        double sampleRate = engine.getSampleRate();
        int64_t startSample = hasSelection ?
            static_cast<int64_t>(waveform.getSelectionStart() * sampleRate) : 0;
        int64_t endSample = hasSelection ?
            static_cast<int64_t>(waveform.getSelectionEnd() * sampleRate) :
            static_cast<int64_t>(engine.getTotalLength() * sampleRate);
        int64_t numSamples = endSample - startSample;

        // Show dialog (initialized with neutral EQ) with preview support
        auto eqParams = ParametricEQ::Parameters::createNeutral();
        auto result = ParametricEQDialog::showDialog(
            &doc->getAudioEngine(),
            &doc->getBufferManager(),
            startSample,
            endSample,
            eqParams);

        if (result.has_value())
        {
            // Apply EQ with undo support
            doc->getUndoManager().beginNewTransaction("Parametric EQ");

            auto* undoAction = new ApplyParametricEQAction(
                doc->getBufferManager(),
                doc->getAudioEngine(),
                doc->getWaveformDisplay(),
                startSample,
                numSamples,
                result.value()
            );

            doc->getUndoManager().perform(undoAction);
            doc->setModified(true);
        }
    }

    /**
     * Show graphical parametric EQ editor and apply EQ to selection (or entire file if no selection).
     */
    void showGraphicalEQDialog()
    {
        Document* doc = m_documentManager.getCurrentDocument();
        if (!doc || !doc->getAudioEngine().isFileLoaded())
            return;

        // Get selection bounds (or entire file if no selection)
        auto& waveform = doc->getWaveformDisplay();
        auto& engine = doc->getAudioEngine();
        bool hasSelection = waveform.hasSelection();

        // Convert time-based selection (seconds) to sample-based (samples)
        double sampleRate = engine.getSampleRate();
        int64_t startSample = hasSelection ?
            static_cast<int64_t>(waveform.getSelectionStart() * sampleRate) : 0;
        int64_t endSample = hasSelection ?
            static_cast<int64_t>(waveform.getSelectionEnd() * sampleRate) :
            static_cast<int64_t>(engine.getTotalLength() * sampleRate);
        int64_t numSamples = endSample - startSample;

        juce::Logger::writeToLog("showGraphicalEQDialog: startSample=" + juce::String(startSample) + ", numSamples=" + juce::String(numSamples));

        // Show graphical editor (initialized with default preset)
        auto eqParams = DynamicParametricEQ::createDefaultPreset();
        auto result = GraphicalEQEditor::showDialog(&engine, eqParams);

        if (result.has_value())
        {
            juce::Logger::writeToLog("GraphicalEQ dialog returned parameters:");
            juce::Logger::writeToLog("  Output Gain: " + juce::String(result->outputGain) + " dB");
            juce::Logger::writeToLog("  Bands: " + juce::String(static_cast<int>(result->bands.size())));
            for (size_t i = 0; i < result->bands.size(); ++i)
            {
                const auto& band = result->bands[i];
                juce::Logger::writeToLog("    Band " + juce::String(static_cast<int>(i)) + ": " +
                    juce::String(band.frequency) + " Hz, " +
                    juce::String(band.gain) + " dB, Q=" + juce::String(band.q) +
                    ", Type=" + DynamicParametricEQ::getFilterTypeName(band.filterType) +
                    (band.enabled ? " [ON]" : " [OFF]"));
            }

            // Apply EQ with undo support
            doc->getUndoManager().beginNewTransaction("Graphical EQ");

            auto* undoAction = new ApplyDynamicParametricEQAction(
                doc->getBufferManager(),
                doc->getAudioEngine(),
                doc->getWaveformDisplay(),
                startSample,
                numSamples,
                result.value()
            );

            juce::Logger::writeToLog("Performing ApplyParametricEQAction...");
            doc->getUndoManager().perform(undoAction);
            doc->setModified(true);
            juce::Logger::writeToLog("ApplyParametricEQAction completed");
        }
        else
        {
            juce::Logger::writeToLog("GraphicalEQ dialog cancelled");
        }
    }

    //==============================================================================
    // Help dialogs

    /**
     * Show About WaveEdit dialog.
     */
    void showAboutDialog()
    {
        juce::String aboutText =
            "WaveEdit - Professional Audio Editor\n"
            "Version 1.0\n\n"
            "Copyright © 2025 ZQ SFX\n"
            "Licensed under GNU GPL v3\n\n"
            "Built with JUCE " + juce::String(JUCE_MAJOR_VERSION) + "." +
            juce::String(JUCE_MINOR_VERSION) + "." + juce::String(JUCE_BUILDNUMBER);

        juce::AlertWindow::showMessageBox(juce::AlertWindow::InfoIcon,
                                          "About WaveEdit",
                                          aboutText,
                                          "OK",
                                          this);
    }

    /**
     * Show keyboard shortcuts reference dialog.
     *
     * Displays a searchable, categorized list of all keyboard shortcuts
     * dynamically loaded from the current keymap template.
     */
    void showKeyboardShortcutsDialog()
    {
        KeyboardCheatSheetDialog::showDialog(this, m_keymapManager, m_commandManager);
    }

    //==============================================================================
    // Plugin management methods

    /**
     * Show the Plugin Manager dialog for browsing and selecting plugins.
     * Uses a simple modal approach for now.
     */
    void showPluginManagerDialog()
    {
        // Create dialog and show modally
        PluginManagerDialog dialog;

        // Show in a dialog window
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "Plugin Manager";
        options.dialogBackgroundColour = juce::Colour(0xff1e1e1e);
        options.content.setNonOwned(&dialog);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;

        // Store selected plugin (if any)
        const juce::PluginDescription* selectedPlugin = nullptr;

        // Set up a simple listener using lambdas (we'll poll the result after modal)
        // Since DialogWindow::runModal blocks, we check dialog state after it returns
        int result = options.runModal();
        juce::ignoreUnused(result);

        // After modal closes, check if user clicked Add and selected a plugin
        selectedPlugin = dialog.getSelectedPlugin();

        // If user clicked Add button (or double-clicked) with a valid selection
        if (dialog.wasAddClicked() && selectedPlugin != nullptr)
        {
            auto* doc = getCurrentDocument();
            if (doc)
            {
                auto& chain = doc->getAudioEngine().getPluginChain();
                int index = chain.addPlugin(*selectedPlugin);
                if (index >= 0)
                {
                    // Enable plugin chain if not already enabled
                    doc->getAudioEngine().setPluginChainEnabled(true);
                    DBG("Added plugin: " + selectedPlugin->name + " at index " + juce::String(index));
                }
                else
                {
                    ErrorDialog::show("Plugin Error", "Failed to load plugin: " + selectedPlugin->name);
                }
            }
        }
    }

    /**
     * Show the unified Plugin Chain window with integrated browser.
     */
    void showPluginChainPanel()
    {
        auto* doc = getCurrentDocument();
        if (!doc)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "Plugin Chain",
                "Please open an audio file first.",
                "OK");
            return;
        }

        auto& chain = doc->getAudioEngine().getPluginChain();

        // Create the unified window
        auto* chainWindow = new PluginChainWindow(&chain);

        // Show in window with keyboard shortcut support (need window pointer for listener)
        auto* window = chainWindow->showInWindow(&m_commandManager);

        // Create and set listener with pointers to allow null-checking after document close
        auto* listener = new ChainWindowListener(this, &chain, &doc->getAudioEngine(), window);
        chainWindow->setListener(listener);

        // Track the listener so we can notify it when document is closed
        m_pluginChainListeners.add(listener);
    }

    /**
     * Apply the plugin chain to the current selection (offline render).
     * Version with explicit render options - called from PluginChainWindow.
     *
     * @param convertToStereo If true and file is mono, convert to stereo before processing
     * @param includeTail If true, extend selection to capture reverb/delay tails
     * @param tailLengthSeconds Length of tail in seconds
     */
    void applyPluginChainToSelectionWithOptions(bool convertToStereo, bool includeTail, double tailLengthSeconds)
    {
        applyPluginChainToSelectionInternal(convertToStereo, includeTail, tailLengthSeconds);
    }

    /**
     * Apply the plugin chain to the current selection (offline render).
     * Default version called from Cmd+P keyboard shortcut - uses default options.
     */
    void applyPluginChainToSelection()
    {
        // When called from keyboard shortcut, use default options (no stereo conversion, no tail)
        // User should use the Plugin Chain window for advanced options
        applyPluginChainToSelectionInternal(false, false, 2.0);
    }

    /**
     * Internal implementation for applying plugin chain with options.
     * Creates independent plugin instances to avoid real-time audio conflicts.
     * Supports progress dialog for large selections and full undo/redo.
     */
    void applyPluginChainToSelectionInternal(bool convertToStereo, bool includeTail, double tailLengthSeconds)
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        auto& engine = doc->getAudioEngine();
        auto& chain = engine.getPluginChain();

        if (chain.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "Apply Plugin Chain",
                "The plugin chain is empty. Add plugins first.",
                "OK");
            return;
        }

        // Check if all plugins are bypassed
        if (chain.areAllBypassed())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "Apply Plugin Chain",
                "All plugins are bypassed. Un-bypass at least one plugin to apply effects.",
                "OK");
            return;
        }

        // Get buffer and determine selection range
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        if (buffer.getNumSamples() == 0)
        {
            return;
        }

        int64_t startSample = 0;
        int64_t numSamples = buffer.getNumSamples();
        bool hasSelection = doc->getWaveformDisplay().hasSelection();

        if (hasSelection)
        {
            startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
            int64_t endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
            numSamples = endSample - startSample;

            if (numSamples <= 0)
            {
                return;
            }
        }

        // Build chain description for undo action name
        juce::String chainDescription = PluginChainRenderer::buildChainDescription(chain);
        juce::String transactionName = "Apply Plugin Chain: " + chainDescription;
        double sampleRate = doc->getBufferManager().getSampleRate();

        // Handle stereo conversion if requested (and file is mono)
        int outputChannels = 0;  // 0 = match source channels
        if (convertToStereo && buffer.getNumChannels() == 1)
        {
            // Convert entire buffer to stereo BEFORE processing
            // This must be done first so the waveform display and audio engine update properly
            doc->getUndoManager().beginNewTransaction("Convert to Stereo");
            auto* convertAction = new ConvertToStereoAction(
                doc->getBufferManager(),
                doc->getWaveformDisplay(),
                doc->getAudioEngine()
            );
            doc->getUndoManager().perform(convertAction);
            doc->setModified(true);

            outputChannels = 2;  // Request stereo output from renderer

            DBG("Converted mono file to stereo for plugin processing");
        }

        // Calculate tail samples if effect tail is requested
        int64_t tailSamples = 0;
        if (includeTail && tailLengthSeconds > 0.0)
        {
            tailSamples = static_cast<int64_t>(tailLengthSeconds * sampleRate);
            DBG("Including tail of " + juce::String(tailLengthSeconds) + " seconds (" + juce::String(tailSamples) + " samples)");
        }

        // Create shared pointers for async operation
        auto renderer = std::make_shared<PluginChainRenderer>();
        auto processedBuffer = std::make_shared<juce::AudioBuffer<float>>();

        // Create the offline chain on the message thread (required for plugin instantiation)
        // This must be done BEFORE starting the background task
        auto offlineChain = std::make_shared<PluginChainRenderer::OfflineChain>(
            PluginChainRenderer::createOfflineChain(chain, sampleRate, renderer->getBlockSize())
        );

        if (!offlineChain->isValid())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Apply Plugin Chain",
                "Failed to create offline plugin instances. Some plugins may not support offline rendering.",
                "OK");
            return;
        }

        DBG("Created offline chain with " + juce::String(offlineChain->instances.size()) + " plugins");

        // Use progress dialog for large selections
        if (numSamples > kProgressDialogThreshold)
        {
            ProgressDialog::runWithProgress(
                transactionName,
                [doc, renderer, offlineChain, processedBuffer, startSample, numSamples, sampleRate, outputChannels, tailSamples](ProgressCallback progress) -> bool {
                    // Render selection through pre-created offline chain (safe for background thread)
                    // Pass outputChannels to preserve stereo output if user requested it
                    // Pass tailSamples to capture effect tail (reverb/delay trails)
                    auto result = renderer->renderWithOfflineChain(
                        doc->getBufferManager().getBuffer(),
                        *offlineChain,
                        sampleRate,
                        startSample,
                        numSamples,
                        progress,
                        outputChannels,
                        tailSamples
                    );

                    if (result.success)
                    {
                        // Copy processed buffer for use in completion callback
                        std::cerr << "[BGTASK] Render succeeded! Result buffer: channels="
                                  << result.processedBuffer.getNumChannels()
                                  << ", samples=" << result.processedBuffer.getNumSamples() << std::endl;
                        std::cerr.flush();

                        processedBuffer->setSize(result.processedBuffer.getNumChannels(),
                                                 result.processedBuffer.getNumSamples());
                        for (int ch = 0; ch < result.processedBuffer.getNumChannels(); ++ch)
                        {
                            processedBuffer->copyFrom(ch, 0, result.processedBuffer, ch, 0,
                                                      result.processedBuffer.getNumSamples());
                        }

                        std::cerr << "[BGTASK] Copied to processedBuffer: channels="
                                  << processedBuffer->getNumChannels()
                                  << ", samples=" << processedBuffer->getNumSamples() << std::endl;
                        std::cerr.flush();

                        return true;
                    }
                    else if (result.cancelled)
                    {
                        return false;  // User cancelled
                    }
                    else
                    {
                        DBG("Plugin chain render failed: " + result.errorMessage);
                        return false;
                    }
                },
                [doc, processedBuffer, startSample, numSamples, tailSamples, transactionName, chainDescription, hasSelection](bool success) {
                    std::cerr << "[CALLBACK] Completion callback entered: success=" << (success ? "true" : "false")
                              << ", processedBuffer samples=" << processedBuffer->getNumSamples()
                              << ", processedBuffer channels=" << processedBuffer->getNumChannels()
                              << ", tailSamples=" << tailSamples << std::endl;
                    std::cerr.flush();

                    if (success && processedBuffer->getNumSamples() > 0)
                    {
                        // For undo, save the ORIGINAL range we're about to overwrite (numSamples, not extended)
                        // IMPORTANT: Create undo action FIRST to save original audio BEFORE modifying buffer
                        doc->getUndoManager().beginNewTransaction(transactionName);
                        auto* undoAction = new ApplyPluginChainAction(
                            doc->getBufferManager(),
                            doc->getAudioEngine(),
                            doc->getWaveformDisplay(),
                            startSample,
                            numSamples,  // Original selection size (undo needs to know what to restore)
                            *processedBuffer,
                            chainDescription
                        );

                        // NOW replace the original range with processed audio (may extend buffer if tail included)
                        // replaceRange handles size differences - it deletes the old range and inserts new audio
                        std::cerr << "[APPLY] About to replaceRange: bufferCh=" << doc->getBufferManager().getBuffer().getNumChannels()
                                  << ", bufferSamples=" << doc->getBufferManager().getBuffer().getNumSamples()
                                  << ", processedCh=" << processedBuffer->getNumChannels()
                                  << ", processedSamples=" << processedBuffer->getNumSamples()
                                  << ", startSample=" << startSample
                                  << ", numSamples=" << numSamples << std::endl;
                        std::cerr.flush();

                        bool replaced = doc->getBufferManager().replaceRange(startSample, numSamples, *processedBuffer);

                        std::cerr << "[APPLY] replaceRange returned: " << (replaced ? "true" : "false")
                                  << ", new buffer size: " << doc->getBufferManager().getBuffer().getNumSamples() << std::endl;
                        std::cerr.flush();

                        if (replaced)
                        {
                            // Mark as already performed and register with undo system
                            undoAction->markAsAlreadyPerformed();
                            doc->getUndoManager().perform(undoAction);
                            doc->setModified(true);

                            // Regenerate waveform thumbnail from modified buffer
                            doc->getWaveformDisplay().reloadFromBuffer(
                                doc->getBufferManager().getBuffer(),
                                doc->getBufferManager().getSampleRate(),
                                true,  // preserveView
                                true   // preserveEditCursor
                            );

                            // Reload audio engine buffer
                            doc->getAudioEngine().reloadBufferPreservingPlayback(
                                doc->getBufferManager().getBuffer(),
                                doc->getBufferManager().getSampleRate(),
                                doc->getBufferManager().getBuffer().getNumChannels()
                            );

                            if (tailSamples > 0)
                            {
                                DBG("Plugin chain applied to " + juce::String(numSamples) + " samples, " +
                                    "extended by " + juce::String(tailSamples) + " tail samples (new size: " +
                                    juce::String(processedBuffer->getNumSamples()) + ")");
                            }
                            else
                            {
                                DBG("Plugin chain applied to " + juce::String(numSamples) + " samples");
                            }
                        }
                        else
                        {
                            delete undoAction;
                            DBG("Failed to replace audio range with processed buffer");
                        }
                    }
                }
            );
        }
        else
        {
            // Small selection: process synchronously without progress dialog
            // Use renderWithOfflineChain to support outputChannels and tailSamples parameters
            auto result = renderer->renderWithOfflineChain(
                buffer,
                *offlineChain,
                sampleRate,
                startSample,
                numSamples,
                nullptr,  // No progress callback for small operations
                outputChannels,
                tailSamples
            );

            if (result.success)
            {
                // IMPORTANT: Create undo action FIRST to save original audio BEFORE modifying buffer
                doc->getUndoManager().beginNewTransaction(transactionName);
                auto* undoAction = new ApplyPluginChainAction(
                    doc->getBufferManager(),
                    doc->getAudioEngine(),
                    doc->getWaveformDisplay(),
                    startSample,
                    numSamples,  // Original selection size (undo needs to know what to restore)
                    result.processedBuffer,
                    chainDescription
                );

                // NOW replace the original range with processed audio (may extend buffer if tail included)
                // replaceRange handles size differences - it deletes the old range and inserts new audio
                bool replaced = doc->getBufferManager().replaceRange(startSample, numSamples, result.processedBuffer);

                if (replaced)
                {
                    // Mark as already performed and register with undo system
                    undoAction->markAsAlreadyPerformed();
                    doc->getUndoManager().perform(undoAction);
                    doc->setModified(true);

                    // Regenerate waveform thumbnail from modified buffer
                    doc->getWaveformDisplay().reloadFromBuffer(
                        doc->getBufferManager().getBuffer(),
                        doc->getBufferManager().getSampleRate(),
                        true,  // preserveView
                        true   // preserveEditCursor
                    );

                    // Reload audio engine buffer
                    doc->getAudioEngine().reloadBufferPreservingPlayback(
                        doc->getBufferManager().getBuffer(),
                        doc->getBufferManager().getSampleRate(),
                        doc->getBufferManager().getBuffer().getNumChannels()
                    );

                    if (tailSamples > 0)
                    {
                        DBG("Plugin chain applied to " + juce::String(numSamples) + " samples, " +
                            "extended by " + juce::String(tailSamples) + " tail samples (new size: " +
                            juce::String(result.processedBuffer.getNumSamples()) + ") (sync)");
                    }
                    else
                    {
                        DBG("Plugin chain applied to " + juce::String(numSamples) + " samples (sync)");
                    }
                }
                else
                {
                    delete undoAction;
                    DBG("Failed to replace audio range with processed buffer (sync)");
                }
            }
            else if (!result.cancelled)
            {
                // Show error message
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Apply Plugin Chain",
                    "Failed to apply plugin chain:\n" + result.errorMessage,
                    "OK");
            }
        }
    }

    /**
     * Show the Offline Plugin dialog for applying a single plugin to selection.
     * Similar to Gain/Normalize dialog workflow with preview support.
     */
    void showOfflinePluginDialog()
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        auto& engine = doc->getAudioEngine();
        if (!engine.isFileLoaded()) return;

        // Determine selection range
        int64_t selectionStart = 0;
        int64_t selectionEnd = doc->getBufferManager().getBuffer().getNumSamples();

        if (doc->getWaveformDisplay().hasSelection())
        {
            selectionStart = doc->getBufferManager().timeToSample(
                doc->getWaveformDisplay().getSelectionStart());
            selectionEnd = doc->getBufferManager().timeToSample(
                doc->getWaveformDisplay().getSelectionEnd());
        }

        // Show the dialog
        auto result = OfflinePluginDialog::showDialog(
            &engine,
            &doc->getBufferManager(),
            selectionStart,
            selectionEnd
        );

        if (result && result->applied)
        {
            // User clicked Apply - process the audio with the selected plugin
            // Pass render options from dialog
            applyOfflinePlugin(result->pluginDescription, result->pluginState,
                              selectionStart, selectionEnd - selectionStart,
                              result->renderOptions.convertToStereo,
                              result->renderOptions.includeTail,
                              result->renderOptions.tailLengthSeconds);
        }
    }

    /**
     * Apply a single plugin offline to selection.
     * Creates undo action and updates waveform.
     * @param pluginDesc The plugin description
     * @param pluginState The plugin state to apply
     * @param startSample Start position in samples
     * @param numSamples Number of samples to process
     * @param convertToStereo Whether to convert mono to stereo before processing
     * @param includeTail Whether to include effect tail (reverb/delay)
     * @param tailLengthSeconds Length of effect tail in seconds
     */
    void applyOfflinePlugin(const juce::PluginDescription& pluginDesc,
                           const juce::MemoryBlock& pluginState,
                           int64_t startSample, int64_t numSamples,
                           bool convertToStereo = false,
                           bool includeTail = false,
                           double tailLengthSeconds = 2.0)
    {
        auto* doc = getCurrentDocument();
        if (!doc) return;

        auto& buffer = doc->getBufferManager().getMutableBuffer();
        double sampleRate = doc->getBufferManager().getSampleRate();

        // Handle stereo conversion if requested (and file is mono)
        int outputChannels = 0;  // 0 = match source channels
        if (convertToStereo && buffer.getNumChannels() == 1)
        {
            // Convert entire buffer to stereo BEFORE processing
            doc->getUndoManager().beginNewTransaction("Convert to Stereo");
            auto* convertAction = new ConvertToStereoAction(
                doc->getBufferManager(),
                doc->getWaveformDisplay(),
                doc->getAudioEngine()
            );
            doc->getUndoManager().perform(convertAction);
            doc->setModified(true);

            outputChannels = 2;  // Request stereo output from renderer

            DBG("Converted mono file to stereo for offline plugin processing");
        }

        // Calculate tail samples if effect tail is requested
        int64_t tailSamples = 0;
        if (includeTail && tailLengthSeconds > 0.0)
        {
            tailSamples = static_cast<int64_t>(tailLengthSeconds * sampleRate);
            DBG("Including tail of " + juce::String(tailLengthSeconds) + " seconds (" + juce::String(tailSamples) + " samples)");
        }

        // Create a temporary single-plugin chain
        PluginChain tempChain;
        int nodeIndex = tempChain.addPlugin(pluginDesc);
        if (nodeIndex < 0)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Offline Plugin",
                "Failed to load plugin: " + pluginDesc.name,
                "OK");
            return;
        }

        // Apply the plugin state
        auto* node = tempChain.getPlugin(nodeIndex);
        if (node != nullptr && pluginState.getSize() > 0)
        {
            node->setState(pluginState);
        }

        // Create renderer
        auto renderer = std::make_shared<PluginChainRenderer>();
        juce::String transactionName = "Apply Plugin: " + pluginDesc.name;

        // Use progress dialog for large selections
        if (numSamples > kProgressDialogThreshold)
        {
            auto processedBuffer = std::make_shared<juce::AudioBuffer<float>>();

            // Create offline chain on message thread
            auto offlineChain = std::make_shared<PluginChainRenderer::OfflineChain>(
                PluginChainRenderer::createOfflineChain(tempChain, sampleRate, renderer->getBlockSize())
            );

            if (!offlineChain->isValid())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Offline Plugin",
                    "Failed to create offline plugin instance.",
                    "OK");
                return;
            }

            ProgressDialog::runWithProgress(
                transactionName,
                [doc, renderer, offlineChain, processedBuffer, startSample, numSamples, sampleRate, outputChannels, tailSamples](ProgressCallback progress) -> bool {
                    // Pass outputChannels to preserve stereo output if user requested it
                    // Pass tailSamples to capture effect tail (reverb/delay)
                    auto result = renderer->renderWithOfflineChain(
                        doc->getBufferManager().getBuffer(),
                        *offlineChain,
                        sampleRate,
                        startSample,
                        numSamples,
                        progress,
                        outputChannels,
                        tailSamples
                    );

                    if (result.success)
                    {
                        processedBuffer->setSize(result.processedBuffer.getNumChannels(),
                                                 result.processedBuffer.getNumSamples());
                        for (int ch = 0; ch < result.processedBuffer.getNumChannels(); ++ch)
                        {
                            processedBuffer->copyFrom(ch, 0, result.processedBuffer, ch, 0,
                                                      result.processedBuffer.getNumSamples());
                        }
                        return true;
                    }
                    return !result.cancelled;
                },
                [doc, processedBuffer, startSample, numSamples, tailSamples, transactionName, pluginDesc](bool success) {
                    if (success && processedBuffer->getNumSamples() > 0)
                    {
                        const int64_t totalProcessedSamples = processedBuffer->getNumSamples();

                        if (tailSamples > 0)
                        {
                            DBG("Offline plugin: processed " + juce::String(totalProcessedSamples) +
                                " samples (including " + juce::String(tailSamples) + " tail)");
                        }

                        doc->getUndoManager().beginNewTransaction(transactionName);
                        auto* undoAction = new ApplyPluginChainAction(
                            doc->getBufferManager(),
                            doc->getAudioEngine(),
                            doc->getWaveformDisplay(),
                            startSample,
                            numSamples,  // Original selection size for undo
                            *processedBuffer,
                            pluginDesc.name
                        );

                        // Use replaceRange to properly handle buffer extension when tail is included
                        // replaceRange deletes the original range and inserts the processed audio,
                        // allowing the buffer to grow if processedBuffer is larger than numSamples
                        bool replaced = doc->getBufferManager().replaceRange(startSample, numSamples, *processedBuffer);

                        if (replaced)
                        {
                            undoAction->markAsAlreadyPerformed();
                            doc->getUndoManager().perform(undoAction);
                            doc->setModified(true);

                            doc->getWaveformDisplay().reloadFromBuffer(
                                doc->getBufferManager().getBuffer(),
                                doc->getBufferManager().getSampleRate(),
                                true, true
                            );

                            doc->getAudioEngine().reloadBufferPreservingPlayback(
                                doc->getBufferManager().getBuffer(),
                                doc->getBufferManager().getSampleRate(),
                                doc->getBufferManager().getBuffer().getNumChannels()
                            );

                            if (tailSamples > 0)
                            {
                                DBG("Applied plugin " + pluginDesc.name + " with effect tail - buffer extended to " +
                                    juce::String(doc->getBufferManager().getBuffer().getNumSamples()) + " samples");
                            }
                            else
                            {
                                DBG("Applied plugin " + pluginDesc.name + " to " + juce::String(numSamples) + " samples");
                            }
                        }
                        else
                        {
                            DBG("Offline plugin: replaceRange failed!");
                            delete undoAction;
                        }
                    }
                }
            );
        }
        else
        {
            // Small selection: process synchronously
            // Create offline chain so we can use renderWithOfflineChain with outputChannels
            auto offlineChain = PluginChainRenderer::createOfflineChain(tempChain, sampleRate, renderer->getBlockSize());
            if (!offlineChain.isValid())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Offline Plugin",
                    "Failed to create offline plugin instance.",
                    "OK");
                return;
            }

            auto result = renderer->renderWithOfflineChain(
                doc->getBufferManager().getBuffer(),
                offlineChain,
                sampleRate,
                startSample,
                numSamples,
                nullptr,
                outputChannels,
                tailSamples
            );

            if (result.success)
            {
                const int64_t totalProcessedSamples = result.processedBuffer.getNumSamples();

                if (tailSamples > 0)
                {
                    DBG("Offline plugin (sync): processed " + juce::String(totalProcessedSamples) +
                        " samples (including " + juce::String(tailSamples) + " tail)");
                }

                doc->getUndoManager().beginNewTransaction(transactionName);
                auto* undoAction = new ApplyPluginChainAction(
                    doc->getBufferManager(),
                    doc->getAudioEngine(),
                    doc->getWaveformDisplay(),
                    startSample,
                    numSamples,  // Original selection size for undo
                    result.processedBuffer,
                    pluginDesc.name
                );

                // Use replaceRange to properly handle buffer extension when tail is included
                // replaceRange deletes the original range and inserts the processed audio,
                // allowing the buffer to grow if processedBuffer is larger than numSamples
                bool replaced = doc->getBufferManager().replaceRange(startSample, numSamples, result.processedBuffer);

                if (replaced)
                {
                    undoAction->markAsAlreadyPerformed();
                    doc->getUndoManager().perform(undoAction);
                    doc->setModified(true);

                    doc->getWaveformDisplay().reloadFromBuffer(
                        doc->getBufferManager().getBuffer(),
                        doc->getBufferManager().getSampleRate(),
                        true, true
                    );

                    doc->getAudioEngine().reloadBufferPreservingPlayback(
                        doc->getBufferManager().getBuffer(),
                        doc->getBufferManager().getSampleRate(),
                        doc->getBufferManager().getBuffer().getNumChannels()
                    );

                    if (tailSamples > 0)
                    {
                        DBG("Applied plugin " + pluginDesc.name + " (sync) with effect tail - buffer extended to " +
                            juce::String(doc->getBufferManager().getBuffer().getNumSamples()) + " samples");
                    }
                    else
                    {
                        DBG("Applied plugin " + pluginDesc.name + " (sync) to " + juce::String(numSamples) + " samples");
                    }
                }
                else
                {
                    DBG("Offline plugin (sync): replaceRange failed!");
                    delete undoAction;
                }
            }
            else if (!result.cancelled)
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Offline Plugin",
                    "Failed to apply plugin:\n" + result.errorMessage,
                    "OK");
            }
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

        /**
         * Mark this action as already performed.
         * Used when the DSP operation was done by a progress-enabled method
         * before registering with the undo system.
         */
        void markAsAlreadyPerformed() { m_alreadyPerformed = true; }

        bool perform() override
        {
            // If already performed (by progress dialog), skip the DSP operation
            if (m_alreadyPerformed)
            {
                m_alreadyPerformed = false; // Reset for redo
                return true;
            }

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
        bool m_alreadyPerformed = false;  // For progress dialog integration
    };

    //==============================================================================
    /**
     * Undo action for converting mono to stereo.
     * Stores the original mono buffer for undo.
     */
    class ConvertToStereoAction : public juce::UndoableAction
    {
    public:
        ConvertToStereoAction(AudioBufferManager& bufferManager,
                              WaveformDisplay& waveform,
                              AudioEngine& audioEngine)
            : m_bufferManager(bufferManager),
              m_waveformDisplay(waveform),
              m_audioEngine(audioEngine)
        {
            // Store the original mono buffer for undo
            const auto& originalBuffer = m_bufferManager.getBuffer();
            m_originalMonoBuffer.setSize(originalBuffer.getNumChannels(),
                                         originalBuffer.getNumSamples());
            m_originalMonoBuffer.makeCopyOf(originalBuffer, true);
        }

        void markAsAlreadyPerformed() { m_alreadyPerformed = true; }

        bool perform() override
        {
            if (m_alreadyPerformed)
            {
                m_alreadyPerformed = false;
                return true;
            }

            // Convert mono to stereo
            if (!m_bufferManager.convertToStereo())
            {
                return false;
            }

            // Reload audio engine with new channel count
            m_audioEngine.reloadBufferPreservingPlayback(
                m_bufferManager.getBuffer(),
                m_bufferManager.getSampleRate(),
                m_bufferManager.getBuffer().getNumChannels()
            );

            // Reload waveform display
            m_waveformDisplay.reloadFromBuffer(
                m_bufferManager.getBuffer(),
                m_bufferManager.getSampleRate(),
                true, true
            );

            return true;
        }

        bool undo() override
        {
            // Restore the original mono buffer
            auto& buffer = m_bufferManager.getMutableBuffer();
            buffer.setSize(m_originalMonoBuffer.getNumChannels(),
                          m_originalMonoBuffer.getNumSamples());
            buffer.makeCopyOf(m_originalMonoBuffer, true);

            // Reload audio engine with mono
            m_audioEngine.reloadBufferPreservingPlayback(
                buffer,
                m_bufferManager.getSampleRate(),
                buffer.getNumChannels()
            );

            // Reload waveform display
            m_waveformDisplay.reloadFromBuffer(
                buffer,
                m_bufferManager.getSampleRate(),
                true, true
            );

            return true;
        }

        int getSizeInUnits() override { return 100; }

    private:
        AudioBufferManager& m_bufferManager;
        WaveformDisplay& m_waveformDisplay;
        AudioEngine& m_audioEngine;
        juce::AudioBuffer<float> m_originalMonoBuffer;
        bool m_alreadyPerformed = false;
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

        // Load last-used curve type from settings
        int lastCurve = Settings::getInstance().getSetting("dsp.lastFadeInCurve", 0);
        FadeCurveType curveType = static_cast<FadeCurveType>(lastCurve);

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
            numSamples,
            curveType
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

        // Load last-used curve type from settings
        int lastCurve = Settings::getInstance().getSetting("dsp.lastFadeOutCurve", 0);
        FadeCurveType curveType = static_cast<FadeCurveType>(lastCurve);

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
            numSamples,
            curveType  // Now respects user preference
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
                           bool isSelection,
                           float targetDB = 0.0f)
            : m_bufferManager(bufferManager),
              m_waveformDisplay(waveform),
              m_audioEngine(audioEngine),
              m_beforeBuffer(),
              m_startSample(startSample),
              m_numSamples(numSamples),
              m_isSelection(isSelection),
              m_targetDB(targetDB)
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
            AudioProcessor::normalize(regionBuffer, m_targetDB);

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
        float m_targetDB;
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
                        int numSamples,
                        FadeCurveType curveType = FadeCurveType::LINEAR)
            : m_bufferManager(bufferManager),
              m_waveformDisplay(waveform),
              m_audioEngine(audioEngine),
              m_beforeBuffer(),
              m_startSample(startSample),
              m_numSamples(numSamples),
              m_curveType(curveType)
        {
            // Store only the affected region to save memory
            m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
            m_beforeBuffer.makeCopyOf(beforeBuffer, true);
        }

        void markAsAlreadyPerformed() { m_alreadyPerformed = true; }

        bool perform() override
        {
            if (m_alreadyPerformed)
            {
                m_alreadyPerformed = false;
                return true;
            }

            // Get the buffer and create a region buffer for fade in
            auto& buffer = m_bufferManager.getMutableBuffer();

            // Extract the region to fade
            juce::AudioBuffer<float> regionBuffer;
            regionBuffer.setSize(buffer.getNumChannels(), m_numSamples);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                regionBuffer.copyFrom(ch, 0, buffer, ch, m_startSample, m_numSamples);
            }

            // Apply fade in to the region with selected curve type
            AudioProcessor::fadeIn(regionBuffer, m_numSamples, m_curveType);

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
        FadeCurveType m_curveType;
        bool m_alreadyPerformed = false;
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
                         int numSamples,
                         FadeCurveType curveType = FadeCurveType::LINEAR)
            : m_bufferManager(bufferManager),
              m_waveformDisplay(waveform),
              m_audioEngine(audioEngine),
              m_beforeBuffer(),
              m_startSample(startSample),
              m_numSamples(numSamples),
              m_curveType(curveType)
        {
            // Store only the affected region to save memory
            m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
            m_beforeBuffer.makeCopyOf(beforeBuffer, true);
        }

        void markAsAlreadyPerformed() { m_alreadyPerformed = true; }

        bool perform() override
        {
            if (m_alreadyPerformed)
            {
                m_alreadyPerformed = false;
                return true;
            }

            // Get the buffer and create a region buffer for fade out
            auto& buffer = m_bufferManager.getMutableBuffer();

            // Extract the region to fade
            juce::AudioBuffer<float> regionBuffer;
            regionBuffer.setSize(buffer.getNumChannels(), m_numSamples);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                regionBuffer.copyFrom(ch, 0, buffer, ch, m_startSample, m_numSamples);
            }

            // Apply fade out to the region with selected curve type
            AudioProcessor::fadeOut(regionBuffer, m_numSamples, m_curveType);

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
        FadeCurveType m_curveType;
        bool m_alreadyPerformed = false;
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
     * Supports both selection-based and entire-file processing.
     */
    class DCOffsetRemovalUndoAction : public juce::UndoableAction
    {
    public:
        DCOffsetRemovalUndoAction(AudioBufferManager& bufferManager,
                                 WaveformDisplay& waveform,
                                 AudioEngine& audioEngine,
                                 const juce::AudioBuffer<float>& beforeBuffer,
                                 int startSample = 0,
                                 int numSamples = -1)
            : m_bufferManager(bufferManager),
              m_waveformDisplay(waveform),
              m_audioEngine(audioEngine),
              m_beforeBuffer(),
              m_startSample(startSample),
              m_numSamples(numSamples)
        {
            // Store the affected region
            m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
            m_beforeBuffer.makeCopyOf(beforeBuffer, true);
        }

        void markAsAlreadyPerformed() { m_alreadyPerformed = true; }

        bool perform() override
        {
            if (m_alreadyPerformed)
            {
                m_alreadyPerformed = false;
                return true;
            }

            auto& buffer = m_bufferManager.getMutableBuffer();

            // Extract the region to process
            juce::AudioBuffer<float> regionBuffer;
            int actualNumSamples = (m_numSamples < 0) ? buffer.getNumSamples() : m_numSamples;
            regionBuffer.setSize(buffer.getNumChannels(), actualNumSamples);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                regionBuffer.copyFrom(ch, 0, buffer, ch, m_startSample, actualNumSamples);
            }

            // Apply DC offset removal to the region
            bool success = AudioProcessor::removeDCOffset(regionBuffer);

            if (!success)
            {
                juce::Logger::writeToLog("DCOffsetRemovalUndoAction::perform - Failed to remove DC offset");
                return false;
            }

            // Copy the processed region back into the main buffer
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                buffer.copyFrom(ch, m_startSample, regionBuffer, ch, 0, actualNumSamples);
            }

            // Reload buffer in AudioEngine - preserve playback if active
            m_audioEngine.reloadBufferPreservingPlayback(
                buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

            // Update waveform display - preserve view and selection
            m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                              true, true); // preserveView=true, preserveEditCursor=true

            // Log the operation
            juce::String message = (m_numSamples < 0) ? "Removed DC offset from entire file"
                                                       : "Removed DC offset from selection";
            juce::Logger::writeToLog(message);

            return true;
        }

        bool undo() override
        {
            // Restore the affected region from before buffer
            auto& buffer = m_bufferManager.getMutableBuffer();

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                buffer.copyFrom(ch, m_startSample, m_beforeBuffer, ch, 0, m_beforeBuffer.getNumSamples());
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
        bool m_alreadyPerformed = false;
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

    //==============================================================================
    // Marker Undo Actions (Phase 3.4)

    /**
     * Undoable action for adding a marker.
     * Stores the marker data to enable undo/redo.
     */
    class AddMarkerUndoAction : public juce::UndoableAction
    {
    public:
        AddMarkerUndoAction(MarkerManager& markerManager,
                           MarkerDisplay* markerDisplay,
                           const Marker& marker)
            : m_markerManager(markerManager),
              m_markerDisplay(markerDisplay),
              m_marker(marker),
              m_markerIndex(-1)
        {
        }

        bool perform() override
        {
            // Add marker and store the index
            m_markerIndex = m_markerManager.addMarker(m_marker);

            // Update display
            if (m_markerDisplay)
                m_markerDisplay->repaint();

            juce::Logger::writeToLog("Added marker: " + m_marker.getName());
            return true;
        }

        bool undo() override
        {
            if (m_markerIndex < 0)
            {
                juce::Logger::writeToLog("AddMarkerUndoAction::undo - Invalid marker index");
                return false;
            }

            // Remove the marker
            m_markerManager.removeMarker(m_markerIndex);

            // Update display
            if (m_markerDisplay)
                m_markerDisplay->repaint();

            juce::Logger::writeToLog("Undid marker addition: " + m_marker.getName());
            return true;
        }

        int getSizeInUnits() override { return sizeof(*this) + sizeof(Marker); }

    private:
        MarkerManager& m_markerManager;
        MarkerDisplay* m_markerDisplay;
        Marker m_marker;
        int m_markerIndex;  // Index where marker was added
    };

    /**
     * Undoable action for deleting a marker.
     * Stores the deleted marker and its index to enable restoration.
     */
    class DeleteMarkerUndoAction : public juce::UndoableAction
    {
    public:
        DeleteMarkerUndoAction(MarkerManager& markerManager,
                              MarkerDisplay* markerDisplay,
                              int markerIndex,
                              const Marker& marker)
            : m_markerManager(markerManager),
              m_markerDisplay(markerDisplay),
              m_markerIndex(markerIndex),
              m_deletedMarker(marker)
        {
        }

        bool perform() override
        {
            // Delete the marker
            m_markerManager.removeMarker(m_markerIndex);

            // Update display
            if (m_markerDisplay)
                m_markerDisplay->repaint();

            juce::Logger::writeToLog("Deleted marker: " + m_deletedMarker.getName());
            return true;
        }

        bool undo() override
        {
            // Re-insert the marker at its original index
            m_markerManager.insertMarkerAt(m_markerIndex, m_deletedMarker);

            // Update display
            if (m_markerDisplay)
                m_markerDisplay->repaint();

            juce::Logger::writeToLog("Undid marker deletion: " + m_deletedMarker.getName());
            return true;
        }

        int getSizeInUnits() override { return sizeof(*this) + sizeof(Marker); }

    private:
        MarkerManager& m_markerManager;
        MarkerDisplay* m_markerDisplay;
        int m_markerIndex;
        Marker m_deletedMarker;
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
    KeymapManager m_keymapManager;  // Keyboard shortcut template system (Phase 3)
    ToolbarManager m_toolbarManager;  // Customizable toolbar template system
    std::unique_ptr<CustomizableToolbar> m_toolbar;  // Customizable toolbar component
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

    // Marker List Panel (Phase 3.5)
    MarkerListPanel* m_markerListPanel = nullptr;  // Window owns this, we just track it
    juce::DocumentWindow* m_markerListWindow = nullptr;

    // Spectrum Analyzer (Phase 4)
    SpectrumAnalyzer* m_spectrumAnalyzer = nullptr;  // Window owns this, we just track it
    CallbackDocumentWindow* m_spectrumAnalyzerWindow = nullptr;

    // Region clipboard (Phase 3.4) - for copy/paste region definitions
    std::vector<Region> m_regionClipboard;
    bool m_hasRegionClipboard = false;

    /**
     * Listener class for Plugin Chain Window events.
     * CRITICAL FIX: Uses pointers instead of references to prevent dangling reference crash
     * when document is closed while Plugin Chain window is still open.
     * This is defined as a nested class (not local) so it can be referenced by m_pluginChainListeners.
     */
    class ChainWindowListener : public PluginChainWindow::Listener,
                                  public juce::DeletedAtShutdown
    {
    public:
        ChainWindowListener(MainComponent* owner, PluginChain* chain, AudioEngine* engine,
                           juce::DocumentWindow* window)
            : m_owner(owner), m_chain(chain), m_audioEngine(engine), m_window(window) {}

        void pluginChainWindowEditPlugin(int index) override
        {
            if (m_chain == nullptr) return;
            auto* node = m_chain->getPlugin(index);
            if (node != nullptr)
            {
                PluginEditorWindow::showForNode(node, &m_owner->m_commandManager);
            }
        }

        void pluginChainWindowApplyToSelection(const PluginChainWindow::RenderOptions& options) override
        {
            if (m_chain == nullptr || m_audioEngine == nullptr) return;
            // Apply with the render options from the window
            m_owner->applyPluginChainToSelectionWithOptions(options.convertToStereo, options.includeTail, options.tailLengthSeconds);
        }

        void pluginChainWindowPluginAdded(const juce::PluginDescription& description) override
        {
            if (m_chain == nullptr || m_audioEngine == nullptr) return;
            int index = m_chain->addPlugin(description);
            if (index >= 0)
            {
                m_audioEngine->setPluginChainEnabled(true);
                DBG("Added plugin: " + description.name);
            }
            else
            {
                ErrorDialog::show("Plugin Error", "Failed to load plugin: " + description.name);
            }
        }

        void pluginChainWindowPluginRemoved(int index) override
        {
            if (m_chain == nullptr) return;
            m_chain->removePlugin(index);
        }

        void pluginChainWindowPluginMoved(int fromIndex, int toIndex) override
        {
            if (m_chain == nullptr) return;
            m_chain->movePlugin(fromIndex, toIndex);
        }

        void pluginChainWindowPluginBypassed(int index, bool bypassed) override
        {
            if (m_chain == nullptr) return;
            auto* node = m_chain->getPlugin(index);
            if (node != nullptr)
            {
                node->setBypassed(bypassed);
            }
        }

        void pluginChainWindowBypassAll(bool bypassed) override
        {
            if (m_chain == nullptr) return;
            m_chain->setAllBypassed(bypassed);
        }

        /** Called when the associated document is closed - invalidates pointers and closes window */
        void documentClosed()
        {
            m_chain = nullptr;
            m_audioEngine = nullptr;
            if (m_window != nullptr)
            {
                m_window->setVisible(false);
                delete m_window;
                m_window = nullptr;
            }
        }

        /** Check if this listener is for the given chain */
        bool isForChain(PluginChain* chain) const { return m_chain == chain; }

    private:
        MainComponent* m_owner;
        PluginChain* m_chain;
        AudioEngine* m_audioEngine;
        juce::DocumentWindow* m_window;
    };

    // Plugin chain window listeners - need to track these to notify when document closes
    juce::Array<ChainWindowListener*> m_pluginChainListeners;

    // Marker placement tracking (Phase 3.4) - reliable cursor position for marker placement
    double m_lastClickTimeInSeconds = 0.0;  // Last mouse click position on waveform
    bool m_hasLastClickPosition = false;    // True if user has clicked on waveform

    // Plugin scan progress tracking (Phase 5 - VST3 hosting)
    bool m_pluginScanInProgress = false;
    float m_pluginScanProgress = 0.0f;
    juce::String m_pluginScanCurrentPlugin;

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
            double fps = 30.0;  // Default FPS (future: user-configurable via settings)
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

    //==============================================================================
    // Marker Display Callbacks (Phase 3.4)

    /**
     * Wire up all marker display callbacks for a document.
     * Must be called when a document is added/loaded.
     */
    void setupMarkerCallbacks(Document* doc)
    {
        if (!doc)
            return;

        auto& markerDisplay = doc->getMarkerDisplay();

        // onMarkerClicked: Select marker and jump playback position
        markerDisplay.onMarkerClicked = [this, doc](int markerIndex)
        {
            if (!doc) return;

            const Marker* marker = doc->getMarkerManager().getMarker(markerIndex);
            if (!marker)
            {
                juce::Logger::writeToLog("Cannot select marker: Invalid marker index");
                return;
            }

            // Set playback position to marker
            doc->getAudioEngine().setPosition(marker->getPosition());

            // Select the marker
            doc->getMarkerManager().setSelectedMarkerIndex(markerIndex);

            juce::Logger::writeToLog(juce::String::formatted(
                "Jumped to marker '%s' at sample %lld",
                marker->getName().toRawUTF8(),
                marker->getPosition()
            ));

            // Repaint to show selection
            doc->getMarkerDisplay().repaint();
        };

        // onMarkerRenamed: Update marker name with undo support
        markerDisplay.onMarkerRenamed = [doc](int markerIndex, const juce::String& newName)
        {
            if (!doc) return;

            Marker* marker = doc->getMarkerManager().getMarker(markerIndex);
            if (!marker)
            {
                juce::Logger::writeToLog("Cannot rename marker: Invalid marker index");
                return;
            }

            // Update marker name directly (no undo for rename in Phase 1)
            marker->setName(newName);

            // Save to sidecar JSON file
            doc->getMarkerManager().saveToFile(doc->getFile());

            // Repaint to show new name
            doc->getMarkerDisplay().repaint();

            juce::Logger::writeToLog("Renamed marker to: " + newName);
        };

        // onMarkerColorChanged: Update marker color with undo support
        markerDisplay.onMarkerColorChanged = [doc](int markerIndex, const juce::Colour& newColor)
        {
            if (!doc) return;

            Marker* marker = doc->getMarkerManager().getMarker(markerIndex);
            if (!marker)
            {
                juce::Logger::writeToLog("Cannot change marker color: Invalid marker index");
                return;
            }

            // Update marker color directly (no undo for color change in Phase 1)
            marker->setColor(newColor);

            // Save to sidecar JSON file
            doc->getMarkerManager().saveToFile(doc->getFile());

            // Repaint to show new color
            doc->getMarkerDisplay().repaint();

            juce::Logger::writeToLog("Changed marker color");
        };

        // onMarkerDeleted: Remove marker from manager with undo support
        markerDisplay.onMarkerDeleted = [doc](int markerIndex)
        {
            if (!doc) return;

            Marker* marker = doc->getMarkerManager().getMarker(markerIndex);
            if (!marker)
            {
                juce::Logger::writeToLog("Cannot delete marker: Invalid marker index");
                return;
            }

            juce::String markerName = marker->getName();

            // Start a new transaction
            juce::String transactionName = "Delete Marker";
            doc->getUndoManager().beginNewTransaction(transactionName);

            // Create undo action (stores marker data before deletion)
            auto* undoAction = new DeleteMarkerUndoAction(
                doc->getMarkerManager(),
                &doc->getMarkerDisplay(),
                markerIndex,
                *marker
            );

            // perform() calls DeleteMarkerUndoAction::perform() which removes the marker
            doc->getUndoManager().perform(undoAction);

            juce::Logger::writeToLog("Deleted marker: " + markerName);
        };

        // onMarkerMoved: Update marker position with undo support
        markerDisplay.onMarkerMoved = [doc](int markerIndex, int64_t oldPos, int64_t newPos)
        {
            if (!doc) return;

            Marker* marker = doc->getMarkerManager().getMarker(markerIndex);
            if (!marker)
            {
                juce::Logger::writeToLog("Cannot move marker: Invalid marker index");
                return;
            }

            // Update marker position directly (no undo for move in Phase 1)
            marker->setPosition(newPos);

            // Re-sort markers (they must stay sorted by position)
            // Remove and re-add to maintain sort order
            Marker movedMarker = *marker;
            doc->getMarkerManager().removeMarker(markerIndex);
            int newIndex = doc->getMarkerManager().addMarker(movedMarker);

            // Update selection to new index
            doc->getMarkerManager().setSelectedMarkerIndex(newIndex);

            // Save to sidecar JSON file
            doc->getMarkerManager().saveToFile(doc->getFile());

            // Repaint to show new position
            doc->getMarkerDisplay().repaint();

            juce::Logger::writeToLog(juce::String::formatted(
                "Moved marker '%s' from sample %lld to %lld",
                movedMarker.getName().toRawUTF8(),
                oldPos,
                newPos
            ));
        };

        // onMarkerDoubleClicked: Show rename dialog
        markerDisplay.onMarkerDoubleClicked = [this, doc](int markerIndex)
        {
            if (!doc) return;

            Marker* marker = doc->getMarkerManager().getMarker(markerIndex);
            if (!marker)
            {
                juce::Logger::writeToLog("Cannot rename marker: Invalid marker index");
                return;
            }

            // Show alert window for rename
            juce::AlertWindow::showAsync(
                juce::MessageBoxOptions()
                    .withTitle("Rename Marker")
                    .withMessage("Enter new name for marker:")
                    .withButton("OK")
                    .withButton("Cancel")
                    .withIconType(juce::MessageBoxIconType::QuestionIcon)
                    .withAssociatedComponent(this),
                [doc, markerIndex, marker](int result)
                {
                    if (result == 1) // OK button
                    {
                        // User entered a name (handled by alert window)
                        // Note: This is a simplified version - full implementation would use TextEditor
                        juce::Logger::writeToLog("Marker rename dialog shown");
                    }
                });
        };

        // Track mouse clicks on waveform for reliable marker placement
        // Create a custom MouseListener to capture click positions
        class WaveformClickTracker : public juce::MouseListener
        {
        public:
            WaveformClickTracker(MainComponent* main, Document* document)
                : m_mainComponent(main), m_document(document) {}

            void mouseDown(const juce::MouseEvent& event) override
            {
                if (!m_document || !m_mainComponent)
                    return;

                // Get the WaveformDisplay component
                auto& waveform = m_document->getWaveformDisplay();

                // Ignore clicks on scrollbar or ruler (same logic as WaveformDisplay)
                if (event.y < 30 || event.y > waveform.getHeight() - 16)
                    return;

                // Convert click position to time
                int clampedX = juce::jlimit(0, waveform.getWidth() - 1, event.x);

                // Calculate time from X position (matches WaveformDisplay::xToTime logic)
                double clickTime = waveform.getVisibleRangeStart() +
                    (static_cast<double>(clampedX) / waveform.getWidth()) *
                    (waveform.getVisibleRangeEnd() - waveform.getVisibleRangeStart());

                // Store the click position
                m_mainComponent->m_lastClickTimeInSeconds = clickTime;
                m_mainComponent->m_hasLastClickPosition = true;

                juce::Logger::writeToLog(juce::String::formatted(
                    "Tracked waveform click at %.3fs", clickTime));
            }

        private:
            MainComponent* m_mainComponent;
            Document* m_document;
        };

        // Create and attach the mouse listener
        // Note: Memory managed by Component's MouseListener list - will be deleted when component is destroyed
        doc->getWaveformDisplay().addMouseListener(new WaveformClickTracker(this, doc), false);
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
    juce::AudioDeviceManager m_audioDeviceManager;
    std::unique_ptr<MainWindow> mainWindow;
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

    // Set up the application factory - required when using custom main()
    juce::JUCEApplicationBase::createInstance = &juce_CreateApplication;

    // Normal application launch
    return juce::JUCEApplicationBase::main(argc, const_cast<const char**>(argv));
}
#endif
