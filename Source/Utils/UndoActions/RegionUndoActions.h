/*
  ==============================================================================

    RegionUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

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
 * Undoable action for adding a region.
 * Stores the region data to enable undo/redo.
 */
class AddRegionUndoAction : public juce::UndoableAction
{
public:
    AddRegionUndoAction(RegionManager& regionManager,
                       RegionDisplay& regionDisplay,
                       const juce::File& audioFile,
                       const Region& region)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_audioFile(audioFile),
          m_region(region),
          m_regionIndex(-1)
    {
    }

    bool perform() override
    {
        // Add region to manager
        m_regionManager.addRegion(m_region);
        m_regionIndex = m_regionManager.getNumRegions() - 1;

        // Save to sidecar JSON file
        m_regionManager.saveToFile(m_audioFile);

        // Update display
        m_regionDisplay.repaint();

        DBG("Added region: " + m_region.getName());
        return true;
    }

    bool undo() override
    {
        // Remove the region we added
        if (m_regionIndex >= 0 && m_regionIndex < m_regionManager.getNumRegions())
        {
            m_regionManager.removeRegion(m_regionIndex);

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            DBG("Undid region addition: " + m_region.getName());
        }
        return true;
    }

    int getSizeInUnits() override { return sizeof(*this) + sizeof(Region); }

private:
    RegionManager& m_regionManager;
    RegionDisplay& m_regionDisplay;
    juce::File m_audioFile;
    Region m_region;
    int m_regionIndex;  // Index where region was added
};

//==============================================================================
/**
 * Undoable action for deleting a region.
 * Stores the deleted region and its index to enable restoration.
 */
class DeleteRegionUndoAction : public juce::UndoableAction
{
public:
    DeleteRegionUndoAction(RegionManager& regionManager,
                          RegionDisplay& regionDisplay,
                          const juce::File& audioFile,
                          int regionIndex)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_audioFile(audioFile),
          m_regionIndex(regionIndex),
          m_deletedRegion("", 0, 0)  // Placeholder, will be filled in perform()
    {
    }

    bool perform() override
    {
        // Store the region before deleting it
        const Region* region = m_regionManager.getRegion(m_regionIndex);
        if (region == nullptr)
        {
            DBG("DeleteRegionUndoAction::perform - Invalid region index");
            return false;
        }

        // Copy the region data
        m_deletedRegion = *region;

        // Delete the region
        m_regionManager.removeRegion(m_regionIndex);

        // Save to sidecar JSON file
        m_regionManager.saveToFile(m_audioFile);

        // Update display
        m_regionDisplay.repaint();

        DBG("Deleted region: " + m_deletedRegion.getName());
        return true;
    }

    bool undo() override
    {
        // Re-insert the region at its original index
        m_regionManager.insertRegionAt(m_regionIndex, m_deletedRegion);

        // Save to sidecar JSON file
        m_regionManager.saveToFile(m_audioFile);

        // Update display
        m_regionDisplay.repaint();

        DBG("Undid region deletion: " + m_deletedRegion.getName());
        return true;
    }

    int getSizeInUnits() override { return sizeof(*this) + sizeof(Region); }

private:
    RegionManager& m_regionManager;
    RegionDisplay& m_regionDisplay;
    juce::File m_audioFile;
    int m_regionIndex;
    Region m_deletedRegion;
};

//==============================================================================
/**
 * Undoable action for renaming a region.
 * Stores old and new names to enable undo/redo.
 */
class RenameRegionUndoAction : public juce::UndoableAction
{
public:
    RenameRegionUndoAction(RegionManager& regionManager,
                          RegionDisplay& regionDisplay,
                          const juce::File& audioFile,
                          int regionIndex,
                          const juce::String& oldName,
                          const juce::String& newName)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_audioFile(audioFile),
          m_regionIndex(regionIndex),
          m_oldName(oldName),
          m_newName(newName)
    {
    }

    bool perform() override
    {
        // Rename the region
        Region* region = m_regionManager.getRegion(m_regionIndex);
        if (region == nullptr)
        {
            DBG("RenameRegionUndoAction::perform - Invalid region index");
            return false;
        }

        region->setName(m_newName);

        // Save to sidecar JSON file
        m_regionManager.saveToFile(m_audioFile);

        // Update display
        m_regionDisplay.repaint();

        DBG("Renamed region from '" + m_oldName + "' to '" + m_newName + "'");
        return true;
    }

