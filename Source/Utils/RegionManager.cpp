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
    : m_primarySelectionIndex(-1)
{
}

RegionManager::~RegionManager()
{
}

//==============================================================================
// Thread safety helper

namespace
{
    /**
     * Checks if we're on the message thread.
     * In debug builds, asserts. In release builds, logs error.
     */
    bool ensureMessageThread(const char* methodName)
    {
        bool onMessageThread = juce::MessageManager::getInstance()->isThisTheMessageThread();

        if (!onMessageThread)
        {
            juce::Logger::writeToLog(
                juce::String("ERROR: ") + methodName +
                " called from wrong thread! Must be called from message thread only.");
        }

        jassert(onMessageThread);
        return onMessageThread;
    }
}

//==============================================================================
// Region lifecycle

int RegionManager::addRegion(const Region& region)
{
    if (!ensureMessageThread("RegionManager::addRegion"))
        return -1;

    juce::ScopedLock lock(m_lock);
    m_regions.add(region);
    return m_regions.size() - 1;
}

void RegionManager::removeRegion(int index)
{
    if (!ensureMessageThread("RegionManager::removeRegion"))
        return;

    juce::ScopedLock lock(m_lock);

    if (index >= 0 && index < m_regions.size())
    {
        m_regions.remove(index);

        // Update multi-selection indices
        std::set<int> updatedSelection;
        for (int selectedIndex : m_selectedRegionIndices)
        {
            if (selectedIndex == index)
            {
                // Skip removed region
                continue;
            }
            else if (selectedIndex > index)
            {
                // Shift down indices above removed region
                updatedSelection.insert(selectedIndex - 1);
            }
            else
            {
                // Keep indices below removed region
                updatedSelection.insert(selectedIndex);
            }
        }
        m_selectedRegionIndices = updatedSelection;

        // Update primary selection
        if (m_primarySelectionIndex == index)
        {
            m_primarySelectionIndex = m_selectedRegionIndices.empty()
                ? -1
                : *m_selectedRegionIndices.begin();
        }
        else if (m_primarySelectionIndex > index)
        {
            m_primarySelectionIndex--;
        }
    }
}

void RegionManager::insertRegionAt(int index, const Region& region)
{
    if (!ensureMessageThread("RegionManager::insertRegionAt"))
        return;

    juce::ScopedLock lock(m_lock);

    if (index >= 0 && index <= m_regions.size())
    {
        m_regions.insert(index, region);

        // Update multi-selection indices (shift up all indices >= insertion point)
        std::set<int> updatedSelection;
        for (int selectedIndex : m_selectedRegionIndices)
        {
            if (selectedIndex >= index)
                updatedSelection.insert(selectedIndex + 1);
            else
                updatedSelection.insert(selectedIndex);
        }
        m_selectedRegionIndices = updatedSelection;

        // Update primary selection
        if (m_primarySelectionIndex >= index)
            m_primarySelectionIndex++;
    }
}

void RegionManager::removeAllRegions()
{
    if (!ensureMessageThread("RegionManager::removeAllRegions"))
        return;

    juce::ScopedLock lock(m_lock);

    m_regions.clear();
    m_selectedRegionIndices.clear();
    m_primarySelectionIndex = -1;
}

//==============================================================================
// Region access

int RegionManager::getNumRegions() const
{
    juce::ScopedLock lock(m_lock);
    return m_regions.size();
}

Region* RegionManager::getRegion(int index)
{
    juce::ScopedLock lock(m_lock);

    if (index >= 0 && index < m_regions.size())
        return &m_regions.getReference(index);

    return nullptr;
}

const Region* RegionManager::getRegion(int index) const
{
    juce::ScopedLock lock(m_lock);

    if (index >= 0 && index < m_regions.size())
        return &m_regions.getReference(index);

    return nullptr;
}

const juce::Array<Region>& RegionManager::getAllRegions() const
{
    juce::ScopedLock lock(m_lock);
    return m_regions;
}

//==============================================================================
// Region navigation

int RegionManager::findRegionAtSample(int64_t sample) const
{
    juce::ScopedLock lock(m_lock);

    for (int i = 0; i < m_regions.size(); ++i)
    {
        if (m_regions[i].containsSample(sample))
            return i;
    }

    return -1;
}

//==============================================================================
// Multi-selection API (Phase 3.5)

