/*
  ==============================================================================

    MenuBuilder.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "MenuBuilder.h"
#include "CommandIDs.h"
#include "../Utils/Settings.h"
#include "../UI/SpectrumAnalyzer.h"

MenuBuilder::MenuBuilder()
{
}

juce::StringArray MenuBuilder::getMenuBarNames()
{
    return { "File", "Edit", "View", "Region", "Marker", "Process", "Plugins", "Tools", "Playback", "Help" };
}

juce::PopupMenu MenuBuilder::getMenuForIndex(int menuIndex,
                                             const juce::String& /*menuName*/,
                                             const MenuContext& context)
{
    juce::PopupMenu menu;

    if (menuIndex == 0) // File menu
    {
        // --- Document ---
        menu.addSectionHeader("Document");
        menu.addCommandItem(context.commandManager, CommandIDs::fileNew);
        menu.addCommandItem(context.commandManager, CommandIDs::fileOpen);
        menu.addCommandItem(context.commandManager, CommandIDs::fileSave);
        menu.addCommandItem(context.commandManager, CommandIDs::fileSaveAs);
        menu.addCommandItem(context.commandManager, CommandIDs::fileClose);

        // --- Metadata ---
        menu.addSectionHeader("Metadata");
        menu.addCommandItem(context.commandManager, CommandIDs::fileProperties);
        menu.addCommandItem(context.commandManager, CommandIDs::fileEditBWFMetadata);
        menu.addCommandItem(context.commandManager, CommandIDs::fileEditiXMLMetadata);

        // Recent files submenu
        auto recentFiles = Settings::getInstance().getRecentFiles();
        if (!recentFiles.isEmpty())
        {
            menu.addSectionHeader("Recent");
            juce::PopupMenu recentFilesMenu;
            for (int i = 0; i < recentFiles.size(); ++i)
            {
                juce::File file(recentFiles[i]);
                recentFilesMenu.addItem(file.getFileName(), [context, file] {
                    if (context.onLoadFile) context.onLoadFile(file);
                });
            }
            recentFilesMenu.addSeparator();
            recentFilesMenu.addItem("Clear Recent Files", [] { Settings::getInstance().clearRecentFiles(); });
            menu.addSubMenu("Recent Files", recentFilesMenu);
        }

        // --- Application ---
        menu.addSectionHeader("Application");
        menu.addCommandItem(context.commandManager, CommandIDs::filePreferences);
        menu.addCommandItem(context.commandManager, CommandIDs::fileExit);
    }
    else if (menuIndex == 1) // Edit menu
    {
        // --- History ---
        menu.addSectionHeader("History");
        menu.addCommandItem(context.commandManager, CommandIDs::editUndo);
        menu.addCommandItem(context.commandManager, CommandIDs::editRedo);

        // --- Clipboard ---
        menu.addSectionHeader("Clipboard");
        menu.addCommandItem(context.commandManager, CommandIDs::editCut);
        menu.addCommandItem(context.commandManager, CommandIDs::editCopy);
        menu.addCommandItem(context.commandManager, CommandIDs::editPaste);
        menu.addCommandItem(context.commandManager, CommandIDs::editDelete);

        // --- Audio Editing ---
        menu.addSectionHeader("Audio Editing");
        menu.addCommandItem(context.commandManager, CommandIDs::editSilence);
        menu.addCommandItem(context.commandManager, CommandIDs::editTrim);

        // --- Selection ---
        menu.addSectionHeader("Selection");
        menu.addCommandItem(context.commandManager, CommandIDs::editSelectAll);
    }
    else if (menuIndex == 2) // View menu
    {
        // --- Zoom ---
        menu.addSectionHeader("Zoom");
        menu.addCommandItem(context.commandManager, CommandIDs::viewZoomIn);
        menu.addCommandItem(context.commandManager, CommandIDs::viewZoomOut);
        menu.addCommandItem(context.commandManager, CommandIDs::viewZoomFit);
        menu.addCommandItem(context.commandManager, CommandIDs::viewZoomSelection);
        menu.addCommandItem(context.commandManager, CommandIDs::viewZoomOneToOne);

        // --- Display Options ---
        menu.addSectionHeader("Display");
        menu.addCommandItem(context.commandManager, CommandIDs::viewCycleTimeFormat);
        menu.addCommandItem(context.commandManager, CommandIDs::viewAutoScroll);
        menu.addCommandItem(context.commandManager, CommandIDs::viewZoomToRegion);
        menu.addCommandItem(context.commandManager, CommandIDs::viewAutoPreviewRegions);
        menu.addCommandItem(context.commandManager, CommandIDs::viewToggleRegions);

        // --- Spectrum Analyzer ---
        menu.addSectionHeader("Spectrum Analyzer");
        menu.addCommandItem(context.commandManager, CommandIDs::viewSpectrumAnalyzer);

        // Spectrum Analyzer Configuration Submenus
        juce::PopupMenu fftSizeMenu;
        fftSizeMenu.addCommandItem(context.commandManager, CommandIDs::viewSpectrumFFTSize512);
        fftSizeMenu.addCommandItem(context.commandManager, CommandIDs::viewSpectrumFFTSize1024);
        fftSizeMenu.addCommandItem(context.commandManager, CommandIDs::viewSpectrumFFTSize2048);
        fftSizeMenu.addCommandItem(context.commandManager, CommandIDs::viewSpectrumFFTSize4096);
        fftSizeMenu.addCommandItem(context.commandManager, CommandIDs::viewSpectrumFFTSize8192);
        menu.addSubMenu("Spectrum FFT Size", fftSizeMenu, context.spectrumAnalyzer != nullptr);

        juce::PopupMenu windowFunctionMenu;
        windowFunctionMenu.addCommandItem(context.commandManager, CommandIDs::viewSpectrumWindowHann);
        windowFunctionMenu.addCommandItem(context.commandManager, CommandIDs::viewSpectrumWindowHamming);
        windowFunctionMenu.addCommandItem(context.commandManager, CommandIDs::viewSpectrumWindowBlackman);
        windowFunctionMenu.addCommandItem(context.commandManager, CommandIDs::viewSpectrumWindowRectangular);
        menu.addSubMenu("Spectrum Window Function", windowFunctionMenu, context.spectrumAnalyzer != nullptr);

        // --- Toolbar ---
        menu.addSectionHeader("Toolbar");
        menu.addCommandItem(context.commandManager, CommandIDs::toolbarCustomize);
        menu.addCommandItem(context.commandManager, CommandIDs::toolbarReset);
    }
    else if (menuIndex == 3) // Region menu
    {
        // --- Create/Delete ---
        menu.addSectionHeader("Create/Delete");
        menu.addCommandItem(context.commandManager, CommandIDs::regionAdd);
        menu.addCommandItem(context.commandManager, CommandIDs::regionDelete);

        // --- Navigation ---
        menu.addSectionHeader("Navigation");
        menu.addCommandItem(context.commandManager, CommandIDs::regionNext);
        menu.addCommandItem(context.commandManager, CommandIDs::regionPrevious);

        // --- Selection ---
        menu.addSectionHeader("Selection");
        menu.addCommandItem(context.commandManager, CommandIDs::regionSelectInverse);
        menu.addCommandItem(context.commandManager, CommandIDs::regionSelectAll);

        // --- Editing ---
        menu.addSectionHeader("Editing");
        menu.addCommandItem(context.commandManager, CommandIDs::regionMerge);
        menu.addCommandItem(context.commandManager, CommandIDs::regionSplit);
        menu.addCommandItem(context.commandManager, CommandIDs::regionCopy);
        menu.addCommandItem(context.commandManager, CommandIDs::regionPaste);

        // --- Batch Operations ---
        menu.addSectionHeader("Batch Operations");
        menu.addCommandItem(context.commandManager, CommandIDs::regionStripSilence);
        menu.addCommandItem(context.commandManager, CommandIDs::regionExportAll);
        menu.addCommandItem(context.commandManager, CommandIDs::regionBatchRename);

        // --- View ---
        menu.addSectionHeader("View");
        menu.addCommandItem(context.commandManager, CommandIDs::regionShowList);

        // --- Convert ---
        menu.addSectionHeader("Convert");
        menu.addCommandItem(context.commandManager, CommandIDs::regionConvertToMarkers);
    }
    else if (menuIndex == 4) // Marker menu (Phase 3.4)
    {
        // --- Create/Delete ---
        menu.addSectionHeader("Create/Delete");
        menu.addCommandItem(context.commandManager, CommandIDs::markerAdd);
        menu.addCommandItem(context.commandManager, CommandIDs::markerDelete);

        // --- Navigation ---
        menu.addSectionHeader("Navigation");
        menu.addCommandItem(context.commandManager, CommandIDs::markerNext);
        menu.addCommandItem(context.commandManager, CommandIDs::markerPrevious);

        // --- View ---
        menu.addSectionHeader("View");
        menu.addCommandItem(context.commandManager, CommandIDs::markerShowList);

        // --- Convert ---
        menu.addSectionHeader("Convert");
        menu.addCommandItem(context.commandManager, CommandIDs::markerConvertToRegions);
    }
    else if (menuIndex == 5) // Process menu
    {
        // --- Volume ---
        menu.addSectionHeader("Volume");
        menu.addCommandItem(context.commandManager, CommandIDs::processGain);
        menu.addCommandItem(context.commandManager, CommandIDs::processNormalize);

        // --- Equalization ---
        menu.addSectionHeader("Equalization");
        menu.addCommandItem(context.commandManager, CommandIDs::processParametricEQ);
        menu.addCommandItem(context.commandManager, CommandIDs::processGraphicalEQ);

        // --- Repair ---
        menu.addSectionHeader("Repair");
        menu.addCommandItem(context.commandManager, CommandIDs::processDCOffset);

        // --- Fades ---
        menu.addSectionHeader("Fades");
        menu.addCommandItem(context.commandManager, CommandIDs::processFadeIn);
        menu.addCommandItem(context.commandManager, CommandIDs::processFadeOut);

        // --- Transform ---
        menu.addSectionHeader("Transform");
        menu.addCommandItem(context.commandManager, CommandIDs::processReverse);
        menu.addCommandItem(context.commandManager, CommandIDs::processInvert);
        menu.addCommandItem(context.commandManager, CommandIDs::processResample);
    }
    else if (menuIndex == 6) // Plugins menu (VST3/AU plugin chain)
    {
        // --- Plugin Chain ---
        menu.addSectionHeader("Plugin Chain");
        menu.addCommandItem(context.commandManager, CommandIDs::pluginShowChain);
        menu.addCommandItem(context.commandManager, CommandIDs::pluginApplyChain);
        menu.addCommandItem(context.commandManager, CommandIDs::pluginBypassAll);

        // --- Offline Processing ---
        menu.addSectionHeader("Offline Processing");
        menu.addCommandItem(context.commandManager, CommandIDs::pluginOffline);

        // --- Plugin Management ---
        menu.addSectionHeader("Plugin Management");
        menu.addCommandItem(context.commandManager, CommandIDs::pluginRescan);
        menu.addCommandItem(context.commandManager, CommandIDs::pluginShowSettings);
        menu.addCommandItem(context.commandManager, CommandIDs::pluginClearCache);
    }
    else if (menuIndex == 7) // Tools menu
    {
        // --- Channel Tools ---
        menu.addSectionHeader("Channel Tools");
        menu.addCommandItem(context.commandManager, CommandIDs::toolsChannelConverter);
        menu.addCommandItem(context.commandManager, CommandIDs::toolsChannelExtractor);

        // --- Processing ---
        menu.addSectionHeader("Processing");
        menu.addCommandItem(context.commandManager, CommandIDs::toolsHeadTail);
        menu.addCommandItem(context.commandManager, CommandIDs::toolsLoopingTools);

        // --- Batch Processing ---
        menu.addSectionHeader("Batch Processing");
        menu.addCommandItem(context.commandManager, CommandIDs::fileBatchProcessor);
    }
    else if (menuIndex == 8) // Playback menu
    {
        // --- Transport ---
        menu.addSectionHeader("Transport");
        menu.addCommandItem(context.commandManager, CommandIDs::playbackPlay);
        menu.addCommandItem(context.commandManager, CommandIDs::playbackPause);
        menu.addCommandItem(context.commandManager, CommandIDs::playbackStop);

        // --- Recording ---
        menu.addSectionHeader("Recording");
        menu.addCommandItem(context.commandManager, CommandIDs::playbackRecord);

        // --- Looping ---
        menu.addSectionHeader("Looping");
        menu.addCommandItem(context.commandManager, CommandIDs::playbackLoop);
        menu.addCommandItem(context.commandManager, CommandIDs::playbackLoopRegion);
    }
    else if (menuIndex == 9) // Help menu
    {
        menu.addCommandItem(context.commandManager, CommandIDs::helpCommandPalette);
        menu.addCommandItem(context.commandManager, CommandIDs::helpShortcuts);
        menu.addSeparator();
        menu.addItem("About WaveEdit", [context] { if (context.onShowAbout) context.onShowAbout(); });
    }

    return menu;
}
