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

#include "WaveformDisplay.h"
#include "../Utils/AudioBufferInputSource.h"
#include <cmath>

//==============================================================================
WaveformDisplay::WaveformDisplay(juce::AudioFormatManager& formatManager)
    : m_thumbnailCache(5),
      m_thumbnail(512, formatManager, m_thumbnailCache),
      m_scrollbar(false),
      m_fileLoaded(false),
      m_isLoading(false),
      m_numChannels(0),
      m_sampleRate(44100.0),
      m_totalDuration(0.0),
      m_visibleStart(0.0),
      m_visibleEnd(10.0),
      m_zoomLevel(1.0),
      m_playbackPosition(0.0),
      m_hasSelection(false),
      m_selectionStart(0.0),
      m_selectionEnd(0.0),
      m_isDraggingSelection(false),
      m_dragStartTime(0.0),
      m_hasEditCursor(false),
      m_editCursorPosition(0.0),
      m_selectionAlpha(0.25f),
      m_selectionAlphaIncreasing(true)
{
    // Set up thumbnail
    m_thumbnail.addChangeListener(this);

    // Set up scrollbar
    m_scrollbar.addListener(this);
    m_scrollbar.setAutoHide(false);
    addAndMakeVisible(m_scrollbar);

    // Enable 60fps repaint for smooth cursor movement
    setOpaque(true);

    // Start animation timer for pulsing selection (30fps)
    startTimer(33);
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
    m_thumbnail.removeChangeListener(this);
    m_scrollbar.removeListener(this);
}

//==============================================================================
// File loading

bool WaveformDisplay::loadFile(const juce::File& file, double sampleRate, int numChannels)
{
    // Validate file
    if (!file.existsAsFile())
    {
        m_lastError = "File does not exist: " + file.getFileName();
        return false;
    }

    if (!file.hasReadAccess())
    {
        m_lastError = "No read permission for file: " + file.getFileName();
        return false;
    }

    clear();

    // Store audio properties
    m_sampleRate = sampleRate;
    m_numChannels = numChannels;

    // Load the file into the thumbnail asynchronously
    // AudioThumbnail takes ownership of the source
    auto source = std::make_unique<juce::FileInputSource>(file);
    m_thumbnail.setSource(source.release());

    // Set loading state
    m_isLoading = true;

    // The thumbnail will call changeListenerCallback when data is ready
    repaint();
    return true;
}