void RegionManager::selectRegion(int index, bool addToSelection)
{
    if (!ensureMessageThread("RegionManager::selectRegion"))
        return;

    juce::ScopedLock lock(m_lock);

    if (index < 0 || index >= m_regions.size())
        return;

    if (!addToSelection)
        m_selectedRegionIndices.clear();

    m_selectedRegionIndices.insert(index);
    m_primarySelectionIndex = index;
}

void RegionManager::selectRegions(const juce::Array<int>& indices)
{
    if (!ensureMessageThread("RegionManager::selectRegions"))
        return;

    juce::ScopedLock lock(m_lock);

    m_selectedRegionIndices.clear();

    for (int index : indices)
    {
        if (index >= 0 && index < m_regions.size())
        {
            m_selectedRegionIndices.insert(index);
            m_primarySelectionIndex = index; // Last valid index becomes primary
        }
    }
}

void RegionManager::selectRegionRange(int startIndex, int endIndex)
{
    if (!ensureMessageThread("RegionManager::selectRegionRange"))
        return;

    juce::ScopedLock lock(m_lock);

    if (startIndex < 0 || endIndex >= m_regions.size())
        return;

    int minIndex = std::min(startIndex, endIndex);
    int maxIndex = std::max(startIndex, endIndex);

    for (int i = minIndex; i <= maxIndex; ++i)
        m_selectedRegionIndices.insert(i);

    m_primarySelectionIndex = endIndex; // Clicked region becomes primary
}

void RegionManager::toggleRegionSelection(int index)
{
    if (!ensureMessageThread("RegionManager::toggleRegionSelection"))
        return;

    juce::ScopedLock lock(m_lock);

    if (index < 0 || index >= m_regions.size())
        return;

    if (m_selectedRegionIndices.count(index) > 0)
    {
        // Deselect
        m_selectedRegionIndices.erase(index);

        // Update primary selection if we removed it
        if (m_primarySelectionIndex == index)
        {
            m_primarySelectionIndex = m_selectedRegionIndices.empty()
                ? -1
                : *m_selectedRegionIndices.begin();
        }
    }
    else
    {
        // Select
        m_selectedRegionIndices.insert(index);
        m_primarySelectionIndex = index;
    }
}

void RegionManager::clearSelection()
{
    if (!ensureMessageThread("RegionManager::clearSelection"))
        return;

    juce::ScopedLock lock(m_lock);

    m_selectedRegionIndices.clear();
    m_primarySelectionIndex = -1;
}

juce::Array<int> RegionManager::getSelectedRegionIndices() const
{
    juce::ScopedLock lock(m_lock);

    juce::Array<int> indices;
    for (int index : m_selectedRegionIndices)
        indices.add(index);
    return indices;
}

int RegionManager::getNumSelectedRegions() const
{
    juce::ScopedLock lock(m_lock);
    return static_cast<int>(m_selectedRegionIndices.size());
}

bool RegionManager::isRegionSelected(int index) const
{
    juce::ScopedLock lock(m_lock);
    return m_selectedRegionIndices.count(index) > 0;
}

int RegionManager::getPrimarySelectionIndex() const
{
    juce::ScopedLock lock(m_lock);
    return m_primarySelectionIndex;
}

//==============================================================================
// Legacy single-selection API (backward compatibility)

int RegionManager::getSelectedRegionIndex() const
{
    juce::ScopedLock lock(m_lock);
    return m_primarySelectionIndex;
}

void RegionManager::setSelectedRegionIndex(int index)
{
    if (!ensureMessageThread("RegionManager::setSelectedRegionIndex"))
        return;

    juce::ScopedLock lock(m_lock);

    m_selectedRegionIndices.clear();

    if (index >= 0 && index < m_regions.size())
    {
        m_selectedRegionIndices.insert(index);
        m_primarySelectionIndex = index;
    }
    else
    {
        m_primarySelectionIndex = -1;
    }
}

