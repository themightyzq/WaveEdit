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
#include "../Utils/Region.h"
#include <functional>

// Forward declarations
class Document;

/**
 * RegionController handles all region manipulation operations.
 * This controller manages region creation, deletion, merging, splitting,
 * and clipboard operations. All region display callbacks are wired here.
 *
 * Responsibilities:
 * - Region add/delete/merge/split operations
 * - Region clipboard (copy/paste)
 * - Region navigation (jump next/previous)
 * - Region selection (select all, inverse)
 * - Region display callback wiring
 * - Strip silence (auto-region) dialog
 * - Batch export dialog
 * - Region boundary nudging
 */
class RegionController
{
public:
    RegionController();
    ~RegionController() = default;

    //==========================================================================
    // Region Creation and Deletion

    void addRegionFromSelection(Document* doc);
    void deleteSelectedRegion(Document* doc);

    //==========================================================================
    // Region Navigation

    void jumpToNextRegion(Document* doc);
    void jumpToPreviousRegion(Document* doc);

    //==========================================================================
    // Region Selection

    void selectInverseOfRegions(Document* doc);
    void selectAllRegions(Document* doc);

    //==========================================================================
    // Region Merging and Splitting

    bool canMergeRegions(Document* doc) const;
    bool canSplitRegion(Document* doc) const;
    void mergeSelectedRegions(Document* doc);
    void splitRegionAtCursor(Document* doc);

    //==========================================================================
    // Region Clipboard

    void copyRegionsToClipboard(Document* doc);
    void pasteRegionsFromClipboard(Document* doc);
    bool hasRegionClipboard() const { return m_hasRegionClipboard; }

    //==========================================================================
    // Region Editing

    void nudgeRegionBoundary(Document* doc, bool nudgeStart, bool moveLeft);
    void showStripSilenceDialog(Document* doc, juce::Component* parent);
    void showBatchExportDialog(Document* doc, juce::Component* parent);

    //==========================================================================
    // Region List Panel Callbacks
    // These handle RegionListPanel::Listener events
    void handleRegionListJumpToRegion(Document* doc, int regionIndex);
    void handleRegionListRegionDeleted(Document* doc);
    void handleRegionListRegionRenamed(Document* doc);
    void handleRegionListRegionSelected(Document* doc, int regionIndex);
    void handleRegionListBatchRenameStart();
    void handleRegionListBatchRenameApply(Document* doc, const std::vector<int>& regionIndices,
                                          const std::vector<juce::String>& newNames);

    //==========================================================================
    // Callback Wiring
    // Call setupRegionCallbacks when a document is loaded/created
    void setupRegionCallbacks(Document* doc);

    //==========================================================================
    // UI Refresh Callback
    // Set a callback to refresh the region list panel when regions change
    void setOnRegionListRefreshNeeded(std::function<void()> callback)
    {
        m_onRegionListRefreshNeeded = callback;
    }

private:
    //==========================================================================
    // Member Variables

    std::vector<Region> m_regionClipboard;
    bool m_hasRegionClipboard = false;
    std::function<void()> m_onRegionListRefreshNeeded;

    //==========================================================================
    // Helper Methods

    /** Helper to create a region with auto-generated name */
    juce::String generateRegionName(Document* doc);
};