bool WaveformDisplay::reloadFromBuffer(const juce::AudioBuffer<float>& buffer,
                                       double sampleRate,
                                       bool preserveView,
                                       bool preserveEditCursor)
{
    // IMPORTANT: Must be called from message thread only
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Validate buffer
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        m_lastError = "Cannot reload from empty buffer";
        juce::Logger::writeToLog("WaveformDisplay::reloadFromBuffer - Empty buffer");
        return false;
    }

    if (sampleRate <= 0.0)
    {
        m_lastError = "Invalid sample rate";
        juce::Logger::writeToLog("WaveformDisplay::reloadFromBuffer - Invalid sample rate: " + juce::String(sampleRate));
        return false;
    }

    // CRITICAL: Save view state BEFORE any changes
    double savedVisibleStart = m_visibleStart;
    double savedVisibleEnd = m_visibleEnd;
    double savedZoomLevel = m_zoomLevel;
    bool savedHasEditCursor = m_hasEditCursor;
    double savedEditCursorPos = m_editCursorPosition;

    juce::Logger::writeToLog(juce::String::formatted(
        "reloadFromBuffer BEFORE - view: %.2f-%.2f, cursor: %s at %.2f, preserveView=%d, preserveEditCursor=%d",
        savedVisibleStart, savedVisibleEnd,
        savedHasEditCursor ? "YES" : "NO", savedEditCursorPos,
        preserveView ? 1 : 0, preserveEditCursor ? 1 : 0));

    // Store audio properties
    m_sampleRate = sampleRate;
    m_numChannels = buffer.getNumChannels();
    m_totalDuration = buffer.getNumSamples() / sampleRate;

    // CRITICAL FIX FOR WAVEFORM DEGRADATION (2025-10-07):
    // =====================================================
    // Previous implementation used manual reset() + addBlock() pattern which caused
    // visual artifacts (blockiness, degradation) after edit operations.
    //
    // ROOT CAUSE:
    // - AudioThumbnail cache not properly cleared before regeneration
    // - Manual addBlock() bypasses JUCE's internal caching/resolution algorithms
    // - No proper cache invalidation when buffer size changes
    //
    // SOLUTION:
    // Use setSource() with AudioBufferInputSource (proper JUCE pattern).
    // This matches how loadFile() works and ensures:
    // - Clean cache management
    // - Proper resolution calculations
    // - Automatic thumbnail regeneration
    // - No visual artifacts
    //
    // AudioBufferInputSource wraps the buffer as a WAV-formatted InputSource,
    // allowing AudioThumbnail to handle it exactly like a file.
    auto source = new AudioBufferInputSource(buffer, sampleRate, m_numChannels);
    m_thumbnail.setSource(source); // AudioThumbnail takes ownership, will delete source

    // Mark as loading (AudioThumbnail will call changeListenerCallback when ready)
    m_fileLoaded = false;
    m_isLoading = true;

    // CRITICAL: Restore view state if requested
    if (preserveView && m_totalDuration > 0)
    {
        // Constrain saved view to new file bounds
        m_visibleStart = juce::jlimit(0.0, m_totalDuration, savedVisibleStart);
        m_visibleEnd = juce::jlimit(m_visibleStart, m_totalDuration, savedVisibleEnd);
        m_zoomLevel = savedZoomLevel;

        juce::Logger::writeToLog(juce::String::formatted(
            "WaveformDisplay::reloadFromBuffer - View preserved: %.2f-%.2f, zoom: %.2f",
            m_visibleStart, m_visibleEnd, m_zoomLevel));
    }
    else
    {
        // Default behavior: fit first 10 seconds to view
        m_visibleStart = 0.0;
        m_visibleEnd = juce::jmin(10.0, m_totalDuration);
        m_zoomLevel = 1.0;
    }

    updateScrollbar();

    // CRITICAL: Restore edit cursor if requested
    if (preserveEditCursor && savedHasEditCursor)
    {
        if (savedEditCursorPos <= m_totalDuration)
        {
            m_editCursorPosition = savedEditCursorPos;
            m_hasEditCursor = true;
            juce::Logger::writeToLog(juce::String::formatted(
                "Edit cursor RESTORED at %.2f", m_editCursorPosition));
        }
        else
        {
            m_editCursorPosition = m_totalDuration;
            m_hasEditCursor = true;
            juce::Logger::writeToLog(juce::String::formatted(
                "Edit cursor MOVED to file end: %.2f", m_editCursorPosition));
        }
    }
    else if (!preserveEditCursor)
    {
        // Clear selection and cursor if not preserving
        clearSelection();
        m_hasEditCursor = false;
        m_playbackPosition = 0.0;
        juce::Logger::writeToLog("Edit cursor CLEARED (not preserving)");
    }
    else
    {
        juce::Logger::writeToLog("Edit cursor NOT restored (savedHasEditCursor=false)");
    }

    juce::Logger::writeToLog("WaveformDisplay: Reloaded from buffer - " +
                             juce::String(buffer.getNumSamples()) + " samples, " +
                             juce::String(m_numChannels) + " channels, " +
                             juce::String(m_totalDuration, 2) + " seconds");

    repaint();
    return true;
}

void WaveformDisplay::clear()
{
    m_thumbnail.clear();
    m_fileLoaded = false;
    m_isLoading = false;
    m_numChannels = 0;
    m_totalDuration = 0.0;
    m_visibleStart = 0.0;
    m_visibleEnd = 10.0;
    m_playbackPosition = 0.0;
    m_lastError.clear();
    clearSelection();
    clearEditCursor();
    updateScrollbar();
    repaint();
}

//==============================================================================
// Playback control

void WaveformDisplay::setPlaybackPosition(double positionInSeconds)
{
    // Use epsilon comparison for floating point
    if (std::abs(m_playbackPosition - positionInSeconds) > 0.001)
    {
        m_playbackPosition = positionInSeconds;

        // Auto-scroll to keep playback cursor visible
        if (m_playbackPosition < m_visibleStart || m_playbackPosition > m_visibleEnd)
        {
            double visibleDuration = m_visibleEnd - m_visibleStart;
            m_visibleStart = m_playbackPosition - visibleDuration * 0.25; // Position at 25% from left
            m_visibleEnd = m_visibleStart + visibleDuration;
            constrainVisibleRange();
            updateScrollbar();
        }

        repaint();
    }
}