int RegionManager::getNextRegionIndex(int64_t currentSample) const
{
    juce::ScopedLock lock(m_lock);

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
    juce::ScopedLock lock(m_lock);

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

std::vector<int> RegionManager::getRegionIndicesInRange(int64_t startSample, int64_t endSample) const
{
    juce::ScopedLock lock(m_lock);

    std::vector<int> indices;

    for (int i = 0; i < m_regions.size(); ++i)
    {
        const auto& region = m_regions[i];
        int64_t regionStart = region.getStartSample();
        int64_t regionEnd = region.getEndSample();

        // Check if region overlaps with the range
        // Overlap occurs if: regionStart <= endSample AND regionEnd >= startSample
        if (regionStart <= endSample && regionEnd >= startSample)
        {
            indices.push_back(i);
        }
    }

    return indices;
}

Region* RegionManager::getRegionAt(int64_t sample)
{
    // findRegionAtSample and getRegion both handle their own locking
    int index = findRegionAtSample(sample);
    return (index >= 0) ? getRegion(index) : nullptr;
}

const Region* RegionManager::getRegionAt(int64_t sample) const
{
    // findRegionAtSample and getRegion both handle their own locking
    int index = findRegionAtSample(sample);
    return (index >= 0) ? getRegion(index) : nullptr;
}

//==============================================================================
// Selection helpers

juce::Array<std::pair<int64_t, int64_t>> RegionManager::getInverseRanges(int64_t totalSamples) const
{
    juce::ScopedLock lock(m_lock);

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
    juce::ScopedLock lock(m_lock);

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
    if (!ensureMessageThread("RegionManager::loadFromFile"))
        return false;

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

    // Clear existing regions (this handles locking internally)
    removeAllRegions();

    // Load regions from JSON (addRegion handles locking internally)
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
    if (!ensureMessageThread("RegionManager::autoCreateRegions"))
        return;

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

//==============================================================================
// Region editing operations - Phase 3.4

bool RegionManager::mergeSelectedRegions()
{
    if (!ensureMessageThread("RegionManager::mergeSelectedRegions"))
        return false;

    juce::ScopedLock lock(m_lock);

    if (m_selectedRegionIndices.empty())
    {
        juce::Logger::writeToLog("Cannot merge: No regions selected");
        return false;
    }

    // Special case: Single region selected - merge with next region (legacy behavior)
    if (m_selectedRegionIndices.size() == 1)
    {
        int index = *m_selectedRegionIndices.begin();
        if (index >= m_regions.size() - 1)
        {
            juce::Logger::writeToLog("Cannot merge: No next region to merge with");
            return false;
        }

        // Temporarily select both regions, then merge
        m_selectedRegionIndices.insert(index + 1);
    }

    // Find bounds of merged region (earliest start, latest end)
    int64_t mergedStart = std::numeric_limits<int64_t>::max();
    int64_t mergedEnd = 0;
    juce::String combinedName;

    bool first = true;
    for (int index : m_selectedRegionIndices)
    {
        const auto& region = m_regions.getReference(index);
        mergedStart = std::min(mergedStart, region.getStartSample());
        mergedEnd = std::max(mergedEnd, region.getEndSample());

        if (!first)
            combinedName += " + ";
        combinedName += region.getName();
        first = false;
    }

    // Use color of first selected region
    int firstSelectedIndex = *m_selectedRegionIndices.begin();
    juce::Colour mergedColor = m_regions.getReference(firstSelectedIndex).getColor();

    // Create merged region
    Region mergedRegion(combinedName, mergedStart, mergedEnd);
    mergedRegion.setColor(mergedColor);

    // Perform merge
    performMerge(m_selectedRegionIndices, mergedRegion);

    juce::Logger::writeToLog("Successfully merged " + juce::String(m_selectedRegionIndices.size()) + " regions: " + combinedName);
    return true;
}

void RegionManager::performMerge(const std::set<int>& indicesToRemove, const Region& mergedRegion)
{
    // NOTE: This method should only be called from methods that already hold the lock
    // It's a helper method for undo/redo and internal operations

    // Remove regions in reverse order (avoids index shifting issues)
    auto it = indicesToRemove.rbegin();
    while (it != indicesToRemove.rend())
    {
        m_regions.remove(*it);
        ++it;
    }

    // Insert merged region (at position where first region was)
    int firstRemovedIndex = *indicesToRemove.begin();
    int insertIndex = std::min(firstRemovedIndex, m_regions.size());
    m_regions.insert(insertIndex, mergedRegion);

    // Select newly merged region (clearSelection and selectRegion handle their own locking,
    // but we're already holding the lock from the caller, so this is safe)
    m_selectedRegionIndices.clear();
    m_selectedRegionIndices.insert(insertIndex);
    m_primarySelectionIndex = insertIndex;
}

bool RegionManager::mergeRegions(int firstIndex, int secondIndex)
{
    if (!ensureMessageThread("RegionManager::mergeRegions"))
        return false;

    juce::ScopedLock lock(m_lock);

    // Validate indices
    if (firstIndex < 0 || firstIndex >= m_regions.size() ||
        secondIndex < 0 || secondIndex >= m_regions.size() ||
        firstIndex == secondIndex)
    {
        juce::Logger::writeToLog("Merge failed: Invalid region indices");
        return false;
    }

    // Ensure firstIndex < secondIndex
    if (firstIndex > secondIndex)
        std::swap(firstIndex, secondIndex);

    Region& first = m_regions.getReference(firstIndex);
    Region& second = m_regions.getReference(secondIndex);

    // Save original end for gap detection
    int64_t originalFirstEnd = first.getEndSample();

    // Create merged region (auto-fills any gap between regions)
    juce::String mergedName = first.getName() + " + " + second.getName();
    first.setName(mergedName);
    first.setEndSample(second.getEndSample());

    // Log if there was a gap that got filled
    if (originalFirstEnd < second.getStartSample())
    {
        int64_t gapSamples = second.getStartSample() - originalFirstEnd;
        juce::Logger::writeToLog(juce::String::formatted(
            "Merged regions with gap of %lld samples (%.3f seconds)",
            gapSamples, gapSamples / 44100.0));
    }

    // Remove the second region
    m_regions.remove(secondIndex);

    // Update multi-selection indices
    std::set<int> updatedSelection;
    for (int selectedIndex : m_selectedRegionIndices)
    {
        if (selectedIndex == secondIndex)
        {
            // Replace removed region with merged region
            updatedSelection.insert(firstIndex);
        }
        else if (selectedIndex > secondIndex)
        {
            // Shift down indices above removed region
            updatedSelection.insert(selectedIndex - 1);
        }
        else
        {
            // Keep indices below removed region
            updatedSelection.insert(selectedIndex);
        }
    }
    m_selectedRegionIndices = updatedSelection;

    // Update primary selection
    if (m_primarySelectionIndex == secondIndex)
        m_primarySelectionIndex = firstIndex;
    else if (m_primarySelectionIndex > secondIndex)
        m_primarySelectionIndex--;

    juce::Logger::writeToLog("Successfully merged regions: " + mergedName);
    return true;
}

bool RegionManager::splitRegion(int regionIndex, int64_t splitSample)
{
    if (!ensureMessageThread("RegionManager::splitRegion"))
        return false;

    juce::ScopedLock lock(m_lock);

    if (regionIndex < 0 || regionIndex >= m_regions.size())
    {
        juce::Logger::writeToLog("Split failed: Invalid region index");
        return false;
    }

    Region& region = m_regions.getReference(regionIndex);

    // Validate split position is within region bounds (not at boundaries)
    if (splitSample <= region.getStartSample() || splitSample >= region.getEndSample())
    {
        juce::Logger::writeToLog("Split failed: Split position outside region bounds or at boundary");
        return false;
    }

    // Create names for split regions
    juce::String originalName = region.getName();
    juce::String firstHalfName = originalName + " (1)";
    juce::String secondHalfName = originalName + " (2)";

    // Save original end sample
    int64_t originalEnd = region.getEndSample();

    // Modify first region (becomes first half)
    region.setName(firstHalfName);
    region.setEndSample(splitSample);

    // Create second region (second half)
    Region secondHalf(secondHalfName, splitSample, originalEnd);
    secondHalf.setColor(region.getColor());  // Inherit color

    // Insert second half immediately after first
    m_regions.insert(regionIndex + 1, secondHalf);

    // Update multi-selection indices (shift up all indices > regionIndex)
    std::set<int> updatedSelection;
    for (int selectedIndex : m_selectedRegionIndices)
    {
        if (selectedIndex > regionIndex)
            updatedSelection.insert(selectedIndex + 1);
        else
            updatedSelection.insert(selectedIndex);
    }
    m_selectedRegionIndices = updatedSelection;

    // Update primary selection
    if (m_primarySelectionIndex > regionIndex)
        m_primarySelectionIndex++;

    juce::Logger::writeToLog("Successfully split region '" + originalName + "' into two parts");
    return true;
}
