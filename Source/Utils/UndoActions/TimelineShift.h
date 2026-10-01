/*
  ==============================================================================

    TimelineShift.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Shared region/marker bookkeeping for edits that insert audio into the
    timeline: InsertAction (paste/generate/record at the cursor) and
    ApplyPluginChainAction when an effect tail extends the selection. One
    copy of the shift rule, plus the snapshot/restore that undo uses.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include "../RegionManager.h"
#include "../Region.h"
#include "../MarkerManager.h"
#include "../Marker.h"

namespace TimelineShift
{
    /** Region and marker positions captured before an edit, for undo. */
    struct Snapshot
    {
        juce::Array<Region> regions;
        juce::Array<Marker> markers;
    };

    /** Capture every region/marker of the given managers (either may be null). */
    inline Snapshot capture(RegionManager* regionManager, MarkerManager* markerManager)
    {
        Snapshot snapshot;

        if (regionManager != nullptr)
        {
            for (int i = 0; i < regionManager->getNumRegions(); ++i)
                if (const auto* region = regionManager->getRegion(i))
                    snapshot.regions.add(*region);
        }

        if (markerManager != nullptr)
        {
            for (int i = 0; i < markerManager->getNumMarkers(); ++i)
                if (const auto* marker = markerManager->getMarker(i))
                    snapshot.markers.add(*marker);
        }

        return snapshot;
    }

    /** Put back the positions captured by capture(). A manager whose snapshot
        is empty is left alone (an insertion never adds or removes items). */
    inline void restore(const Snapshot& snapshot,
                        RegionManager* regionManager,
                        MarkerManager* markerManager)
    {
        if (regionManager != nullptr && !snapshot.regions.isEmpty())
        {
            regionManager->removeAllRegions();
            for (const auto& region : snapshot.regions)
                regionManager->addRegion(region);
        }

        if (markerManager != nullptr && !snapshot.markers.isEmpty())
        {
            markerManager->removeAllMarkers();
            for (const auto& marker : snapshot.markers)
                markerManager->addMarker(marker);
        }
    }

    /**
     * Move regions and markers so they keep pointing at the same audio after
     * @p numSamples samples were inserted at @p insertPosition:
     * - Region entirely before the insertion point: unaffected.
     * - Region starting at/after the insertion point: shifted forward.
     * - Insertion point inside a region: the region grows to include the
     *   inserted audio (its start does not move).
     * - Marker at/after the insertion point: shifted forward; a marker
     *   strictly before it is unaffected. (H3)
     */
    inline void shiftForInsert(RegionManager* regionManager,
                               MarkerManager* markerManager,
                               int64_t insertPosition,
                               int64_t numSamples)
    {
        if (numSamples == 0)
            return;

        if (regionManager != nullptr)
        {
            for (int i = 0; i < regionManager->getNumRegions(); ++i)
            {
                Region* region = regionManager->getRegion(i);
                if (!region) continue;

                const int64_t regionStart = region->getStartSample();
                const int64_t regionEnd = region->getEndSample();

                if (regionEnd <= insertPosition)
                {
                    continue;
                }
                else if (regionStart >= insertPosition)
                {
                    region->setStartSample(regionStart + numSamples);
                    region->setEndSample(regionEnd + numSamples);
                }
                else
                {
                    region->setEndSample(regionEnd + numSamples);
                }
            }
        }

        if (markerManager != nullptr)
        {
            for (int i = 0; i < markerManager->getNumMarkers(); ++i)
            {
                if (Marker* marker = markerManager->getMarker(i))
                {
                    if (marker->getPosition() >= insertPosition)
                        marker->setPosition(marker->getPosition() + numSamples);
                }
            }
        }
    }
}
