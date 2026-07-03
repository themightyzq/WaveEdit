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
          m_oldStart(oldStart),
          m_oldEnd(oldEnd),
          m_newStart(newStart),
          m_newEnd(newEnd)
    {
        // Bind to the region's stable ID (H8).
        if (const Region* region = m_regionManager.getRegion(regionIndex))
            m_regionId = region->getId();
    }

    bool perform() override
    {
        // Resize the region (located by stable ID)
        Region* region = m_regionManager.getRegionById(m_regionId);
        if (region == nullptr)
        {
            DBG("ResizeRegionUndoAction::perform - region not found by id");
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
        // Restore the old boundaries (located by stable ID)
        Region* region = m_regionManager.getRegionById(m_regionId);
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
    int64_t m_regionId = -1;
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
          m_nudgeStart(nudgeStart),
          m_oldPosition(oldPosition),
          m_newPosition(newPosition)
    {
        jassert(regionIndex >= 0 && regionIndex < m_regionManager.getNumRegions());
        // Bind to the region's stable ID (H8).
        if (const Region* region = m_regionManager.getRegion(regionIndex))
            m_regionId = region->getId();
    }

    bool perform() override
    {
        Region* region = m_regionManager.getRegionById(m_regionId);
        if (!region) return false;

        if (m_nudgeStart) region->setStartSample(m_newPosition);
        else              region->setEndSample(m_newPosition);

        if (m_regionDisplay) m_regionDisplay->repaint();
        return true;
    }

    bool undo() override
    {
        Region* region = m_regionManager.getRegionById(m_regionId);
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
    int64_t m_regionId = -1;
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
          m_originalFirst(originalFirst),
          m_originalSecond(originalSecond)
    {
        // Bind to stable IDs so a later add/delete cannot make undo merge or
        // restore the wrong pair (H8). mergeRegions() keeps the first region's
        // identity; the second region is removed by the merge.
        if (const Region* first = m_regionManager.getRegion(firstIndex))
            m_firstId = first->getId();
        if (const Region* second = m_regionManager.getRegion(secondIndex))
            m_secondId = second->getId();
    }

    bool perform() override
    {
        const int firstIndex  = m_regionManager.getIndexOfRegionId(m_firstId);
        const int secondIndex = m_regionManager.getIndexOfRegionId(m_secondId);
        if (firstIndex < 0 || secondIndex < 0) return false;

        const bool success = m_regionManager.mergeRegions(firstIndex, secondIndex);
        if (success && m_regionDisplay) m_regionDisplay->repaint();
        return success;
    }

    bool undo() override
    {
        // The merged region retains the first region's stable ID.
        Region* mergedRegion = m_regionManager.getRegionById(m_firstId);
        if (!mergedRegion) return false;

        const int firstIndex = m_regionManager.getIndexOfRegionId(m_firstId);

        mergedRegion->setName(m_originalFirst.getName());
        mergedRegion->setEndSample(m_originalFirst.getEndSample());

        // Re-insert the removed second region just after the first, clamped to
        // the current list size (order-independent of other edits).
        int insertIndex = juce::jlimit(0, m_regionManager.getNumRegions(), firstIndex + 1);
        m_regionManager.insertRegionAt(insertIndex, m_originalSecond);

        if (m_regionDisplay) m_regionDisplay->repaint();
        return true;
    }

    int getSizeInUnits() override { return sizeof(*this); }

private:
    RegionManager& m_regionManager;
    RegionDisplay* m_regionDisplay;
    int64_t m_firstId = -1, m_secondId = -1;
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
          m_originalRegions(originalRegions)
    {
    }

    bool perform() override
    {
        // Merge using RegionManager's multi-selection merge. The merge leaves
        // the new region selected as primary, so we capture its stable ID to
        // locate it on undo regardless of any later list changes.
        if (!m_regionManager.mergeSelectedRegions())
        {
            DBG("MultiMergeRegionsUndoAction::perform() - Merge failed");
            return false;
        }

        int mergedIndex = m_regionManager.getPrimarySelectionIndex();
        if (const Region* merged = m_regionManager.getRegion(mergedIndex))
            m_mergedRegionId = merged->getId();

        // Save to sidecar JSON file
        m_regionManager.saveToFile(m_audioFile);

        // Update display
        m_regionDisplay.repaint();

        DBG("Merged " + juce::String(m_originalRegions.size()) + " regions");
        return true;
    }

    bool undo() override
    {
        // Remove the merged region by its stable ID.
        int mergedIndex = m_regionManager.getIndexOfRegionId(m_mergedRegionId);
        if (mergedIndex >= 0)
            m_regionManager.removeRegion(mergedIndex);

        // Restore original regions. H10: insert in ascending index order and
        // clamp each target to the current list size so earlier insertions
        // shift later ones into place instead of corrupting the order. The
        // restored regions keep their original stable IDs.
        juce::Array<int> order;
        for (int i = 0; i < m_originalIndices.size(); ++i)
            order.add(i);

        std::sort(order.begin(), order.end(),
                  [this](int a, int b)
                  { return m_originalIndices[a] < m_originalIndices[b]; });

        for (int slot : order)
        {
            int target = juce::jlimit(0, m_regionManager.getNumRegions(),
                                      m_originalIndices[slot]);
            m_regionManager.insertRegionAt(target, m_originalRegions[slot]);
        }

        // Save to sidecar JSON file
        m_regionManager.saveToFile(m_audioFile);

        // Update display
        m_regionDisplay.repaint();

        DBG("Undid merge of " + juce::String(m_originalRegions.size()) + " regions");
        return true;
    }

    int getSizeInUnits() override
    {
        return static_cast<int>(sizeof(*this) + static_cast<size_t>(m_originalRegions.size()) * sizeof(Region));
    }

private:
    RegionManager& m_regionManager;
    RegionDisplay& m_regionDisplay;
    juce::File m_audioFile;
    juce::Array<int> m_originalIndices;  // Original indices of regions being merged
    juce::Array<Region> m_originalRegions;  // Original regions before merge
    int64_t m_mergedRegionId = -1;  // Stable ID of the merged region
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
          m_splitSample(splitSample),
          m_originalRegion(originalRegion)
    {
        // Bind to the region's stable ID (H8). splitRegion() keeps the first
        // half's identity and inserts the second half right after it.
        if (const Region* region = m_regionManager.getRegion(regionIndex))
            m_regionId = region->getId();
    }

    bool perform() override
    {
        const int regionIndex = m_regionManager.getIndexOfRegionId(m_regionId);
        if (regionIndex < 0) return false;

        const bool success = m_regionManager.splitRegion(regionIndex, m_splitSample);
        if (success)
        {
            // Capture the newly created second half's stable ID so undo can
            // remove it by ID rather than by recomputed array position (H8).
            // splitRegion() inserts the second half immediately after the
            // first, which still holds m_regionId.
            const int firstIndex = m_regionManager.getIndexOfRegionId(m_regionId);
            if (const Region* secondHalf = m_regionManager.getRegion(firstIndex + 1))
                m_secondHalfId = secondHalf->getId();

            if (m_regionDisplay) m_regionDisplay->repaint();
        }
        return success;
    }

    bool undo() override
    {
        // The first half keeps the original stable ID; the second half was
        // captured by stable ID during perform(). Remove the second half by
        // ID (H8 -- do NOT trust firstIndex + 1, which an interleaved region
        // add/delete could point at an unrelated region), then restore the
        // first half's original boundaries.
        const int secondIndex = m_regionManager.getIndexOfRegionId(m_secondHalfId);
        if (secondIndex >= 0)
            m_regionManager.removeRegion(secondIndex);

        Region* firstHalf = m_regionManager.getRegionById(m_regionId);
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
    int64_t m_regionId = -1;
    int64_t m_secondHalfId = -1;  // Stable ID of the second half (captured on perform)
    int64_t m_splitSample;
    Region m_originalRegion;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SplitRegionUndoAction)
};
