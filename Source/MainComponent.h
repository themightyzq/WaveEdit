/*
  ==============================================================================

    MainComponent.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Holds the application's top-level Component class plus its companion
    SelectionInfoPanel and CallbackDocumentWindow helpers. Extracted from
    Main.cpp so the Main translation unit only contains the application
    bootstrap (WaveEditApplication + main()).

    Methods are kept inline in the class body to minimise the diff against
    the previous Main.cpp organisation. Per CLAUDE.md §7.5 this header
    exceeds the 500-line cap; further-splitting the implementation into a
    separate MainComponent.cpp + per-domain controller classes is tracked
    in TODO.md.

  ==============================================================================
*/

#pragma once


#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "Audio/AudioEngine.h"
#include "Audio/AudioFileManager.h"
#include "Audio/AudioBufferManager.h"
#include "Audio/AudioProcessor.h"
#include "Audio/ChannelLayout.h"
#include "Automation/AutomationRecorder.h"
#include "Commands/CommandIDs.h"
#include "UI/ThemeManager.h"
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
#include "Controllers/PlaybackController.h"
#include "Controllers/RecordingController.h"
#include "Controllers/DialogController.h"
#include "Controllers/PluginController.h"
#include "Controllers/ChainWindowListener.h"
#include "UI/WaveformDisplay.h"
#include "UI/TransportControls.h"
#include "UI/Meters.h"
#include "UI/GainDialog.h"
#include "UI/NewFileDialog.h"
#include "UI/NormalizeDialog.h"
#include "UI/FadeDialog.h"
// DCOffsetDialog removed - DC Offset now runs automatically without dialog
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
#include "UI/SelectionInfoPanel.h"
#include "UI/UIConstants.h"
#include "UI/CallbackDocumentWindow.h"

//==============================================================================
// Progress Dialog Threshold
// Operations affecting more than this many samples will show a progress dialog.
// 500,000 samples = ~11 seconds at 44.1kHz, ~10.4 seconds at 48kHz
constexpr int64_t kProgressDialogThreshold = 500000;

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
    // CommandHandler::performCommand() lives in Commands/CommandHandler.cpp
    // per CLAUDE.md §8.1; granting it friendship lets the dispatcher call
    // through to controllers and helpers without exposing them publicly.
    friend class CommandHandler;

