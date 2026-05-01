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
          m_marker(marker),
          m_markerIndex(-1)
    {
    }

    bool perform() override
    {
        // Add marker and store the index
        m_markerIndex = m_markerManager.addMarker(m_marker);

        // Update display
        if (m_markerDisplay)
            m_markerDisplay->repaint();

        DBG("Added marker: " + m_marker.getName());
        return true;
    }

    bool undo() override
    {
        if (m_markerIndex < 0)
        {
            DBG("AddMarkerUndoAction::undo - Invalid marker index");
            return false;
        }

        // Remove the marker
        m_markerManager.removeMarker(m_markerIndex);

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
    Marker m_marker;
    int m_markerIndex;  // Index where marker was added
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
        // Delete the marker
        m_markerManager.removeMarker(m_markerIndex);

        // Update display
        if (m_markerDisplay)
            m_markerDisplay->repaint();

        DBG("Deleted marker: " + m_deletedMarker.getName());
        return true;
    }

    bool undo() override
    {
        // Re-insert the marker at its original index
        m_markerManager.insertMarkerAt(m_markerIndex, m_deletedMarker);

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
        for (const auto& marker : m_markers)
        {
            m_markerManager.addMarker(marker);
        }
        m_markerManager.saveToFile(m_audioFile);
        if (m_markerDisplay) m_markerDisplay->repaint();
        return true;
    }

    bool undo() override
    {
        // Remove the last N markers (the ones we added)
        int total = m_markerManager.getNumMarkers();
        for (int i = m_markers.size() - 1; i >= 0; --i)
        {
            int idx = total - m_markers.size() + i;
            if (idx >= 0 && idx < m_markerManager.getNumMarkers())
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
          m_markerIndex(markerIndex),
          m_oldName(oldName),
          m_newName(newName)
    {
    }

    bool perform() override
    {
        if (auto* m = m_markerManager.getMarker(m_markerIndex))
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
        if (auto* m = m_markerManager.getMarker(m_markerIndex))
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
    int m_markerIndex;
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
          m_markerIndex(markerIndex),
          m_oldColor(oldColor),
          m_newColor(newColor)
    {
    }

    bool perform() override
    {
        if (auto* m = m_markerManager.getMarker(m_markerIndex))
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
        if (auto* m = m_markerManager.getMarker(m_markerIndex))
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
    int m_markerIndex;
    juce::Colour m_oldColor;
    juce::Colour m_newColor;
};
