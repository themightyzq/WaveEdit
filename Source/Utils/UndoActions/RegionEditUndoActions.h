/*
  ==============================================================================

    RegionEditUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Region boundary-edit undo actions split out of RegionUndoActions.h
    per CLAUDE.md §7.5: resize, nudge, merge (pair + multi), split.

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
            DBG("ResizeRegionUndoAction::perform - Invalid region index");
            return false;
        }

        region->setStartSample(m_newStart);
        region->setEndSample(m_newEnd);

        // Save to sidecar JSON file
        m_regionManager.saveToFile(m_audioFile);

        // Update display
        m_regionDisplay.repaint();

        DBG(juce::String::formatted(
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

            DBG("Undid region resize");
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

//==============================================================================
/**
 * Undoable region boundary nudge operation.
 * Stores the old and new boundary positions for a region.
 */
class NudgeRegionUndoAction : public juce::UndoableAction
{
public:
    NudgeRegionUndoAction(RegionManager& regionManager,
                          RegionDisplay* regionDisplay,
                          int regionIndex,
                          bool nudgeStart,
                          int64_t oldPosition,
                          int64_t newPosition)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_regionIndex(regionIndex),
          m_nudgeStart(nudgeStart),
          m_oldPosition(oldPosition),
          m_newPosition(newPosition)
    {
        jassert(regionIndex >= 0 && regionIndex < m_regionManager.getNumRegions());
    }

    bool perform() override
    {
        Region* region = m_regionManager.getRegion(m_regionIndex);
        if (!region) return false;

        if (m_nudgeStart) region->setStartSample(m_newPosition);
        else              region->setEndSample(m_newPosition);

        if (m_regionDisplay) m_regionDisplay->repaint();
        return true;
    }

    bool undo() override
    {
        Region* region = m_regionManager.getRegion(m_regionIndex);
        if (!region) return false;

        if (m_nudgeStart) region->setStartSample(m_oldPosition);
        else              region->setEndSample(m_oldPosition);

        if (m_regionDisplay) m_regionDisplay->repaint();
        return true;
    }

    int getSizeInUnits() override { return sizeof(*this); }

private:
    RegionManager& m_regionManager;
    RegionDisplay* m_regionDisplay;
    int m_regionIndex;
    bool m_nudgeStart;
    int64_t m_oldPosition;
    int64_t m_newPosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NudgeRegionUndoAction)
};

//==============================================================================
/**
 * Undoable merge of two adjacent regions.
 * Stores the original two regions so undo can restore them.
 */
class MergeRegionsUndoAction : public juce::UndoableAction
{
public:
    MergeRegionsUndoAction(RegionManager& regionManager,
                           RegionDisplay* regionDisplay,
                           int firstIndex,
                           int secondIndex,
                           const Region& originalFirst,
                           const Region& originalSecond)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_firstIndex(firstIndex),
          m_secondIndex(secondIndex),
          m_originalFirst(originalFirst),
          m_originalSecond(originalSecond)
    {
    }

    bool perform() override
    {
        const bool success = m_regionManager.mergeRegions(m_firstIndex, m_secondIndex);
        if (success && m_regionDisplay) m_regionDisplay->repaint();
        return success;
    }

    bool undo() override
    {
        Region* mergedRegion = m_regionManager.getRegion(m_firstIndex);
        if (!mergedRegion) return false;

        if (m_secondIndex > m_regionManager.getNumRegions())
        {
            DBG("Warning: Cannot undo merge - region index out of bounds");
            return false;
        }

        mergedRegion->setName(m_originalFirst.getName());
        mergedRegion->setEndSample(m_originalFirst.getEndSample());
        m_regionManager.insertRegionAt(m_secondIndex, m_originalSecond);

        if (m_regionDisplay) m_regionDisplay->repaint();
        return true;
    }

    int getSizeInUnits() override { return sizeof(*this); }

private:
    RegionManager& m_regionManager;
    RegionDisplay* m_regionDisplay;
    int m_firstIndex, m_secondIndex;
    Region m_originalFirst, m_originalSecond;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MergeRegionsUndoAction)
};

//==============================================================================
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
            DBG("MultiMergeRegionsUndoAction::perform() - Merge failed");
            return false;
        }

        // Store the index of the merged region (should be where first region was)
        m_mergedRegionIndex = m_originalIndices[0];

        // Save to sidecar JSON file
        m_regionManager.saveToFile(m_audioFile);

        // Update display
        m_regionDisplay.repaint();

        DBG("Merged " + juce::String(m_originalRegions.size()) + " regions");
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

            DBG("Undid merge of " + juce::String(m_originalRegions.size()) + " regions");
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

//==============================================================================
/**
 * Undoable split of a region.
 * Stores the original region so undo can restore it.
 */
class SplitRegionUndoAction : public juce::UndoableAction
{
public:
    SplitRegionUndoAction(RegionManager& regionManager,
                          RegionDisplay* regionDisplay,
                          int regionIndex,
                          int64_t splitSample,
                          const Region& originalRegion)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_regionIndex(regionIndex),
          m_splitSample(splitSample),
          m_originalRegion(originalRegion)
    {
    }

    bool perform() override
    {
        const bool success = m_regionManager.splitRegion(m_regionIndex, m_splitSample);
        if (success && m_regionDisplay) m_regionDisplay->repaint();
        return success;
    }

    bool undo() override
    {
        m_regionManager.removeRegion(m_regionIndex + 1);

        Region* firstHalf = m_regionManager.getRegion(m_regionIndex);
        if (!firstHalf) return false;

        firstHalf->setName(m_originalRegion.getName());
        firstHalf->setEndSample(m_originalRegion.getEndSample());

        if (m_regionDisplay) m_regionDisplay->repaint();
        return true;
    }

    int getSizeInUnits() override { return sizeof(*this); }

private:
    RegionManager& m_regionManager;
    RegionDisplay* m_regionDisplay;
    int m_regionIndex;
    int64_t m_splitSample;
    Region m_originalRegion;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SplitRegionUndoAction)
};
