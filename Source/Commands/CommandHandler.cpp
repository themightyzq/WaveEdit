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
#include "../Automation/AutomationRecorder.h"
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
        CommandIDs::processTimeStretch,
        CommandIDs::processPitchShift,
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
        CommandIDs::automationToggleRecordArm,
        CommandIDs::pluginShowAutomationLanes,
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


// CommandHandler::getCommandInfo() lives in CommandHandler_GetInfo.cpp
// (split out under CLAUDE.md §7.5).



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

        case CommandIDs::processTimeStretch:
            if (!doc) return false;
            mc.m_dspController.showTimeStretchDialog(doc, &mc);
            return true;

        case CommandIDs::processPitchShift:
            if (!doc) return false;
            mc.m_dspController.showPitchShiftDialog(doc, &mc);
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

        case CommandIDs::automationToggleRecordArm:
            if (!doc) return false;
            if (auto* rec = doc->getAutomationManager().getRecorder())
                rec->toggleGlobalArm();
            return true;

        case CommandIDs::pluginShowAutomationLanes:
            if (!doc) return false;
            mc.showAutomationLanesPanel();
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
