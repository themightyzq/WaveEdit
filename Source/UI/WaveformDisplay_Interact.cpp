/*
  ==============================================================================

    WaveformDisplay_Interact.cpp
    WaveformEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Split out from WaveformDisplay.cpp under CLAUDE.md §7.5 (file-size
    cap). Hosts the user-input surface: mouse events
    (mouseDown / Drag / Up / DoubleClick / WheelMove), the zoom
    primitives (zoomIn / Out / Fit / Selection / OneToOne / ToRegion),
    and the navigation primitives (navigateLeft / Right / ToStart /
    ToEnd / PageLeft / PageRight / ToVisibleStart / ToVisibleEnd,
    centerViewOnCursor, setVisibleRange, getZoomPercentage). Render
    and state plumbing live elsewhere.

  ==============================================================================
*/

#include "WaveformDisplay.h"
#include "../Utils/RegionManager.h"
#include "../Utils/Region.h"
#include "../Utils/Settings.h"
#include "../Audio/ChannelLayout.h"
#include <cmath>
#include <juce_gui_extra/juce_gui_extra.h>

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

    // Notify listeners (RegionDisplay) of visible range change
    if (onVisibleRangeChanged)
        onVisibleRangeChanged(m_visibleStart, m_visibleEnd);
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

    // Notify listeners (RegionDisplay) of visible range change
    if (onVisibleRangeChanged)
        onVisibleRangeChanged(m_visibleStart, m_visibleEnd);
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

    // Notify listeners (RegionDisplay) of visible range change
    if (onVisibleRangeChanged)
        onVisibleRangeChanged(m_visibleStart, m_visibleEnd);
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

    // Notify listeners (RegionDisplay) of visible range change
    if (onVisibleRangeChanged)
        onVisibleRangeChanged(m_visibleStart, m_visibleEnd);
}

//==============================================================================
// Two-tier snap mode system

double WaveformDisplay::getZoomPercentage() const
{
    if (!m_fileLoaded || m_totalDuration <= 0.0)
        return 100.0;

    // Calculate how much of the file is visible
    double visibleRatio = (m_visibleEnd - m_visibleStart) / m_totalDuration;

    // 100% = entire file visible (fit to window)
    // >100% = zoomed out (shouldn't happen with constrainVisibleRange)
    // <100% = zoomed in (e.g., 10% means 10x zoom)
    return visibleRatio * 100.0;
}

void WaveformDisplay::zoomOneToOne()
{
    if (!m_fileLoaded)
        return;

    // Calculate the duration that would give us 1 pixel per sample
    int width = getWidth();
    if (width <= 0)
        return;

    double samplesPerPixel = 1.0;
    double visibleDuration = (width * samplesPerPixel) / m_sampleRate;

    // Center on current cursor or playback position
    double centerTime = m_hasEditCursor ? m_editCursorPosition : m_playbackPosition;

    m_visibleStart = centerTime - visibleDuration * 0.5;
    m_visibleEnd = centerTime + visibleDuration * 0.5;

    constrainVisibleRange();
    updateScrollbar();
    repaint();
}

void WaveformDisplay::zoomToRegion(int regionIndex)
{
    if (!m_fileLoaded || !m_regionManager)
        return;

    // If regionIndex is -1, use the currently selected region
    if (regionIndex == -1)
    {
        regionIndex = m_regionManager->getSelectedRegionIndex();
    }

    // Validate region exists
    if (regionIndex < 0 || regionIndex >= m_regionManager->getNumRegions())
        return;

    const Region* region = m_regionManager->getRegion(regionIndex);
    if (!region)
        return;

    // Convert sample positions to seconds
    double regionStart = static_cast<double>(region->getStartSample()) / m_sampleRate;
    double regionEnd = static_cast<double>(region->getEndSample()) / m_sampleRate;
    double regionDuration = regionEnd - regionStart;

    // Add 10% padding on each side (same as zoomToSelection)
    double padding = regionDuration * 0.1;

    m_visibleStart = juce::jmax(0.0, regionStart - padding);
    m_visibleEnd = juce::jmin(m_totalDuration, regionEnd + padding);

    constrainVisibleRange();
    updateScrollbar();
    repaint();

    // Notify listeners (RegionDisplay) of visible range change
    if (onVisibleRangeChanged)
        onVisibleRangeChanged(m_visibleStart, m_visibleEnd);
}