//==============================================================================
// Selection

void WaveformDisplay::setSelection(double startInSeconds, double endInSeconds)
{
    m_hasSelection = true;
    m_selectionStart = juce::jmin(startInSeconds, endInSeconds);
    m_selectionEnd = juce::jmax(startInSeconds, endInSeconds);

    // Force immediate repaint to show selection
    repaint();
}

void WaveformDisplay::clearSelection()
{
    m_hasSelection = false;
    m_selectionStart = 0.0;
    m_selectionEnd = 0.0;
    repaint();
}

double WaveformDisplay::getSelectionDuration() const
{
    if (!m_hasSelection)
    {
        return 0.0;
    }
    return std::abs(m_selectionEnd - m_selectionStart);
}

juce::String WaveformDisplay::getSelectionStartString() const
{
    if (!m_hasSelection)
    {
        return "--:--:--.---";
    }

    double time = m_selectionStart;
    int hours = static_cast<int>(time / 3600.0);
    int minutes = static_cast<int>((time - hours * 3600.0) / 60.0);
    double seconds = time - hours * 3600.0 - minutes * 60.0;

    if (hours > 0)
    {
        return juce::String::formatted("%02d:%02d:%06.3f", hours, minutes, seconds);
    }
    else
    {
        return juce::String::formatted("%02d:%06.3f", minutes, seconds);
    }
}

juce::String WaveformDisplay::getSelectionEndString() const
{
    if (!m_hasSelection)
    {
        return "--:--:--.---";
    }

    double time = m_selectionEnd;
    int hours = static_cast<int>(time / 3600.0);
    int minutes = static_cast<int>((time - hours * 3600.0) / 60.0);
    double seconds = time - hours * 3600.0 - minutes * 60.0;

    if (hours > 0)
    {
        return juce::String::formatted("%02d:%02d:%06.3f", hours, minutes, seconds);
    }
    else
    {
        return juce::String::formatted("%02d:%06.3f", minutes, seconds);
    }
}

juce::String WaveformDisplay::getSelectionDurationString() const
{
    if (!m_hasSelection)
    {
        return "--:--:--.---";
    }

    double duration = getSelectionDuration();
    int hours = static_cast<int>(duration / 3600.0);
    int minutes = static_cast<int>((duration - hours * 3600.0) / 60.0);
    double seconds = duration - hours * 3600.0 - minutes * 60.0;

    if (hours > 0)
    {
        return juce::String::formatted("%02d:%02d:%06.3f", hours, minutes, seconds);
    }
    else
    {
        return juce::String::formatted("%02d:%06.3f", minutes, seconds);
    }
}

//==============================================================================
// Edit cursor

void WaveformDisplay::setEditCursor(double positionInSeconds)
{
    // Clamp position to valid range
    m_editCursorPosition = juce::jlimit(0.0, m_totalDuration, positionInSeconds);
    m_hasEditCursor = true;

    // Auto-scroll to show cursor if it's outside visible range
    if (m_editCursorPosition < m_visibleStart || m_editCursorPosition > m_visibleEnd)
    {
        double visibleDuration = m_visibleEnd - m_visibleStart;
        m_visibleStart = m_editCursorPosition - visibleDuration * 0.25; // Position at 25% from left
        m_visibleEnd = m_visibleStart + visibleDuration;
        constrainVisibleRange();
        updateScrollbar();
    }

    repaint();
}

void WaveformDisplay::clearEditCursor()
{
    m_hasEditCursor = false;
    m_editCursorPosition = 0.0;
    repaint();
}

void WaveformDisplay::moveEditCursor(double deltaInSeconds)
{
    if (!m_hasEditCursor)
    {
        // If no cursor, create one at the current playback position
        setEditCursor(m_playbackPosition);
        return;
    }

    double newPosition = m_editCursorPosition + deltaInSeconds;
    setEditCursor(newPosition);
}

//==============================================================================
// Zoom and navigation

void WaveformDisplay::zoomIn()
{
    if (!m_fileLoaded)
    {
        return;
    }

    double centerTime = (m_visibleStart + m_visibleEnd) * 0.5;
    double newDuration = (m_visibleEnd - m_visibleStart) * 0.5;

    m_visibleStart = centerTime - newDuration * 0.5;
    m_visibleEnd = centerTime + newDuration * 0.5;

    constrainVisibleRange();
    updateScrollbar();
    repaint();
}

