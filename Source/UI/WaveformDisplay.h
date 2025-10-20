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
#include <functional>
#include "../Utils/AudioUnits.h"
#include "../Utils/NavigationPreferences.h"

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

    /**
     * Enables or disables follow-playback mode (auto-scroll).
     * When enabled, the waveform automatically scrolls to keep the playback cursor visible.
     *
     * @param shouldFollow true to enable follow mode, false to disable
     */
    void setFollowPlayback(bool shouldFollow);

    /**
     * Returns true if follow-playback mode is enabled.
     */
    bool isFollowPlayback() const { return m_followPlayback; }

    //==============================================================================
    // View state callbacks

    /**
     * Callback triggered when the visible range changes (zoom, scroll, etc.).
     * Parameters: (startTime, endTime) in seconds.
     */
    std::function<void(double, double)> onVisibleRangeChanged;

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
     * Zooms to 1:1 pixel-per-sample view (maximum detail).
     */
    void zoomOneToOne();

    /**
     * Zooms to fit a specific region with small margins.
     * If regionIndex is -1, uses the currently selected region from RegionManager.
     *
     * @param regionIndex Index of region to zoom to, or -1 for selected region
     */
    void zoomToRegion(int regionIndex = -1);

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

    /**
     * Returns the current zoom level as a percentage.
     * 100% = fit to window, >100% = zoomed in, <100% = zoomed out
     */
    double getZoomPercentage() const;

    //==============================================================================
    // Two-tier snap mode system (unit selection + increment cycling)

    /**
     * Sets the snap unit type (Tier 1: unit selection).
     *
     * @param unitType Unit type (Samples, Milliseconds, Seconds, Frames)
     */
    void setSnapUnit(AudioUnits::UnitType unitType);

    /**
     * Gets the current snap unit type.
     */
    AudioUnits::UnitType getSnapUnit() const
    {
        juce::ScopedLock lock(m_snapLock);
        return m_snapUnitType;
    }

    /**
     * Cycles to the next snap increment within the current unit (Tier 2: increment dropdown).
     * Wraps around to the first increment after the last one.
     */
    void cycleSnapIncrement();

    /**
     * Toggles snap on/off (G key).
     * When toggling on, restores the last used increment.
     * When toggling off, remembers current increment for next enable.
     */
    void toggleSnap();

    /**
     * Checks if snap is enabled.
     */
    bool isSnapEnabled() const
    {
        juce::ScopedLock lock(m_snapLock);
        return m_snapEnabled;
    }

    /**
     * Gets the current snap increment value (raw, depends on unit type).
     */
    int getSnapIncrement() const;

    /**
     * Gets the current snap increment index (0 = off, 1+ = specific increments).
     */
    size_t getSnapIncrementIndex() const
    {
        juce::ScopedLock lock(m_snapLock);
        return m_snapIncrementIndex;
    }

    /**
     * Gets the current snap increment converted to seconds.
     * Used for navigation - arrow keys move by this amount.
     * Returns 0.0 if snap is off (increment index = 0).
     *
     * @return Snap increment in seconds
     */
    double getSnapIncrementInSeconds() const;

    /**
     * Toggles zero-crossing snap on/off (independent Z key toggle).
     */
    void toggleZeroCrossing();

    /**
     * Checks if zero-crossing snap is enabled.
     */
    bool isZeroCrossingEnabled() const
    {
        juce::ScopedLock lock(m_snapLock);
        return m_zeroCrossingEnabled;
    }

    /**
     * Sets the navigation preferences for keyboard navigation.
     */
    void setNavigationPreferences(const NavigationPreferences& prefs);

    /**
     * Gets the frame rate from navigation preferences.
     * Used for frame-based time display and navigation.
     *
     * @return Frame rate in frames per second (default: 30.0)
     */
    double getFrameRate() const
    {
        return m_navigationPrefs.getFrameRate();
    }

    /**
     * Gets a reference to the audio buffer (if loaded from buffer).
     * Used for zero-crossing snapping.
     */
    void setAudioBufferReference(const juce::AudioBuffer<float>* buffer);

    /**
     * Sets the RegionManager reference for drawing region overlays.
     *
     * @param regionManager Pointer to the RegionManager (nullptr to disable overlays)
     */
    void setRegionManager(class RegionManager* regionManager);

    //==============================================================================
    // Audio-unit based keyboard navigation

    /**
     * Navigates left by the current snap increment.
     * The increment is determined by the current snap mode (G key).
     *
     * @param extend If true, extends selection instead of moving cursor
     */
    void navigateLeft(bool extend = false);

    /**
     * Navigates right by the current snap increment.
     * The increment is determined by the current snap mode (G key).
     *
     * @param extend If true, extends selection instead of moving cursor
     */
    void navigateRight(bool extend = false);

    /**
     * Jumps to the start of the file.
     *
     * @param extend If true, extends selection to start
     */
    void navigateToStart(bool extend = false);

    /**
     * Jumps to the end of the file.
     *
     * @param extend If true, extends selection to end
     */
    void navigateToEnd(bool extend = false);

    /**
     * Navigates left by page increment (1 second default).
     * @param extend If true, extends selection instead of moving cursor
     */
    void navigatePageLeft(bool extend = false);

    /**
     * Navigates right by page increment (1 second default).
     * @param extend If true, extends selection instead of moving cursor
     */
    void navigatePageRight(bool extend = false);

    /**
     * Jumps to the first visible sample in the current view.
     *
     * @param extend If true, extends selection
     */
    void navigateToVisibleStart(bool extend = false);

    /**
     * Jumps to the last visible sample in the current view.
     *
     * @param extend If true, extends selection
     */
    void navigateToVisibleEnd(bool extend = false);

    /**
     * Centers the view on the current cursor or selection.
     */
    void centerViewOnCursor();

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
     * @param sendNotification If false, doesn't trigger scrollBarMoved callback
     */
    void updateScrollbar(bool sendNotification = true);

    /**
     * Draws the time ruler at the top of the display.
     */
    void drawTimeRuler(juce::Graphics& g, juce::Rectangle<int> bounds);

    /**
     * Draws the waveform for a single channel.
     */
    void drawChannelWaveform(juce::Graphics& g, juce::Rectangle<int> bounds, int channelNum);

    /**
     * Fast direct rendering from cached buffer (bypasses thumbnail regeneration).
     * Used immediately after edit operations for <10ms visual feedback.
     */
    void drawChannelWaveformDirect(juce::Graphics& g, juce::Rectangle<int> bounds, int channelNum);

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
     * Draws semi-transparent region overlays on the waveform.
     * Uses the same color palette as the Auto Region preview dialog.
     */
    void drawRegionOverlays(juce::Graphics& g, juce::Rectangle<int> bounds);

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
    bool m_followPlayback;              ///< Auto-scroll during playback when true
    double m_lastPlaybackPosition;      ///< Previous playback position for movement detection
    double m_lastUserScrollTime;        ///< Timestamp of last manual scroll (for auto-disable logic)
    bool m_isScrollingProgrammatically; ///< Flag to distinguish auto-scroll from user scroll
    mutable juce::CriticalSection m_playbackLock; ///< Thread safety for playback position updates

    // Selection state
    bool m_hasSelection;
    double m_selectionStart;
    double m_selectionEnd;
    bool m_isDraggingSelection;
    double m_dragStartTime;

    // Bidirectional selection extension (for Shift+Arrow navigation)
    bool m_isExtendingSelection;      // True when actively extending with Shift+arrows
    double m_selectionAnchor;         // Anchor point that stays fixed while extending

    // Edit cursor state (for paste operations)
    bool m_hasEditCursor;
    double m_editCursorPosition;

    // Selection animation state
    float m_selectionAlpha;
    bool m_selectionAlphaIncreasing;

    // Two-tier snap mode system (unit selection + increment cycling)
    AudioUnits::UnitType m_snapUnitType;       // Tier 1: Unit type (Samples, Ms, Seconds, Frames)
    size_t m_snapIncrementIndex;                // Tier 2: Index into increment array (0 = off)
    bool m_snapEnabled;                         // G key toggle: snap on/off (maintains last increment)
    size_t m_lastSnapIncrementIndex;            // Remember last non-zero increment when disabled
    bool m_zeroCrossingEnabled;                 // Z key toggle: independent zero-crossing snap
    NavigationPreferences m_navigationPrefs;
    const juce::AudioBuffer<float>* m_audioBufferRef; // Reference for zero-crossing snap
    juce::CriticalSection m_snapLock;           // Thread safety for snap mode changes

    // Fast direct rendering (for <10ms waveform updates after edits)
    juce::AudioBuffer<float> m_cachedBuffer;  // Cached copy for direct rendering
    bool m_useDirectRendering;  // If true, bypass thumbnail, render from buffer
    juce::CriticalSection m_bufferLock;  // Thread safety for buffer access

    // Region overlay rendering (optional, nullptr if not set)
    class RegionManager* m_regionManager;  // For drawing semi-transparent region overlays

    // Layout constants
    static constexpr int RULER_HEIGHT = 30;
    static constexpr int SCROLLBAR_HEIGHT = 16;
    static constexpr int CHANNEL_GAP = 4;

    // Time comparison epsilon (1ms for sample-accurate comparisons)
    static constexpr double TIME_EPSILON = 0.001;

    // Auto-scroll behavior constants
    static constexpr double SCROLL_TRIGGER_RIGHT = 0.75;  ///< Trigger scroll when cursor reaches 75% from left
    static constexpr double SCROLL_TRIGGER_LEFT = 0.20;   ///< Trigger scroll when cursor goes below 20% from left
    static constexpr double CURSOR_POSITION_RATIO = 0.25; ///< Position cursor at 25% from left edge during auto-scroll

    // Animation callback
    void timerCallback() override;

    // Helper: Snaps a time position using current snap settings
    double snapTimeToUnit(double time) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
