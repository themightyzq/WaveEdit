/*
  ==============================================================================

    RegionLifecycleUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Region lifecycle / metadata undo actions split out of
    RegionUndoActions.h per CLAUDE.md §7.5: add, delete, rename
    (single + batch), color change.

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
          m_region(region)
    {
    }

    bool perform() override
    {
        // Add region to manager. m_region carries a stable ID, so redo
        // re-adds the same identity and undo can find it regardless of
        // any other regions added/removed in between (H8).
        m_regionManager.addRegion(m_region);

        // Save to sidecar JSON file
        m_regionManager.saveToFile(m_audioFile);

        // Update display
        m_regionDisplay.repaint();

        DBG("Added region: " + m_region.getName());
        return true;
    }

    bool undo() override
    {
        // Remove the region we added, located by its stable ID.
        int index = m_regionManager.getIndexOfRegionId(m_region.getId());
        if (index >= 0)
        {
            m_regionManager.removeRegion(index);

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
    Region m_region;  // Carries the stable ID used to locate it on undo
};

//==============================================================================
/**
 * Undoable action for pasting one or more regions from the clipboard (C3).
 *
 * Stores the pasted regions (each carrying a stable ID) so undo removes
 * exactly those, regardless of other regions added before the undo.
 */
class PasteRegionsUndoAction : public juce::UndoableAction
{
public:
    PasteRegionsUndoAction(RegionManager& regionManager,
                           RegionDisplay& regionDisplay,
                           const juce::File& audioFile,
                           const juce::Array<Region>& regionsToPaste)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_audioFile(audioFile),
          m_regions(regionsToPaste)
    {
    }

    bool perform() override
    {
        for (const auto& region : m_regions)
            m_regionManager.addRegion(region);

        m_regionManager.saveToFile(m_audioFile);
        m_regionDisplay.repaint();

        DBG("Pasted " + juce::String(m_regions.size()) + " region(s)");
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

        DBG("Undid paste of " + juce::String(m_regions.size()) + " region(s)");
        return true;
    }

    int getSizeInUnits() override { return static_cast<int>(sizeof(*this) + static_cast<size_t>(m_regions.size()) * sizeof(Region)); }

private:
    RegionManager& m_regionManager;
    RegionDisplay& m_regionDisplay;
    juce::File m_audioFile;
    juce::Array<Region> m_regions;
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
          m_deletedRegion("", 0, 0)  // Filled from the index below
    {
        // Capture the region (including its stable ID) at construction so the
        // restored region keeps its identity and re-insert position even if
        // the list shifts before perform()/undo() (H8).
        if (const Region* region = m_regionManager.getRegion(m_regionIndex))
            m_deletedRegion = *region;
    }

    bool perform() override
    {
        // Locate by stable ID so a list shift since construction can't make
        // us delete the wrong region.
        int index = m_regionManager.getIndexOfRegionId(m_deletedRegion.getId());
        if (index < 0)
        {
            DBG("DeleteRegionUndoAction::perform - region not found by id");
            return false;
        }

        m_regionIndex = index;  // Remember where it was for a faithful undo
        m_regionManager.removeRegion(index);

        // Save to sidecar JSON file
        m_regionManager.saveToFile(m_audioFile);

        // Update display
        m_regionDisplay.repaint();

        DBG("Deleted region: " + m_deletedRegion.getName());
        return true;
    }

    bool undo() override
    {
        // Re-insert the stored region (same ID) at its original index, clamped
        // to the current list size in case other regions were removed.
        int insertIndex = juce::jlimit(0, m_regionManager.getNumRegions(), m_regionIndex);
        m_regionManager.insertRegionAt(insertIndex, m_deletedRegion);

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
          m_oldName(oldName),
          m_newName(newName)
    {
        // Bind to the region's stable ID so a later add/delete that shifts the
        // list cannot make this rename hit a different region (H8).
        if (const Region* region = m_regionManager.getRegion(regionIndex))
            m_regionId = region->getId();
    }

    bool perform() override
    {
        // Rename the region (located by stable ID)
        Region* region = m_regionManager.getRegionById(m_regionId);
        if (region == nullptr)
        {
            DBG("RenameRegionUndoAction::perform - region not found by id");
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
        // Restore the old name (located by stable ID)
        Region* region = m_regionManager.getRegionById(m_regionId);
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

    int getSizeInUnits() override { return static_cast<int>(sizeof(*this)) + m_oldName.length() + m_newName.length(); }

private:
    RegionManager& m_regionManager;
    RegionDisplay& m_regionDisplay;
    juce::File m_audioFile;
    int64_t m_regionId = -1;
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
          m_oldColor(oldColor),
          m_newColor(newColor)
    {
        // Bind to the region's stable ID (H8).
        if (const Region* region = m_regionManager.getRegion(regionIndex))
            m_regionId = region->getId();
    }

    bool perform() override
    {
        // Change the region color (located by stable ID)
        Region* region = m_regionManager.getRegionById(m_regionId);
        if (region == nullptr)
        {
            DBG("ChangeRegionColorUndoAction::perform - region not found by id");
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
        // Restore the old color (located by stable ID)
        Region* region = m_regionManager.getRegionById(m_regionId);
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
    int64_t m_regionId = -1;
    juce::Colour m_oldColor;
    juce::Colour m_newColor;
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
          m_oldNames(oldNames),
          m_newNames(newNames)
    {
        jassert(regionIndices.size() == oldNames.size());
        jassert(regionIndices.size() == newNames.size());

        // Resolve each index to its region's stable ID at construction so the
        // batch rename stays bound to the right regions even if the list is
        // reordered before perform()/undo() (H8). An empty selection is a
        // valid no-op.
        m_regionIds.reserve(regionIndices.size());
        for (int index : regionIndices)
        {
            const Region* region = m_regionManager.getRegion(index);
            m_regionIds.push_back(region ? region->getId() : (int64_t) -1);
        }
    }

    bool perform() override
    {
        for (size_t i = 0; i < m_regionIds.size(); ++i)
        {
            Region* region = m_regionManager.getRegionById(m_regionIds[i]);
            if (!region) return false;
            region->setName(m_newNames[i]);
        }

        if (m_regionDisplay) m_regionDisplay->repaint();
        return true;
    }

    bool undo() override
    {
        for (size_t i = 0; i < m_regionIds.size(); ++i)
        {
            Region* region = m_regionManager.getRegionById(m_regionIds[i]);
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
    std::vector<int64_t> m_regionIds;
    std::vector<juce::String> m_oldNames;
    std::vector<juce::String> m_newNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BatchRenameRegionUndoAction)
};
