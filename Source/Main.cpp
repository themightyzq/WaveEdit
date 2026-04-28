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
#include "Audio/ChannelLayout.h"
#include "Commands/CommandIDs.h"
#include "Commands/CommandHandler.h"
#include "Commands/MenuBuilder.h"
#include "Utils/Settings.h"
#include "Utils/AudioClipboard.h"
#include "Utils/UndoableEdits.h"
#include "Utils/UndoActions/AudioUndoActions.h"
#include "Utils/UndoActions/RegionUndoActions.h"
#include "Utils/UndoActions/MarkerUndoActions.h"
#include "Controllers/FileController.h"
#include "Controllers/DSPController.h"
#include "Controllers/RegionController.h"
#include "Controllers/MarkerController.h"
#include "Controllers/ClipboardController.h"
#include "Controllers/ChainWindowListener.h"
#include "UI/WaveformDisplay.h"
#include "UI/TransportControls.h"
#include "UI/Meters.h"
#include "UI/GainDialog.h"
#include "UI/NewFileDialog.h"
#include "UI/NormalizeDialog.h"
#include "UI/FadeDialog.h"
// DCOffsetDialog removed - DC Offset now runs automatically without dialog
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
#include "Batch/BatchProcessorDialog.h"
#include "UI/ChannelConverterDialog.h"
#include "UI/ChannelExtractorDialog.h"
#include "UI/WaveEditLookAndFeel.h"
#include "UI/CommandPalette.h"

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
          m_keymapManager(m_commandManager),
          m_fileController(m_documentManager, m_fileManager, m_commandManager),
          m_clipboardController(m_documentManager)
    {
        setSize(1200, 750);

        // Wire up FileController callbacks
        m_fileController.setOnUIRefreshNeeded([this]() { repaint(); });
        m_fileController.setSaveAsParent(this);

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

        // Wire CompactTransport record button to command system
        if (auto* transport = m_toolbar->getCompactTransport())
        {
            transport->onRecordRequested = [this]()
            {
                m_commandManager.invokeDirectly(CommandIDs::playbackRecord, true);
            };
        }

        // Initialize command palette overlay
        m_commandPalette = std::make_unique<CommandPalette>(m_commandManager);
        addChildComponent(m_commandPalette.get());  // Hidden by default

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

            m_spectrumAnalyzerWindow.reset();  // SafePointer auto-nulls m_spectrumAnalyzer
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
            m_regionListWindow.reset();  // SafePointer auto-nulls m_regionListPanel
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

        // Wire up channel solo/mute callbacks for this document
        setupSoloMuteCallbacks(document);

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
            m_regionController.handleRegionListJumpToRegion(doc, regionIndex);
        }
    }

    void regionListPanelRegionDeleted(int regionIndex) override
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            m_regionController.handleRegionListRegionDeleted(doc);
        }
    }

    void regionListPanelRegionRenamed(int regionIndex, const juce::String& newName) override
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            m_regionController.handleRegionListRegionRenamed(doc);
        }
    }

    void regionListPanelRegionSelected(int regionIndex) override
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            m_regionController.handleRegionListRegionSelected(doc, regionIndex);
        }
    }

    void regionListPanelBatchRename(const std::vector<int>& regionIndices) override
    {
        if (!m_regionListPanel)
            return;

        // Expand the batch rename section (if collapsed)
        m_regionListPanel->expandBatchRenameSection(true);

        // Delegate to controller to handle any additional logic
        m_regionController.handleRegionListBatchRenameStart();
    }

    void regionListPanelBatchRenameApply(const std::vector<int>& regionIndices,
                                          const std::vector<juce::String>& newNames) override
    {
        auto* doc = m_documentManager.getCurrentDocument();
        if (!doc)
            return;

        m_regionController.handleRegionListBatchRenameApply(doc, regionIndices, newNames);

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
            m_markerController.handleMarkerListJumpToMarker(doc, markerIndex);
            repaint();
        }
    }

    void markerListPanelMarkerDeleted(int markerIndex) override
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            m_markerController.handleMarkerListMarkerDeleted(doc);
        }
    }

    void markerListPanelMarkerRenamed(int markerIndex, const juce::String& newName) override
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            m_markerController.handleMarkerListMarkerRenamed(doc);
        }
    }

    void markerListPanelMarkerSelected(int markerIndex) override
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            m_markerController.handleMarkerListMarkerSelected(doc, markerIndex);
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
            m_currentDocumentContainer->addAndMakeVisible(doc->getMarkerDisplay());
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
        m_clipboardController.selectAll(getCurrentDocument());
    }

    bool validateSelection() const
    {
        return m_clipboardController.validateSelection(getCurrentDocument());
    }

    void copySelection()
    {
        m_clipboardController.copySelection(getCurrentDocument());
        repaint(); // Update status bar to show clipboard info
    }

    void cutSelection()
    {
        m_clipboardController.cutSelection(getCurrentDocument());
    }

    void pasteAtCursor()
    {
        m_clipboardController.pasteAtCursor(getCurrentDocument());
        repaint();
    }

    void deleteSelection()
    {
        m_clipboardController.deleteSelection(getCurrentDocument());
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

        // Command palette overlay (always positioned at top-center)
        if (m_commandPalette)
        {
            int paletteX = (getWidth() - CommandPalette::kWidth) / 2;
            m_commandPalette->setBounds(paletteX, 8,
                CommandPalette::kWidth, CommandPalette::kMaxHeight);
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

        // Update preview position indicator (Phase 6 finalization)
        // Shows a distinct ORANGE cursor during DSP preview mode in processing dialogs
        auto previewMode = doc->getAudioEngine().getPreviewMode();
        if (previewMode != PreviewMode::DISABLED && doc->getAudioEngine().isPlaying())
        {
            // Preview is active - show the preview position cursor
            double previewPos = doc->getAudioEngine().getCurrentPosition();
            doc->getWaveformDisplay().setPreviewPosition(previewPos);
        }
        else
        {
            // Preview not active - clear the preview cursor
            doc->getWaveformDisplay().clearPreviewPosition();
        }

        // Auto-save check (Phase 3.5 - Priority #6)
        // Check auto-save every ~60 seconds (1200 ticks at 50ms each)
        m_autoSaveTimerTicks++;
        if (m_autoSaveTimerTicks >= m_autoSaveCheckInterval)
        {
            m_autoSaveTimerTicks = 0;
            m_fileController.performAutoSave();
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
        m_commandHandler.getAllCommands(commands);
    }

    void getCommandInfo(juce::CommandID commandID, juce::ApplicationCommandInfo& result) override
    {
        CommandContext context;
        context.currentDoc = getCurrentDocument();
        context.documentManager = &m_documentManager;
        context.keymapManager = &m_keymapManager;
        context.hasRegionClipboard = m_regionController.hasRegionClipboard();
        context.autoPreviewRegions = Settings::getInstance().getAutoPreviewRegions();
        context.regionsVisible = Settings::getInstance().getRegionsVisible();
        context.spectrumAnalyzer = m_spectrumAnalyzer;
        context.spectrumWindow = m_spectrumAnalyzerWindow.get();
        context.canMergeRegions = [this](Document* doc) { return canMergeRegions(doc); };
        context.canSplitRegion = [this](Document* doc) { return canSplitRegion(doc); };
        m_commandHandler.getCommandInfo(commandID, result, context);
    }


    bool perform(const InvocationInfo& info) override
    {
        auto* doc = getCurrentDocument();

        // CRITICAL FIX: Don't early return - allow document-independent commands
        // Commands like File → Open and File → Exit must work without a document

        switch (info.commandID)
        {
            case CommandIDs::fileNew:
                handleNewFileCommand();
                return true;

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
                    DBG("BWF metadata updated - document marked as modified");
                });
                return true;

            case CommandIDs::fileEditiXMLMetadata:
                if (!doc) return false;
                iXMLEditorDialog::showDialog(this, doc->getiXMLMetadata(),
                                            doc->getFilename(), [this, doc]()
                {
                    // Mark document as modified when iXML metadata changes
                    doc->setModified(true);
                    DBG("iXML metadata updated - document marked as modified");
                });
                return true;

            case CommandIDs::editUndo:
                if (!doc) return false;
                if (doc->getUndoManager().canUndo())
                {
                    DBG(juce::String::formatted(
                        "Undo: %s (stack depth before: %d)",
                        doc->getUndoManager().getUndoDescription().toRawUTF8(),
                        doc->getUndoManager().getNumberOfUnitsTakenUpByStoredCommands()));

                    doc->getUndoManager().undo();
                    doc->setModified(true);

                    DBG(juce::String::formatted(
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
                    DBG(juce::String::formatted(
                        "Redo: %s (stack depth before: %d)",
                        doc->getUndoManager().getRedoDescription().toRawUTF8(),
                        doc->getUndoManager().getNumberOfUnitsTakenUpByStoredCommands()));

                    doc->getUndoManager().redo();
                    doc->setModified(true);

                    DBG(juce::String::formatted(
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
                if (!doc) return false;
                handleLoopRegion(doc);
                return true;

            case CommandIDs::playbackRecord:
                handleRecordCommand();
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
                toggleSpectrumAnalyzer();
                return true;

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
                DBG("selectExtendPageLeft command triggered");
                doc->getWaveformDisplay().navigatePageLeft(true);
                return true;

            case CommandIDs::selectExtendPageRight:
                if (!doc) return false;
                DBG("selectExtendPageRight command triggered");
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
                m_regionController.addRegionFromSelection(doc);
                return true;

            case CommandIDs::regionDelete:
                if (!doc) return false;
                m_regionController.deleteSelectedRegion(doc);
                return true;

            case CommandIDs::regionNext:
                if (!doc) return false;
                m_regionController.jumpToNextRegion(doc);
                return true;

            case CommandIDs::regionPrevious:
                if (!doc) return false;
                m_regionController.jumpToPreviousRegion(doc);
                return true;

            case CommandIDs::regionSelectInverse:
                if (!doc) return false;
                m_regionController.selectInverseOfRegions(doc);
                return true;

            case CommandIDs::regionSelectAll:
                if (!doc) return false;
                m_regionController.selectAllRegions(doc);
                return true;

            case CommandIDs::regionStripSilence:
                if (!doc) return false;
                m_regionController.showStripSilenceDialog(doc, this);
                return true;

            case CommandIDs::regionExportAll:
                if (!doc) return false;
                m_regionController.showBatchExportDialog(doc, this);
                return true;

            case CommandIDs::regionBatchRename:
                if (!doc) return false;
                handleRegionBatchRename();
                return true;

            // Region editing commands (Phase 3.4)
            case CommandIDs::regionMerge:
                if (!doc) return false;
                m_regionController.mergeSelectedRegions(doc);
                return true;

            case CommandIDs::regionSplit:
                if (!doc) return false;
                m_regionController.splitRegionAtCursor(doc);
                return true;

            case CommandIDs::regionCopy:
                if (!doc) return false;
                m_regionController.copyRegionsToClipboard(doc);
                return true;

            case CommandIDs::regionPaste:
                if (!doc) return false;
                m_regionController.pasteRegionsFromClipboard(doc);
                return true;

            case CommandIDs::regionShowList:
                toggleRegionListPanel();
                return true;

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
                m_regionController.nudgeRegionBoundary(doc, true, true);
                return true;

            case CommandIDs::regionNudgeStartRight:
                if (!doc) return false;
                m_regionController.nudgeRegionBoundary(doc, true, false);
                return true;

            case CommandIDs::regionNudgeEndLeft:
                if (!doc) return false;
                m_regionController.nudgeRegionBoundary(doc, false, true);
                return true;

            case CommandIDs::regionNudgeEndRight:
                if (!doc) return false;
                m_regionController.nudgeRegionBoundary(doc, false, false);
                return true;

            // Marker operations (Phase 3.4)
            case CommandIDs::markerAdd:
                if (!doc) return false;
                m_markerController.addMarkerAtCursor(doc, m_lastClickTimeInSeconds, m_hasLastClickPosition);
                return true;

            case CommandIDs::markerDelete:
                if (!doc) return false;
                m_markerController.deleteSelectedMarker(doc);
                return true;

            case CommandIDs::markerNext:
                if (!doc) return false;
                m_markerController.jumpToNextMarker(doc);
                return true;

            case CommandIDs::markerPrevious:
                if (!doc) return false;
                m_markerController.jumpToPreviousMarker(doc);
                return true;

            case CommandIDs::markerShowList:
                toggleMarkerListPanel();
                return true;

            case CommandIDs::markerConvertToRegions:
                if (!doc) return false;
                m_markerController.convertMarkersToRegions(doc);
                return true;

            case CommandIDs::regionConvertToMarkers:
                if (!doc) return false;
                m_markerController.convertRegionsToMarkers(doc);
                return true;

            // Processing operations
            case CommandIDs::processGain:
                if (!doc) return false;
                m_dspController.showGainDialog(doc, this);
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
                m_dspController.showParametricEQDialog(doc, this);
                return true;

            case CommandIDs::processGraphicalEQ:
                if (!doc) return false;
                m_dspController.showGraphicalEQDialog(doc, this);
                return true;

            case CommandIDs::toolsChannelConverter:
                if (!doc) return false;
                m_dspController.showChannelConverterDialog(doc, this);
                return true;

            case CommandIDs::toolsChannelExtractor:
                if (!doc) return false;
                m_dspController.showChannelExtractorDialog(doc, this);
                return true;

            case CommandIDs::toolsHeadTail:
                if (!doc) return false;
                m_dspController.showHeadTailDialog(doc, this);
                return true;

            case CommandIDs::toolsLoopingTools:
                if (!doc) return false;
                m_dspController.showLoopingToolsDialog(doc, this);
                return true;

            case CommandIDs::processNormalize:
                if (!doc) return false;
                m_dspController.showNormalizeDialog(doc, this);
                return true;

            case CommandIDs::processFadeIn:
                if (!doc) return false;
                m_dspController.showFadeInDialog(doc, this);
                return true;

            case CommandIDs::processFadeOut:
                if (!doc) return false;
                m_dspController.showFadeOutDialog(doc, this);
                return true;

            case CommandIDs::processDCOffset:
                if (!doc) return false;
                applyDCOffset();  // Run automatically without dialog
                return true;

            case CommandIDs::processReverse:
                if (!doc) return false;
                m_dspController.reverseSelection(doc);
                return true;

            case CommandIDs::processInvert:
                if (!doc) return false;
                m_dspController.invertSelection(doc);
                return true;

            case CommandIDs::processResample:
                if (!doc) return false;
                m_dspController.showResampleDialog(doc, this);
                return true;

            case CommandIDs::editSilence:
                if (!doc) return false;
                m_dspController.silenceSelection(doc);
                return true;

            case CommandIDs::editTrim:
                if (!doc) return false;
                m_dspController.trimToSelection(doc);
                return true;

            // Help commands
            case CommandIDs::helpAbout:
                showAboutDialog();
                return true;

            case CommandIDs::helpShortcuts:
                showKeyboardShortcutsDialog();
                return true;

            case CommandIDs::helpCommandPalette:
                if (m_commandPalette)
                    m_commandPalette->show();
                return true;

            //==============================================================================
            // Plugin commands (VST3/AU plugin chain)
            case CommandIDs::pluginShowChain:
                showPluginChainPanel();
                return true;

            case CommandIDs::pluginApplyChain:
                if (!doc) return false;
                m_dspController.applyPluginChainToSelection(doc);
                return true;

            case CommandIDs::pluginOffline:
                if (!doc) return false;
                m_dspController.showOfflinePluginDialog(doc, this);
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

            case CommandIDs::fileBatchProcessor:
            {
                // Show batch processor dialog
                waveedit::BatchProcessorDialog::showDialog();
                return true;
            }

            default:
                return false;
        }
    }

    //==============================================================================
    // Command Handler Private Helpers (extracted from perform() for clarity)

    /**
     * Handle File → New command: show dialog and create new document with user settings.
     */
    void handleNewFileCommand()
    {
        auto settings = NewFileDialog::showDialog();
        if (settings.has_value())
        {
            auto* newDoc = m_documentManager.createDocument();
            if (newDoc != nullptr)
            {
                int64_t numSamples = static_cast<int64_t>(settings->durationSeconds * settings->sampleRate);
                juce::AudioBuffer<float> emptyBuffer(settings->numChannels, static_cast<int>(numSamples));
                emptyBuffer.clear();

                auto& buffer = newDoc->getBufferManager().getMutableBuffer();
                buffer.setSize(settings->numChannels, static_cast<int>(numSamples));
                buffer.clear();

                newDoc->getAudioEngine().loadFromBuffer(emptyBuffer, settings->sampleRate, settings->numChannels);
                newDoc->getWaveformDisplay().reloadFromBuffer(emptyBuffer, settings->sampleRate, false, false);

                newDoc->getRegionDisplay().setSampleRate(settings->sampleRate);
                newDoc->getRegionDisplay().setTotalDuration(settings->durationSeconds);
                newDoc->getRegionDisplay().setVisibleRange(0.0, settings->durationSeconds);
                newDoc->getRegionDisplay().setAudioBuffer(&buffer);

                newDoc->getMarkerDisplay().setSampleRate(settings->sampleRate);
                newDoc->getMarkerDisplay().setTotalDuration(settings->durationSeconds);

                newDoc->setModified(true);
                m_documentManager.setCurrentDocument(newDoc);

                DBG("Created new audio file: " +
                    juce::String(numSamples) + " samples, " +
                    juce::String(settings->sampleRate) + " Hz, " +
                    juce::String(settings->numChannels) + " channels");
            }
        }
    }

    /**
     * Handle Playback → Record command: show recording dialog and handle recorded audio.
     * Supports both append-to-existing and create-new-document modes.
     */
    void handleRecordCommand()
    {
        auto* currentDoc = m_documentManager.getCurrentDocument();
        bool appendToExisting = false;

        if (currentDoc != nullptr)
        {
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

            if (choice == 0) return;
            appendToExisting = (choice == 1);
        }

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
                    appendToDocument(m_targetDocument, audioBuffer, sampleRate, numChannels);
                else
                    createNewDocument(audioBuffer, sampleRate, numChannels);
            }

        private:
            void appendToDocument(Document* targetDoc, const juce::AudioBuffer<float>& audioBuffer,
                                 double sampleRate, int numChannels)
            {
                double cursorPositionSeconds = targetDoc->getWaveformDisplay().getPlaybackPosition();
                auto& currentBuffer = targetDoc->getBufferManager().getMutableBuffer();
                double currentSampleRate = targetDoc->getAudioEngine().getSampleRate();
                int insertPositionSamples = static_cast<int>(cursorPositionSeconds * currentSampleRate);
                insertPositionSamples = juce::jlimit(0, currentBuffer.getNumSamples(), insertPositionSamples);

                int currentSamples = currentBuffer.getNumSamples();
                int newSamples = audioBuffer.getNumSamples();
                int totalSamples = currentSamples + newSamples;

                juce::AudioBuffer<float> combinedBuffer(
                    juce::jmax(currentBuffer.getNumChannels(), audioBuffer.getNumChannels()),
                    totalSamples
                );

                for (int ch = 0; ch < currentBuffer.getNumChannels(); ++ch)
                    combinedBuffer.copyFrom(ch, 0, currentBuffer, ch, 0, insertPositionSamples);

                for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
                    combinedBuffer.copyFrom(ch, insertPositionSamples, audioBuffer, ch, 0, newSamples);

                int remainingSamples = currentSamples - insertPositionSamples;
                if (remainingSamples > 0)
                {
                    for (int ch = 0; ch < currentBuffer.getNumChannels(); ++ch)
                    {
                        combinedBuffer.copyFrom(ch, insertPositionSamples + newSamples,
                                               currentBuffer, ch, insertPositionSamples, remainingSamples);
                    }
                }

                currentBuffer.makeCopyOf(combinedBuffer, true);
                targetDoc->getAudioEngine().loadFromBuffer(combinedBuffer, sampleRate,
                                                           combinedBuffer.getNumChannels());
                targetDoc->getWaveformDisplay().reloadFromBuffer(combinedBuffer, sampleRate, false, false);
                targetDoc->getRegionDisplay().setTotalDuration(totalSamples / sampleRate);
                targetDoc->getMarkerDisplay().setTotalDuration(totalSamples / sampleRate);
                targetDoc->setModified(true);

                DBG("Recording inserted at cursor position (" +
                    juce::String(cursorPositionSeconds, 3) + "s): " +
                    juce::String(newSamples) + " samples added");
            }

            void createNewDocument(const juce::AudioBuffer<float>& audioBuffer,
                                  double sampleRate, int numChannels)
            {
                auto* newDoc = m_documentManager->createDocument();
                if (newDoc != nullptr)
                {
                    auto& buffer = newDoc->getBufferManager().getMutableBuffer();
                    buffer.setSize(audioBuffer.getNumChannels(), audioBuffer.getNumSamples());
                    buffer.makeCopyOf(audioBuffer, true);

                    newDoc->getAudioEngine().loadFromBuffer(audioBuffer, sampleRate, numChannels);
                    newDoc->getWaveformDisplay().reloadFromBuffer(audioBuffer, sampleRate, false, false);

                    newDoc->getRegionDisplay().setSampleRate(sampleRate);
                    newDoc->getRegionDisplay().setTotalDuration(audioBuffer.getNumSamples() / sampleRate);
                    newDoc->getRegionDisplay().setVisibleRange(0.0, audioBuffer.getNumSamples() / sampleRate);
                    newDoc->getRegionDisplay().setAudioBuffer(&buffer);

                    newDoc->getMarkerDisplay().setSampleRate(sampleRate);
                    newDoc->getMarkerDisplay().setTotalDuration(audioBuffer.getNumSamples() / sampleRate);

                    newDoc->setModified(true);

                    DBG("Recording completed: " +
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

        RecordingDialog::showDialog(this, m_audioDeviceManager,
                                   new RecordingListener(&m_documentManager, currentDoc, appendToExisting));
    }

    /**
     * Handle View → Spectrum Analyzer toggle: create window on first use, then toggle visibility.
     */
    void toggleSpectrumAnalyzer()
    {
        if (!m_spectrumAnalyzer)
        {
            auto* analyzer = new SpectrumAnalyzer();
            m_spectrumAnalyzer = analyzer;

            m_spectrumAnalyzerWindow = std::make_unique<CallbackDocumentWindow>("Spectrum Analyzer",
                                                                      juce::Colour(0xff2a2a2a),
                                                                      juce::DocumentWindow::allButtons,
                                                                      [this]()
            {
                if (m_spectrumAnalyzer)
                {
                    if (auto* doc = getCurrentDocument())
                        doc->getAudioEngine().setSpectrumAnalyzer(nullptr);
                }
                m_commandManager.commandStatusChanged();
            });

            m_spectrumAnalyzerWindow->setContentOwned(analyzer, false);
            m_spectrumAnalyzerWindow->setResizable(true, true);
            m_spectrumAnalyzerWindow->setSize(600, 400);
            m_spectrumAnalyzerWindow->setAlwaysOnTop(true);
            m_spectrumAnalyzerWindow->centreWithSize(600, 400);
            m_spectrumAnalyzerWindow->setUsingNativeTitleBar(true);

            if (auto* doc = getCurrentDocument())
                doc->getAudioEngine().setSpectrumAnalyzer(m_spectrumAnalyzer);
        }

        if (m_spectrumAnalyzerWindow)
        {
            bool isVisible = m_spectrumAnalyzerWindow->isVisible();
            m_spectrumAnalyzerWindow->setVisible(!isVisible);

            if (!isVisible && m_spectrumAnalyzer)
            {
                if (auto* doc = getCurrentDocument())
                    doc->getAudioEngine().setSpectrumAnalyzer(m_spectrumAnalyzer);
            }
            else if (isVisible && m_spectrumAnalyzer)
            {
                if (auto* doc = getCurrentDocument())
                    doc->getAudioEngine().setSpectrumAnalyzer(nullptr);
            }
        }
    }

    /**
     * Handle Playback → Loop Region: set selection to region bounds and start looping playback.
     */
    void handleLoopRegion(Document* doc)
    {
        auto& regionMgr = doc->getRegionManager();
        int selectedIndex = regionMgr.getSelectedRegionIndex();

        if (selectedIndex < 0 || selectedIndex >= regionMgr.getNumRegions())
        {
            DBG("No region selected for loop playback");
            return;
        }

        auto* region = regionMgr.getRegion(selectedIndex);
        if (!region)
        {
            DBG("Invalid region for loop playback");
            return;
        }

        auto& audioEngine = doc->getAudioEngine();
        double sampleRate = audioEngine.getSampleRate();
        double startTime = region->getStartSample() / sampleRate;
        double endTime = region->getEndSample() / sampleRate;

        doc->getWaveformDisplay().setSelection(region->getStartSample(), region->getEndSample());
        audioEngine.setLooping(true);
        audioEngine.setPosition(startTime);
        audioEngine.play();

        DBG("Loop region: " + region->getName() +
            " (" + juce::String(startTime, 3) + "s - " +
            juce::String(endTime, 3) + "s)");
    }

    /**
     * Handle Region → Show List: create/show region list panel in window.
     */
    void toggleRegionListPanel()
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
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
                m_regionListPanel->setSampleRate(doc->getBufferManager().getSampleRate());
            }

            if (!m_regionListWindow || !m_regionListWindow->isVisible())
            {
                m_regionListWindow.reset(m_regionListPanel->showInWindow(false));
            }
            else
            {
                m_regionListWindow->toFront(true);
            }
        }
    }

    /**
     * Handle Marker → Show List: create/show marker list panel in window.
     */
    void toggleMarkerListPanel()
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            if (!m_markerListPanel)
            {
                m_markerListPanel = new MarkerListPanel(
                    &doc->getMarkerManager(),
                    doc->getBufferManager().getSampleRate()
                );
                m_markerListPanel->setListener(this);
                m_markerListPanel->setCommandManager(&m_commandManager);
                m_markerListWindow.reset(m_markerListPanel->showInWindow(false));
            }
            else if (m_markerListWindow)
            {
                bool isVisible = m_markerListWindow->isVisible();
                m_markerListWindow->setVisible(!isVisible);
                if (!isVisible)
                    m_markerListPanel->refresh();
            }
        }
    }

    /**
     * Handle Region → Batch Rename: show region list panel with batch rename section expanded.
     */
    void handleRegionBatchRename()
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
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
                m_regionListPanel->setSampleRate(doc->getBufferManager().getSampleRate());
            }

            if (!m_regionListWindow || !m_regionListWindow->isVisible())
            {
                m_regionListWindow.reset(m_regionListPanel->showInWindow(false));
            }
            else
            {
                m_regionListWindow->toFront(true);
            }

            m_regionListPanel->expandBatchRenameSection(true);
        }
    }

    //==============================================================================
    // MenuBarModel Implementation

    juce::StringArray getMenuBarNames() override
    {
        return m_menuBuilder.getMenuBarNames();
    }

    
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& menuName) override
    {
        MenuContext context;
        context.commandManager = &m_commandManager;
        context.spectrumAnalyzer = m_spectrumAnalyzer;
        context.onLoadFile = [this](const juce::File& file) { loadFile(file); };
        context.onShowAbout = [this]() { showAbout(); };
        return m_menuBuilder.getMenuForIndex(menuIndex, menuName, context);
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
        m_fileController.handleFileDrop(files, this);
    }

    //==============================================================================
    // File Operations (delegated to FileController)

    void openFile() { m_fileController.openFile(this); }
    void loadFile(const juce::File& file) { m_fileController.loadFile(file, this); }
    void saveFile() { m_fileController.saveFile(getCurrentDocument()); }
    void saveFileAs() { m_fileController.saveFileAs(getCurrentDocument(), this); }
    void closeFile() { m_fileController.closeFile(getCurrentDocument(), this); }
    bool hasUnsavedChanges() const { return m_fileController.hasUnsavedChanges(); }
    bool saveAllModifiedDocuments() { return m_fileController.saveAllModifiedDocuments(); }
    bool confirmDiscardChanges() { return m_fileController.confirmDiscardChanges(); }

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

        DBG(juce::String("Loop mode: ") + (loopEnabled ? "ON" : "OFF"));
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
            DBG(message);
        }
        else
        {
            DBG("Snap: OFF");
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
        DBG(message);
    }

    //==============================================================================
    // Region editing helper methods (Phase 3.4)

    /**
     * Checks if the selected region can be merged with the next region.
     */
    bool canMergeRegions(Document* doc) const
    {
        return m_regionController.canMergeRegions(doc);
    }

    /**
     * Checks if the region under cursor can be split.
     */
    bool canSplitRegion(Document* doc) const
    {
        return m_regionController.canSplitRegion(doc);
    }

    //==============================================================================
    // DSP Application Methods - Delegation to DSPController

    /**
     * Apply gain adjustment - delegates to DSPController
     */
    void applyGainAdjustment(float gainDB, int64_t startSampleParam = -1, int64_t endSampleParam = -1)
    {
        m_dspController.applyGainAdjustment(getCurrentDocument(), gainDB, startSampleParam, endSampleParam);
    }

    /**
     * Apply DC offset removal - delegates to DSPController
     */
    void applyDCOffset()
    {
        m_dspController.applyDCOffset(getCurrentDocument());
    }

    /**
     * Apply plugin chain to selection with options - delegates to DSPController
     * (kept for PluginChainWindow callback)
     */
    void applyPluginChainToSelectionWithOptions(bool convertToStereo, bool includeTail, double tailLengthSeconds)
    {
        m_dspController.applyPluginChainToSelectionWithOptions(getCurrentDocument(), convertToStereo, includeTail, tailLengthSeconds);
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

        // Create and set listener with callback for plugin chain application
        auto* listener = new ChainWindowListener(
            m_commandManager,
            &chain,
            &doc->getAudioEngine(),
            window,
            [this](bool stereo, bool tail, double len)
            {
                m_dspController.applyPluginChainToSelectionWithOptions(
                    getCurrentDocument(), stereo, tail, len);
            });
        chainWindow->setListener(listener);

        // Track the listener so we can notify it when document is closed
        m_pluginChainListeners.add(listener);
    }


    // Undo action classes moved to Utils/UndoActions/ (AudioUndoActions.h, RegionUndoActions.h, MarkerUndoActions.h)

