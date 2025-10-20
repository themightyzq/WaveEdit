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

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
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
     * Shows a color picker for a region.
     */
    void showColorPicker(int regionIndex);

    //==============================================================================
    // Member variables

    RegionManager& m_regionManager;

    // View state (synchronized with WaveformDisplay)
    double m_visibleStart;
    double m_visibleEnd;
    double m_sampleRate;
    double m_totalDuration;

    // Interaction state
    int m_hoveredRegionIndex;
    int m_draggedRegionIndex;

    // Layout constants
    static constexpr int BAR_HEIGHT = 24;
    static constexpr int LABEL_MARGIN = 4;
    static constexpr float SELECTED_ALPHA = 1.0f;
    static constexpr float UNSELECTED_ALPHA = 0.7f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RegionDisplay)
};