void WaveformDisplay::zoomOut()
{
    if (!m_fileLoaded)
    {
        return;
    }

    double centerTime = (m_visibleStart + m_visibleEnd) * 0.5;
    double newDuration = (m_visibleEnd - m_visibleStart) * 2.0;

    // Don't zoom out beyond the file duration
    if (newDuration >= m_totalDuration)
    {
        zoomToFit();
        return;
    }

    m_visibleStart = centerTime - newDuration * 0.5;
    m_visibleEnd = centerTime + newDuration * 0.5;

    constrainVisibleRange();
    updateScrollbar();
    repaint();
}

void WaveformDisplay::zoomToFit()
{
    if (!m_fileLoaded)
    {
        return;
    }

    m_visibleStart = 0.0;
    m_visibleEnd = m_totalDuration;
    m_zoomLevel = 1.0;

    updateScrollbar();
    repaint();
}

void WaveformDisplay::zoomToSelection()
{
    if (!m_fileLoaded || !m_hasSelection)
    {
        return;
    }

    // Add 10% padding on each side
    double selectionDuration = m_selectionEnd - m_selectionStart;
    double padding = selectionDuration * 0.1;

    m_visibleStart = juce::jmax(0.0, m_selectionStart - padding);
    m_visibleEnd = juce::jmin(m_totalDuration, m_selectionEnd + padding);

    constrainVisibleRange();
    updateScrollbar();
    repaint();
}

void WaveformDisplay::setVisibleRange(double startTime, double endTime)
{
    m_visibleStart = startTime;
    m_visibleEnd = endTime;
    constrainVisibleRange();
    updateScrollbar();
    repaint();
}

//==============================================================================
// Component overrides

void WaveformDisplay::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff1e1e1e)); // Dark grey background

    auto bounds = getLocalBounds();

    // Reserve space for scrollbar
    bounds.removeFromBottom(SCROLLBAR_HEIGHT);

    // Draw time ruler
    auto rulerBounds = bounds.removeFromTop(RULER_HEIGHT);
    drawTimeRuler(g, rulerBounds);

    if (m_isLoading)
    {
        // Loading in progress
        g.setColour(juce::Colours::white);
        g.setFont(16.0f);
        g.drawText("Loading waveform...", bounds, juce::Justification::centred, true);
        return;
    }

    if (!m_fileLoaded)
    {
        // No file loaded
        g.setColour(juce::Colours::grey);
        g.setFont(14.0f);
        g.drawText("No audio file loaded", bounds, juce::Justification::centred, true);
        return;
    }

    // Save the full waveform area bounds BEFORE we start modifying it
    auto waveformArea = bounds;

    // Draw waveforms for each channel FIRST
    if (m_numChannels == 1)
    {
        // Mono - use full height
        drawChannelWaveform(g, bounds, 0);
    }
    else if (m_numChannels >= 2)
    {
        // Stereo or multichannel - split vertically
        int channelHeight = bounds.getHeight() / m_numChannels;

        for (int ch = 0; ch < m_numChannels; ++ch)
        {
            auto channelBounds = bounds.removeFromTop(channelHeight);
            if (ch < m_numChannels - 1)
            {
                channelBounds.removeFromBottom(CHANNEL_GAP);
            }
            drawChannelWaveform(g, channelBounds, ch);
        }
    }

    // Draw selection highlight ON TOP of waveform (so it's visible!)
    if (m_hasSelection)
    {
        drawSelection(g, waveformArea);
    }

    // Draw playback cursor (green)
    drawPlaybackCursor(g, waveformArea);

    // Draw edit cursor on top of everything (yellow, shows paste position)
    drawEditCursor(g, waveformArea);
}

