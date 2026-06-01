/*
  ==============================================================================

    MarkerUndoActions.h
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
#include "../MarkerManager.h"
#include "../Marker.h"
#include "../../UI/MarkerDisplay.h"

//==============================================================================
/**
 * Undoable action for adding a marker.
 * Stores the marker data to enable undo/redo.
 */
class AddMarkerUndoAction : public juce::UndoableAction
{
public:
    AddMarkerUndoAction(MarkerManager& markerManager,
                       MarkerDisplay* markerDisplay,
                       const Marker& marker)
        : m_markerManager(markerManager),
          m_markerDisplay(markerDisplay),
          m_marker(marker)
    {
    }

    bool perform() override
    {
        // m_marker carries a stable ID; redo re-adds the same identity and
        // undo locates it by ID regardless of other markers added/removed (H9).
        m_markerManager.addMarker(m_marker);

        // Update display
        if (m_markerDisplay)
            m_markerDisplay->repaint();

        DBG("Added marker: " + m_marker.getName());
        return true;
    }

    bool undo() override
    {
        // Remove the marker located by its stable ID.
        int index = m_markerManager.getIndexOfMarkerId(m_marker.getId());
        if (index < 0)
        {
            DBG("AddMarkerUndoAction::undo - marker not found by id");
            return false;
        }

        m_markerManager.removeMarker(index);

        // Update display
        if (m_markerDisplay)
            m_markerDisplay->repaint();

        DBG("Undid marker addition: " + m_marker.getName());
        return true;
    }

    int getSizeInUnits() override { return sizeof(*this) + sizeof(Marker); }

private:
    MarkerManager& m_markerManager;
    MarkerDisplay* m_markerDisplay;
    Marker m_marker;  // Carries the stable ID used to locate it on undo
};

//==============================================================================
/**
 * Undoable action for deleting a marker.
 * Stores the deleted marker and its index to enable restoration.
 */
class DeleteMarkerUndoAction : public juce::UndoableAction
{
public:
    DeleteMarkerUndoAction(MarkerManager& markerManager,
                          MarkerDisplay* markerDisplay,
                          int markerIndex,
                          const Marker& marker)
        : m_markerManager(markerManager),
          m_markerDisplay(markerDisplay),
          m_markerIndex(markerIndex),
          m_deletedMarker(marker)
    {
    }

    bool perform() override
    {
        // Locate by stable ID so a list shift since construction can't make us
        // delete the wrong marker (H9). Remember the index for a faithful undo.
        int index = m_markerManager.getIndexOfMarkerId(m_deletedMarker.getId());
        if (index < 0)
        {
            DBG("DeleteMarkerUndoAction::perform - marker not found by id");
            return false;
        }

        m_markerIndex = index;
        m_markerManager.removeMarker(index);

        // Update display
        if (m_markerDisplay)
            m_markerDisplay->repaint();

        DBG("Deleted marker: " + m_deletedMarker.getName());
        return true;
    }

    bool undo() override
    {
        // Re-insert the stored marker (same ID), clamped to current size.
        int insertIndex = juce::jlimit(0, m_markerManager.getNumMarkers(), m_markerIndex);
        m_markerManager.insertMarkerAt(insertIndex, m_deletedMarker);

        // Update display
        if (m_markerDisplay)
            m_markerDisplay->repaint();

        DBG("Undid marker deletion: " + m_deletedMarker.getName());
        return true;
    }

    int getSizeInUnits() override { return sizeof(*this) + sizeof(Marker); }

private:
    MarkerManager& m_markerManager;
    MarkerDisplay* m_markerDisplay;
    int m_markerIndex;
    Marker m_deletedMarker;
};

//==============================================================================
/**
 * Undo action for converting regions to markers.
 * Stores the created markers for undo/redo.
 */
class RegionsToMarkersUndoAction : public juce::UndoableAction
{
public:
    RegionsToMarkersUndoAction(MarkerManager& markerManager,
                               MarkerDisplay* markerDisplay,
                               const juce::File& audioFile,
                               const juce::Array<Marker>& markersToCreate)
        : m_markerManager(markerManager),
          m_markerDisplay(markerDisplay),
          m_audioFile(audioFile),
          m_markers(markersToCreate)
    {
    }

