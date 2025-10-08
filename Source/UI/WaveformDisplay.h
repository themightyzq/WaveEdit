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
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

/**
 * High-performance waveform display component.
 *
 * Features:
 * - Smooth 60fps scrolling and zooming
 * - Stereo/multichannel waveform rendering
 * - Selection highlighting
 * - Playback cursor visualization
 * - Time ruler with sample/time markers
 * - Horizontal scrollbar for navigation
 */
class WaveformDisplay : public juce::Component,
                        public juce::ChangeListener,
                        public juce::ScrollBar::Listener,
                        public juce::Timer
{
public:
    WaveformDisplay(juce::AudioFormatManager& formatManager);
    ~WaveformDisplay() override;

    //==============================================================================
    // File loading

    /**
     * Loads an audio file and generates a thumbnail for display.
     *
     * @param file The audio file to load
     * @param sampleRate The sample rate of the audio file
     * @param numChannels The number of channels in the audio file
     * @return true if successful, false otherwise
     */
    bool loadFile(const juce::File& file, double sampleRate, int numChannels);

    /**
     * Reloads the waveform display from an audio buffer (used after edits).
     * This regenerates the thumbnail from the edited buffer data.
     *
     * @param buffer The audio buffer containing edited samples
     * @param sampleRate The sample rate of the audio data
     * @param preserveView If true, maintains current zoom and scroll position
     * @param preserveEditCursor If true, maintains edit cursor position
     * @return true if successful, false otherwise
     */
    bool reloadFromBuffer(const juce::AudioBuffer<float>& buffer,
                          double sampleRate,
                          bool preserveView = false,
                          bool preserveEditCursor = false);

    /**
     * Clears the current waveform display.
     */
    void clear();

    /**
     * Returns true if a file is currently loaded.
     */
    bool isFileLoaded() const { return m_fileLoaded; }

    //==============================================================================
    // Playback control

    /**
     * Sets the current playback position in seconds.
     *
     * @param positionInSeconds Current position in the audio file
     */
    void setPlaybackPosition(double positionInSeconds);

    /**
     * Returns the current playback position in seconds.
     */
    double getPlaybackPosition() const { return m_playbackPosition; }

    //==============================================================================
    // Selection

    /**
     * Sets the selected region in seconds.
     *
     * @param startInSeconds Selection start position
     * @param endInSeconds Selection end position
     */
    void setSelection(double startInSeconds, double endInSeconds);

    /**
     * Clears the current selection.
     */
    void clearSelection();

    /**
     * Returns the selection start position in seconds.
     */
    double getSelectionStart() const { return m_selectionStart; }

    /**
     * Returns the selection end position in seconds.
     */
    double getSelectionEnd() const { return m_selectionEnd; }

    /**
     * Returns true if there is a selection.
     */
    bool hasSelection() const { return m_hasSelection; }

    /**
     * Gets the selection duration in seconds.
     */
    double getSelectionDuration() const;

    /**
     * Gets the selection start as a formatted time string.
     */
    juce::String getSelectionStartString() const;

    /**
     * Gets the selection end as a formatted time string.
     */
    juce::String getSelectionEndString() const;

    /**
     * Gets the selection duration as a formatted time string.
     */
    juce::String getSelectionDurationString() const;

    //==============================================================================
    // Edit cursor (for paste operations)

    /**
     * Sets the edit cursor position in seconds.
     * This is where paste operations will insert audio.
     *
     * @param positionInSeconds Position to set the cursor
     */
    void setEditCursor(double positionInSeconds);

    /**
     * Clears the edit cursor.
     */
    void clearEditCursor();

    /**
     * Returns true if the edit cursor is active.
     */
    bool hasEditCursor() const { return m_hasEditCursor; }

    /**
     * Gets the edit cursor position in seconds.
     */
    double getEditCursorPosition() const { return m_editCursorPosition; }

    /**
     * Moves the edit cursor left/right by a specified time delta.
     *
     * @param deltaInSeconds Amount to move (negative = left, positive = right)
     */
    void moveEditCursor(double deltaInSeconds);

    //==============================================================================
    // Zoom and navigation

    /**
     * Zooms in by a factor of 2.
     */
    void zoomIn();

    /**
     * Zooms out by a factor of 2.
     */
    void zoomOut();

    /**
     * Fits the entire waveform in the view.
     */
    void zoomToFit();

    /**
     * Zooms to the current selection.
     */
    void zoomToSelection();

    /**
     * Sets the visible range in seconds.
     *
     * @param startTime Start of visible range
     * @param endTime End of visible range
     */
    void setVisibleRange(double startTime, double endTime);

    /**
     * Returns the start of the visible range in seconds.
     */
    double getVisibleRangeStart() const { return m_visibleStart; }

    /**
     * Returns the end of the visible range in seconds.
     */
    double getVisibleRangeEnd() const { return m_visibleEnd; }

    /**
     * Returns the total duration of the loaded audio in seconds.
     */
    double getTotalDuration() const { return m_totalDuration; }

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    //==============================================================================
    // ChangeListener override

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    //==============================================================================
    // ScrollBar::Listener override

    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

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
     * Updates the scrollbar range and position.
     */
    void updateScrollbar();

    /**
     * Draws the time ruler at the top of the display.
     */
    void drawTimeRuler(juce::Graphics& g, juce::Rectangle<int> bounds);

    /**
     * Draws the waveform for a single channel.
     */
    void drawChannelWaveform(juce::Graphics& g, juce::Rectangle<int> bounds, int channelNum);

    /**
     * Draws the selection highlight.
     */
    void drawSelection(juce::Graphics& g, juce::Rectangle<int> bounds);

    /**
     * Draws the playback cursor.
     */
    void drawPlaybackCursor(juce::Graphics& g, juce::Rectangle<int> bounds);

    /**
     * Draws the edit cursor (yellow cursor indicating paste position).
     */
    void drawEditCursor(juce::Graphics& g, juce::Rectangle<int> bounds);

    /**
     * Constrains the visible range to valid bounds.
     */
    void constrainVisibleRange();

    //==============================================================================
    // Member variables

    // Audio thumbnail for waveform rendering
    juce::AudioThumbnailCache m_thumbnailCache;
    juce::AudioThumbnail m_thumbnail;

    // Scrollbar for navigation
    juce::ScrollBar m_scrollbar;

    // File state
    bool m_fileLoaded;
    bool m_isLoading;
    int m_numChannels;
    double m_sampleRate;
    double m_totalDuration;
    juce::String m_lastError;

    // View state
    double m_visibleStart;
    double m_visibleEnd;
    double m_zoomLevel;

    // Playback state
    double m_playbackPosition;

    // Selection state
    bool m_hasSelection;
    double m_selectionStart;
    double m_selectionEnd;
    bool m_isDraggingSelection;
    double m_dragStartTime;

    // Edit cursor state (for paste operations)
    bool m_hasEditCursor;
    double m_editCursorPosition;

    // Selection animation state
    float m_selectionAlpha;
    bool m_selectionAlphaIncreasing;

    // Layout constants
    static constexpr int RULER_HEIGHT = 30;
    static constexpr int SCROLLBAR_HEIGHT = 16;
    static constexpr int CHANNEL_GAP = 4;

    // Animation callback
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