//==============================================================================
// Audio-unit based keyboard navigation

void WaveformDisplay::navigateLeft(bool extend)
{
    if (!m_fileLoaded)
        return;

    // Get increment from current snap mode (arrow keys now honor snap mode)
    double delta = getSnapIncrementInSeconds();

    if (extend)
    {
        // Bidirectional selection extension - active edge moves, anchor stays fixed
        if (!m_hasSelection)
        {
            // Start new selection from cursor position
            // Anchor is the starting position, active edge moves left from there
            m_selectionAnchor = m_hasEditCursor ? m_editCursorPosition : m_playbackPosition;
            m_isExtendingSelection = true;
            double activePos = juce::jmax(0.0, m_selectionAnchor - delta);
            setSelection(m_selectionAnchor, activePos);  // setSelection normalizes start < end
        }
        else
        {
            // Extend existing selection
            if (!m_isExtendingSelection)
            {
                // First extend - anchor is left edge (start), active is right edge (end)
                // This means Shift+Arrow moves the right edge by default
                m_selectionAnchor = m_selectionStart;
                m_isExtendingSelection = true;
            }

            // Calculate active position by getting current active edge and moving it
            // Active edge is the one that's NOT the anchor
            double currentActive;
            if (std::abs(m_selectionStart - m_selectionAnchor) < 0.001)
            {
                // Start is anchor, so end is active
                currentActive = m_selectionEnd;
            }
            else
            {
                // End is anchor, so start is active
                currentActive = m_selectionStart;
            }

            // Move active position left by delta
            double newActive = juce::jmax(0.0, currentActive - delta);

            // Skip zero-width selection: if active edge would land within one increment of anchor,
            // jump directly to one increment on the other side to avoid invisible selection
            if (std::abs(newActive - m_selectionAnchor) < delta)
            {
                // Would create zero or near-zero selection, jump to other side
                newActive = juce::jmax(0.0, m_selectionAnchor - delta);
            }

            // Set selection with anchor and new active position
            setSelection(m_selectionAnchor, newActive);
        }
    }
    else
    {
        // Move cursor left (not extending selection)
        m_isExtendingSelection = false;  // Clear extending flag

        double newPos = m_hasEditCursor ? m_editCursorPosition : m_playbackPosition;
        newPos = juce::jmax(0.0, newPos - delta);
        setEditCursor(newPos);
        clearSelection();

        // Auto-scroll if needed
        if (newPos < m_visibleStart)
        {
            double viewDuration = m_visibleEnd - m_visibleStart;
            setVisibleRange(newPos, newPos + viewDuration);
        }
    }
}

