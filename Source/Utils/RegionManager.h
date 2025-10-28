/*
  ==============================================================================

    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

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

#include "Region.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <set>

/**
 * Manages a collection of regions for a document.
 *
 * RegionManager provides:
 * - Adding/removing regions
 * - Region navigation (find, select)
 * - "Select inverse" functionality (select everything NOT in regions)
 * - JSON persistence to sidecar files
 * - Auto-region creation (Strip Silence algorithm - Phase 3 Tier 3)
 *
 * Persistence Format:
 * Audio file: example.wav
 * Region file: example.wav.regions.json
 *
 * Use Cases:
 * - Podcast segments (intro, interview, ads, outro)
 * - Sound effect takes/variations
 * - Batch processing targets
 * - Navigation between important sections
 */
class RegionManager
{
public:
    /**
     * Creates an empty region manager.
     */
    RegionManager();

    /**
     * Destructor.
     */
    ~RegionManager();

    //==============================================================================
    // Region lifecycle

    /**
     * Adds a new region to the collection.
     *
     * @param region Region to add
     * @return Index of the added region
     */
    int addRegion(const Region& region);

    /**
     * Removes a region at the specified index.
     *
     * @param index Index of region to remove
     */
    void removeRegion(int index);

    /**
     * Inserts a region at the specified index.
     * Used for undo/redo to restore a region at its exact original position.
     *
     * @param index Index to insert at (shifts existing regions to the right)
     * @param region Region to insert
     */
    void insertRegionAt(int index, const Region& region);

    /**
     * Removes all regions.
     */
    void removeAllRegions();

    //==============================================================================
    // Region access

    /**
     * Gets the number of regions.
     */
    int getNumRegions() const;

    /**
     * Gets a region at the specified index (mutable).
     *
     * @param index Region index
     * @return Pointer to region, or nullptr if index out of bounds
     */
    Region* getRegion(int index);

    /**
     * Gets a region at the specified index (const).
     *
     * @param index Region index
     * @return Pointer to region, or nullptr if index out of bounds
     */
    const Region* getRegion(int index) const;

    /**
     * Gets all regions (for iteration).
     */
    const juce::Array<Region>& getAllRegions() const;

    //==============================================================================
    // Region navigation

    /**
     * Finds the index of the region containing a sample position.
     *
     * @param sample Sample position to search for
     * @return Index of containing region, or -1 if not in any region
     */
    int findRegionAtSample(int64_t sample) const;

    //==============================================================================
    // Multi-selection API (Phase 3.5)

    /**
     * Selects a single region, optionally adding to existing selection.
     *
     * @param index Region index to select
     * @param addToSelection If true, adds to selection; if false, replaces selection
     */
    void selectRegion(int index, bool addToSelection = false);

    /**
     * Selects multiple regions by indices.
     *
     * @param indices Array of region indices to select
     */
    void selectRegions(const juce::Array<int>& indices);

    /**
     * Selects a range of regions (inclusive).
     * Used for Shift+Click range selection.
     *
     * @param startIndex First region in range
     * @param endIndex Last region in range
     */
    void selectRegionRange(int startIndex, int endIndex);

    /**
     * Toggles a region in/out of selection.
     * Used for Cmd+Click individual toggle.
     *
     * @param index Region index to toggle
     */
    void toggleRegionSelection(int index);

    /**
     * Clears all region selections.
     */
    void clearSelection();

    /**
     * Gets all selected region indices.
     *
     * @return Array of selected indices (sorted)
     */
    juce::Array<int> getSelectedRegionIndices() const;

    /**
     * Gets the number of selected regions.
     *
     * @return Count of selected regions
     */
    int getNumSelectedRegions() const;

    /**
     * Checks if a region is selected.
     *
     * @param index Region index to check
     * @return true if region is selected
     */
    bool isRegionSelected(int index) const;

    /**
     * Gets the primary selection index (last clicked region).
     * Used as anchor for Shift+Click range selection.
     *
     * @return Primary selection index, or -1 if no selection
     */
    int getPrimarySelectionIndex() const;

    //==============================================================================
    // Legacy single-selection API (backward compatibility)

    /**
     * Gets the currently selected region index.
     * For backward compatibility - returns primary selection.
     *
     * @return Selected region index, or -1 if no selection
     */
    int getSelectedRegionIndex() const;

    /**
     * Sets the currently selected region.
     * For backward compatibility - clears multi-selection and selects one region.
     *
     * @param index Region index to select, or -1 to clear selection
     */
    void setSelectedRegionIndex(int index);

    /**
     * Gets the next region after the specified sample position.
     *
     * @param currentSample Current sample position
     * @return Index of next region, or -1 if none
     */
    int getNextRegionIndex(int64_t currentSample) const;

    /**
     * Gets the previous region before the specified sample position.
     *
     * @param currentSample Current sample position
     * @return Index of previous region, or -1 if none
     */
    int getPreviousRegionIndex(int64_t currentSample) const;

