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