void WaveformDisplay::resized()
{
    auto bounds = getLocalBounds();

    // Position scrollbar at bottom
    m_scrollbar.setBounds(bounds.removeFromBottom(SCROLLBAR_HEIGHT));

    // Update scrollbar after layout
    updateScrollbar();
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& event)
{
    if (!m_fileLoaded)
    {
        return;
    }

    // Ignore clicks on scrollbar or ruler
    if (event.y < RULER_HEIGHT || event.y > getHeight() - SCROLLBAR_HEIGHT)
    {
        return;
    }

    // Start selection drag
    m_isDraggingSelection = true;
    // CRITICAL FIX (2025-10-07): Clamp coordinates for consistency with mouseDrag
    int clampedX = juce::jlimit(0, getWidth() - 1, event.x);
    m_dragStartTime = xToTime(clampedX);

    // Set edit cursor on click (will be cleared if user drags)
    setEditCursor(m_dragStartTime);

    // Initialize selection at click point
    setSelection(m_dragStartTime, m_dragStartTime);
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& event)
{
    if (!m_fileLoaded || !m_isDraggingSelection)
    {
        return;
    }

    // CRITICAL FIX (2025-10-07): Clamp mouse coordinates before xToTime conversion
    // Prevents negative sample positions when mouse is outside widget bounds
    int clampedX = juce::jlimit(0, getWidth() - 1, event.x);

    double currentTime = xToTime(clampedX);

    // Clear edit cursor when user starts dragging (creating a selection)
    // This is intentional: edit cursor is for single-click positioning,
    // while drag is for selection. They are mutually exclusive.
    if (std::abs(currentTime - m_dragStartTime) > 0.01) // 10ms threshold
    {
        clearEditCursor();
    }

    setSelection(m_dragStartTime, currentTime);
}

void WaveformDisplay::mouseUp(const juce::MouseEvent& /*event*/)
{
    if (!m_fileLoaded || !m_isDraggingSelection)
    {
        return;
    }

    m_isDraggingSelection = false;

    // If selection is too small (less than 0.01 seconds), treat as a single click
    // Keep the edit cursor, clear the selection
    if (std::abs(m_selectionEnd - m_selectionStart) < 0.01)
    {
        clearSelection();
        // Edit cursor is already set from mouseDown
    }
    else
    {
        // User created a selection by dragging, clear edit cursor
        clearEditCursor();
    }
}

void WaveformDisplay::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (!m_fileLoaded)
    {
        return;
    }

    // Zoom with mouse wheel (works without modifiers for easier UX)
    // Zoom centered on mouse position
    double mouseTime = xToTime(event.x);
    double visibleDuration = m_visibleEnd - m_visibleStart;

    juce::Logger::writeToLog(juce::String::formatted(
        "Mouse wheel: deltaY=%.3f, current view: %.2f-%.2f",
        wheel.deltaY, m_visibleStart, m_visibleEnd));

    if (wheel.deltaY > 0.0f)
    {
        // Zoom in
        visibleDuration *= 0.8;
        juce::Logger::writeToLog("Zooming IN");
    }
    else if (wheel.deltaY < 0.0f)
    {
        // Zoom out
        visibleDuration *= 1.25;
        juce::Logger::writeToLog("Zooming OUT");
        if (visibleDuration >= m_totalDuration)
        {
            zoomToFit();
            return;
        }
    }

    // Calculate new visible range centered on mouse
    double mouseRatio = (mouseTime - m_visibleStart) / (m_visibleEnd - m_visibleStart);
    m_visibleStart = mouseTime - visibleDuration * mouseRatio;
    m_visibleEnd = m_visibleStart + visibleDuration;

    constrainVisibleRange();
    updateScrollbar();
    repaint();

    juce::Logger::writeToLog(juce::String::formatted(
        "After zoom: view: %.2f-%.2f",
        m_visibleStart, m_visibleEnd));
}

//==============================================================================
// Timer callback for selection animation

void WaveformDisplay::timerCallback()
{
    if (!m_hasSelection)
    {
        return;
    }

    // Animate selection overlay alpha between 0.25 and 0.35 for subtle pulsing effect
    const float alphaStep = 0.01f; // Smooth animation
    const float minAlpha = 0.25f;
    const float maxAlpha = 0.35f;

    if (m_selectionAlphaIncreasing)
    {
        m_selectionAlpha += alphaStep;
        if (m_selectionAlpha >= maxAlpha)
        {
            m_selectionAlpha = maxAlpha;
            m_selectionAlphaIncreasing = false;
        }
    }
    else
    {
        m_selectionAlpha -= alphaStep;
        if (m_selectionAlpha <= minAlpha)
        {
            m_selectionAlpha = minAlpha;
            m_selectionAlphaIncreasing = true;
        }
    }

    // Trigger repaint for animation
    repaint();
}

//==============================================================================
// ChangeListener override