public:
    MainComponent(juce::AudioDeviceManager& deviceManager)
        : m_audioDeviceManager(deviceManager),
          m_tabComponent(m_documentManager),
          m_keymapManager(m_commandManager),
          m_fileController(m_documentManager, m_fileManager),
          m_clipboardController(m_documentManager)
    {
        setSize(1200, 750);

        // Wire up FileController callbacks
        m_fileController.setOnUIRefreshNeeded([this]() { repaint(); });
        m_fileController.setSaveAsParent(this);

        // Listen to document manager events
        m_documentManager.addListener(this);

        // Setup tab component. Tab-close "Save" on a read-only source format
        // (e.g. m4a) routes to the Save As flow.
        m_tabComponent.onSaveAsRequested =
            [this](Document* doc) { m_fileController.saveFileAs(doc, this); };
        addAndMakeVisible(m_tabComponent);

        // Setup no-file label
        m_noFileLabel.setText("No audio file loaded", juce::dontSendNotification);
        m_noFileLabel.setFont(waveedit::ui::legacyFont(20.0f));
        m_noFileLabel.setJustificationType(juce::Justification::centred);
        m_noFileLabel.setColour(juce::Label::textColourId,
                                waveedit::ThemeManager::getInstance().getCurrent().textMuted);
        addAndMakeVisible(m_noFileLabel);

        // Empty-state call to action so the first 5 seconds aren't a dead end.
        m_openFileButton.setButtonText("Open File...");
        m_openFileButton.onClick = [this] { openFile(); };
        addAndMakeVisible(m_openFileButton);

        // Create container for current document components
        m_currentDocumentContainer = std::make_unique<juce::Component>();
        addAndMakeVisible(*m_currentDocumentContainer);

        // Add keyboard focus to handle shortcuts
        setWantsKeyboardFocus(true);

        // Set up command manager
        m_commandManager.registerAllCommandsForTarget(this);

        // Keep the menu bar's command-item enabled-states in sync with the
        // command manager. Without this watch, the native macOS menu never
        // reflects getCommandInfo() after its initial build, so command items --
        // even always-on ones like File > Open -- stay greyed. Pairs with the
        // commandStatusChanged() call on document changes (see currentDocumentChanged).
        setApplicationCommandManagerToWatch(&m_commandManager);

        // Add keyboard mappings
        addKeyListener(m_commandManager.getKeyMappings());

        #if JUCE_MAC
        // Register menu bar with macOS native menu system.
        // CRITICAL: This enables Cmd+, and other macOS system shortcuts to work.
        // Put Preferences under the application (WaveEdit) menu where Mac users
        // expect it, rather than in the File menu (see MenuBuilder).
        juce::PopupMenu extraAppleMenuItems;
        // Direct-action item, NOT a command item: setApplicationCommandManagerToWatch
        // only refreshes the MenuBarModel's own menus, not the extraAppleMenuItems
        // passed to setMacMainMenu. As a command item its enabled-state was never
        // synced, so "Settings..." stayed permanently greyed (and macOS swallowed
        // Cmd+, into the disabled item). A lambda item is always enabled and routes
        // straight to the command handler.
        extraAppleMenuItems.addItem("Preferences...", [this] {
            m_commandManager.invokeDirectly(CommandIDs::filePreferences, true);
        });
        juce::MenuBarModel::setMacMainMenu(this, &extraAppleMenuItems);
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

        // Wire the plugin-editor automation toolbar to the AutomationManager
        // of whichever document owns the plugin node. (Phase 4 UI surface +
        // Phase 5 entry point.)
        installAutomationEditorHandlers();

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

        // Refresh menu/command enabled-states for the new document: a file may
        // now be loaded (or unloaded), so File/Edit/Process/Plugin items must
        // re-evaluate. The watch set in the constructor pushes this to the menu.
        m_commandManager.commandStatusChanged();
    }

    void documentAdded(Document* document, int /*index*/) override
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

    void documentRemoved(Document* document, int /*index*/) override
    {
        // C14: Prevent a dangling raw pointer. If the document being removed is
        // the one tracked as "previous", null it so a later document switch does
        // not call into freed memory (UAF).
        if (m_previousDocument == document)
        {
            m_previousDocument = nullptr;
        }

        m_pluginController.notifyDocumentClosed(document);
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

    void regionListPanelRegionDeleted(int /*regionIndex*/) override
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            m_regionController.handleRegionListRegionDeleted(doc);
        }
    }

    void regionListPanelRegionRenamed(int /*regionIndex*/, const juce::String& /*newName*/) override
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

    void regionListPanelBatchRename(const std::vector<int>& /*regionIndices*/) override
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

    void markerListPanelMarkerDeleted(int /*markerIndex*/) override
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
            m_markerController.handleMarkerListMarkerRenamed(doc, markerIndex, newName);
        }
    }

    void markerListPanelMarkerColorChanged(int markerIndex, juce::Colour newColor) override
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
        {
            m_markerController.handleMarkerListMarkerColorChanged(doc, markerIndex, newColor);
        }
    }

    void markerListPanelExportRequested() override
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
            m_markerController.exportMarkers(doc, this);
    }

    void markerListPanelImportRequested() override
    {
        if (auto* doc = m_documentManager.getCurrentDocument())
            m_markerController.importMarkers(doc, this);
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
        m_openFileButton.setVisible(!hasDoc);
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
        const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();

        // CRITICAL FIX: Always draw background and status bar, regardless of document state
        // The UI chrome must always be visible

        // Background
        g.fillAll(theme.background);

        // Status bar at bottom
        auto bounds = getLocalBounds();
        auto statusBar = bounds.removeFromBottom(25);

        // Draw status bar background
        g.setColour(theme.panel);
        g.fillRect(statusBar);

        // Draw status bar border
        g.setColour(theme.border);
        g.drawLine(statusBar.getX(), statusBar.getY(), statusBar.getRight(), statusBar.getY(), 1.0f);

        // Plugin scan progress indicator (displayed on the right side when scanning)
        if (m_pluginScanInProgress)
        {
            auto scanSection = statusBar.removeFromRight(300);

            // Progress bar background
            auto progressBarBounds = scanSection.reduced(8, 6);
            g.setColour(theme.panelAlternate);
            g.fillRoundedRectangle(progressBarBounds.toFloat(), 3.0f);

            // Progress bar fill (semi-transparent so the themed text below
            // stays readable over both the filled and unfilled portions)
            auto fillBounds = progressBarBounds;
            fillBounds.setWidth(static_cast<int>(progressBarBounds.getWidth() * m_pluginScanProgress));
            g.setColour(theme.accent.withAlpha(0.45f));
            g.fillRoundedRectangle(fillBounds.toFloat(), 3.0f);

            // Progress bar border
            g.setColour(theme.border);
            g.drawRoundedRectangle(progressBarBounds.toFloat(), 3.0f, 1.0f);

            // Scan text overlay
            g.setColour(theme.text);
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
            g.setColour(theme.success);
            g.setFont(11.0f);
            g.drawText(m_pluginScanCurrentPlugin, scanSection.reduced(8, 0), juce::Justification::centredRight, true);
        }

        // Status bar text - adapt based on document state
        if (doc && doc->getAudioEngine().isFileLoaded())
        {
            g.setColour(theme.text);
            g.setFont(waveedit::ui::smallFont());

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

            // Surround fold-down badge (H7): never fold down silently. Shown at the
            // far right in the theme warning colour when the file has more channels
            // than the device outputs (e.g. a 5.1 file monitored on a stereo device).
            if (doc->getAudioEngine().isFoldDownActive())
            {
                auto foldSection = statusBar.removeFromRight(180);
                const int srcCh = doc->getAudioEngine().getFoldDownSourceChannels();
                const int outCh = doc->getAudioEngine().getFoldDownOutputChannels();
                waveedit::ChannelLayout srcLayout(srcCh);
                juce::String outLabel = (outCh == 2) ? "ST"
                                        : (outCh == 1) ? "Mono"
                                        : juce::String(outCh) + " ch";
                juce::String foldBadge = "[" + srcLayout.getLayoutName() + " -> " + outLabel + "]";
                g.setColour(theme.warning);
                g.setFont(waveedit::ui::smallFont());
                g.drawText(foldBadge, foldSection.reduced(5, 0), juce::Justification::centred, true);
            }

            // Clipboard info in the middle
            if (AudioClipboard::getInstance().hasAudio())
            {
                auto clipboardSection = statusBar.reduced(300, 0);
                clipboardSection.setX(statusBar.getCentreX() - 150);

                g.setColour(theme.success);
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

            g.setColour(theme.accent);
            g.setFont(waveedit::ui::smallFont());
            g.drawText(zoomText, zoomSection.reduced(5, 0), juce::Justification::centred, true);

            // Time format indicator (clickable) - fixed position to left of snap settings
            auto formatSection = statusBar.removeFromRight(120);

            // Store bounds for mouse click detection
            m_formatIndicatorBounds = formatSection;

            juce::String formatName = AudioUnits::timeFormatToString(m_timeFormat);
            juce::String formatText = "[" + formatName + "]";

            g.setColour(theme.accent);
            g.setFont(waveedit::ui::smallFont());
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
                snapColor = theme.textMuted;
            }
            else
            {
                snapText = "[" + AudioUnits::formatIncrement(increment, unitType) + "]";
                snapColor = theme.accent;
            }

            // Add zero-crossing indicator if enabled
            if (zeroCrossing)
            {
                snapText += " [Zero X]";
                snapColor = theme.warning;
            }

            g.setColour(snapColor);
            g.setFont(waveedit::ui::smallFont());
            g.drawText(snapText, snapSection.reduced(5, 0), juce::Justification::centred, true);

            // State indicator on the right
            auto rightSection = statusBar.removeFromRight(100);
            g.setColour(theme.text);
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
            g.setColour(theme.textMuted);
            g.setFont(waveedit::ui::smallFont());
           #if JUCE_MAC
            const char* openHint = "Press Cmd+O or drag in a file to open";
           #else
            const char* openHint = "Press Ctrl+O or drag in a file to open";
           #endif
            g.drawText(openHint, statusBar.reduced(10, 0), juce::Justification::centredLeft, true);
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

            // Center the no-file label, with an Open button just below it.
            m_noFileLabel.setBounds(bounds);
            m_openFileButton.setBounds(
                juce::Rectangle<int>(0, 0, 150, 34)
                    .withCentre({ bounds.getCentreX(), bounds.getCentreY() + 44 }));
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
        return m_commandHandler.performCommand(*this, info);
    }


    //==============================================================================
    // Command Handler Private Helpers (extracted from perform() for clarity)

    /**
     * Handle File → New command: show dialog and create new document with user settings.
     */
    void handleNewFileCommand() { m_fileController.createNewFile(); }

    void handleRecordCommand()
    {
        // Mirror the dialog's live capture state on the persistent toolbar
        // transport so the REC indicator is driven by real recording, not just
        // the (non-modal) dialog being open.
        juce::Component::SafePointer<MainComponent> safeThis(this);
        m_recordingController.handleRecordCommand(
            this, m_documentManager, m_audioDeviceManager,
            [safeThis](bool isRecording)
            {
                if (safeThis == nullptr)
                    return;
                if (auto* transport = safeThis->m_toolbar->getCompactTransport())
                    transport->setRecording(isRecording);
            });
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
                                                                      waveedit::ThemeManager::getInstance().getCurrent().panel,
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

    void handleLoopRegion(Document* doc) { m_playbackController.loopRegion(doc); }

    //==============================================================================
    // perform() helpers — keep individual cases at ≤5 lines per CLAUDE.md §6.7

    void handleUndo(Document* doc)
    {
        auto& undoMgr = doc->getUndoManager();
        if (! undoMgr.canUndo()) return;

        undoMgr.undo();
        doc->setModified(true);
        if (m_regionListPanel) m_regionListPanel->refresh();
        if (m_markerListPanel) m_markerListPanel->refresh();
        repaint();
    }

    void handleRedo(Document* doc)
    {
        auto& undoMgr = doc->getUndoManager();
        if (! undoMgr.canRedo()) return;

        undoMgr.redo();
        doc->setModified(true);
        if (m_regionListPanel) m_regionListPanel->refresh();
        if (m_markerListPanel) m_markerListPanel->refresh();
        repaint();
    }

    void showBWFMetadataDialog(Document* doc)
    {
        BWFEditorDialog::showDialog(this, doc->getBWFMetadata(),
                                    [doc]() { doc->setModified(true); });
    }

    void showiXMLMetadataDialog(Document* doc)
    {
        iXMLEditorDialog::showDialog(this, doc->getiXMLMetadata(), doc->getFilename(),
                                     [doc]() { doc->setModified(true); });
    }

    void cycleTimeFormat()
    {
        m_timeFormat = AudioUnits::getNextTimeFormat(m_timeFormat);
        Settings::getInstance().setSetting("display.timeFormat",
                                           static_cast<int>(m_timeFormat));
        Settings::getInstance().save();
        repaint();
    }

    void toggleRegionVisibility()
    {
        const bool currentlyVisible = Settings::getInstance().getRegionsVisible();
        Settings::getInstance().setRegionsVisible(! currentlyVisible);

        for (int i = 0; i < m_documentManager.getNumDocuments(); ++i)
        {
            if (auto* document = m_documentManager.getDocument(i))
            {
                document->getRegionDisplay().setVisible(! currentlyVisible);
                document->getWaveformDisplay().repaint();
            }
        }
    }

    void setSpectrumFFTSize(SpectrumAnalyzer::FFTSize size)
    {
        if (m_spectrumAnalyzer)
        {
            m_spectrumAnalyzer->setFFTSize(size);
            m_commandManager.commandStatusChanged();
        }
    }

    void setSpectrumWindow(SpectrumAnalyzer::WindowFunction fn)
    {
        if (m_spectrumAnalyzer)
        {
            m_spectrumAnalyzer->setWindowFunction(fn);
            m_commandManager.commandStatusChanged();
        }
    }

    void showGoToPositionDialog(Document* doc)
    {
        auto& engine = doc->getAudioEngine();
        GoToPositionDialog::showDialog(
            this,
            m_timeFormat,
            engine.getSampleRate(),
            30.0,
            static_cast<int64_t>(engine.getTotalLength() * engine.getSampleRate()),
            [this](int64_t positionInSamples)
            {
                if (auto* d = m_documentManager.getCurrentDocument())
                {
                    const double seconds = AudioUnits::samplesToSeconds(
                        positionInSamples, d->getAudioEngine().getSampleRate());
                    d->getWaveformDisplay().setEditCursor(seconds);
                    d->getWaveformDisplay().clearSelection();
                    d->getWaveformDisplay().centerViewOnCursor();
                }
            });
    }

    void toggleSnapRegionsToZeroCrossings()
    {
        const bool current = Settings::getInstance().getSnapRegionsToZeroCrossings();
        Settings::getInstance().setSnapRegionsToZeroCrossings(! current);
        m_commandManager.commandStatusChanged();
    }

    void togglePluginChainBypass(Document* doc)
    {
        auto& chain = doc->getAudioEngine().getPluginChain();
        chain.setAllBypassed(! chain.areAllBypassed());
    }

    void handlePluginClearCache()
    {
        const bool ok = juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::QuestionIcon,
            "Clear Plugin Cache",
            "This will delete all cached plugin data and perform a fresh scan.\n\n"
            "Useful when plugins have been added/removed or scan is misbehaving.\n\n"
            "Continue?",
            "Clear & Rescan",
            "Cancel");

        if (! ok) return;

        if (! PluginManager::getInstance().clearCache())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Cache Clear Warning",
                "Some cache files could not be deleted. The rescan will still run; "
                "manual deletion of the WaveEdit config folder may be needed for "
                "a complete reset.");
        }
        startPluginScan(true);
    }

    void showToolbarCustomization()
    {
        if (! ToolbarCustomizationDialog::showDialog(m_toolbarManager, m_commandManager))
            return;

        m_toolbar->loadLayout(m_toolbarManager.getCurrentLayout());
        m_toolbar->resized();
        resized();
    }

    void resetToolbarToDefault()
    {
        m_toolbarManager.loadLayout("Default");
        m_toolbar->loadLayout(m_toolbarManager.getCurrentLayout());
        m_toolbar->resized();
        resized();
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
            if (FileController::isDroppableAudioFile(juce::File(filename)))
                return true;
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
    void offerUntitledCrashRecovery() { m_fileController.offerUntitledCrashRecovery(); }

    //==============================================================================
    // Playback Control (delegated to PlaybackController)

    void togglePlayback() { m_playbackController.togglePlayback(getCurrentDocument()); repaint(); }
    void stopPlayback()   { m_playbackController.stopPlayback(getCurrentDocument());   repaint(); }
    void pausePlayback()  { m_playbackController.pausePlayback(getCurrentDocument());  repaint(); }
    void toggleLoop()     { m_playbackController.toggleLoop(getCurrentDocument());     repaint(); }

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
    void showAboutDialog() { m_dialogController.showAboutDialog(this); }

    void showKeyboardShortcutsDialog()
    {
        m_dialogController.showKeyboardShortcutsDialog(this, m_keymapManager, m_commandManager);
    }

    //==============================================================================
    // Plugin management methods

    void showPluginManagerDialog()
    {
        m_pluginController.showPluginManagerDialog(getCurrentDocument());
    }

    void showPluginChainPanel()
    {
        m_pluginController.showPluginChainPanel(
            getCurrentDocument(),
            m_commandManager,
            m_dspController,
            [this]() { return getCurrentDocument(); });
    }

    void showAutomationLanesPanel(int scrollToPluginIndex = -1,
                                  int scrollToParameterIndex = -1)
    {
        m_pluginController.showAutomationLanesPanel(
            getCurrentDocument(),
            m_commandManager,
            scrollToPluginIndex,
            scrollToParameterIndex);
    }

private:
    /** Find the open document whose plugin chain owns a given node, or
        nullptr if the node belongs to none of the open documents. */
    Document* findDocumentForNode(PluginChainNode* node, int* outPluginIndex = nullptr) const
    {
        if (node == nullptr) return nullptr;
        for (int d = 0; d < m_documentManager.getNumDocuments(); ++d)
        {
            auto* doc = m_documentManager.getDocument(d);
            if (doc == nullptr) continue;
            auto& chain = doc->getAudioEngine().getPluginChain();
            for (int i = 0; i < chain.getNumPlugins(); ++i)
            {
                if (chain.getPlugin(i) == node)
                {
                    if (outPluginIndex != nullptr) *outPluginIndex = i;
                    return doc;
                }
            }
        }
        return nullptr;
    }

    void installAutomationEditorHandlers()
    {
        PluginEditorWindow::setAutomationHandlers(
            // openLanes(node, paramIndex)
            [this](PluginChainNode* node, int paramIndex)
            {
                int pluginIndex = -1;
                auto* doc = findDocumentForNode(node, &pluginIndex);
                if (doc == nullptr)
                    return;
                m_pluginController.showAutomationLanesPanel(
                    doc, m_commandManager, pluginIndex, paramIndex);
            },
            // toggleArm(node, paramIndex)
            [this](PluginChainNode* node, int paramIndex)
            {
                int pluginIndex = -1;
                auto* doc = findDocumentForNode(node, &pluginIndex);
                if (doc == nullptr || node == nullptr || node->getPlugin() == nullptr)
                    return;
                auto& mgr = doc->getAutomationManager();
                auto* recorder = mgr.getRecorder();
                if (recorder == nullptr)
                    return;

                auto& params = node->getPlugin()->getParameters();
                if (paramIndex < 0 || paramIndex >= params.size())
                    return;
                auto* param = params[paramIndex];

                const bool currentlyArmed = (mgr.findLane(pluginIndex, paramIndex) != nullptr
                                             && mgr.findLane(pluginIndex, paramIndex)->isRecording);
                if (currentlyArmed)
                {
                    recorder->disarmParameter(pluginIndex, paramIndex);
                }
                else
                {
                    recorder->armParameter(pluginIndex, paramIndex,
                                            node->getName(),
                                            param->getName(64),
                                            param->getName(64));  // no stable param ID exposed
                    // Phase-4 capture also requires the global arm flag to be on;
                    // flip it on opportunistically when the user arms their first
                    // parameter, since the menu item is the user's primary intent.
                    if (! recorder->isGlobalArmed())
                        recorder->setGlobalArm(true);
                }
            },
            // isArmed(node, paramIndex)
            [this](PluginChainNode* node, int paramIndex) -> bool
            {
                int pluginIndex = -1;
                auto* doc = findDocumentForNode(node, &pluginIndex);
                if (doc == nullptr) return false;
                auto* lane = doc->getAutomationManager().findLane(pluginIndex, paramIndex);
                return lane != nullptr && lane->isRecording;
            });
    }

public:


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
    PlaybackController m_playbackController;
    RecordingController m_recordingController;
    DialogController m_dialogController;
    PluginController m_pluginController;

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
    juce::TextButton m_openFileButton;

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

    // Plugin chain window listeners are owned by PluginController.

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
