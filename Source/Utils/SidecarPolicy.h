/*
  ==============================================================================

    SidecarPolicy.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include "Region.h"
#include "Marker.h"

class RegionManager;
class MarkerManager;

/**
 * Decides whether a document's markers/regions can be fully represented inside
 * a WAV's embedded cue/adtl chunks, or whether they need a companion
 * .regions.json / .markers.json sidecar.
 *
 * With cues embedded, a sidecar is only needed when the WAV cannot carry the
 * data losslessly, i.e. when a region/marker has:
 *   (a) a color that differs from its deterministic auto-assignment
 *       (a genuinely user-chosen color), or
 *   (b) a name containing non-ASCII characters (labl is ASCII/Latin-1).
 *
 * "Non-custom" color for a region means EITHER the flat default (lightblue,
 * used by manual Cmd+M creation) OR the palette color auto-assign would give at
 * that index (used by Strip Silence / embedded import). Only a color outside
 * that set forces a sidecar. For markers the non-custom color is yellow.
 *
 * These are pure functions (no UI, no state) so they are unit-testable and can
 * be shared by the managers, controllers and the load/save orchestration.
 */
namespace SidecarPolicy
{
    /** Number of colors in the deterministic region palette. */
    int numRegionPaletteColors();

    /** The deterministic auto-assigned region color for a given index. */
    juce::Colour autoRegionColor(int index);

    /** The default (non-custom) marker color. */
    juce::Colour defaultMarkerColor();

    /** True if every character in @p s is ASCII (code point < 128). */
    bool isPureAscii(const juce::String& s);

    /** Lossy ASCII/Latin-1 transliteration for a labl (non-ASCII -> '?'). */
    juce::String toAsciiLabel(const juce::String& s);

    /** True if @p region at list index @p index needs no sidecar to survive. */
    bool regionEmbeddable(const Region& region, int index);

    /** True if @p marker needs no sidecar to survive. */
    bool markerEmbeddable(const Marker& marker);

    /** True if any region in the manager requires a sidecar. */
    bool regionsNeedSidecar(const RegionManager& regions);

    /** True if any marker in the manager requires a sidecar. */
    bool markersNeedSidecar(const MarkerManager& markers);

    /**
     * Compares a sidecar's recorded audio fingerprint (see "audioLength" /
     * "audioModTime" written by RegionManager/MarkerManager::saveToFile)
     * against the audio file on disk.
     *
     * @return true if the file changed since the sidecar was written. Old
     *         sidecars that predate the fingerprint fields are treated as fresh
     *         (returns false) for back-compat.
     */
    bool sidecarStaleAgainst(const juce::File& sidecarFile, const juce::File& audioFile);
}