void WaveformDisplay::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &m_thumbnail)
    {
        // Check if thumbnail is ready
        if (m_isLoading && m_thumbnail.getTotalLength() > 0.0)
        {
            // Thumbnail loaded successfully
            m_fileLoaded = true;
            m_isLoading = false;
            m_totalDuration = m_thumbnail.getTotalLength();

            // Set visible range to show entire file initially
            m_visibleStart = 0.0;
            m_visibleEnd = m_totalDuration;
            m_zoomLevel = 1.0;

            // Reset playback position
            m_playbackPosition = 0.0;

            // Clear selection
            clearSelection();

            // Update scrollbar
            updateScrollbar();
        }

        // Repaint for loading progress or completion
        repaint();
    }
}

//==============================================================================
// ScrollBar::Listener override

void WaveformDisplay::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (!m_fileLoaded || scrollBarThatHasMoved != &m_scrollbar)
    {
        return;
    }

    double visibleDuration = m_visibleEnd - m_visibleStart;
    m_visibleStart = newRangeStart;
    m_visibleEnd = m_visibleStart + visibleDuration;

    constrainVisibleRange();
    repaint();
}

//==============================================================================
// Helper methods

int WaveformDisplay::timeToX(double timeInSeconds) const
{
    // CRITICAL FIX (2025-10-07): Guard against invalid state and zero width
    int width = getWidth();
    if (m_visibleEnd <= m_visibleStart || width <= 0)
    {
        return 0;
    }

    // Clamp time to visible range before conversion
    double clampedTime = juce::jlimit(m_visibleStart, m_visibleEnd, timeInSeconds);
    double ratio = (clampedTime - m_visibleStart) / (m_visibleEnd - m_visibleStart);

    // Clamp result to valid pixel range
    return juce::jlimit(0, width - 1, static_cast<int>(ratio * width));
}

double WaveformDisplay::xToTime(int x) const
{
    // CRITICAL FIX (2025-10-07): Guard against zero width (component not laid out yet)
    int width = getWidth();
    if (width <= 0)
    {
        return m_visibleStart;  // Return start of visible range when width invalid
    }

    // CRITICAL FIX (2025-10-07): Prevent negative time when mouse is outside widget bounds
    // Clamp x to [0, width-1] to ensure valid pixel coordinates
    int clampedX = juce::jlimit(0, width - 1, x);

    double ratio = static_cast<double>(clampedX) / width;
    double time = m_visibleStart + ratio * (m_visibleEnd - m_visibleStart);

    // Clamp time to [0, m_totalDuration] to ensure valid time range
    return juce::jlimit(0.0, m_totalDuration, time);
}

void WaveformDisplay::updateScrollbar()
{
    if (!m_fileLoaded)
    {
        m_scrollbar.setRangeLimits(0.0, 1.0);
        m_scrollbar.setCurrentRange(0.0, 1.0);
        return;
    }

    // Set scrollbar range
    m_scrollbar.setRangeLimits(0.0, m_totalDuration);

    // Set visible range
    double visibleDuration = m_visibleEnd - m_visibleStart;
    m_scrollbar.setCurrentRange(m_visibleStart, visibleDuration);
}

void WaveformDisplay::drawTimeRuler(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Background for ruler
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(bounds);

    // Border
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawLine(bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getBottom(), 1.0f);

    if (!m_fileLoaded)
    {
        return;
    }

    // Calculate time markers
    double visibleDuration = m_visibleEnd - m_visibleStart;
    double pixelsPerSecond = bounds.getWidth() / visibleDuration;

    // Determine marker interval based on zoom level
    double markerInterval = 1.0; // Default: 1 second
    if (pixelsPerSecond < 10)
    {
        markerInterval = 10.0;
    }
    else if (pixelsPerSecond < 50)
    {
        markerInterval = 5.0;
    }
    else if (pixelsPerSecond > 200)
    {
        markerInterval = 0.1;
    }
    else if (pixelsPerSecond > 100)
    {
        markerInterval = 0.5;
    }

    // Draw time markers
    g.setColour(juce::Colours::white);
    g.setFont(12.0f);

    double firstMarker = std::ceil(m_visibleStart / markerInterval) * markerInterval;
    for (double time = firstMarker; time <= m_visibleEnd; time += markerInterval)
    {
        int x = timeToX(time);

        // Draw tick mark
        g.drawLine(x, bounds.getBottom() - 8, x, bounds.getBottom(), 1.0f);

        // Draw time label
        juce::String timeLabel;
        if (time >= 60.0)
        {
            int minutes = static_cast<int>(time / 60.0);
            double seconds = time - minutes * 60.0;
            timeLabel = juce::String::formatted("%d:%05.2f", minutes, seconds);
        }
        else
        {
            timeLabel = juce::String::formatted("%.2fs", time);
        }

        g.drawText(timeLabel, x - 30, bounds.getY() + 2, 60, bounds.getHeight() - 4,
                   juce::Justification::centred, true);
    }
}

