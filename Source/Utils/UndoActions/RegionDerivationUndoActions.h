/*
  ==============================================================================

    RegionDerivationUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Region-derivation undo actions split out of RegionUndoActions.h per
    CLAUDE.md §7.5: actions that derive regions from other data
    (auto-region from silence detection, marker → region conversion).

    Reach this header through the umbrella `RegionUndoActions.h`.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../RegionManager.h"
#include "../Region.h"
#include "../../UI/RegionDisplay.h"

//==============================================================================
// NOTE (C13): The former `StripSilenceUndoAction` was removed. It stored a
// `const juce::AudioBuffer<float>&` reference to the document's live buffer,
// which dangles after a file reload swaps the buffer -> use-after-free on undo.
// It was dead code (never instantiated; the dialog wires the safe
// RetrospectiveStripSilenceUndoAction below, which stores region snapshots by
// value). If an apply-from-scratch variant is ever needed, copy the buffer by
// value instead of holding a reference.

//==============================================================================
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

        DBG("Redo Auto Region: Restored " + juce::String(m_newRegions.size()) + " regions");
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

        DBG("Undo Auto Region: Restored " + juce::String(m_oldRegions.size()) + " original regions");
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
/**
 * Undo action for converting markers to regions.
 * Stores the created regions for undo/redo.
 */
class MarkersToRegionsUndoAction : public juce::UndoableAction
{
public:
    MarkersToRegionsUndoAction(RegionManager& regionManager,
                               RegionDisplay& regionDisplay,
                               const juce::File& audioFile,
                               const juce::Array<Region>& regionsToCreate)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_audioFile(audioFile),
          m_regions(regionsToCreate)
    {
    }

    bool perform() override
    {
        // Add the exact regions we own (each carries a stable ID) so undo can
        // remove precisely those, regardless of other regions added in between
        // (H11). "Last N" removal is unsafe.
        for (const auto& region : m_regions)
            m_regionManager.addRegion(region);

        m_regionManager.saveToFile(m_audioFile);
        m_regionDisplay.repaint();
        return true;
    }

    bool undo() override
    {
        // Remove by stable ID, not by position.
        for (const auto& region : m_regions)
        {
            int idx = m_regionManager.getIndexOfRegionId(region.getId());
            if (idx >= 0)
                m_regionManager.removeRegion(idx);
        }
        m_regionManager.saveToFile(m_audioFile);
        m_regionDisplay.repaint();
        return true;
    }

    int getSizeInUnits() override { return sizeof(*this) + m_regions.size() * sizeof(Region); }

private:
    RegionManager& m_regionManager;
    RegionDisplay& m_regionDisplay;
    juce::File m_audioFile;
    juce::Array<Region> m_regions;
};
