/*
 * WaveEdit by ZQ SFX
 * Copyright (C) 2025 ZQ SFX
 * Licensed under GPL v3
 *
 * MarkerManager.h
 *
 * Document-scoped manager for timeline markers
 * Thread-safe with CriticalSection protection
 */

#pragma once

#include <juce_core/juce_core.h>
#include "Marker.h"

/**
 * Manages markers for a single audio document
 *
 * NOT a singleton - each Document instance owns its own MarkerManager
 *
 * Thread Safety:
 * - All methods use CriticalSection for thread-safe access
 * - Mutating methods enforce message thread (debug/logging)
 * - Read methods are safe from any thread
 */
class MarkerManager : public juce::ChangeBroadcaster
{
public:
    MarkerManager();
    ~MarkerManager() = default;

    // Lifecycle
    /**
     * Add a new marker
     *
     * @param marker Marker to add
     * @return Index of added marker (after sorting by position)
     */
    int addMarker(const Marker& marker);

    /**
     * Remove marker at index
     *
     * @param index Marker index
     */
    void removeMarker(int index);

    /**
     * Remove all markers
     */
    void removeAllMarkers();

    /**
     * Insert marker at specific index (for undo/redo)
     *
     * @param index Target index
     * @param marker Marker to insert
     */
    void insertMarkerAt(int index, const Marker& marker);

    // Access
    /**
     * Get number of markers
     *
     * @return Marker count
     */
    int getNumMarkers() const;

    /**
     * Get marker at index (mutable)
     *
     * @param index Marker index
     * @return Pointer to marker, or nullptr if index invalid
     */
    Marker* getMarker(int index);

    /**
     * Get marker at index (const)
     *
     * @param index Marker index
     * @return Pointer to marker, or nullptr if index invalid
     */
    const Marker* getMarker(int index) const;

    /**
     * Get all markers (returns copy for thread safety)
     *
     * @return Copy of all markers
     */
    juce::Array<Marker> getAllMarkers() const;

    /**
     * Get marker by its stable ID (mutable).
     *
     * Preferred over getMarker(index) for undo/redo: the ID is invariant to
     * later inserts/removes (markers re-sort on add), so an action can re-find
     * its exact marker even after the list order changed.
     *
     * @param id Stable marker ID (Marker::getId())
     * @return Pointer to marker, or nullptr if no marker has that ID
     */
    Marker* getMarkerById(int64_t id);

    /**
     * Get the current index of the marker with the given stable ID.
     *
     * @param id Stable marker ID (Marker::getId())
     * @return Index, or -1 if no marker has that ID
     */
    int getIndexOfMarkerId(int64_t id) const;

    // Navigation
    /**
     * Find marker at or near sample position
     *
     * @param sample Sample position
     * @param tolerance Tolerance in samples (default: 10)
     * @return Marker index, or -1 if not found
     */
    int findMarkerAtSample(int64_t sample, int64_t tolerance = 10) const;

    /**
     * Get next marker after current sample
     *
     * @param currentSample Current playback/cursor position
     * @return Index of next marker, or -1 if none
     */
    int getNextMarkerIndex(int64_t currentSample) const;

    /**
     * Get previous marker before current sample
     *
     * @param currentSample Current playback/cursor position
     * @return Index of previous marker, or -1 if none
     */
    int getPreviousMarkerIndex(int64_t currentSample) const;

    // Selection
    /**
     * Get currently selected marker index
     *
     * @return Selected marker index, or -1 if none
     */
    int getSelectedMarkerIndex() const { return m_selectedMarkerIndex; }

    /**
     * Set selected marker
     *
     * @param index Marker index, or -1 to clear selection
     */
    void setSelectedMarkerIndex(int index);

    /**
     * Clear marker selection
     */
    void clearSelection() { setSelectedMarkerIndex(-1); }

    // Persistence
    /**
     * Save markers to JSON sidecar file
     *
     * Creates file: audioFile.markers.json
     *
     * @param audioFile Source audio file
     * @param sampleRateScale Ratio (targetSampleRate / sourceSampleRate) to
     *        apply to every marker position before writing. Pass 1.0
     *        (default) for a normal save at the buffer's current rate. A
     *        Save-As that resamples must pass the actual ratio so markers
     *        stay aligned with the resampled audio on reopen.
     * @return true if saved successfully
     */
    bool saveToFile(const juce::File& audioFile, double sampleRateScale = 1.0) const;

    /**
     * Load markers from JSON sidecar file
     *
     * @param audioFile Source audio file
     * @return true if loaded successfully (false if file doesn't exist)
     */
    bool loadFromFile(const juce::File& audioFile, int64_t totalSamples = -1);

    /**
     * Get path to marker sidecar file
     *
     * @param audioFile Source audio file
     * @return Path to .markers.json file
     */
    static juce::File getMarkerFilePath(const juce::File& audioFile);

private:
    /**
     * Ensure we're on the message thread. Mirrors RegionManager's
     * (structurally identical) ensureMessageThread: logs an ERROR and
     * returns false on a wrong-thread call instead of only asserting, so a
     * Release build (where jassert is compiled out) still refuses the
     * mutation rather than silently racing the message thread. Callers must
     * check the return value and bail out on false, exactly like
     * RegionManager's callers do.
     */
    bool ensureMessageThread(const juce::String& methodName) const;

    // Data
    juce::Array<Marker> m_markers;              // Main marker storage (sorted by position)
    int m_selectedMarkerIndex;                   // Currently selected marker (-1 = none)
    mutable juce::CriticalSection m_lock;        // Thread safety

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MarkerManager)
};