    bool undo() override
    {
        // Restore the old name
        Region* region = m_regionManager.getRegion(m_regionIndex);
        if (region != nullptr)
        {
            region->setName(m_oldName);

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            DBG("Undid region rename: restored '" + m_oldName + "'");
        }
        return true;
    }

    int getSizeInUnits() override { return sizeof(*this) + m_oldName.length() + m_newName.length(); }

private:
    RegionManager& m_regionManager;
    RegionDisplay& m_regionDisplay;
    juce::File m_audioFile;
    int m_regionIndex;
    juce::String m_oldName;
    juce::String m_newName;
};

//==============================================================================
/**
 * Undoable action for changing a region's color.
 * Stores old and new colors to enable undo/redo.
 */
class ChangeRegionColorUndoAction : public juce::UndoableAction
{
public:
    ChangeRegionColorUndoAction(RegionManager& regionManager,
                               RegionDisplay& regionDisplay,
                               const juce::File& audioFile,
                               int regionIndex,
                               const juce::Colour& oldColor,
                               const juce::Colour& newColor)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_audioFile(audioFile),
          m_regionIndex(regionIndex),
          m_oldColor(oldColor),
          m_newColor(newColor)
    {
    }

    bool perform() override
    {
        // Change the region color
        Region* region = m_regionManager.getRegion(m_regionIndex);
        if (region == nullptr)
        {
            DBG("ChangeRegionColorUndoAction::perform - Invalid region index");
            return false;
        }

        region->setColor(m_newColor);

        // Save to sidecar JSON file
        m_regionManager.saveToFile(m_audioFile);

        // Update display
        m_regionDisplay.repaint();

        DBG("Changed region color");
        return true;
    }

    bool undo() override
    {
        // Restore the old color
        Region* region = m_regionManager.getRegion(m_regionIndex);
        if (region != nullptr)
        {
            region->setColor(m_oldColor);

            // Save to sidecar JSON file
            m_regionManager.saveToFile(m_audioFile);

            // Update display
            m_regionDisplay.repaint();

            DBG("Undid region color change");
        }
        return true;
    }

    int getSizeInUnits() override { return sizeof(*this) + sizeof(juce::Colour) * 2; }

private:
    RegionManager& m_regionManager;
    RegionDisplay& m_regionDisplay;
    juce::File m_audioFile;
    int m_regionIndex;
    juce::Colour m_oldColor;
    juce::Colour m_newColor;
};

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
 * Undoable batch rename for multiple regions.
 * Stores old and new names for all renamed regions.
 */
class BatchRenameRegionUndoAction : public juce::UndoableAction
{
public:
    BatchRenameRegionUndoAction(RegionManager& regionManager,
                                 RegionDisplay* regionDisplay,
                                 const std::vector<int>& regionIndices,
                                 const std::vector<juce::String>& oldNames,
                                 const std::vector<juce::String>& newNames)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_regionIndices(regionIndices),
          m_oldNames(oldNames),
          m_newNames(newNames)
    {
        jassert(regionIndices.size() == oldNames.size());
        jassert(regionIndices.size() == newNames.size());
        jassert(!regionIndices.empty());
    }

    bool perform() override
    {
        for (size_t i = 0; i < m_regionIndices.size(); ++i)
        {
            Region* region = m_regionManager.getRegion(m_regionIndices[i]);
            if (!region) return false;
            region->setName(m_newNames[i]);
        }

        if (m_regionDisplay) m_regionDisplay->repaint();
        return true;
    }

    bool undo() override
    {
        for (size_t i = 0; i < m_regionIndices.size(); ++i)
        {
            Region* region = m_regionManager.getRegion(m_regionIndices[i]);
            if (!region) return false;
            region->setName(m_oldNames[i]);
        }

        if (m_regionDisplay) m_regionDisplay->repaint();
        return true;
    }

    int getSizeInUnits() override
    {
        int size = sizeof(*this);
        for (const auto& name : m_oldNames) size += name.length();
        for (const auto& name : m_newNames) size += name.length();
        return size;
    }

private:
    RegionManager& m_regionManager;
    RegionDisplay* m_regionDisplay;
    std::vector<int> m_regionIndices;
    std::vector<juce::String> m_oldNames;
    std::vector<juce::String> m_newNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BatchRenameRegionUndoAction)
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