void WaveformDisplay::navigateRight(bool extend)
{
    if (!m_fileLoaded)
        return;

    // Get increment from current snap mode (arrow keys now honor snap mode)
    double delta = getSnapIncrementInSeconds();

    if (extend)
    {
        // Bidirectional selection extension - active edge moves, anchor stays fixed
        if (!m_hasSelection)
        {
            // Start new selection from cursor position
            // Anchor is the starting position, active edge moves right from there
            m_selectionAnchor = m_hasEditCursor ? m_editCursorPosition : m_playbackPosition;
            m_isExtendingSelection = true;
            double activePos = juce::jmin(m_totalDuration, m_selectionAnchor + delta);
            setSelection(m_selectionAnchor, activePos);  // setSelection normalizes start < end
        }
        else
        {
            // Extend existing selection
            if (!m_isExtendingSelection)
            {
                // First extend - anchor is left edge (start), active is right edge (end)
                // This means Shift+Arrow moves the right edge by default
                m_selectionAnchor = m_selectionStart;
                m_isExtendingSelection = true;
            }

            // Calculate active position by getting current active edge and moving it
            // Active edge is the one that's NOT the anchor
            double currentActive;
            if (std::abs(m_selectionStart - m_selectionAnchor) < 0.001)
            {
                // Start is anchor, so end is active
                currentActive = m_selectionEnd;
            }
            else
            {
                // End is anchor, so start is active
                currentActive = m_selectionStart;
            }

            // Move active position right by delta
            double newActive = juce::jmin(m_totalDuration, currentActive + delta);

            // Skip zero-width selection: if active edge would land within one increment of anchor,
            // jump directly to one increment on the other side to avoid invisible selection
            if (std::abs(newActive - m_selectionAnchor) < delta)
            {
                // Would create zero or near-zero selection, jump to other side
                newActive = juce::jmin(m_totalDuration, m_selectionAnchor + delta);
            }

            // Set selection with anchor and new active position
            setSelection(m_selectionAnchor, newActive);
        }
    }
    else
    {
        // Move cursor right (not extending selection)
        m_isExtendingSelection = false;  // Clear extending flag

        double newPos = m_hasEditCursor ? m_editCursorPosition : m_playbackPosition;
        newPos = juce::jmin(m_totalDuration, newPos + delta);
        setEditCursor(newPos);
        clearSelection();

        // Auto-scroll if needed
        if (newPos > m_visibleEnd)
        {
            double viewDuration = m_visibleEnd - m_visibleStart;
            setVisibleRange(newPos - viewDuration, newPos);
        }
    }
}

void WaveformDisplay::navigateToStart(bool extend)
{
    if (!m_fileLoaded)
        return;

    if (extend)
    {
        // Extend selection to start
        if (m_hasSelection)
        {
            setSelection(0.0, m_selectionEnd);
        }
        else if (m_hasEditCursor)
        {
            setSelection(0.0, m_editCursorPosition);
        }
    }
    else
    {
        // Jump cursor to start
        setEditCursor(0.0);
        clearSelection();

        // Scroll to show start
        double viewDuration = m_visibleEnd - m_visibleStart;
        setVisibleRange(0.0, viewDuration);
    }
}

void WaveformDisplay::navigateToEnd(bool extend)
{
    if (!m_fileLoaded)
        return;

    if (extend)
    {
        // Extend selection to end
        if (m_hasSelection)
        {
            setSelection(m_selectionStart, m_totalDuration);
        }
        else if (m_hasEditCursor)
        {
            setSelection(m_editCursorPosition, m_totalDuration);
        }
    }
    else
    {
        // Jump cursor to end
        setEditCursor(m_totalDuration);
        clearSelection();

        // Scroll to show end
        double viewDuration = m_visibleEnd - m_visibleStart;
        setVisibleRange(m_totalDuration - viewDuration, m_totalDuration);
    }
}

void WaveformDisplay::navigatePageLeft(bool extend)
{
    if (!m_fileLoaded)
        return;

    // Get page increment (default: 1000ms = 1 second)
    int increment = m_navigationPrefs.getNavigationIncrementPage();
    double delta = increment / 1000.0; // Convert ms to seconds

    if (extend)
    {
        // Bidirectional selection extension by page increment
        if (!m_hasSelection)
        {
            // Start new selection from cursor
            m_selectionAnchor = m_hasEditCursor ? m_editCursorPosition : m_playbackPosition;
            m_isExtendingSelection = true;
            double activePos = juce::jmax(0.0, m_selectionAnchor - delta);
            setSelection(m_selectionAnchor, activePos);
        }
        else
        {
            if (!m_isExtendingSelection)
            {
                // First extend - anchor is left edge (start), active is right edge (end)
                m_selectionAnchor = m_selectionStart;
                m_isExtendingSelection = true;
            }

            // Get current active position (the edge that's NOT the anchor)
            double currentActive = (std::abs(m_selectionStart - m_selectionAnchor) < 0.001)
                                   ? m_selectionEnd : m_selectionStart;

            // Move active position left by page delta
            double newActive = juce::jmax(0.0, currentActive - delta);

            // Skip zero-width selection: if would land within one increment of anchor, jump over
            if (std::abs(newActive - m_selectionAnchor) < delta)
            {
                newActive = juce::jmax(0.0, m_selectionAnchor - delta);
            }

            setSelection(m_selectionAnchor, newActive);
        }
    }
    else
    {
        // Move cursor left by page increment
        m_isExtendingSelection = false;
        double newPos = m_hasEditCursor ? m_editCursorPosition : m_playbackPosition;
        newPos = juce::jmax(0.0, newPos - delta);
        setEditCursor(newPos);
        clearSelection();

        // Auto-scroll if needed
        if (newPos < m_visibleStart)
        {
            double viewDuration = m_visibleEnd - m_visibleStart;
            setVisibleRange(newPos, newPos + viewDuration);
        }
    }
}