private:
    // Audio device manager (shared across all documents)
    juce::AudioDeviceManager& m_audioDeviceManager;

    // Multi-document management
    DocumentManager m_documentManager;
    TabComponent m_tabComponent;
    Document* m_previousDocument = nullptr;  // Track previous document for cleanup during switching

    // Shared resources
    AudioFileManager m_fileManager;
    juce::ApplicationCommandManager m_commandManager;
    KeymapManager m_keymapManager;
    ToolbarManager m_toolbarManager;
    std::unique_ptr<CustomizableToolbar> m_toolbar;

    // Command and menu routing
    CommandHandler m_commandHandler;
    MenuBuilder m_menuBuilder;

    // Controllers
    FileController m_fileController;
    DSPController m_dspController;
    RegionController m_regionController;
    MarkerController m_markerController;
    ClipboardController m_clipboardController;

    // UI preferences
    AudioUnits::TimeFormat m_timeFormat = AudioUnits::TimeFormat::Seconds;
    juce::Rectangle<int> m_formatIndicatorBounds;

    // Auto-save tracking (timer counter — thread pool is in FileController)
    int m_autoSaveTimerTicks = 0;
    const int m_autoSaveCheckInterval = 1200;

    // Command Palette overlay
    std::unique_ptr<CommandPalette> m_commandPalette;

    // "No file open" label
    juce::Label m_noFileLabel;

    // Current document display containers (for showing active document's components)
    std::unique_ptr<juce::Component> m_currentDocumentContainer;

    // Region List Panel (Phase 3.4)
    juce::Component::SafePointer<RegionListPanel> m_regionListPanel;  // Auto-nulls when window destroys it
    std::unique_ptr<juce::DocumentWindow> m_regionListWindow;

    // Marker List Panel (Phase 3.5)
    juce::Component::SafePointer<MarkerListPanel> m_markerListPanel;  // Auto-nulls when window destroys it
    std::unique_ptr<juce::DocumentWindow> m_markerListWindow;

    // Spectrum Analyzer (Phase 4)
    juce::Component::SafePointer<SpectrumAnalyzer> m_spectrumAnalyzer;  // Auto-nulls when window destroys it
    std::unique_ptr<CallbackDocumentWindow> m_spectrumAnalyzerWindow;

    /**
     * Listener class for Plugin Chain Window events.
     * CRITICAL FIX: Uses pointers instead of references to prevent dangling reference crash
     * when document is closed while Plugin Chain window is still open.
     * This is defined as a nested class (not local) so it can be referenced by m_pluginChainListeners.
     * Owned by m_pluginChainListeners (OwnedArray), so no need for DeletedAtShutdown.
     */
    // Plugin chain window listeners - need to track these to notify when document closes
    // OwnedArray handles cleanup of listeners when they are removed
    juce::OwnedArray<ChainWindowListener> m_pluginChainListeners;

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

    /**
     * Wires up region callbacks for a document.
     * This connects RegionDisplay user interactions to RegionManager state updates.
     */
    void setupRegionCallbacks(Document* doc)
    {
        // Delegate to RegionController
        m_regionController.setupRegionCallbacks(doc);
    }

    //==============================================================================
    // Marker Display Callbacks (Phase 3.4)

    /**
     * Wire up all marker display callbacks for a document.
     * Must be called when a document is added/loaded.
     */
    void setupMarkerCallbacks(Document* doc)
    {
        // Delegate to MarkerController
        m_markerController.setupMarkerCallbacks(doc);
    }

    /**
     * Sets up channel solo/mute callbacks for a document.
     * Wires WaveformDisplay callbacks to AudioEngine solo/mute state.
     */
    void setupSoloMuteCallbacks(Document* doc)
    {
        if (!doc)
            return;

        auto& waveform = doc->getWaveformDisplay();
        auto& engine = doc->getAudioEngine();

        // onChannelSoloChanged: Toggle solo state for a channel
        waveform.onChannelSoloChanged = [&engine](int channel, bool solo)
        {
            engine.setChannelSolo(channel, solo);
            DBG(juce::String::formatted(
                "Channel %d solo: %s", channel + 1, solo ? "ON" : "OFF"));
        };

        // onChannelMuteChanged: Toggle mute state for a channel
        waveform.onChannelMuteChanged = [&engine](int channel, bool mute)
        {
            engine.setChannelMute(channel, mute);
            DBG(juce::String::formatted(
                "Channel %d mute: %s", channel + 1, mute ? "ON" : "OFF"));
        };

        // getChannelSolo: Query solo state from engine
        waveform.getChannelSolo = [&engine](int channel) -> bool
        {
            return engine.isChannelSolo(channel);
        };

        // getChannelMute: Query mute state from engine
        waveform.getChannelMute = [&engine](int channel) -> bool
        {
            return engine.isChannelMute(channel);
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

    // AutoSaveJob, performAutoSave(), cleanupOldAutoSaves() → FileController

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
