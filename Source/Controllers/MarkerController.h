/*
  ==============================================================================

    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

// Forward declarations
class Document;

/**
 * MarkerController handles all marker manipulation operations.
 * This controller manages marker creation, deletion, navigation, and renaming.
 * All marker display callbacks are wired here.
 *
 * Responsibilities:
 * - Marker add/delete operations
 * - Marker navigation (jump next/previous)
 * - Marker display callback wiring
 * - Marker name/color/position changes
 */
class MarkerController
{
public:
    MarkerController();
    ~MarkerController() = default;

    //==========================================================================
    // Marker Creation and Deletion

    void addMarkerAtCursor(Document* doc, double lastClickTimeInSeconds, bool hasLastClickPosition);
    void deleteSelectedMarker(Document* doc);

    //==========================================================================
    // Marker Navigation

    void jumpToNextMarker(Document* doc);
    void jumpToPreviousMarker(Document* doc);

    //==========================================================================
    // Marker-Region Conversion

    void convertMarkersToRegions(Document* doc);
    void convertRegionsToMarkers(Document* doc);

    //==========================================================================
    // Marker List Panel Callbacks
    // These handle MarkerListPanel::Listener events
    void handleMarkerListJumpToMarker(Document* doc, int markerIndex);
    void handleMarkerListMarkerDeleted(Document* doc);
    void handleMarkerListMarkerRenamed(Document* doc);
    void handleMarkerListMarkerSelected(Document* doc, int markerIndex);

    //==========================================================================
    // Callback Wiring
    // Call setupMarkerCallbacks when a document is loaded/created
    void setupMarkerCallbacks(Document* doc);

    //==========================================================================
    // UI Refresh Callback
    // Set a callback to refresh the marker list panel when markers change
    void setOnMarkerListRefreshNeeded(std::function<void()> callback)
    {
        m_onMarkerListRefreshNeeded = callback;
    }

private:
    //==========================================================================
    // Member Variables

    std::function<void()> m_onMarkerListRefreshNeeded;
};