void WaveformDisplay::navigatePageRight(bool extend)
{
    if (!m_fileLoaded)
        return;

    // Get page increment (default: 1000ms = 1 second)
    int increment = m_navigationPrefs.getNavigationIncrementPage();
    double delta = increment / 1000.0; // Convert ms to seconds

    if (extend)
    {
        // Bidirectional selection extension by page increment
        if (!m_hasSelection)
        {
            // Start new selection from cursor
            m_selectionAnchor = m_hasEditCursor ? m_editCursorPosition : m_playbackPosition;
            m_isExtendingSelection = true;
            double activePos = juce::jmin(m_totalDuration, m_selectionAnchor + delta);
            setSelection(m_selectionAnchor, activePos);
        }
        else
        {
            if (!m_isExtendingSelection)
            {
                // First extend - anchor is left edge (start), active is right edge (end)
                m_selectionAnchor = m_selectionStart;
                m_isExtendingSelection = true;
            }

            // Get current active position (the edge that's NOT the anchor)
            double currentActive = (std::abs(m_selectionStart - m_selectionAnchor) < 0.001)
                                   ? m_selectionEnd : m_selectionStart;

            // Move active position right by page delta
            double newActive = juce::jmin(m_totalDuration, currentActive + delta);

            // Skip zero-width selection: if would land within one increment of anchor, jump over
            if (std::abs(newActive - m_selectionAnchor) < delta)
            {
                newActive = juce::jmin(m_totalDuration, m_selectionAnchor + delta);
            }

            setSelection(m_selectionAnchor, newActive);
        }
    }
    else
    {
        // Move cursor right by page increment
        m_isExtendingSelection = false;
        double newPos = m_hasEditCursor ? m_editCursorPosition : m_playbackPosition;
        newPos = juce::jmin(m_totalDuration, newPos + delta);
        setEditCursor(newPos);
        clearSelection();

        // Auto-scroll if needed
        if (newPos > m_visibleEnd)
        {
            double viewDuration = m_visibleEnd - m_visibleStart;
            setVisibleRange(newPos - viewDuration, newPos);
        }
    }
}

void WaveformDisplay::navigateToVisibleStart(bool extend)
{
    if (!m_fileLoaded)
        return;

    if (extend)
    {
        // Extend selection to visible start
        if (m_hasSelection)
        {
            setSelection(m_visibleStart, m_selectionEnd);
        }
        else if (m_hasEditCursor)
        {
            setSelection(m_visibleStart, m_editCursorPosition);
        }
    }
    else
    {
        // Jump cursor to visible start
        setEditCursor(m_visibleStart);
        clearSelection();
    }
}

void WaveformDisplay::navigateToVisibleEnd(bool extend)
{
    if (!m_fileLoaded)
        return;

    if (extend)
    {
        // Extend selection to visible end
        if (m_hasSelection)
        {
            setSelection(m_selectionStart, m_visibleEnd);
        }
        else if (m_hasEditCursor)
        {
            setSelection(m_editCursorPosition, m_visibleEnd);
        }
    }
    else
    {
        // Jump cursor to visible end
        setEditCursor(m_visibleEnd);
        clearSelection();
    }
}

