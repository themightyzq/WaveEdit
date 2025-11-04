/*
 * WaveEdit by ZQ SFX
 * Copyright (C) 2025 ZQ SFX
 * Licensed under GPL v3
 *
 * MarkerDisplay.h
 *
 * Visual component for displaying and interacting with timeline markers
 * Renders vertical lines with labels on the waveform timeline
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/MarkerManager.h"

/**
 * Displays markers as vertical lines on the timeline
 *
 * Interaction modes:
 * - Click marker: Select it
 * - Double-click: Rename
 * - Drag: Move marker to new position
 * - Right-click: Context menu (rename, change color, delete)
 *
 * Visual styling:
 * - Vertical line from top to bottom of component
 * - Color-coded based on marker color
 * - Label at top showing marker name
 * - Selected marker has thicker line + highlight
 *
 * Thread Safety:
 * - All UI operations on message thread only
 * - Reads from MarkerManager with lock protection
 */
class MarkerDisplay : public juce::Component
{
public:
    /**
     * Constructor
     *
     * @param markerManager Reference to document's MarkerManager
     */
    explicit MarkerDisplay(MarkerManager& markerManager);

    ~MarkerDisplay() override = default;

    // JUCE Component overrides
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;

    // View synchronization (called by WaveformDisplay)
    /**
     * Update visible time range (for coordinate conversion)
     *
     * @param startTime Visible start time in seconds
     * @param endTime Visible end time in seconds
     */
    void setVisibleRange(double startTime, double endTime);

    /**
     * Set audio sample rate (for time ↔ sample conversion)
     *
     * @param sampleRate Sample rate in Hz
     */
    void setSampleRate(double sampleRate);

    /**
     * Set total audio duration
     *
     * @param duration Duration in seconds
     */
    void setTotalDuration(double duration);

    // Callbacks (for MainWindow integration)
    std::function<void(int markerIndex)> onMarkerClicked;
    std::function<void(int markerIndex)> onMarkerDoubleClicked;
    std::function<void(int markerIndex, juce::String newName)> onMarkerRenamed;
    std::function<void(int markerIndex, juce::Colour newColor)> onMarkerColorChanged;
    std::function<void(int markerIndex)> onMarkerDeleted;
    std::function<void(int markerIndex, int64_t oldPosition, int64_t newPosition)> onMarkerMoved;
    std::function<void()> onMarkerMoving;  // Real-time drag feedback

private:
    // Coordinate conversion
    /**
     * Convert time (seconds) to X coordinate
     *
     * @param timeInSeconds Time in seconds
     * @return X pixel coordinate
     */
    int timeToX(double timeInSeconds) const;

    /**
     * Convert X coordinate to time (seconds)
     *
     * @param x X pixel coordinate
     * @return Time in seconds
     */
    double xToTime(int x) const;

    /**
     * Convert X coordinate to sample position
     *
     * @param x X pixel coordinate
     * @return Sample position
     */
    int64_t xToSample(int x) const;

    // Hit testing
    /**
     * Find marker at X coordinate
     *
     * @param x X pixel coordinate
     * @param tolerance Pixel tolerance for hit detection (default: 5px)
     * @return Marker index, or -1 if not found
     */
    int findMarkerAtX(int x, int tolerance = 5) const;

    // Context menu
    /**
     * Show right-click context menu
     *
     * @param markerIndex Marker to show menu for
     */
    void showContextMenu(int markerIndex);

    /**
     * Show color picker popup
     *
     * @param markerIndex Marker to change color for
     */
    void showColorPicker(int markerIndex);

    // Visual constants
    static constexpr int LINE_WIDTH_NORMAL = 2;
    static constexpr int LINE_WIDTH_SELECTED = 3;
    static constexpr int LABEL_HEIGHT = 20;
    static constexpr int GRAB_TOLERANCE = 5;  // Pixels

    // Data
    MarkerManager& m_markerManager;
    double m_visibleStart;      // Visible start time (seconds)
    double m_visibleEnd;        // Visible end time (seconds)
    double m_sampleRate;        // Audio sample rate
    double m_totalDuration;     // Total audio duration (seconds)

    // Drag state
    bool m_isDragging;
    int m_draggedMarkerIndex;
    int64_t m_originalMarkerPosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MarkerDisplay)
};
