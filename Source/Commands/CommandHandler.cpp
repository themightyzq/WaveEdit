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
#include "../Utils/Document.h"
#include "../Utils/DocumentManager.h"
#include "../Utils/KeymapManager.h"
#include "../Utils/Settings.h"
#include "../Utils/AudioClipboard.h"
#include "../UI/SpectrumAnalyzer.h"
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