void WaveformDisplay::centerViewOnCursor()
{
    if (!m_fileLoaded)
        return;

    // Get center position (prefer edit cursor, then selection, then playback)
    double centerTime = 0.0;
    if (m_hasEditCursor)
    {
        centerTime = m_editCursorPosition;
    }
    else if (m_hasSelection)
    {
        centerTime = (m_selectionStart + m_selectionEnd) * 0.5;
    }
    else
    {
        centerTime = m_playbackPosition;
    }

    // Center view on this time
    double viewDuration = m_visibleEnd - m_visibleStart;
    m_visibleStart = centerTime - viewDuration * 0.5;
    m_visibleEnd = centerTime + viewDuration * 0.5;

    constrainVisibleRange();
    updateScrollbar();
    repaint();

    // Notify listeners (RegionDisplay) of visible range change
    if (onVisibleRangeChanged)
        onVisibleRangeChanged(m_visibleStart, m_visibleEnd);
}

void WaveformDisplay::setVisibleRange(double startTime, double endTime)
{
    m_visibleStart = startTime;
    m_visibleEnd = endTime;
    constrainVisibleRange();
    updateScrollbar();
    repaint();

    // Notify listeners (RegionDisplay) of visible range change
    if (onVisibleRangeChanged)
        onVisibleRangeChanged(m_visibleStart, m_visibleEnd);
}

//==============================================================================
// Component overrides

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

    // Check for channel label clicks (solo/mute/context menu)
    juce::Point<int> clickPos(event.x, event.y);
    for (int ch = 0; ch < m_numChannels && ch < 8; ++ch)
    {
        if (m_channelLabelBounds[static_cast<size_t>(ch)].contains(clickPos))
        {
            // Right-click (or Ctrl+click on Mac) = show context menu
            if (event.mods.isPopupMenu())
            {
                showChannelContextMenu(ch, event.getScreenPosition());
                return;
            }
            // Alt+click (Option on Mac) = toggle mute
            else if (event.mods.isAltDown())
            {
                if (onChannelMuteChanged && getChannelMute)
                {
                    bool currentMute = getChannelMute(ch);
                    onChannelMuteChanged(ch, !currentMute);
                    repaint();
                }
            }
            // Regular click = toggle solo
            else
            {
                if (onChannelSoloChanged && getChannelSolo)
                {
                    bool currentSolo = getChannelSolo(ch);
                    onChannelSoloChanged(ch, !currentSolo);
                    repaint();
                }
            }
            return; // Don't start selection on label click
        }
    }

    // Selection-edge resize: if there's an existing selection and the
    // click landed within a few pixels of either edge, drag-resizes
    // that edge instead of starting a fresh selection.
    m_resizingLeftEdge  = false;
    m_resizingRightEdge = false;
    if (m_hasSelection)
    {
        const int selStartX = timeToX(m_selectionStart);
        const int selEndX   = timeToX(m_selectionEnd);
        if (std::abs(event.x - selStartX) <= kEdgeHandleHaloPx)
        {
            m_resizingLeftEdge = true;
            m_resizeAnchor     = m_selectionEnd;
        }
        else if (std::abs(event.x - selEndX) <= kEdgeHandleHaloPx)
        {
            m_resizingRightEdge = true;
            m_resizeAnchor      = m_selectionStart;
        }
    }

    // Start selection drag
    m_isDraggingSelection = true;
    m_isExtendingSelection = false;  // Clear keyboard extension flag when mouse selecting
    // CRITICAL FIX (2025-10-07): Clamp coordinates for consistency with mouseDrag
    int clampedX = juce::jlimit(0, getWidth() - 1, event.x);
    double approximateTime = xToTime(clampedX);

    // CRITICAL: Sample-accurate snapping (audio-unit based, not pixel-based)
    m_dragStartTime = snapTimeToUnit(approximateTime);

    // If we're resizing an edge, leave the selection intact and let
    // mouseDrag pivot it around the anchor.
    if (m_resizingLeftEdge || m_resizingRightEdge)
        return;

    // Only clear selection if clicking OUTSIDE of it
    // This allows user to set playback cursor without losing selection
    if (!m_hasSelection ||
        m_dragStartTime < m_selectionStart ||
        m_dragStartTime > m_selectionEnd)
    {
        // Clicked outside selection - clear it and start new drag
        clearSelection();
    }

    // Set edit cursor on click (will be cleared if user drags)
    setEditCursor(m_dragStartTime);
}