    bool perform() override
    {
        // Add the exact markers we own (each carries a stable ID) so undo can
        // remove precisely those, regardless of other markers added in between
        // (H11). Markers re-sort on add, so "last N" removal is unsafe.
        for (const auto& marker : m_markers)
            m_markerManager.addMarker(marker);

        m_markerManager.saveToFile(m_audioFile);
        if (m_markerDisplay) m_markerDisplay->repaint();
        return true;
    }

    bool undo() override
    {
        // Remove by stable ID, not by position.
        for (const auto& marker : m_markers)
        {
            int idx = m_markerManager.getIndexOfMarkerId(marker.getId());
            if (idx >= 0)
                m_markerManager.removeMarker(idx);
        }
        m_markerManager.saveToFile(m_audioFile);
        if (m_markerDisplay) m_markerDisplay->repaint();
        return true;
    }

    int getSizeInUnits() override { return sizeof(*this) + m_markers.size() * sizeof(Marker); }

private:
    MarkerManager& m_markerManager;
    MarkerDisplay* m_markerDisplay;
    juce::File m_audioFile;
    juce::Array<Marker> m_markers;
};

//==============================================================================
/**
 * Undoable action for renaming a marker.
 * Stores old and new names to enable undo/redo.
 */
class RenameMarkerUndoAction : public juce::UndoableAction
{
public:
    RenameMarkerUndoAction(MarkerManager& markerManager,
                           MarkerDisplay* markerDisplay,
                           const juce::File& audioFile,
                           int markerIndex,
                           const juce::String& oldName,
                           const juce::String& newName)
        : m_markerManager(markerManager),
          m_markerDisplay(markerDisplay),
          m_audioFile(audioFile),
          m_oldName(oldName),
          m_newName(newName)
    {
        // Bind to the marker's stable ID so a later add/delete (markers
        // re-sort on add) cannot make undo rename the wrong marker (H9).
        if (const Marker* m = m_markerManager.getMarker(markerIndex))
            m_markerId = m->getId();
    }

    bool perform() override
    {
        if (auto* m = m_markerManager.getMarkerById(m_markerId))
        {
            m->setName(m_newName);
            m_markerManager.saveToFile(m_audioFile);
            if (m_markerDisplay) m_markerDisplay->repaint();
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (auto* m = m_markerManager.getMarkerById(m_markerId))
        {
            m->setName(m_oldName);
            m_markerManager.saveToFile(m_audioFile);
            if (m_markerDisplay) m_markerDisplay->repaint();
            return true;
        }
        return false;
    }

    int getSizeInUnits() override
    {
        return sizeof(*this) + m_oldName.length() + m_newName.length();
    }

private:
    MarkerManager& m_markerManager;
    MarkerDisplay* m_markerDisplay;
    juce::File m_audioFile;
    int64_t m_markerId = -1;
    juce::String m_oldName;
    juce::String m_newName;
};

//==============================================================================
/**
 * Undoable action for changing a marker's color.
 * Stores old and new colors to enable undo/redo.
 */
class ChangeMarkerColorUndoAction : public juce::UndoableAction
{
public:
    ChangeMarkerColorUndoAction(MarkerManager& markerManager,
                                MarkerDisplay* markerDisplay,
                                const juce::File& audioFile,
                                int markerIndex,
                                juce::Colour oldColor,
                                juce::Colour newColor)
        : m_markerManager(markerManager),
          m_markerDisplay(markerDisplay),
          m_audioFile(audioFile),
          m_oldColor(oldColor),
          m_newColor(newColor)
    {
        // Bind to the marker's stable ID (H9).
        if (const Marker* m = m_markerManager.getMarker(markerIndex))
            m_markerId = m->getId();
    }

    bool perform() override
    {
        if (auto* m = m_markerManager.getMarkerById(m_markerId))
        {
            m->setColor(m_newColor);
            m_markerManager.saveToFile(m_audioFile);
            if (m_markerDisplay) m_markerDisplay->repaint();
            return true;
        }
        return false;
    }