void WaveformDisplay::drawChannelWaveform(juce::Graphics& g, juce::Rectangle<int> bounds, int channelNum)
{
    // Channel background
    g.setColour(juce::Colour(0xff252525));
    g.fillRect(bounds);

    // Center line
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawLine(bounds.getX(), bounds.getCentreY(),
               bounds.getRight(), bounds.getCentreY(), 1.0f);

    // Draw waveform
    g.setColour(juce::Colour(0xff00d4aa)); // JUCE brand color
    m_thumbnail.drawChannel(g, bounds, m_visibleStart, m_visibleEnd, channelNum, 1.0f);

    // Channel label (for stereo)
    if (m_numChannels == 2)
    {
        g.setColour(juce::Colours::grey);
        g.setFont(10.0f);
        juce::String label = (channelNum == 0) ? "L" : "R";
        g.drawText(label, bounds.getX() + 5, bounds.getY() + 5, 20, 20,
                   juce::Justification::centred, true);
    }
}

void WaveformDisplay::drawSelection(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    int startX = timeToX(m_selectionStart);
    int endX = timeToX(m_selectionEnd);

    // Clamp to visible area
    startX = juce::jmax(0, startX);
    endX = juce::jmin(getWidth(), endX);

    // Debug logging to verify coordinates
    if (endX <= startX)
    {
        // Selection is too small or outside visible range
        return;
    }

    // Create a clipped graphics context to ensure we only draw within bounds
    g.saveState();
    g.reduceClipRegion(bounds);

    // METHOD 1: Animated pulsing overlay for better visibility
    // Use animated alpha for subtle pulsing effect (Phase 1.2 requirement)
    g.setColour(juce::Colours::black.withAlpha(m_selectionAlpha)); // Black overlay with animated alpha
    g.fillRect(startX, bounds.getY(), endX - startX, bounds.getHeight());

    // METHOD 2: Inverted/brightened area using blend mode
    // Draw a bright accent color on the edges for clear boundaries
    g.setColour(juce::Colour(0x88FFFF00)); // Semi-transparent yellow highlight
    g.fillRect(startX, bounds.getY(), 3, bounds.getHeight()); // Left edge highlight
    g.fillRect(endX - 3, bounds.getY(), 3, bounds.getHeight()); // Right edge highlight

    // METHOD 3: Strong border lines for clear delineation (2.5px as per Phase 1.2)
    // Draw thick white borders with drop shadow effect
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawLine(startX - 1, bounds.getY(), startX - 1, bounds.getBottom(), 2.5f); // Shadow left
    g.drawLine(endX + 1, bounds.getY(), endX + 1, bounds.getBottom(), 2.5f); // Shadow right

    g.setColour(juce::Colours::white);
    g.drawLine(startX, bounds.getY(), startX, bounds.getBottom(), 2.5f); // Bright left edge
    g.drawLine(endX, bounds.getY(), endX, bounds.getBottom(), 2.5f); // Bright right edge

    // METHOD 4: Yellow corner handles for visual anchoring (Phase 1.2)
    g.setColour(juce::Colour(0xFFFFFF00)); // Bright yellow
    g.fillRect(startX - 3, bounds.getY(), 6, 12); // Top left handle
    g.fillRect(startX - 3, bounds.getBottom() - 12, 6, 12); // Bottom left handle
    g.fillRect(endX - 3, bounds.getY(), 6, 12); // Top right handle
    g.fillRect(endX - 3, bounds.getBottom() - 12, 6, 12); // Bottom right handle

    // METHOD 5: Duration label at center of selection (Phase 1.2)
    double duration = getSelectionDuration();
    juce::String durationLabel;
    int hours = static_cast<int>(duration / 3600.0);
    int minutes = static_cast<int>((duration - hours * 3600.0) / 60.0);
    double seconds = duration - hours * 3600.0 - minutes * 60.0;

    if (hours > 0)
    {
        durationLabel = juce::String::formatted("%dh %dm %.1fs", hours, minutes, seconds);
    }
    else if (minutes > 0)
    {
        durationLabel = juce::String::formatted("%dm %.1fs", minutes, seconds);
    }
    else
    {
        durationLabel = juce::String::formatted("%.2fs", duration);
    }

    // Draw label at center of selection
    int centerX = (startX + endX) / 2;
    int centerY = bounds.getCentreY();
    int labelWidth = 100;
    int labelHeight = 24;

    // Only draw label if selection is wide enough
    if (endX - startX > labelWidth)
    {
        // Draw label background with semi-transparent black
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillRoundedRectangle(centerX - labelWidth / 2.0f, centerY - labelHeight / 2.0f,
                               static_cast<float>(labelWidth), static_cast<float>(labelHeight), 4.0f);

        // Draw label text in bright yellow
        g.setColour(juce::Colour(0xFFFFFF00));
        g.setFont(13.0f);
        g.drawText(durationLabel, centerX - labelWidth / 2, centerY - labelHeight / 2,
                   labelWidth, labelHeight, juce::Justification::centred, true);
    }

    // Restore graphics state
    g.restoreState();
}