void WaveformDisplay::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (!m_fileLoaded)
        return;

    // Ignore double-clicks on scrollbar or ruler
    if (event.y < RULER_HEIGHT || event.y > getHeight() - SCROLLBAR_HEIGHT)
        return;

    // Check for double-click on channel labels - toggle focus for that channel
    juce::Point<int> clickPos(event.x, event.y);
    for (int ch = 0; ch < m_numChannels && ch < 8; ++ch)
    {
        if (m_channelLabelBounds[static_cast<size_t>(ch)].contains(clickPos))
        {
            // Double-click on channel label toggles focus:
            // - If this is the only focused channel, go back to "all channels"
            // - Otherwise, focus only this channel
            if (m_focusedChannels == (1 << ch))
            {
                // Already focused on just this channel - unfocus (return to all)
                focusChannel(-1);
                DBG(juce::String::formatted(
                    "Double-click label: Unfocused channel %d, now ALL channels focused", ch));
            }
            else
            {
                // Focus only this channel
                focusChannel(ch);
                DBG(juce::String::formatted(
                    "Double-click label: Focused channel %d only", ch));
            }
            repaint();
            return;
        }
    }

    // Determine which channel was double-clicked (in waveform area)
    int channel = getChannelAtY(event.y);

    if (channel >= 0)
    {
        // Double-clicked inside a specific channel waveform
        // Toggle focus: if already focused on just this channel, unfocus
        if (m_focusedChannels == (1 << channel))
        {
            // Already focused on just this channel - unfocus (return to all)
            focusChannel(-1);
            DBG(juce::String::formatted(
                "Double-click waveform: Unfocused channel %d, now ALL channels focused, selected entire duration (%.3f sec)",
                channel, m_totalDuration));
        }
        else
        {
            // Focus this channel
            focusChannel(channel);
            DBG(juce::String::formatted(
                "Double-click waveform: Focused channel %d, selected entire duration (%.3f sec)",
                channel, m_totalDuration));
        }
    }
    else
    {
        // Double-clicked in gap between channels (or mono file)
        // Focus all channels and select entire duration
        focusChannel(-1);

        DBG(juce::String::formatted(
            "Double-click: Focused ALL channels, selected entire duration (%.3f sec)",
            m_totalDuration));
    }

    // Select entire duration
    setSelection(0.0, m_totalDuration);

    // Clear edit cursor since we have a selection
    clearEditCursor();
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
    double approximateTime = xToTime(clampedX);

    // CRITICAL: Sample-accurate snapping (audio-unit based, not pixel-based)
    double currentTime = snapTimeToUnit(approximateTime);

    // Edge-resize gesture: pivot the selection around the opposite edge
    // (m_resizeAnchor). setSelection normalizes start < end so the user
    // can even drag past the anchor to flip the active edge.
    if (m_resizingLeftEdge || m_resizingRightEdge)
    {
        clearEditCursor();
        setSelection(m_resizeAnchor, currentTime);
        return;
    }

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
    const bool wasResizing = m_resizingLeftEdge || m_resizingRightEdge;
    m_resizingLeftEdge  = false;
    m_resizingRightEdge = false;

    // Resize gestures keep whatever the user landed on, even if zero-width
    // (clicking the edge without dragging just preserves the selection).
    if (wasResizing)
        return;

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

    DBG(juce::String::formatted(
        "Mouse wheel: deltaY=%.3f, current view: %.2f-%.2f",
        wheel.deltaY, m_visibleStart, m_visibleEnd));

    if (wheel.deltaY > 0.0f)
    {
        // Zoom in
        visibleDuration *= 0.8;
        DBG("Zooming IN");
    }
    else if (wheel.deltaY < 0.0f)
    {
        // Zoom out
        visibleDuration *= 1.25;
        DBG("Zooming OUT");
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

    // Notify listeners of visible range change (e.g., RegionDisplay needs to update)
    if (onVisibleRangeChanged)
        onVisibleRangeChanged(m_visibleStart, m_visibleEnd);

    DBG(juce::String::formatted(
        "After zoom: view: %.2f-%.2f",
        m_visibleStart, m_visibleEnd));
}

//==============================================================================
// Timer callback for selection animation

