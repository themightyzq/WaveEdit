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

        DBG("Auto Region: Created " + juce::String(m_newRegions.size()) + " regions");
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

        DBG("Undid Auto Region: Restored " + juce::String(m_oldRegions.size()) + " original regions");
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
        for (const auto& region : m_regions)
        {
            m_regionManager.addRegion(region);
        }
        m_regionManager.saveToFile(m_audioFile);
        m_regionDisplay.repaint();
        return true;
    }

    bool undo() override
    {
        // Remove the last N regions (the ones we added)
        int total = m_regionManager.getNumRegions();
        for (int i = m_regions.size() - 1; i >= 0; --i)
        {
            int idx = total - m_regions.size() + i;
            if (idx >= 0 && idx < m_regionManager.getNumRegions())
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
