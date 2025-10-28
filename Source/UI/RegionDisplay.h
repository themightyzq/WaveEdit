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

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/RegionManager.h"

/**
 * Visual component that displays regions as colored bars above the waveform.
 *
 * Features:
 * - Colored bars with region names
 * - Click to select region
 * - Double-click to rename region
 * - Right-click menu (rename, delete, change color, export)
 * - Visual feedback for selected region
 *
 * Coordinates with WaveformDisplay for timeline synchronization.
 */
class RegionDisplay : public juce::Component
{
public:
    /**
     * Creates a region display component.
     *
     * @param regionManager Reference to the region manager
     */
    RegionDisplay(RegionManager& regionManager);

    /**
     * Destructor.
     */
    ~RegionDisplay() override;

    //==============================================================================
    // View state (synchronized with WaveformDisplay)

    /**
     * Sets the visible time range (for coordinate conversion).
     *
     * @param startTime Start of visible range in seconds
     * @param endTime End of visible range in seconds
     */
    void setVisibleRange(double startTime, double endTime);

    /**
     * Sets the sample rate (for time/sample conversions).
     *
     * @param sampleRate Sample rate in Hz
     */
    void setSampleRate(double sampleRate);

    /**
     * Sets the total duration of the audio file.
     *
     * @param duration Total duration in seconds
     */
    void setTotalDuration(double duration);

    /**
     * Sets the audio buffer pointer for zero-crossing snap (Phase 3.3).
     * RegionDisplay does not own the buffer, just references it.
     *
     * @param buffer Pointer to the audio buffer (may be nullptr)
     */
    void setAudioBuffer(const juce::AudioBuffer<float>* buffer);

    //==============================================================================
    // Region interaction callbacks

    /**
     * Callback when a region is clicked (to select its range).
     */
    std::function<void(int regionIndex)> onRegionClicked;

    /**
     * Callback when a region is double-clicked (to rename).
     */
    std::function<void(int regionIndex)> onRegionDoubleClicked;

    /**
     * Callback when a region's name is changed.
     */
    std::function<void(int regionIndex, const juce::String& newName)> onRegionRenamed;

    /**
     * Callback when a region's color is changed.
     */
    std::function<void(int regionIndex, const juce::Colour& newColor)> onRegionColorChanged;

    /**
     * Callback when a region is deleted.
     */
    std::function<void(int regionIndex)> onRegionDeleted;

    /**
     * Callback when a region is resized (boundaries changed via drag).
     * Receives both old and new boundaries for undo support.
     */
    std::function<void(int regionIndex, int64_t oldStart, int64_t oldEnd, int64_t newStart, int64_t newEnd)> onRegionResized;

    /**
     * Callback during region resize drag (for real-time visual feedback).
     * Called continuously during mouseDrag() to update WaveformDisplay.
     */
    std::function<void()> onRegionResizing;

    /**
     * Callback when "Edit Boundaries..." is selected from context menu.
     * Called to show the numeric boundary editing dialog.
     */
    std::function<void(int regionIndex)> onRegionEditBoundaries;

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;

private:
    //==============================================================================
    // Helper methods

    /**
     * Converts a time position to an x-coordinate on screen.
     */
    int timeToX(double timeInSeconds) const;

    /**
     * Converts an x-coordinate to a time position in seconds.
     */
    double xToTime(int x) const;

    /**
     * Converts sample position to time in seconds.
     */
    double sampleToTime(int64_t sample) const;

    /**
     * Finds the region at a given x-coordinate.
     *
     * @param x X-coordinate in component space
     * @return Region index, or -1 if none
     */
    int findRegionAtX(int x) const;

    /**
     * Edge proximity detection for resize handles.
     */
    enum class EdgeProximity
    {
        None,
        StartEdge,
        EndEdge
    };

    /**
     * Checks if mouse is near start or end edge of a region.
     *
     * @param regionIndex Index of region to check
     * @param x X-coordinate in component space
     * @return EdgeProximity indicating which edge (if any) is near
     */
    EdgeProximity getEdgeProximity(int regionIndex, int x) const;

    /**
     * Gets appropriate cursor for edge proximity.
     *
     * @param edge Edge proximity state
     * @return MouseCursor for resize or normal pointer
     */
    juce::MouseCursor getResizeCursorForEdge(EdgeProximity edge) const;

    /**
     * Draws a single region bar.
     */
    void drawRegion(juce::Graphics& g, const Region& region, int regionIndex, juce::Rectangle<int> bounds);

    /**
     * Shows the right-click context menu for a region.
     */
    void showRegionContextMenu(int regionIndex, juce::Point<int> position);

    /**
     * Shows a dialog to rename a region.
     */
    void showRenameDialog(int regionIndex);

    /**
     * Shows a color picker for a region (quick swatches + custom option).
     */
    void showColorPicker(int regionIndex);

    /**
     * Shows custom color picker dialog with full RGB/HSV controls.
     */
    void showCustomColorPicker(int regionIndex);

    //==============================================================================
    // Member variables

    RegionManager& m_regionManager;

    // View state (synchronized with WaveformDisplay)
    double m_visibleStart;
    double m_visibleEnd;
    double m_sampleRate;
    double m_totalDuration;

    // Audio buffer (for zero-crossing snap, Phase 3.3)
    const juce::AudioBuffer<float>* m_audioBuffer;  // Not owned, just referenced

    // Interaction state
    int m_hoveredRegionIndex;
    int m_draggedRegionIndex;

    // Resize/Move state
    enum class ResizeMode
    {
        None,
        ResizingStart,
        ResizingEnd,
        MovingRegion  // Phase 3: Drag-to-move
    };

    ResizeMode m_resizeMode;
    int m_resizeRegionIndex;
    int64_t m_originalRegionStart;  // Original start sample before resize/move (single region)
    int64_t m_originalRegionEnd;    // Original end sample before resize/move (single region)
    int m_dragStartX;               // Mouse X position when drag started

    // Multi-region move state (Phase 3.5)
    struct OriginalRegionBounds
    {
        int regionIndex;
        int64_t startSample;
        int64_t endSample;
    };
    juce::Array<OriginalRegionBounds> m_originalMultiRegionBounds;  // Original bounds for multi-region moves

    // Layout constants
    static constexpr int BAR_HEIGHT = 24;
    static constexpr int LABEL_MARGIN = 4;
    static constexpr float SELECTED_ALPHA = 1.0f;
    static constexpr float UNSELECTED_ALPHA = 0.7f;
    static constexpr int EDGE_GRAB_TOLERANCE = 8;  // Pixels tolerance for edge grab

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RegionDisplay)
};
