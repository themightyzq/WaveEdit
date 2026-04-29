/*
  ==============================================================================

    CommandHandler.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "CommandHandler.h"
#include "CommandIDs.h"
#include "../MainComponent.h"
#include "../Utils/Document.h"
#include "../Utils/DocumentManager.h"
#include "../Utils/KeymapManager.h"
#include "../Utils/Settings.h"
#include "../Utils/AudioClipboard.h"
#include "../UI/SpectrumAnalyzer.h"
#include "../UI/RegionListPanel.h"
#include "../UI/MarkerListPanel.h"
#include "../UI/FilePropertiesDialog.h"
#include "../Plugins/PluginPathsPanel.h"
#include <functional>

CommandHandler::CommandHandler()
{
}

void CommandHandler::getAllCommands(juce::Array<juce::CommandID>& commands)
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
        CommandIDs::processReverse,
        CommandIDs::processInvert,
        CommandIDs::processResample,
        // Tools commands
        CommandIDs::toolsChannelConverter,
        CommandIDs::toolsChannelExtractor,
        CommandIDs::toolsHeadTail,
        CommandIDs::toolsLoopingTools,
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
        // Marker-Region Conversion commands
        CommandIDs::markerConvertToRegions,
        CommandIDs::regionConvertToMarkers,
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
        CommandIDs::helpCommandPalette,
        // Toolbar commands
        CommandIDs::toolbarCustomize,
        CommandIDs::toolbarReset,
        // Batch commands
        CommandIDs::fileBatchProcessor
    };

    commands.addArray(ids, juce::numElementsInArray(ids));
}

void CommandHandler::getCommandInfo(juce::CommandID commandID,
                                    juce::ApplicationCommandInfo& result,
                                    const CommandContext& context)
{
    auto* doc = context.currentDoc;

    // Get keyboard shortcut from active template
    auto keyPress = context.keymapManager->getKeyPress(commandID);

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
                result.setActive((context.currentDoc != nullptr));
                break;

            case CommandIDs::tabCloseAll:
                result.setInfo("Close All Tabs", "Close all open tabs", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive((context.currentDoc != nullptr));
                break;

            case CommandIDs::tabNext:
                result.setInfo("Next Tab", "Switch to next tab", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Ctrl+Tab (moved from Cmd+Tab to avoid macOS App Switcher conflict)
                result.setActive(context.documentManager->getNumDocuments() > 1);
                break;

            case CommandIDs::tabPrevious:
                result.setInfo("Previous Tab", "Switch to previous tab", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Ctrl+Shift+Tab (moved from Cmd+Shift+Tab to avoid macOS App Switcher conflict)
                result.setActive(context.documentManager->getNumDocuments() > 1);
                break;

            case CommandIDs::tabSelect1:
                result.setInfo("Jump to Tab 1", "Switch to tab 1", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(context.documentManager->getNumDocuments() >= 1);
                break;

            case CommandIDs::tabSelect2:
                result.setInfo("Jump to Tab 2", "Switch to tab 2", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(context.documentManager->getNumDocuments() >= 2);
                break;

            case CommandIDs::tabSelect3:
                result.setInfo("Jump to Tab 3", "Switch to tab 3", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(context.documentManager->getNumDocuments() >= 3);
                break;

            case CommandIDs::tabSelect4:
                result.setInfo("Jump to Tab 4", "Switch to tab 4", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(context.documentManager->getNumDocuments() >= 4);
                break;

            case CommandIDs::tabSelect5:
                result.setInfo("Jump to Tab 5", "Switch to tab 5", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(context.documentManager->getNumDocuments() >= 5);
                break;

            case CommandIDs::tabSelect6:
                result.setInfo("Jump to Tab 6", "Switch to tab 6", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(context.documentManager->getNumDocuments() >= 6);
                break;

            case CommandIDs::tabSelect7:
                result.setInfo("Jump to Tab 7", "Switch to tab 7", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(context.documentManager->getNumDocuments() >= 7);
                break;

            case CommandIDs::tabSelect8:
                result.setInfo("Jump to Tab 8", "Switch to tab 8", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(context.documentManager->getNumDocuments() >= 8);
                break;

            case CommandIDs::tabSelect9:
                result.setInfo("Jump to Tab 9", "Switch to tab 9", "File", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(context.documentManager->getNumDocuments() >= 9);
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
                result.setTicked(context.autoPreviewRegions);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded() &&
                                 doc->getRegionManager().getNumRegions() > 0);
                break;

            case CommandIDs::viewToggleRegions:
                result.setInfo("Show/Hide Regions", "Toggle region visibility in waveform display", "View", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+Shift+H
                result.setTicked(context.regionsVisible);
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::viewSpectrumAnalyzer:
                result.setInfo("Spectrum Analyzer", "Show/hide real-time spectrum analyzer", "View", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+Alt+S
                result.setTicked(context.spectrumWindow && context.spectrumWindow->isVisible());
                result.setActive(true);  // Always available
                break;

            // Spectrum Analyzer FFT Size Commands
            case CommandIDs::viewSpectrumFFTSize512:
                result.setInfo("FFT Size: 512", "Set FFT size to 512 samples (faster, lower resolution)", "View", 0);
                result.setTicked(context.spectrumAnalyzer && context.spectrumAnalyzer->getFFTSize() == SpectrumAnalyzer::FFTSize::SIZE_512);
                result.setActive(context.spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumFFTSize1024:
                result.setInfo("FFT Size: 1024", "Set FFT size to 1024 samples", "View", 0);
                result.setTicked(context.spectrumAnalyzer && context.spectrumAnalyzer->getFFTSize() == SpectrumAnalyzer::FFTSize::SIZE_1024);
                result.setActive(context.spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumFFTSize2048:
                result.setInfo("FFT Size: 2048", "Set FFT size to 2048 samples (default, balanced)", "View", 0);
                result.setTicked(context.spectrumAnalyzer && context.spectrumAnalyzer->getFFTSize() == SpectrumAnalyzer::FFTSize::SIZE_2048);
                result.setActive(context.spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumFFTSize4096:
                result.setInfo("FFT Size: 4096", "Set FFT size to 4096 samples (higher resolution)", "View", 0);
                result.setTicked(context.spectrumAnalyzer && context.spectrumAnalyzer->getFFTSize() == SpectrumAnalyzer::FFTSize::SIZE_4096);
                result.setActive(context.spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumFFTSize8192:
                result.setInfo("FFT Size: 8192", "Set FFT size to 8192 samples (highest resolution, slower)", "View", 0);
                result.setTicked(context.spectrumAnalyzer && context.spectrumAnalyzer->getFFTSize() == SpectrumAnalyzer::FFTSize::SIZE_8192);
                result.setActive(context.spectrumAnalyzer != nullptr);
                break;

            // Spectrum Analyzer Window Function Commands
            case CommandIDs::viewSpectrumWindowHann:
                result.setInfo("Window: Hann", "Use Hann window function (default, good general purpose)", "View", 0);
                result.setTicked(context.spectrumAnalyzer && context.spectrumAnalyzer->getWindowFunction() == SpectrumAnalyzer::WindowFunction::HANN);
                result.setActive(context.spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumWindowHamming:
                result.setInfo("Window: Hamming", "Use Hamming window function (slightly narrower main lobe)", "View", 0);
                result.setTicked(context.spectrumAnalyzer && context.spectrumAnalyzer->getWindowFunction() == SpectrumAnalyzer::WindowFunction::HAMMING);
                result.setActive(context.spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumWindowBlackman:
                result.setInfo("Window: Blackman", "Use Blackman window function (better sidelobe suppression)", "View", 0);
                result.setTicked(context.spectrumAnalyzer && context.spectrumAnalyzer->getWindowFunction() == SpectrumAnalyzer::WindowFunction::BLACKMAN);
                result.setActive(context.spectrumAnalyzer != nullptr);
                break;

            case CommandIDs::viewSpectrumWindowRectangular:
                result.setInfo("Window: Rectangular", "Use rectangular window (no windowing, best frequency resolution)", "View", 0);
                result.setTicked(context.spectrumAnalyzer && context.spectrumAnalyzer->getWindowFunction() == SpectrumAnalyzer::WindowFunction::RECTANGULAR);
                result.setActive(context.spectrumAnalyzer != nullptr);
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

            case CommandIDs::toolsChannelConverter:
                result.setInfo("Channel Converter...", "Convert between mono, stereo, and surround formats", "Tools", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::toolsChannelExtractor:
                result.setInfo("Channel Extractor...", "Extract individual channels to separate files", "Tools", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::toolsHeadTail:
                result.setInfo("Head && Tail...", "Trim heads/tails, add silence, apply fades", "Tools", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::toolsLoopingTools:
                result.setInfo("Looping Tools...", "Create seamless loops from selection", "Tools", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc != nullptr
                                 && doc->getAudioEngine().isFileLoaded()
                                 && doc->getWaveformDisplay().hasSelection());
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

            case CommandIDs::processReverse:
                result.setInfo("Reverse", "Reverse audio in selection or entire file", "Process", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::processInvert:
                result.setInfo("Invert", "Invert polarity of selection or entire file", "Process", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc && doc->getAudioEngine().isFileLoaded());
                break;

            case CommandIDs::processResample:
                result.setInfo("Resample...", "Change sample rate of audio file", "Process", 0);
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
                result.setActive(doc && context.canMergeRegions(doc));
                break;

            case CommandIDs::regionSplit:
                result.setInfo("Split Region at Cursor", "Split region at cursor position", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());  // Cmd+K (K = "kut/split", moved from Cmd+R to avoid conflict with playbackRecord)
                result.setActive(doc && context.canSplitRegion(doc));
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
                result.setActive(doc && context.hasRegionClipboard);
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

            case CommandIDs::markerConvertToRegions:
                result.setInfo("Markers \xe2\x86\x92 Regions", "Create regions between consecutive markers", "Marker", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc != nullptr &&
                                 doc->getMarkerManager().getNumMarkers() >= 2);
                break;

            case CommandIDs::regionConvertToMarkers:
                result.setInfo("Regions \xe2\x86\x92 Markers", "Create markers at region boundaries", "Region", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                result.setActive(doc != nullptr &&
                                 doc->getRegionManager().getNumRegions() >= 1);
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

            case CommandIDs::helpCommandPalette:
                result.setInfo("Command Palette", "Search and execute any command", "Help", 0);
                if (keyPress.isValid())
                    result.addDefaultKeypress(keyPress.getKeyCode(), keyPress.getModifiers());
                else
                    result.addDefaultKeypress('a', juce::ModifierKeys::commandModifier
                                                   | juce::ModifierKeys::shiftModifier);
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

            case CommandIDs::fileBatchProcessor:
                result.setInfo("Batch Processor...", "Open batch processing dialog for multiple files", "File", 0);
                result.addDefaultKeypress('b', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier);
                result.setActive(true);  // Always available
                break;

            default:
                break;
        }
    }


//==============================================================================
// performCommand() — the perform() switch lives here per CLAUDE.md §8.1.
// Moved from Source/MainComponent.h. CommandHandler is a friend of
// MainComponent so it can reach the controller members it dispatches to.
//==============================================================================

bool CommandHandler::performCommand(MainComponent& mc,
                                 const juce::ApplicationCommandTarget::InvocationInfo& info)
{
    auto* doc = mc.getCurrentDocument();

    // CRITICAL FIX: Don't early return - allow document-independent commands
    // Commands like File → Open and File → Exit must work without a document

    switch (info.commandID)
    {
        case CommandIDs::fileNew:
            mc.handleNewFileCommand();
            return true;

        case CommandIDs::fileOpen:
            mc.openFile();
            return true;

        case CommandIDs::fileExit:
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            return true;

        case CommandIDs::filePreferences:
            SettingsPanel::showDialog(&mc, mc.m_audioDeviceManager, mc.m_commandManager, mc.m_keymapManager);
            return true;

        // Tab operations
        case CommandIDs::tabClose:
            if (!doc) return false;
            mc.m_documentManager.closeDocument(doc);
            return true;

        case CommandIDs::tabCloseAll:
            mc.m_documentManager.closeAllDocuments();
            return true;

        case CommandIDs::tabNext:
            mc.m_documentManager.selectNextDocument();
            return true;

        case CommandIDs::tabPrevious:
            mc.m_documentManager.selectPreviousDocument();
            return true;

        case CommandIDs::tabSelect1:
            mc.m_documentManager.setCurrentDocumentIndex(0);
            return true;

        case CommandIDs::tabSelect2:
            mc.m_documentManager.setCurrentDocumentIndex(1);
            return true;

        case CommandIDs::tabSelect3:
            mc.m_documentManager.setCurrentDocumentIndex(2);
            return true;

        case CommandIDs::tabSelect4:
            mc.m_documentManager.setCurrentDocumentIndex(3);
            return true;

        case CommandIDs::tabSelect5:
            mc.m_documentManager.setCurrentDocumentIndex(4);
            return true;

        case CommandIDs::tabSelect6:
            mc.m_documentManager.setCurrentDocumentIndex(5);
            return true;

        case CommandIDs::tabSelect7:
            mc.m_documentManager.setCurrentDocumentIndex(6);
            return true;

        case CommandIDs::tabSelect8:
            mc.m_documentManager.setCurrentDocumentIndex(7);
            return true;

        case CommandIDs::tabSelect9:
            mc.m_documentManager.setCurrentDocumentIndex(8);
            return true;

        case CommandIDs::fileSave:
            if (!doc) return false;
            mc.saveFile();
            return true;

        case CommandIDs::fileSaveAs:
            if (!doc) return false;
            mc.saveFileAs();
            return true;

        case CommandIDs::fileClose:
            if (!doc) return false;
            mc.closeFile();
            return true;

        case CommandIDs::fileProperties:
            if (!doc) return false;
            FilePropertiesDialog::showDialog(&mc, *doc);
            return true;

        case CommandIDs::fileEditBWFMetadata:
            if (!doc) return false;
            mc.showBWFMetadataDialog(doc);
            return true;

        case CommandIDs::fileEditiXMLMetadata:
            if (!doc) return false;
            mc.showiXMLMetadataDialog(doc);
            return true;

        case CommandIDs::editUndo:
            if (!doc) return false;
            mc.handleUndo(doc);
            return true;

        case CommandIDs::editRedo:
            if (!doc) return false;
            mc.handleRedo(doc);
            return true;

        case CommandIDs::editSelectAll:
            if (!doc) return false;
            mc.selectAll();
            return true;

        case CommandIDs::editCut:
            if (!doc) return false;
            mc.cutSelection();
            return true;

        case CommandIDs::editCopy:
            if (!doc) return false;
            mc.copySelection();
            return true;

        case CommandIDs::editPaste:
            if (!doc) return false;
            mc.pasteAtCursor();
            return true;

        case CommandIDs::editDelete:
            if (!doc) return false;
            mc.deleteSelection();
            return true;

        case CommandIDs::playbackPlay:
            if (!doc) return false;
            mc.togglePlayback();
            return true;

        case CommandIDs::playbackPause:
            if (!doc) return false;
            mc.pausePlayback();
            return true;

        case CommandIDs::playbackStop:
            if (!doc) return false;
            mc.stopPlayback();
            return true;

        case CommandIDs::playbackLoop:
            if (!doc) return false;
            mc.toggleLoop();
            return true;

        case CommandIDs::playbackLoopRegion:
            if (!doc) return false;
            mc.handleLoopRegion(doc);
            return true;

        case CommandIDs::playbackRecord:
            mc.handleRecordCommand();
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
            mc.cycleTimeFormat();
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
            mc.toggleRegionVisibility();
            return true;

        case CommandIDs::viewSpectrumAnalyzer:
            mc.toggleSpectrumAnalyzer();
            return true;

        // Spectrum Analyzer FFT Size Commands
        case CommandIDs::viewSpectrumFFTSize512:
            mc.setSpectrumFFTSize(SpectrumAnalyzer::FFTSize::SIZE_512);
            return true;

        case CommandIDs::viewSpectrumFFTSize1024:
            mc.setSpectrumFFTSize(SpectrumAnalyzer::FFTSize::SIZE_1024);
            return true;

        case CommandIDs::viewSpectrumFFTSize2048:
            mc.setSpectrumFFTSize(SpectrumAnalyzer::FFTSize::SIZE_2048);
            return true;

        case CommandIDs::viewSpectrumFFTSize4096:
            mc.setSpectrumFFTSize(SpectrumAnalyzer::FFTSize::SIZE_4096);
            return true;

        case CommandIDs::viewSpectrumFFTSize8192:
            mc.setSpectrumFFTSize(SpectrumAnalyzer::FFTSize::SIZE_8192);
            return true;

        // Spectrum Analyzer Window Function Commands
        case CommandIDs::viewSpectrumWindowHann:
            mc.setSpectrumWindow(SpectrumAnalyzer::WindowFunction::HANN);
            return true;

        case CommandIDs::viewSpectrumWindowHamming:
            mc.setSpectrumWindow(SpectrumAnalyzer::WindowFunction::HAMMING);
            return true;

        case CommandIDs::viewSpectrumWindowBlackman:
            mc.setSpectrumWindow(SpectrumAnalyzer::WindowFunction::BLACKMAN);
            return true;

        case CommandIDs::viewSpectrumWindowRectangular:
            mc.setSpectrumWindow(SpectrumAnalyzer::WindowFunction::RECTANGULAR);
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
            if (!doc) return false;
            mc.showGoToPositionDialog(doc);
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
            mc.cycleSnapMode();
            return true;

        case CommandIDs::snapToggleZeroCrossing:
            if (!doc) return false;
            mc.toggleZeroCrossingSnap();
            return true;

        // Region operations (Phase 3 Tier 2)
        case CommandIDs::regionAdd:
            if (!doc) return false;
            mc.m_regionController.addRegionFromSelection(doc);
            return true;

        case CommandIDs::regionDelete:
            if (!doc) return false;
            mc.m_regionController.deleteSelectedRegion(doc);
            return true;

        case CommandIDs::regionNext:
            if (!doc) return false;
            mc.m_regionController.jumpToNextRegion(doc);
            return true;

        case CommandIDs::regionPrevious:
            if (!doc) return false;
            mc.m_regionController.jumpToPreviousRegion(doc);
            return true;

        case CommandIDs::regionSelectInverse:
            if (!doc) return false;
            mc.m_regionController.selectInverseOfRegions(doc);
            return true;

        case CommandIDs::regionSelectAll:
            if (!doc) return false;
            mc.m_regionController.selectAllRegions(doc);
            return true;

        case CommandIDs::regionStripSilence:
            if (!doc) return false;
            mc.m_regionController.showStripSilenceDialog(doc, &mc);
            return true;

        case CommandIDs::regionExportAll:
            if (!doc) return false;
            mc.m_regionController.showBatchExportDialog(doc, &mc);
            return true;

        case CommandIDs::regionBatchRename:
            if (!doc) return false;
            mc.handleRegionBatchRename();
            return true;

        // Region editing commands (Phase 3.4)
        case CommandIDs::regionMerge:
            if (!doc) return false;
            mc.m_regionController.mergeSelectedRegions(doc);
            return true;

        case CommandIDs::regionSplit:
            if (!doc) return false;
            mc.m_regionController.splitRegionAtCursor(doc);
            return true;

        case CommandIDs::regionCopy:
            if (!doc) return false;
            mc.m_regionController.copyRegionsToClipboard(doc);
            return true;

        case CommandIDs::regionPaste:
            if (!doc) return false;
            mc.m_regionController.pasteRegionsFromClipboard(doc);
            return true;

        case CommandIDs::regionShowList:
            mc.toggleRegionListPanel();
            return true;

        case CommandIDs::regionSnapToZeroCrossing:
            mc.toggleSnapRegionsToZeroCrossings();
            return true;

        // Region nudge commands (Phase 3.3 - Feature #6)
        case CommandIDs::regionNudgeStartLeft:
            if (!doc) return false;
            mc.m_regionController.nudgeRegionBoundary(doc, true, true);
            return true;

        case CommandIDs::regionNudgeStartRight:
            if (!doc) return false;
            mc.m_regionController.nudgeRegionBoundary(doc, true, false);
            return true;

        case CommandIDs::regionNudgeEndLeft:
            if (!doc) return false;
            mc.m_regionController.nudgeRegionBoundary(doc, false, true);
            return true;

        case CommandIDs::regionNudgeEndRight:
            if (!doc) return false;
            mc.m_regionController.nudgeRegionBoundary(doc, false, false);
            return true;

        // Marker operations (Phase 3.4)
        case CommandIDs::markerAdd:
            if (!doc) return false;
            mc.m_markerController.addMarkerAtCursor(doc, mc.m_lastClickTimeInSeconds, mc.m_hasLastClickPosition);
            return true;

        case CommandIDs::markerDelete:
            if (!doc) return false;
            mc.m_markerController.deleteSelectedMarker(doc);
            return true;

        case CommandIDs::markerNext:
            if (!doc) return false;
            mc.m_markerController.jumpToNextMarker(doc);
            return true;

        case CommandIDs::markerPrevious:
            if (!doc) return false;
            mc.m_markerController.jumpToPreviousMarker(doc);
            return true;

        case CommandIDs::markerShowList:
            mc.toggleMarkerListPanel();
            return true;

        case CommandIDs::markerConvertToRegions:
            if (!doc) return false;
            mc.m_markerController.convertMarkersToRegions(doc);
            return true;

        case CommandIDs::regionConvertToMarkers:
            if (!doc) return false;
            mc.m_markerController.convertRegionsToMarkers(doc);
            return true;

        // Processing operations
        case CommandIDs::processGain:
            if (!doc) return false;
            mc.m_dspController.showGainDialog(doc, &mc);
            return true;

        case CommandIDs::processIncreaseGain:
            if (!doc) return false;
            mc.applyGainAdjustment(+1.0f);
            return true;

        case CommandIDs::processDecreaseGain:
            if (!doc) return false;
            mc.applyGainAdjustment(-1.0f);
            return true;

        case CommandIDs::processParametricEQ:
            if (!doc) return false;
            mc.m_dspController.showParametricEQDialog(doc, &mc);
            return true;

        case CommandIDs::processGraphicalEQ:
            if (!doc) return false;
            mc.m_dspController.showGraphicalEQDialog(doc, &mc);
            return true;

        case CommandIDs::toolsChannelConverter:
            if (!doc) return false;
            mc.m_dspController.showChannelConverterDialog(doc, &mc);
            return true;

        case CommandIDs::toolsChannelExtractor:
            if (!doc) return false;
            mc.m_dspController.showChannelExtractorDialog(doc, &mc);
            return true;

        case CommandIDs::toolsHeadTail:
            if (!doc) return false;
            mc.m_dspController.showHeadTailDialog(doc, &mc);
            return true;

        case CommandIDs::toolsLoopingTools:
            if (!doc) return false;
            mc.m_dspController.showLoopingToolsDialog(doc, &mc);
            return true;

        case CommandIDs::processNormalize:
            if (!doc) return false;
            mc.m_dspController.showNormalizeDialog(doc, &mc);
            return true;

        case CommandIDs::processFadeIn:
            if (!doc) return false;
            mc.m_dspController.showFadeInDialog(doc, &mc);
            return true;

        case CommandIDs::processFadeOut:
            if (!doc) return false;
            mc.m_dspController.showFadeOutDialog(doc, &mc);
            return true;

        case CommandIDs::processDCOffset:
            if (!doc) return false;
            mc.applyDCOffset();  // Run automatically without dialog
            return true;

        case CommandIDs::processReverse:
            if (!doc) return false;
            mc.m_dspController.reverseSelection(doc);
            return true;

        case CommandIDs::processInvert:
            if (!doc) return false;
            mc.m_dspController.invertSelection(doc);
            return true;

        case CommandIDs::processResample:
            if (!doc) return false;
            mc.m_dspController.showResampleDialog(doc, &mc);
            return true;

        case CommandIDs::editSilence:
            if (!doc) return false;
            mc.m_dspController.silenceSelection(doc);
            return true;

        case CommandIDs::editTrim:
            if (!doc) return false;
            mc.m_dspController.trimToSelection(doc);
            return true;

        // Help commands
        case CommandIDs::helpAbout:
            mc.showAboutDialog();
            return true;

        case CommandIDs::helpShortcuts:
            mc.showKeyboardShortcutsDialog();
            return true;

        case CommandIDs::helpCommandPalette:
            if (mc.m_commandPalette)
                mc.m_commandPalette->show();
            return true;

        //==============================================================================
        // Plugin commands (VST3/AU plugin chain)
        case CommandIDs::pluginShowChain:
            mc.showPluginChainPanel();
            return true;

        case CommandIDs::pluginApplyChain:
            if (!doc) return false;
            mc.m_dspController.applyPluginChainToSelection(doc);
            return true;

        case CommandIDs::pluginOffline:
            if (!doc) return false;
            mc.m_dspController.showOfflinePluginDialog(doc, &mc);
            return true;

        case CommandIDs::pluginBypassAll:
            if (!doc) return false;
            mc.togglePluginChainBypass(doc);
            return true;

        case CommandIDs::pluginRescan:
            // Use mc.startPluginScan() to get progress bar in status bar
            mc.startPluginScan(true);  // true = force rescan
            return true;

        case CommandIDs::pluginShowSettings:
            PluginPathsPanel::showDialog();
            return true;

        case CommandIDs::pluginClearCache:
            mc.handlePluginClearCache();
            return true;

        //==============================================================================
        // Toolbar commands
        case CommandIDs::toolbarCustomize:
            mc.showToolbarCustomization();
            return true;

        case CommandIDs::toolbarReset:
            mc.resetToolbarToDefault();
            return true;

        case CommandIDs::fileBatchProcessor:
            mc.m_dialogController.showBatchProcessorDialog();
            return true;

        default:
            return false;
    }
}
