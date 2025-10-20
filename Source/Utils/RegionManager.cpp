/*
  ==============================================================================

    RegionManager.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "RegionManager.h"
#include <juce_core/juce_core.h>

namespace
{
    // Color palette for region visualization
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
}

RegionManager::RegionManager()
    : m_selectedRegionIndex(-1)
{
}

RegionManager::~RegionManager()
{
}

//==============================================================================
// Region lifecycle

int RegionManager::addRegion(const Region& region)
{
    m_regions.add(region);
    return m_regions.size() - 1;
}

void RegionManager::removeRegion(int index)
{
    if (index >= 0 && index < m_regions.size())
    {
        m_regions.remove(index);

        // Update selected index if necessary
        if (m_selectedRegionIndex == index)
        {
            m_selectedRegionIndex = -1;
        }
        else if (m_selectedRegionIndex > index)
        {
            m_selectedRegionIndex--;
        }
    }
}

void RegionManager::insertRegionAt(int index, const Region& region)
{
    if (index >= 0 && index <= m_regions.size())
    {
        m_regions.insert(index, region);

        // Update selected index if necessary
        if (m_selectedRegionIndex >= index)
        {
            m_selectedRegionIndex++;
        }
    }
}

void RegionManager::removeAllRegions()
{
    m_regions.clear();
    m_selectedRegionIndex = -1;
}

//==============================================================================
// Region access

int RegionManager::getNumRegions() const
{
    return m_regions.size();
}

Region* RegionManager::getRegion(int index)
{
    if (index >= 0 && index < m_regions.size())
        return &m_regions.getReference(index);

    return nullptr;
}

const Region* RegionManager::getRegion(int index) const
{
    if (index >= 0 && index < m_regions.size())
        return &m_regions.getReference(index);

    return nullptr;
}

const juce::Array<Region>& RegionManager::getAllRegions() const
{
    return m_regions;
}

//==============================================================================
// Region navigation

int RegionManager::findRegionAtSample(int64_t sample) const
{
    for (int i = 0; i < m_regions.size(); ++i)
    {
        if (m_regions[i].containsSample(sample))
            return i;
    }

    return -1;
}

int RegionManager::getSelectedRegionIndex() const
{
    return m_selectedRegionIndex;
}

void RegionManager::setSelectedRegionIndex(int index)
{
    if (index >= -1 && index < m_regions.size())
    {
        m_selectedRegionIndex = index;
    }
}

void RegionManager::clearSelection()
{
    m_selectedRegionIndex = -1;
}

int RegionManager::getNextRegionIndex(int64_t currentSample) const
{
    int closestIndex = -1;
    int64_t closestDistance = std::numeric_limits<int64_t>::max();

    for (int i = 0; i < m_regions.size(); ++i)
    {
        int64_t regionStart = m_regions[i].getStartSample();

        // Find the closest region that starts after currentSample
        if (regionStart > currentSample)
        {
            int64_t distance = regionStart - currentSample;
            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestIndex = i;
            }
        }
    }

    return closestIndex;
}

int RegionManager::getPreviousRegionIndex(int64_t currentSample) const
{
    int closestIndex = -1;
    int64_t closestDistance = std::numeric_limits<int64_t>::max();

    for (int i = 0; i < m_regions.size(); ++i)
    {
        int64_t regionEnd = m_regions[i].getEndSample();

        // Find the closest region that ends before currentSample
        if (regionEnd < currentSample)
        {
            int64_t distance = currentSample - regionEnd;
            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestIndex = i;
            }
        }
    }

    return closestIndex;
}

//==============================================================================
// Selection helpers

juce::Array<std::pair<int64_t, int64_t>> RegionManager::getInverseRanges(int64_t totalSamples) const
{
    juce::Array<std::pair<int64_t, int64_t>> inverseRanges;

    if (m_regions.isEmpty())
    {
        // No regions: entire file is inverse
        inverseRanges.add({0, totalSamples});
        return inverseRanges;
    }

    // Sort regions by start sample (make a sorted copy)
    juce::Array<Region> sortedRegions = m_regions;
    std::sort(sortedRegions.begin(), sortedRegions.end(),
              [](const Region& a, const Region& b)
              {
                  return a.getStartSample() < b.getStartSample();
              });

    // Add gap before first region (if exists)
    if (sortedRegions[0].getStartSample() > 0)
    {
        inverseRanges.add({0, sortedRegions[0].getStartSample()});
    }

    // Add gaps between regions
    for (int i = 0; i < sortedRegions.size() - 1; ++i)
    {
        int64_t gapStart = sortedRegions[i].getEndSample();
        int64_t gapEnd = sortedRegions[i + 1].getStartSample();

        if (gapStart < gapEnd)
        {
            inverseRanges.add({gapStart, gapEnd});
        }
    }

    // Add gap after last region (if exists)
    int64_t lastEnd = sortedRegions.getLast().getEndSample();
    if (lastEnd < totalSamples)
    {
        inverseRanges.add({lastEnd, totalSamples});
    }

    return inverseRanges;
}

//==============================================================================
// Persistence

juce::File RegionManager::getRegionFilePath(const juce::File& audioFile)
{
    return audioFile.withFileExtension(audioFile.getFileExtension() + ".regions.json");
}

bool RegionManager::saveToFile(const juce::File& audioFile) const
{
    juce::File regionFile = getRegionFilePath(audioFile);

    // Create JSON object
    auto* root = new juce::DynamicObject();

    root->setProperty("version", "1.0");
    root->setProperty("audioFile", audioFile.getFileName());

    // Serialize regions array
    juce::Array<juce::var> regionsArray;
    for (const auto& region : m_regions)
    {
        regionsArray.add(region.toJSON());
    }
    root->setProperty("regions", regionsArray);

    // Write to file
    juce::var jsonData(root);
    juce::String jsonString = juce::JSON::toString(jsonData, true);

    return regionFile.replaceWithText(jsonString);
}

bool RegionManager::loadFromFile(const juce::File& audioFile)
{
    juce::File regionFile = getRegionFilePath(audioFile);

    if (!regionFile.existsAsFile())
        return false;

    // Parse JSON
    juce::String jsonString = regionFile.loadFileAsString();
    juce::var jsonData = juce::JSON::parse(jsonString);

    if (!jsonData.isObject())
        return false;

    auto* root = jsonData.getDynamicObject();
    if (root == nullptr)
        return false;

    // Clear existing regions
    removeAllRegions();

    // Load regions from JSON
    juce::var regionsVar = root->getProperty("regions");
    if (regionsVar.isArray())
    {
        juce::Array<juce::var>* regionsArray = regionsVar.getArray();
        if (regionsArray != nullptr)
        {
            for (const auto& regionVar : *regionsArray)
            {
                Region region = Region::fromJSON(regionVar);
                addRegion(region);
            }
        }
    }

    return true;
}

//==============================================================================
// Auto-region creation (Strip Silence)

void RegionManager::autoCreateRegions(const juce::AudioBuffer<float>& buffer,
                                       double sampleRate,
                                       float thresholdDB,
                                       float minRegionLengthMs,
                                       float minSilenceLengthMs,
                                       float preRollMs,
                                       float postRollMs)
{
    // Convert parameters to samples
    float threshold = juce::Decibels::decibelsToGain(thresholdDB);
    int minRegionSamples = static_cast<int>(minRegionLengthMs * sampleRate / 1000.0);
    int minSilenceSamples = static_cast<int>(minSilenceLengthMs * sampleRate / 1000.0);
    int preRollSamples = static_cast<int>(preRollMs * sampleRate / 1000.0);
    int postRollSamples = static_cast<int>(postRollMs * sampleRate / 1000.0);

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    // Algorithm:
    // 1. Scan buffer for sections above threshold (non-silent)
    // 2. Detect silence gaps (below threshold for min silence duration)
    // 3. Create regions for non-silent sections
    // 4. Filter out regions shorter than min length
    // 5. Add pre/post-roll margins

    struct RegionCandidate
    {
        int64_t start;
        int64_t end;
    };

    juce::Array<RegionCandidate> candidates;
    bool inRegion = false;
    int64_t regionStart = 0;
    int silenceCounter = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        // Calculate RMS across all channels at this sample
        float maxAbs = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float sample = std::abs(buffer.getSample(ch, i));
            maxAbs = std::max(maxAbs, sample);
        }

        bool isSilent = maxAbs < threshold;

        if (!inRegion)
        {
            // Looking for start of region (non-silent sample)
            if (!isSilent)
            {
                inRegion = true;
                regionStart = i;
                silenceCounter = 0;
            }
        }
        else
        {
            // In a region: looking for end (sustained silence)
            if (isSilent)
            {
                silenceCounter++;

                if (silenceCounter >= minSilenceSamples)
                {
                    // End of region detected
                    int64_t regionEnd = i - minSilenceSamples;

                    // Check if region meets minimum length
                    if ((regionEnd - regionStart) >= minRegionSamples)
                    {
                        candidates.add({regionStart, regionEnd});
                    }

                    inRegion = false;
                    silenceCounter = 0;
                }
            }
            else
            {
                silenceCounter = 0; // Reset silence counter on non-silent sample
            }
        }
    }

    // Handle region that extends to end of file
    if (inRegion)
    {
        int64_t regionEnd = numSamples;
        if ((regionEnd - regionStart) >= minRegionSamples)
        {
            candidates.add({regionStart, regionEnd});
        }
    }

    // Clear existing regions and add new ones with pre/post-roll
    removeAllRegions();

    for (int i = 0; i < candidates.size(); ++i)
    {
        int64_t start = std::max<int64_t>(0, candidates[i].start - preRollSamples);
        int64_t end = std::min<int64_t>(numSamples, candidates[i].end + postRollSamples);

        juce::String regionName = "Region " + juce::String(i + 1);
        Region region(regionName, start, end);

        // Assign color based on index (cycle through palette)
        int colorIndex = i % kNumRegionColors;
        region.setColor(kRegionColorPalette[colorIndex]);

        addRegion(region);
    }
}