    bool undo() override
    {
        if (auto* m = m_markerManager.getMarkerById(m_markerId))
        {
            m->setColor(m_oldColor);
            m_markerManager.saveToFile(m_audioFile);
            if (m_markerDisplay) m_markerDisplay->repaint();
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return sizeof(*this) + sizeof(juce::Colour) * 2; }

private:
    MarkerManager& m_markerManager;
    MarkerDisplay* m_markerDisplay;
    juce::File m_audioFile;
    int64_t m_markerId = -1;
    juce::Colour m_oldColor;
    juce::Colour m_newColor;
};

//==============================================================================
/**
 * Undoable action for moving a marker to a new sample position (C4).
 *
 * Markers stay sorted by position, so moving one can change its index; the
 * action keys off the marker's stable ID and updates its position in place
 * via the manager's index-then-id lookup, then lets the list re-sort.
 */
class MoveMarkerUndoAction : public juce::UndoableAction
{
public:
    MoveMarkerUndoAction(MarkerManager& markerManager,
                         MarkerDisplay* markerDisplay,
                         const juce::File& audioFile,
                         int64_t markerId,
                         int64_t oldPosition,
                         int64_t newPosition)
        : m_markerManager(markerManager),
          m_markerDisplay(markerDisplay),
          m_audioFile(audioFile),
          m_markerId(markerId),
          m_oldPosition(oldPosition),
          m_newPosition(newPosition)
    {
    }

    bool perform() override { return moveTo(m_newPosition); }
    bool undo() override    { return moveTo(m_oldPosition); }

    int getSizeInUnits() override { return sizeof(*this); }

private:
    bool moveTo(int64_t position)
    {
        // Re-sort by removing and re-adding so the manager keeps positional
        // order. The re-added marker retains its stable ID.
        int index = m_markerManager.getIndexOfMarkerId(m_markerId);
        if (index < 0) return false;

        Marker* live = m_markerManager.getMarker(index);
        if (!live) return false;

        Marker moved = *live;        // preserves ID, name, color
        moved.setPosition(position);

        m_markerManager.removeMarker(index);
        m_markerManager.addMarker(moved);

        m_markerManager.saveToFile(m_audioFile);
        if (m_markerDisplay) m_markerDisplay->repaint();
        return true;
    }

    MarkerManager& m_markerManager;
    MarkerDisplay* m_markerDisplay;
    juce::File m_audioFile;
    int64_t m_markerId;
    int64_t m_oldPosition;
    int64_t m_newPosition;
};

//==============================================================================
/**
 * Undoable bulk add of imported markers (C4).
 *
 * Stores the imported markers (each with a stable ID) so undo can remove
 * exactly those, regardless of other markers added before the undo.
 */
class ImportMarkersUndoAction : public juce::UndoableAction
{
public:
    ImportMarkersUndoAction(MarkerManager& markerManager,
                            MarkerDisplay* markerDisplay,
                            const juce::File& audioFile,
                            const juce::Array<Marker>& markersToAdd)
        : m_markerManager(markerManager),
          m_markerDisplay(markerDisplay),
          m_audioFile(audioFile),
          m_markers(markersToAdd)
    {
    }

    bool perform() override
    {
        for (const auto& marker : m_markers)
            m_markerManager.addMarker(marker);

        m_markerManager.saveToFile(m_audioFile);
        if (m_markerDisplay) m_markerDisplay->repaint();
        return true;
    }

    bool undo() override
    {
        // Remove by stable ID, not by position.
        for (const auto& marker : m_markers)
        {
            int idx = m_markerManager.getIndexOfMarkerId(marker.getId());
            if (idx >= 0)
                m_markerManager.removeMarker(idx);
        }
        m_markerManager.saveToFile(m_audioFile);
        if (m_markerDisplay) m_markerDisplay->repaint();
        return true;
    }

    int getSizeInUnits() override { return sizeof(*this) + m_markers.size() * sizeof(Marker); }

private:
    MarkerManager& m_markerManager;
    MarkerDisplay* m_markerDisplay;
    juce::File m_audioFile;
    juce::Array<Marker> m_markers;
};
