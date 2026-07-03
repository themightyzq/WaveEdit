/*
  ==============================================================================

    SidecarPolicy.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "SidecarPolicy.h"
#include "RegionManager.h"
#include "MarkerManager.h"

namespace
{
    // Source of truth for the deterministic region color palette. Region
    // auto-creation (Strip Silence) and embedded-cue import both assign
    // palette[index % N]; RegionManager references this via SidecarPolicy.
    const juce::Colour kRegionColorPalette[] = {
        juce::Colours::lightblue,
        juce::Colours::lightgreen,
        juce::Colours::lightyellow,
        juce::Colours::lightcoral,
        juce::Colours::lightpink,
        juce::Colours::lightsalmon,
        juce::Colours::lightseagreen,
        juce::Colours::lightskyblue
    };
    constexpr int kNumRegionColors = 8;

    // Fingerprint tolerance: filesystem mod-time granularity can differ by a
    // second or two between write and read, so only a larger drift counts.
    constexpr juce::int64 kModTimeToleranceMs = 2000;
}

namespace SidecarPolicy
{
    int numRegionPaletteColors()
    {
        return kNumRegionColors;
    }

    juce::Colour autoRegionColor(int index)
    {
        if (index < 0)
            index = 0;
        return kRegionColorPalette[index % kNumRegionColors];
    }

    juce::Colour defaultMarkerColor()
    {
        return juce::Colours::yellow;
    }

    bool isPureAscii(const juce::String& s)
    {
        for (int i = 0; i < s.length(); ++i)
            if (s[i] > 127 || s[i] < 0)
                return false;
        return true;
    }

    juce::String toAsciiLabel(const juce::String& s)
    {
        juce::String out;
        out.preallocateBytes(static_cast<size_t>(s.length()) + 1);

        for (int i = 0; i < s.length(); ++i)
        {
            const juce::juce_wchar c = s[i];
            out += (c > 0 && c < 128) ? static_cast<juce::juce_wchar>(c)
                                      : static_cast<juce::juce_wchar>('?');
        }
        return out;
    }

    bool regionEmbeddable(const Region& region, int index)
    {
        if (!isPureAscii(region.getName()))
            return false;

        const juce::Colour c = region.getColor();

        // Non-custom == flat default (manual Cmd+M) OR the palette color
        // auto-assign / import would give at this index.
        return c == juce::Colours::lightblue || c == autoRegionColor(index);
    }

    bool markerEmbeddable(const Marker& marker)
    {
        return isPureAscii(marker.getName()) && marker.getColor() == defaultMarkerColor();
    }

    bool regionsNeedSidecar(const RegionManager& regions)
    {
        const auto& all = regions.getAllRegions();
        for (int i = 0; i < all.size(); ++i)
            if (!regionEmbeddable(all.getReference(i), i))
                return true;
        return false;
    }

    bool markersNeedSidecar(const MarkerManager& markers)
    {
        const auto all = markers.getAllMarkers();
        for (const auto& m : all)
            if (!markerEmbeddable(m))
                return true;
        return false;
    }

    bool sidecarStaleAgainst(const juce::File& sidecarFile, const juce::File& audioFile)
    {
        if (!sidecarFile.existsAsFile() || !audioFile.existsAsFile())
            return false;

        const juce::var json = juce::JSON::parse(sidecarFile.loadFileAsString());
        auto* obj = json.getDynamicObject();
        if (obj == nullptr)
            return false;

        const juce::var lengthVar = obj->getProperty("audioLength");
        const juce::var modVar = obj->getProperty("audioModTime");

        // Back-compat: sidecars written before the fingerprint fields have
        // neither -> treat as fresh so old files keep loading silently.
        const bool hasLength = lengthVar.isInt() || lengthVar.isInt64() || lengthVar.isDouble();
        const bool hasMod = modVar.isInt() || modVar.isInt64() || modVar.isDouble();
        if (!hasLength && !hasMod)
            return false;

        if (hasLength)
        {
            const juce::int64 storedLength = static_cast<juce::int64>(lengthVar);
            if (storedLength != audioFile.getSize())
                return true;
        }

        if (hasMod)
        {
            const juce::int64 storedMod = static_cast<juce::int64>(modVar);
            const juce::int64 actualMod = audioFile.getLastModificationTime().toMilliseconds();
            juce::int64 diff = storedMod - actualMod;
            if (diff < 0)
                diff = -diff;
            if (diff > kModTimeToleranceMs)
                return true;
        }

        return false;
    }
}