void WaveformDisplay::drawPlaybackCursor(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (m_playbackPosition < m_visibleStart || m_playbackPosition > m_visibleEnd)
    {
        return; // Cursor not visible
    }

    int x = timeToX(m_playbackPosition);

    // Draw playback cursor in GREEN (distinct from yellow edit cursor)
    g.setColour(juce::Colour(0xFF00FF00)); // Bright green for playback
    g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 2.0f);

    // Draw triangle at top
    juce::Path triangle;
    triangle.addTriangle(x - 5, bounds.getY(),
                        x + 5, bounds.getY(),
                        x, bounds.getY() + 8);
    g.fillPath(triangle);
}

void WaveformDisplay::drawEditCursor(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (!m_hasEditCursor)
    {
        return; // No edit cursor active
    }

    if (m_editCursorPosition < m_visibleStart || m_editCursorPosition > m_visibleEnd)
    {
        return; // Cursor not visible
    }

    int x = timeToX(m_editCursorPosition);

    // Draw edit cursor in YELLOW (distinct from green playback cursor)
    g.setColour(juce::Colour(0xFFFFFF00)); // Bright yellow for edit cursor
    g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 2.5f); // Slightly thicker than playback

    // Draw triangle at top
    juce::Path triangle;
    triangle.addTriangle(x - 6, bounds.getY(),
                        x + 6, bounds.getY(),
                        x, bounds.getY() + 10);
    g.fillPath(triangle);

    // Draw time label above cursor
    juce::String timeLabel;
    double time = m_editCursorPosition;
    int hours = static_cast<int>(time / 3600.0);
    int minutes = static_cast<int>((time - hours * 3600.0) / 60.0);
    double seconds = time - hours * 3600.0 - minutes * 60.0;

    if (hours > 0)
    {
        timeLabel = juce::String::formatted("%02d:%02d:%06.3f", hours, minutes, seconds);
    }
    else if (minutes > 0)
    {
        timeLabel = juce::String::formatted("%02d:%06.3f", minutes, seconds);
    }
    else
    {
        timeLabel = juce::String::formatted("%.3fs", time);
    }

    // Draw label background
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    int labelWidth = 80;
    int labelHeight = 20;
    int labelX = x - labelWidth / 2;
    int labelY = bounds.getY() + 12; // Just below the triangle
    g.fillRect(labelX, labelY, labelWidth, labelHeight);

    // Draw label text
    g.setColour(juce::Colours::yellow);
    g.setFont(12.0f);
    g.drawText(timeLabel, labelX, labelY, labelWidth, labelHeight,
               juce::Justification::centred, true);
}

void WaveformDisplay::constrainVisibleRange()
{
    if (!m_fileLoaded)
    {
        return;
    }

    // Ensure visible range doesn't exceed file bounds
    double visibleDuration = m_visibleEnd - m_visibleStart;

    if (visibleDuration > m_totalDuration)
    {
        // View is wider than file
        m_visibleStart = 0.0;
        m_visibleEnd = m_totalDuration;
    }
    else
    {
        // Constrain start
        if (m_visibleStart < 0.0)
        {
            m_visibleStart = 0.0;
            m_visibleEnd = visibleDuration;
        }

        // Constrain end
        if (m_visibleEnd > m_totalDuration)
        {
            m_visibleEnd = m_totalDuration;
            m_visibleStart = m_totalDuration - visibleDuration;
        }
    }
}