    /**
     * Gets all region indices that overlap with a sample range.
     * Used for batch operations on regions within a time selection.
     *
     * @param startSample Start of sample range (inclusive)
     * @param endSample End of sample range (inclusive)
     * @return Vector of region indices that overlap the range
     */
    std::vector<int> getRegionIndicesInRange(int64_t startSample, int64_t endSample) const;

    /**
     * Gets the region at the specified sample position.
     * Convenience wrapper around findRegionAtSample() that returns the Region pointer.
     *
     * @param sample Sample position to search for
     * @return Pointer to region, or nullptr if not in any region
     */
    Region* getRegionAt(int64_t sample);

    /**
     * Gets the region at the specified sample position (const version).
     *
     * @param sample Sample position to search for
     * @return Pointer to region, or nullptr if not in any region
     */
    const Region* getRegionAt(int64_t sample) const;

    //==============================================================================
    // Selection helpers (for "select inverse" workflow)

    /**
     * Gets all sample ranges that are NOT covered by any regions.
     * Used for "select inverse" functionality.
     *
     * @param totalSamples Total number of samples in the file
     * @return Array of [start, end] sample pairs representing gaps between regions
     */
    juce::Array<std::pair<int64_t, int64_t>> getInverseRanges(int64_t totalSamples) const;

    //==============================================================================
    // Persistence (JSON sidecar files)

    /**
     * Saves regions to a JSON sidecar file.
     *
     * Format: {audioFile}.regions.json
     * Example: example.wav -> example.wav.regions.json
     *
     * @param audioFile The audio file (not the .regions.json file)
     * @return true if successful, false on error
     */
    bool saveToFile(const juce::File& audioFile) const;

    /**
     * Loads regions from a JSON sidecar file.
     *
     * @param audioFile The audio file (not the .regions.json file)
     * @return true if successful, false if file doesn't exist or parse error
     */
    bool loadFromFile(const juce::File& audioFile);

    /**
     * Gets the sidecar file path for an audio file.
     *
     * @param audioFile The audio file
     * @return Path to the .regions.json sidecar file
     */
    static juce::File getRegionFilePath(const juce::File& audioFile);

    //==============================================================================
    // Auto-region creation (Strip Silence) - Phase 3 Tier 3

    /**
     * Automatically creates regions based on silence detection (Strip Silence algorithm).
     *
     * Algorithm:
     * 1. Scan buffer for sections above threshold (non-silent)
     * 2. Detect silence gaps (below threshold for min silence duration)
     * 3. Create regions for non-silent sections
     * 4. Filter out regions shorter than min length
     * 5. Add pre/post-roll margins
     * 6. Name regions automatically ("Region 1", "Region 2", etc.)
     *
     * @param buffer Audio buffer to analyze
     * @param sampleRate Sample rate for time conversions
     * @param thresholdDB Threshold in dB (audio below this is considered silence)
     * @param minRegionLengthMs Minimum region length in milliseconds
     * @param minSilenceLengthMs Minimum silence gap length in milliseconds
     * @param preRollMs Pre-roll margin in milliseconds
     * @param postRollMs Post-roll margin in milliseconds
     */
    void autoCreateRegions(const juce::AudioBuffer<float>& buffer,
                            double sampleRate,
                            float thresholdDB,
                            float minRegionLengthMs,
                            float minSilenceLengthMs,
                            float preRollMs,
                            float postRollMs);

    //==============================================================================
    // Region editing operations - Phase 3.4

    /**
     * Merges all selected regions into a single region.
     * If only one region is selected, merges with next region (legacy behavior).
     * The merged region spans from earliest start to latest end of all selected regions.
     * Name format: "RegionA + RegionB + RegionC + ..."
     * Fills gaps between non-adjacent regions automatically.
     *
     * @return true if successful, false if no regions selected or merge failed
     */
    bool mergeSelectedRegions();

    /**
     * Legacy merge method for backward compatibility.
     * Merges two specific regions by index.
     *
     * @param firstIndex Index of the first region
     * @param secondIndex Index of the second region
     * @return true if successful, false if invalid indices
     */
    bool mergeRegions(int firstIndex, int secondIndex);

    /**
     * Splits a region at a sample position.
     * Creates two regions from one, dividing at the split point.
     * Names: "RegionName (1)" and "RegionName (2)"
     *
     * @param regionIndex Index of the region to split
     * @param splitSample Sample position for the split (must be within region bounds)
     * @return true if successful, false if split position invalid
     */
    bool splitRegion(int regionIndex, int64_t splitSample);

    //==============================================================================
    // Internal helpers

    /**
     * Performs merge operation (used by undo/redo).
     * Removes specified regions and inserts merged region.
     *
     * @param indicesToRemove Sorted set of region indices to remove
     * @param mergedRegion The new merged region to insert
     */
    void performMerge(const std::set<int>& indicesToRemove, const Region& mergedRegion);

private:
    juce::Array<Region> m_regions;

    // Multi-selection state (Phase 3.5)
    std::set<int> m_selectedRegionIndices;  // Sorted, unique selected indices
    int m_primarySelectionIndex;             // Last clicked region (for Shift+Click anchor)

    // Thread safety (proper locking, not just debug asserts)
    mutable juce::CriticalSection m_lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RegionManager)
};
